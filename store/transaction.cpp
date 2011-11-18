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
#include <typeinfo>

#include "engine/action.hpp"
#include "engine/atf_iface/test_program.hpp"
#include "engine/context.hpp"
#include "engine/plain_iface/test_program.hpp"
#include "engine/test_result.hpp"
#include "store/backend.hpp"
#include "store/exceptions.hpp"
#include "store/transaction.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.hpp"
#include "utils/sqlite/transaction.hpp"

namespace atf_iface = engine::atf_iface;
namespace fs = utils::fs;
namespace sqlite = utils::sqlite;
namespace plain_iface = engine::plain_iface;


namespace {


/// Retrieves the environment variables of a context.
///
/// \param db The SQLite database.
/// \param context_id The identifier of the context.
///
/// \return The environment variables of the specified context.
///
/// \throw sqlite::error If there is a problem storing the variables.
static std::map< std::string, std::string >
get_env_vars(sqlite::database& db, const int64_t context_id)
{
    std::map< std::string, std::string > env;

    sqlite::statement stmt = db.create_statement(
        "SELECT var_name, var_value FROM env_vars "
        "WHERE context_id == :context_id");
    stmt.bind_int64(":context_id", context_id);

    while (stmt.step()) {
        const std::string name = stmt.safe_column_text("var_name");
        const std::string value = stmt.safe_column_text("var_value");
        env[name] = value;
    }

    return env;
}


/// Retrieves a result from the database.
///
/// \param stmt The statement with the data for the result to load.
/// \param type_column The name of the column containing the type of the result.
/// \param reason_column The name of the column containing the reason for the
///     result, if any.
///
/// \return The loaded result.
///
/// \throw integrity_error If the data in the database is invalid.
static engine::test_result
parse_result(sqlite::statement& stmt, const char* type_column,
             const char* reason_column)
{
    using engine::test_result;

    try {
        const std::string type = stmt.safe_column_text(type_column);
        if (type == "passed") {
            if (stmt.column_type(stmt.column_id(reason_column)) !=
                sqlite::type_null)
                throw store::integrity_error("Result of type 'passed' has a "
                                             "non-NULL reason");
            return test_result(test_result::passed);
        } else if (type == "broken") {
            return test_result(test_result::broken,
                               stmt.safe_column_text(reason_column));
        } else if (type == "expected_failure") {
            return test_result(test_result::expected_failure, 
                               stmt.safe_column_text(reason_column));
        } else if (type == "failed") {
            return test_result(test_result::failed,
                               stmt.safe_column_text(reason_column));
        } else if (type == "skipped") {
            return test_result(test_result::skipped,
                               stmt.safe_column_text(reason_column));
        } else {
            throw store::integrity_error(F("Unknown test result type %s") %
                                         type);
        }
    } catch (const sqlite::error& e) {
        throw store::integrity_error(e.what());
    }
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


/// Stores the metadata variables of a test case.
///
/// \param db The SQLite database.
/// \param test_case_id The identifier of the test case.
/// \param metadata The metadata properties.
///
/// \throw sqlite::error If there is a problem storing the variables.
static void
put_metadata(sqlite::database& db, const int64_t test_case_id,
             const engine::properties_map& metadata)
{
    sqlite::statement stmt = db.create_statement(
        "INSERT INTO test_cases_metadata (test_case_id, var_name, var_value) "
        "VALUES (:test_case_id, :var_name, :var_value)");
    stmt.bind_int64(":test_case_id", test_case_id);
    for (engine::properties_map::const_iterator iter = metadata.begin();
         iter != metadata.end(); iter++) {
        stmt.bind_text(":var_name", (*iter).first);
        stmt.bind_text(":var_value", (*iter).second);
        stmt.step_without_results();
        stmt.reset();
    }
}


}  // anonymous namespace


/// Internal implementation for a results iterator.
struct store::results_iterator::impl {
    /// The statement to iterate on.
    sqlite::statement _stmt;

    /// Whether the iterator is still valid or not.
    bool _valid;

