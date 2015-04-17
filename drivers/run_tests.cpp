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

#include <deque>

#include "engine/config.hpp"
#include "engine/filters.hpp"
#include "engine/kyuafile.hpp"
#include "engine/runner.hpp"
#include "engine/scanner.hpp"
#include "engine/scheduler.hpp"
#include "model/context.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "store/write_backend.hpp"
#include "store/write_transaction.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace runner = engine::runner;
namespace scheduler = engine::scheduler;

using utils::none;
using utils::optional;


namespace {


/// Puts a test program in the store and returns its identifier.
///
/// This function is idempotent: we maintain a side cache of already-put test
/// programs so that we can return their identifiers without having to put them
/// again.
/// TODO(jmmv): It's possible that the store module should offer this
/// functionality and not have to do this ourselves here.
///
/// \param test_program The test program being put.
/// \param [in,out] tx Writable transaction on the store.
/// \param [in,out] ids_cache Cache of already-put test programs.
///
/// \return A test program identifier.
static int64_t
find_test_program_id(const model::test_program_ptr test_program,
                     store::write_transaction& tx,
                     std::map< fs::path, int64_t > ids_cache)
{
    const fs::path& key = test_program->relative_path();
    std::map< fs::path, int64_t >::const_iterator iter = ids_cache.find(key);
    if (iter == ids_cache.end()) {
        const int64_t id = tx.put_test_program(*test_program);
        ids_cache.insert(std::make_pair(key, id));
        return id;
    } else {
        return (*iter).second;
    }
}


/// Stores the result of an execution in the database.
///
/// \param test_case_id Identifier of the test case in the database.
/// \param result The result of the execution.
/// \param [in,out] tx Writable transaction where to store the result data.
static void
put_test_result(const int64_t test_case_id,
                const scheduler::result_handle& result,
                store::write_transaction& tx)
{
    tx.put_result(result.test_result(), test_case_id,
                  result.start_time(), result.end_time());
    tx.put_test_case_file("__STDOUT__", result.stdout_file(), test_case_id);
    tx.put_test_case_file("__STDERR__", result.stderr_file(), test_case_id);

}


/// Cleans up a test case and folds any errors into the test result.
///
/// \param handle The result handle for the test.
///
/// \return The test result if the cleanup succeeds; a broken test result
/// otherwise.
model::test_result
safe_cleanup(scheduler::result_handle handle) throw()
{
    try {
        handle.cleanup();
        return handle.test_result();
    } catch (const std::exception& e) {
        return model::test_result(
            model::test_result_broken,
            F("Failed to clean up test case's work directory %s: %s") %
            handle.work_directory() % e.what());
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
/// \param filters The test case filters as provided by the user.
/// \param user_config The end-user configuration properties.
/// \param hooks The hooks for this execution.
///
/// \returns A structure with all results computed by this driver.
drivers::run_tests::result
drivers::run_tests::drive(const fs::path& kyuafile_path,
                          const optional< fs::path > build_root,
                          const fs::path& store_path,
                          const std::set< engine::test_filter >& filters,
                          const config::tree& user_config,
                          base_hooks& hooks)
{
    const engine::kyuafile kyuafile = engine::kyuafile::load(
        kyuafile_path, build_root, user_config);
    store::write_backend db = store::write_backend::open_rw(store_path);
    store::write_transaction tx = db.start_write();

    {
        const model::context context = runner::current_context();
        (void)tx.put_context(context);
    }

    // TODO(jmmv): The scanner currently does not handle interrupts, so if we
    // abort we probably do not clean up the directory in which test programs
    // are executed in list mode.  Should share interrupts handling between both
    // the executor and the scanner, or funnel the scanner operations via the
    // executor.
    scheduler::scheduler_handle handle = scheduler::setup();
    engine::scanner scanner(kyuafile.test_programs(), filters);

    // Map of test program identifiers (relative paths) to their identifiers in
    // the database.  We need to keep this in memory because test programs can
    // be returned by the scanner in any order, and we only want to put each
    // test program once.
    std::map< fs::path, int64_t > ids_cache;

    // Map of in-flight test cases to their identifiers in the database.
    std::map< scheduler::exec_handle, int64_t > in_flight;

    const std::size_t slots = user_config.lookup< config::positive_int_node >(
        "parallelism");
    INV(slots >= 1);
    do {
        INV(in_flight.size() <= slots);

        // Spawn as many jobs as needed to fill our execution slots.  We do this
        // first with the assumption that the spawning is faster than any single
        // job, so we want to keep as many jobs in the background as possible.
        while (in_flight.size() < slots) {
            optional< engine::scan_result > match = scanner.yield();
            if (!match)
                break;

            hooks.got_test_case(*match.get().first, match.get().second);

            const int64_t test_program_id = find_test_program_id(
                match.get().first, tx, ids_cache);
            const int64_t test_case_id = tx.put_test_case(
                *match.get().first, match.get().second, test_program_id);

            const scheduler::exec_handle exec_handle = handle.spawn_test(
                match.get().first, match.get().second, user_config);
            in_flight.insert(std::make_pair(exec_handle, test_case_id));
        }

        // If there are any used slots, consume any at random and return the
        // result.  We consume slots one at a time to give preference to the
        // spawning of new tests as detailed above.
        if (!in_flight.empty()) {
            scheduler::result_handle result = handle.wait_any_test();

            const std::map< scheduler::exec_handle, int64_t >::iterator
                iter = in_flight.find(result.original_exec_handle());
            const int64_t test_case_id = (*iter).second;
            in_flight.erase(iter);

            put_test_result(test_case_id, result, tx);

            const model::test_result test_result = safe_cleanup(result);
            hooks.got_result(*result.test_program(), result.test_case_name(),
                             result.test_result(),
                             result.end_time() - result.start_time());
        }
    } while (!in_flight.empty() || !scanner.done());

    tx.commit();

    handle.cleanup();

    return result(scanner.unused_filters());
}
