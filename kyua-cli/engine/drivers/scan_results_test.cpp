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

#include "engine/drivers/scan_results.hpp"

#include <set>

#include <atf-c++.hpp>

#include "engine/context.hpp"
#include "engine/test_result.hpp"
#include "store/exceptions.hpp"
#include "store/read_transaction.hpp"
#include "store/write_backend.hpp"
#include "store/write_transaction.hpp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace scan_results = engine::drivers::scan_results;

using utils::none;
using utils::optional;


namespace {


/// Records the callback values for futher investigation.
class capture_hooks : public scan_results::base_hooks {
public:
    /// Whether begin() was called or not.
    bool _begin_called;

    /// The captured driver result, if any.
    optional< scan_results::result > _end_result;

    /// The captured context, if any.
    optional< engine::context > _context;

    /// The captured results, flattened as "program:test_case:result".
    std::set< std::string > _results;

    /// Constructor.
    capture_hooks(void) :
        _begin_called(false)
    {
    }

    /// Callback executed before any operation is performed.
    void
    begin(void)
    {
        _begin_called = true;
    }

    /// Callback executed after all operations are performed.
    ///
    /// \param r A structure with all results computed by this driver.  Note
    ///     that this is also returned by the drive operation.
    void
    end(const scan_results::result& r)
    {
        PRE(!_end_result);
        _end_result = r;
    }

    /// Callback executed when the context is loaded.
    ///
    /// \param context The context loaded from the database.
    void got_context(const engine::context& context)
    {
        PRE(!_context);
        _context = context;
    }

    /// Callback executed when a test results is found.
    ///
    /// \param iter Container for the test result's data.
    void got_result(store::results_iterator& iter)
    {
        const char* type;
        switch (iter.result().type()) {
        case engine::test_result::passed: type = "passed"; break;
        case engine::test_result::skipped: type = "skipped"; break;
        default:
            UNREACHABLE_MSG("Formatting unimplemented");
        }
        _results.insert(F("%s:%s:%s:%s:%s:%s") %
                        iter.test_program()->absolute_path() %
                        iter.test_case_name() % type % iter.result().reason() %
                        iter.duration().seconds % iter.duration().useconds);
    }
};


/// Populates a test database with a new action.
///
/// It is not OK to call this function multiple times on the same file.
///
/// \param db_name The database to update.
/// \param count A number that indicates how many elements to insert in the
///     action.  Can be used to determine from the caller which particular
///     action has been loaded.
static void
populate_db(const char* db_name, const int count)
{
    store::write_backend backend = store::write_backend::open_rw(
        fs::path(db_name));

    store::write_transaction tx = backend.start_write();

    std::map< std::string, std::string > env;
    for (int i = 0; i < count; i++)
        env[F("VAR%s") % i] = F("Value %s") % i;
    const engine::context context(fs::path("/root"), env);
    tx.put_context(context);

    for (int i = 0; i < count; i++) {
        const engine::test_program test_program(
            "plain", fs::path(F("dir/prog_%s") % i), fs::path("/root"),
            F("suite_%s") % i, engine::metadata_builder().build());
        const int64_t tp_id = tx.put_test_program(test_program);

        for (int j = 0; j < count; j++) {
            const engine::test_case test_case(
                "plain", test_program, "main",
                engine::metadata_builder().build());
            const engine::test_result result(engine::test_result::skipped,
                                             F("Count %s") % j);
            const int64_t tc_id = tx.put_test_case(test_case, tp_id);
            const datetime::timestamp start =
                datetime::timestamp::from_microseconds(1000010);
            const datetime::timestamp end =
                datetime::timestamp::from_microseconds(5000020 + i + j);
            tx.put_result(result, tc_id, start, end);
        }
    }

    tx.commit();
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(ok);
ATF_TEST_CASE_BODY(ok)
{
    populate_db("test.db", 2);

    capture_hooks hooks;
    scan_results::drive(fs::path("test.db"), hooks);
    ATF_REQUIRE(hooks._begin_called);
    ATF_REQUIRE(hooks._end_result);

    std::map< std::string, std::string > env;
    env["VAR0"] = "Value 0";
    env["VAR1"] = "Value 1";
    const engine::context context(fs::path("/root"), env);
    ATF_REQUIRE(context == hooks._context.get());

    std::set< std::string > results;
    results.insert("/root/dir/prog_0:main:skipped:Count 0:4:10");
    results.insert("/root/dir/prog_0:main:skipped:Count 1:4:11");
    results.insert("/root/dir/prog_1:main:skipped:Count 0:4:11");
    results.insert("/root/dir/prog_1:main:skipped:Count 1:4:12");
    ATF_REQUIRE(results == hooks._results);
}


ATF_TEST_CASE_WITHOUT_HEAD(missing_db);
ATF_TEST_CASE_BODY(missing_db)
{
    capture_hooks hooks;
    ATF_REQUIRE_THROW(store::error,
                      scan_results::drive(fs::path("test.db"), hooks));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ok);
    ATF_ADD_TEST_CASE(tcs, missing_db);
}