    /// Constructor.
    impl(sqlite::database& db_, const int64_t action_id_) :
        _stmt(db_.create_statement(
                  "SELECT test_programs.binary_path, test_cases.name, "
                  "    test_results.result_type, test_results.result_reason "
                  "FROM test_programs NATURAL JOIN test_cases "
                  "    NATURAL JOIN test_results "
                  "WHERE test_programs.action_id == :action_id"))
    {
        _stmt.bind_int64(":action_id", action_id_);
        _valid = _stmt.step();
    }
};


/// Constructor.
///
/// \param pimpl_ The internal implementation details of the iterator.
store::results_iterator::results_iterator(
    std::tr1::shared_ptr< impl > pimpl_) :
    _pimpl(pimpl_)
{
}


/// Destructor.
store::results_iterator::~results_iterator(void)
{
}


/// Moves the iterator forward by one result.
///
/// \return The iterator itself.
store::results_iterator&
store::results_iterator::operator++(void)
{
    _pimpl->_valid = _pimpl->_stmt.step();
    return *this;
}


/// Checks whether the iterator is still valid.
///
/// \return True if there is more elements to iterate on, false otherwise.
store::results_iterator::operator bool(void) const
{
    return _pimpl->_valid;
}


/// Gets the absolute path to the test program pointed by the iterator.
///
/// \return An absolute path.
fs::path
store::results_iterator::binary_path(void) const
{
    return fs::path(_pimpl->_stmt.safe_column_text("binary_path"));
}


/// Gets the name of the test case pointed by the iterator.
///
/// \return A test case name, unique within the test program.
std::string
store::results_iterator::test_case_name(void) const
{
    return _pimpl->_stmt.safe_column_text("name");
}


/// Gets the result of the test case pointed by the iterator.
///
/// \return A test case result.
engine::test_result
store::results_iterator::result(void) const
{
    return parse_result(_pimpl->_stmt, "result_type", "result_reason");
}


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


/// Retrieves an action from the database.
///
/// \param action_id The identifier of the action to retrieve.
///
/// \return The retrieved action.
///
/// \throw error If there is a problem loading the action.
engine::action
store::transaction::get_action(const int64_t action_id)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "SELECT context_id FROM actions WHERE action_id == :action_id");
        stmt.bind_int64(":action_id", action_id);
        if (!stmt.step())
            throw error(F("Error loading action %d: does not exist") %
                        action_id);

        return engine::action(
            get_context(stmt.safe_column_int64("context_id")));
    } catch (const sqlite::error& e) {
        throw error(F("Error loading action %d: %s") % action_id % e.what());
    }
}


/// Creates a new iterator to scan the test results of an action.
///
/// \param action_id The identifier of the action for which to get the results.
///
/// \return The constructed iterator.
///
/// \throw error If there is any problem constructing the iterator.
store::results_iterator
store::transaction::get_action_results(const int64_t action_id)
{
    try {
        return results_iterator(std::tr1::shared_ptr< results_iterator::impl >(
           new results_iterator::impl(_pimpl->_db, action_id)));
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Retrieves the latest action from the database.
///
/// \return The retrieved action.
///
/// \throw error If there is a problem loading the action.
std::pair< int64_t, engine::action >
store::transaction::get_latest_action(void)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "SELECT action_id, context_id FROM actions WHERE "
            "action_id == (SELECT max(action_id) FROM actions)");
        if (!stmt.step())
            throw error("No actions in the database");

        const int64_t action_id = stmt.safe_column_int64("action_id");
        const engine::context context = get_context(
            stmt.safe_column_int64("context_id"));

        return std::pair< int64_t, engine::action >(
            action_id, engine::action(context));
    } catch (const sqlite::error& e) {
        throw error(F("Error loading latest action: %s") % e.what());
    }
}


