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
#include "utils/defs.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.hpp"
#include "utils/sqlite/transaction.hpp"

namespace sqlite = utils::sqlite;


namespace {


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

    /// Opens a transaction.
    ///
    /// \param backend_ The backend this transaction is connected to.
    impl(backend& backend_) :
        _db(backend_.database()),
        _tx(backend_.database().begin_transaction())
    {
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
    try {
        _pimpl->_tx.commit();
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
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
/// \param unused_action The action to put.
/// \param context_id The identifier for the action's context.
///
/// \return The identifier of the inserted action.
///
/// \throw error If there is any problem when talking to the database.
int64_t
store::transaction::put_action(const engine::action& UTILS_UNUSED_PARAM(action),
                               const int64_t context_id)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO actions (context_id) VALUES (:context_id)");
        stmt.bind_int64(":context_id", context_id);
        stmt.step_without_results();
        const int64_t action_id = _pimpl->_db.last_insert_rowid();

        return action_id;
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
/// \return The identifier of the inserted context.
///
/// \throw error If there is any problem when talking to the database.
int64_t
store::transaction::put_context(const engine::context& context)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO contexts (cwd) VALUES (:cwd)");
        stmt.bind_text(":cwd", context.cwd().str());
        stmt.step_without_results();
        const int64_t context_id = _pimpl->_db.last_insert_rowid();

        put_env_vars(_pimpl->_db, context_id, context.env());

        return context_id;
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}
