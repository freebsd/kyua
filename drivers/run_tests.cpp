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

#include "drivers/run_tests.hpp"

#include "engine/filters.hpp"
#include "engine/kyuafile.hpp"
#include "engine/runner.hpp"
#include "model/context.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "store/write_backend.hpp"
#include "store/write_transaction.hpp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/auto_cleaners.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/signals/interrupts.hpp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace signals = utils::signals;
namespace runner = engine::runner;

using utils::optional;


namespace {


/// Test case hooks to save the output into the database.
class file_saver_hooks : public runner::test_case_hooks {
    /// Open write transaction for the test case's data.
    store::write_transaction& _tx;

    /// Identifier of the test case being stored.
    const int64_t _test_case_id;

public:
    /// Constructs a new set of hooks.
    ///
    /// \param tx_ Open write transaction for the test case's data.
    /// \param test_case_id_ Identifier of the test case being stored.
    file_saver_hooks(store::write_transaction& tx_,
                     const int64_t test_case_id_) :
        _tx(tx_), _test_case_id(test_case_id_)
    {
    }

    /// Stores the stdout of the test case into the database.
    ///
    /// \param file Path to the stdout of the test case.
    void
    got_stdout(const fs::path& file)
    {
        _tx.put_test_case_file("__STDOUT__", file, _test_case_id);
    }

    /// Stores the stderr of the test case into the database.
    ///
    /// \param file Path to the stderr of the test case.
    void
    got_stderr(const fs::path& file)
    {
        _tx.put_test_case_file("__STDERR__", file, _test_case_id);
    }
};


/// Runs a test program in a controlled manner.
///
/// If the test program fails to provide a list of test cases, a fake test case
/// named '__test_program__' is created and it is reported as broken.
///
/// \param test_program The test program to execute.
/// \param user_config The configuration variables provided by the user.
/// \param filters The matching state of the filters.
/// \param hooks The user hooks to receive asynchronous notifications.
/// \param work_directory Temporary directory to use.
/// \param tx The store transaction into which to put the results.
void
run_test_program(model::test_program& test_program,
                 const config::tree& user_config,
                 engine::filters_state& filters,
                 drivers::run_tests::base_hooks& hooks,
                 const fs::path& work_directory,
                 store::write_transaction& tx)
{
    LI(F("Processing test program '%s'") % test_program.relative_path());
    const int64_t test_program_id = tx.put_test_program(test_program);

    const model::test_cases_map& test_cases = test_program.test_cases();
    for (model::test_cases_map::const_iterator iter = test_cases.begin();
         iter != test_cases.end(); iter++) {
        const std::string& test_case_name = (*iter).first;

        if (!filters.match_test_case(test_program.relative_path(),
                                     test_case_name))
            continue;

        const int64_t test_case_id = tx.put_test_case(test_program,
                                                      test_case_name,
                                                      test_program_id);
        file_saver_hooks test_hooks(tx, test_case_id);
        hooks.got_test_case(test_program, test_case_name);
        const datetime::timestamp start_time = datetime::timestamp::now();
        const model::test_result result = runner::run_test_case(
            &test_program, test_case_name, user_config, test_hooks,
            work_directory);
        const datetime::timestamp end_time = datetime::timestamp::now();
        tx.put_result(result, test_case_id, start_time, end_time);
        hooks.got_result(test_program, test_case_name, result,
                         end_time - start_time);

        signals::check_interrupt();
    }
}


}  // anonymous namespace


/// Pure abstract destructor.
drivers::run_tests::base_hooks::~base_hooks(void)
{
}


/// Executes the operation.
///
/// \param kyuafile_path The path to the Kyuafile to be loaded.
/// \param build_root If not none, path to the built test programs.
/// \param store_path The path to the store to be used.
/// \param raw_filters The test case filters as provided by the user.
/// \param user_config The end-user configuration properties.
/// \param hooks The hooks for this execution.
///
/// \returns A structure with all results computed by this driver.
drivers::run_tests::result
drivers::run_tests::drive(const fs::path& kyuafile_path,
                          const optional< fs::path > build_root,
                          const fs::path& store_path,
                          const std::set< engine::test_filter >& raw_filters,
                          const config::tree& user_config,
                          base_hooks& hooks)
{
    const engine::kyuafile kyuafile = engine::kyuafile::load(
        kyuafile_path, build_root);
    engine::filters_state filters(raw_filters);
    store::write_backend db = store::write_backend::open_rw(store_path);
    store::write_transaction tx = db.start_write();

    const model::context context = runner::current_context();
    (void)tx.put_context(context);

    signals::interrupts_handler interrupts;

    const fs::auto_directory work_directory = fs::auto_directory::mkdtemp(
        "kyua.XXXXXX");

    for (model::test_programs_vector::const_iterator iter =
         kyuafile.test_programs().begin();
         iter != kyuafile.test_programs().end(); iter++) {
        const model::test_program_ptr& test_program = *iter;

        if (!filters.match_test_program(test_program->relative_path()))
            continue;

        run_test_program(*test_program, user_config, filters, hooks,
                         work_directory.directory(), tx);

        signals::check_interrupt();
    }

    tx.commit();

    return result(filters.unused());
}