/// Retrieves an context from the database.
///
/// \param context_id The identifier of the context to retrieve.
///
/// \return The retrieved context.
///
/// \throw error If there is a problem loading the context.
engine::context
store::transaction::get_context(const int64_t context_id)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "SELECT cwd FROM contexts WHERE context_id == :context_id");
        stmt.bind_int64(":context_id", context_id);
        if (!stmt.step())
            throw error(F("Error loading context %d: does not exist") %
                        context_id);

        return engine::context(fs::path(stmt.safe_column_text("cwd")),
                               get_env_vars(_pimpl->_db, context_id));
    } catch (const sqlite::error& e) {
        throw error(F("Error loading context %d: %s") % context_id % e.what());
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


/// Puts a test program into the database.
///
/// \pre The test program has not been put yet.
/// \post The test program is stored into the database with a new identifier.
///
/// \param test_program The test program to put.
/// \param action_id The action this test program belongs to.
///
/// \return The identifier of the inserted test program.
///
/// \throw error If there is any problem when talking to the database.
int64_t
store::transaction::put_test_program(
    const engine::base_test_program& test_program,
    const int64_t action_id)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO test_programs (action_id, binary_path, "
            "                           test_suite_name) "
            "VALUES (:action_id, :binary_path, :test_suite_name)");
        stmt.bind_int64(":action_id", action_id);
        const fs::path binary_path = test_program.absolute_path();
        stmt.bind_text(":binary_path", binary_path.is_absolute() ?
                       binary_path.str() : binary_path.to_absolute().str());
        stmt.bind_text(":test_suite_name", test_program.test_suite_name());
        stmt.step_without_results();
        const int64_t test_program_id = _pimpl->_db.last_insert_rowid();

        return test_program_id;
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Puts a test case into the database.
///
/// \pre The test case has not been put yet.
/// \post The test case is stored into the database with a new identifier.
///
/// \param test_case The test case to put.
/// \param test_program_id The test program this test case belongs to.
///
/// \return The identifier of the inserted test case.
///
/// \throw error If there is any problem when talking to the database.
int64_t
store::transaction::put_test_case(const engine::base_test_case& test_case,
                                  const int64_t test_program_id)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO test_cases (test_program_id, name) "
            "VALUES (:test_program_id, :name)");
        stmt.bind_int64(":test_program_id", test_program_id);
        stmt.bind_text(":name", test_case.name());
        stmt.step_without_results();
        const int64_t test_case_id = _pimpl->_db.last_insert_rowid();

        put_metadata(_pimpl->_db, test_case_id, test_case.all_properties());

        return test_case_id;
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}


/// Puts a result into the database.
///
/// \pre The result has not been put yet.
/// \post The result is stored into the database with a new identifier.
///
/// \param result The result to put.
/// \param test_case_id The test case this result corresponds to.
///
/// \return The identifier of the inserted result.
///
/// \throw error If there is any problem when talking to the database.
int64_t
store::transaction::put_result(const engine::test_result& result,
                               const int64_t test_case_id)
{
    try {
        sqlite::statement stmt = _pimpl->_db.create_statement(
            "INSERT INTO test_results (test_case_id, result_type, "
            "                          result_reason) "
            "VALUES (:test_case_id, :result_type, :result_reason)");
        stmt.bind_int64(":test_case_id", test_case_id);

        switch (result.type()) {
        case engine::test_result::broken:
            stmt.bind_text(":result_type", "broken");
            break;

        case engine::test_result::expected_failure:
            stmt.bind_text(":result_type", "expected_failure");
            break;

        case engine::test_result::failed:
            stmt.bind_text(":result_type", "failed");
            break;

        case engine::test_result::passed:
            stmt.bind_text(":result_type", "passed");
            break;

        case engine::test_result::skipped:
            stmt.bind_text(":result_type", "skipped");
            break;

        default:
            UNREACHABLE;
        }

        if (result.reason().empty())
            stmt.bind_null(":result_reason");
        else
            stmt.bind_text(":result_reason", result.reason());

        stmt.step_without_results();
        const int64_t result_id = _pimpl->_db.last_insert_rowid();

        return result_id;
    } catch (const sqlite::error& e) {
        throw error(e.what());
    }
}
