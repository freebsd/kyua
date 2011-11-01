// Copyright 2011 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

extern "C" {
#include <stdint.h>
}

#include <map>

#include "engine/action.hpp"
#include "engine/context.hpp"
#include "store/backend.hpp"
#include "store/exceptions.hpp"
#include "store/transaction.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.hpp"
#include "utils/sqlite/transaction.hpp"

namespace sqlite = utils::sqlite;

using utils::none;
using utils::optional;


namespace {


/// Mapping between unique object addresses to OIDs.
///
/// Classes from the engine layer that want to be persistent need to expose a
/// unique_address() method that returns a unique value representing a
/// particular instance of the class.  Such unique value is derived from the
/// memory position storing the internal representation of the instance, which
/// is shared across all non-pointer instances to the same data.
///
/// \todo All this belongs in the backend abstraction because OID mappings are
/// database-specific.
typedef std::map< intptr_t, int64_t > oid_map;


/// Mapping of all known objects to their OIDs.
///
/// Because we use a transactional model in our backend store, we need to
/// overlay a separate oid_map to this one during the execution of a
/// transaction.
static oid_map committed_objects;


/// Finds the OID corresponding to an object.
///
/// Use transaction::impl::find_oid instead of this global function.
///
/// \param map The map from which to query the OID.
/// \param object The object to get the OID from.  Must implement the
///     unique_address() method.
///
/// \return The OID for the object if it is known; none otherwise.  Every object
/// in memory must have an OID in this map if the object was previously loaded
/// from the database.  Objects that do not have an OID yet are objects that
/// have yet to be stored in the database.
template< class T >
optional< int64_t >
find_oid(oid_map& map, const T& object)
{
    const oid_map::const_iterator iter = map.find(object.unique_address());
    if (iter == map.end())
        return none;
    else
        return optional< int64_t >((*iter).second);
}


/// Inserts an new mapping of an object to an OID into the global map.
///
/// Use transaction::impl::insert_oid instead of this global function.
///
/// \param map The map into which to insert the new OID.
/// \param object The object whose OID to store.  Must implement the
///     unique_address() method.
/// \param oid The new OID for the object.
template< class T >
static void
insert_oid(oid_map& map, const T& object, const int64_t oid)
{
    PRE(!find_oid(map, object));
    map[object.unique_address()] = oid;
}


/// Stores the environment variables of a context.
///
/// \param db The SQLite database.
/// \param context_id The identifier of the context.
/// \param env The environment variables to store.
///
/// \throw sqlite::error If there is a problem storing the variables.
static void
put_env_vars(sqlite::database& db, const int64_t context_id,
             const std::map< std::string, std::string >& env)
{
    sqlite::statement stmt = db.create_statement(
        "INSERT INTO env_vars (context_id, var_name, var_value) "
        "VALUES (:context_id, :var_name, :var_value)");
    stmt.bind_int64(":context_id", context_id);
    for (std::map< std::string, std::string >::const_iterator iter =
             env.begin(); iter != env.end(); iter++) {
        stmt.bind_text(":var_name", (*iter).first);
        stmt.bind_text(":var_value", (*iter).second);
        stmt.step_without_results();
        stmt.reset();
    }
}


}  // anonymous namespace


/// Internal implementation for a store transaction.
struct store::transaction::impl {
    /// The SQLite database this transaction deals with.
    sqlite::database _db;

    /// The backing SQLite transaction.
    sqlite::transaction _tx;

    /// The not-yet committed mappings that are part of this transaction.
    oid_map _objects;

    /// Opens a transaction.
    ///
    /// \param backend_ The backend this transaction is connected to.
    impl(backend& backend_) :
        _db(backend_.database()),
        _tx(backend_.database().begin_transaction())
    {
    }

    /// Finds the OID corresponding to an object.
    ///
    /// \param object The object to get the OID from.  Must implement the
    ///     unique_address() method.
    ///
    /// \return The OID for the object if it is known; none otherwise.  Every
    /// object in memory must have an OID in this map if the object was
    /// previously loaded from the database.  Objects that do not have an OID
    /// yet are objects that have yet to be stored in the database.
    template< class T >
    optional< int64_t >
    find_oid(const T& object)
    {
        const optional< int64_t > oid = ::find_oid(_objects, object);
        if (oid)
            return oid;
        else
            return ::find_oid(committed_objects, object);
    }

    /// Inserts an new mapping of an object to an OID into the global map.
    ///
    /// \param object The object whose OID to store.  Must implement the
    ///     unique_address() method.
    /// \param oid The new OID for the object.
    template< class T >
    void
    insert_oid(const T& object, const int64_t oid)
    {
        ::insert_oid(_objects, object, oid);
    }

    /// Commits the transaction.
    ///
    /// Aside from actually committing the changes to the database, this applies
    /// any pending object to OID mappings to the global table.
    ///
    /// \throw error If there is any problem when talking to the database.
    void
    commit(void)
    {
        try {
            _tx.commit();
        } catch (const sqlite::error& e) {
            throw error(e.what());
        }

        for (oid_map::const_iterator iter = _objects.begin();
             iter != _objects.end(); ++iter) {
            INV(committed_objects.find((*iter).first) ==
                committed_objects.end());
            committed_objects.insert(*iter);
        }
    }
};


/// Creates a new transaction.
///
/// \param backend_ The backend this transaction belongs to.
store::transaction::transaction(backend& backend_) :
    _pimpl(new impl(backend_))
{
}


/// Destructor.
store::transaction::~transaction(void)
{
}


/// Commits the transaction.
///
/// \throw error If there is any problem when talking to the database.
void
store::transaction::commit(void)
{
    _pimpl->commit();
}


/// Rolls the transaction back.
///
/// \throw error If there is any problem when talking to the database.
void
store::transaction::rollback(void)
{
    try {
        _pimpl->_tx.rollback();
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Puts an action into the database.
///
/// \pre The action has not been put yet.
/// \pre The dependent objects have already been put.
/// \post The action is stored into the database with a new identifier.
///
/// \param action The action to put.
///
/// \throw error If there is any problem when talking to the database.
void
store::transaction::put(const engine::action& action)
{
    const optional< int64_t > oid = _pimpl->find_oid(action);
    PRE_MSG(!oid, "Immutable object; cannot doubleput");

    const optional< int64_t > context_id = _pimpl->find_oid(action.context());
    PRE_MSG(context_id, "Context not yet stored");

    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO actions (context_id) VALUES (:context_id)");
        stmt.bind_int64(":context_id", context_id.get());
        stmt.step_without_results();

        _pimpl->insert_oid(action, _pimpl->_db.last_insert_rowid());
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Puts a context into the database.
///
/// \pre The context has not been put yet.
/// \post The context is stored into the database with a new identifier.
///
/// \param context The context to put.
///
/// \throw error If there is any problem when talking to the database.
void
store::transaction::put(const engine::context& context)
{
    const optional< int64_t > oid = _pimpl->find_oid(context);
    PRE_MSG(!oid, "Immutable object; cannot doubleput");

    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO contexts (cwd) VALUES (:cwd)");
        stmt.bind_text(":cwd", context.cwd().str());
        stmt.step_without_results();
        const int64_t context_id = _pimpl->_db.last_insert_rowid();

        put_env_vars(_pimpl->_db, context_id, context.env());

        _pimpl->insert_oid(context, context_id);
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}
