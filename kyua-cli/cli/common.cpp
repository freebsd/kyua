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

#include "cli/common.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "engine/filters.hpp"
#include "engine/test_case.hpp"
#include "engine/test_program.hpp"
#include "engine/test_result.hpp"
#include "store/layout.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace layout = store::layout;

using utils::none;
using utils::optional;


/// Standard definition of the option to specify the build root.
const cmdline::path_option cli::build_root_option(
    "build-root",
    "Path to the built test programs, if different from the location of the "
    "Kyuafile scripts",
    "path");


/// Standard definition of the option to specify a Kyuafile.
const cmdline::path_option cli::kyuafile_option(
    'k', "kyuafile",
    "Path to the test suite definition",
    "file", "Kyuafile");


/// Standard definition of the option to specify filters on test results.
const cmdline::list_option cli::results_filter_option(
    "results-filter", "Comma-separated list of result types to include in "
    "the report", "types", "skipped,xfail,broken,failed");


/// Standard definition of the option to specify the results file.
///
/// TODO(jmmv): Should support a git-like syntax to go back in time, like
/// --results-file=LATEST^N where N indicates how many runs to go back to.
const cmdline::path_option cli::results_file_option(
    'r', "results-file",
    "Path to the results-file database",
    "file", "LATEST");


namespace {


/// Constant that represents the path to stdout.
static const fs::path stdout_path("/dev/stdout");
/// Constant that represents the path to stderr.
static const fs::path stderr_path("/dev/stderr");


/// Converts a set of result type names to identifiers.
///
/// \param names The collection of names to process; may be empty.
///
/// \return The result type identifiers corresponding to the input names.
///
/// \throw std::runtime_error If any name in the input names is invalid.
static cli::result_types
parse_types(const std::vector< std::string >& names)
{
    using engine::test_result;
    typedef std::map< std::string, test_result::result_type > types_map;
    types_map valid_types;
    valid_types["broken"] = test_result::broken;
    valid_types["failed"] = test_result::failed;
    valid_types["passed"] = test_result::passed;
    valid_types["skipped"] = test_result::skipped;
    valid_types["xfail"] = test_result::expected_failure;

    cli::result_types types;
    for (std::vector< std::string >::const_iterator iter = names.begin();
         iter != names.end(); ++iter) {
        const types_map::const_iterator match = valid_types.find(*iter);
        if (match == valid_types.end())
            throw std::runtime_error(F("Unknown result type '%s'") % *iter);
        else
            types.push_back((*match).second);
    }
    return types;
}


}  // anonymous namespace


/// Opens a new file for output, respecting the stdout and stderr streams.
///
/// \param path The path to the output file to be created.
///
/// \return A pointer to a new output stream.
std::auto_ptr< std::ostream >
cli::open_output_file(const fs::path& path)
{
    std::auto_ptr< std::ostream > out;
    if (path == stdout_path) {
        // We should use ui->out() somehow to funnel all output via the ui
        // object, but it's not worth the hassle.  This would be tricky because
        // the ui object does not provide a stream-like interface, which is
        // arguably a shortcoming.
        out.reset(new std::ofstream());
        out->copyfmt(std::cout);
        out->clear(std::cout.rdstate());
        out->basic_ios< char >::rdbuf(std::cout.rdbuf());
    } else if (path == stderr_path) {
        // We should use ui->err() somehow to funnel all output via the ui
        // object, but it's not worth the hassle.  This would be tricky because
        // the ui object does not provide a stream-like interface, which is
        // arguably a shortcoming.
        out.reset(new std::ofstream());
        out->copyfmt(std::cerr);
        out->clear(std::cerr.rdstate());
        out->basic_ios< char >::rdbuf(std::cerr.rdbuf());
    } else {
        out.reset(new std::ofstream(path.c_str()));
        if (!(*out)) {
            throw std::runtime_error(F("Cannot open output file %s") % path);
        }
    }
    INV(out.get() != NULL);
    return out;
}


/// Gets the path to the build root, if any.
///
/// This is just syntactic sugar to simplify quierying the 'build_root_option'.
///
/// \param cmdline The parsed command line.
///
/// \return The path to the build root, if specified; none otherwise.
optional< fs::path >
cli::build_root_path(const cmdline::parsed_cmdline& cmdline)
{
    optional< fs::path > build_root;
    if (cmdline.has_option(build_root_option.long_name()))
        build_root = cmdline.get_option< cmdline::path_option >(
            build_root_option.long_name());
    return build_root;
}


/// Gets the path to the Kyuafile to be loaded.
///
/// This is just syntactic sugar to simplify quierying the 'kyuafile_option'.
///
/// \param cmdline The parsed command line.
///
/// \return The path to the Kyuafile to be loaded.
fs::path
cli::kyuafile_path(const cmdline::parsed_cmdline& cmdline)
{
    return cmdline.get_option< cmdline::path_option >(
        kyuafile_option.long_name());
}


/// Gets the path to the database file for a new action.
///
/// This has the side-effect of creating the directory in which to store the
/// database if and only if the path to the database matches the default value.
/// When the user does not specify an override for the location of the database,
/// he should not care about the directory existing.  Any of this is not a big
/// deal though, because logs are also stored within ~/.kyua and thus we will
/// most likely end up creating the directory anyway.
///
/// \param cmdline The parsed command line from which to extract any possible
///     override for the location of the database via the --results-file flag.
///
/// \return The path to the database to be used.
///
/// \throw fs::error If the creation of the database directory fails.
/// \throw store::error If the location of the database cannot be computed.
fs::path
cli::results_file_new(const cmdline::parsed_cmdline& cmdline)
{
    // We need the command line to include the --kyuafile flag because the path
    // to the Kyuafile defines the root of the test suite, and we need this
    // information when auto-determining the path to the database.
    PRE(cmdline.has_option(kyuafile_option.long_name()));

    fs::path store = cmdline.get_option< cmdline::path_option >(
        results_file_option.long_name());
    if (store == fs::path(results_file_option.default_value())) {
        optional< fs::path > home = utils::get_home();
        if (home) {
            const fs::path old_db = home.get() / ".kyua/store.db";
            if (fs::exists(old_db)) {
                if (old_db.is_absolute())
                    store = old_db;
                else
                    store = old_db.to_absolute();
            }
        }

        if (store == fs::path(results_file_option.default_value())) {
            const std::string test_suite = layout::test_suite_for_path(
                kyuafile_path(cmdline).branch_path());
            store = layout::new_db(test_suite);
            fs::mkdir_p(store.branch_path(), 0755);
        }
    }
    LI(F("Creating new store %s") % store);
    return store;
}


/// Gets the path to the database file for an existing action.
///
/// \param cmdline The parsed command line from which to extract any possible
///     override for the location of the database via the --results-file flag.
///
/// \return The path to the database to be used.
///
/// \throw store::error If the location of the database cannot be computed.
fs::path
cli::results_file_open(const cmdline::parsed_cmdline& cmdline)
{
    fs::path store = cmdline.get_option< cmdline::path_option >(
        results_file_option.long_name());
    if (store == fs::path(results_file_option.default_value())) {
        optional< fs::path > home = utils::get_home();
        if (home) {
            const fs::path old_db = home.get() / ".kyua/store.db";
            if (fs::exists(old_db)) {
                if (old_db.is_absolute())
                    store = old_db;
                else
                    store = old_db.to_absolute();
            }
        }

        if (store == fs::path(results_file_option.default_value())) {
            const std::string test_suite = layout::test_suite_for_path(
                fs::current_path());
            store = layout::find_latest(test_suite);
        }
    }
    LI(F("Opening existing store %s") % store);
    return store;
}


/// Gets the filters for the result types.
///
/// \param cmdline The parsed command line.
///
/// \return A collection of result types to be used for filtering.
///
/// \throw std::runtime_error If any of the user-provided filters is invalid.
cli::result_types
cli::get_result_types(const utils::cmdline::parsed_cmdline& cmdline)
{
    result_types types = parse_types(
        cmdline.get_option< cmdline::list_option >("results-filter"));
    if (types.empty()) {
        types.push_back(engine::test_result::passed);
        types.push_back(engine::test_result::skipped);
        types.push_back(engine::test_result::expected_failure);
        types.push_back(engine::test_result::broken);
        types.push_back(engine::test_result::failed);
    }
    return types;
}


/// Parses a set of command-line arguments to construct test filters.
///
/// \param args The command-line arguments representing test filters.
///
/// \throw cmdline:error If any of the arguments is invalid, or if they
///     represent a non-disjoint collection of filters.
std::set< engine::test_filter >
cli::parse_filters(const cmdline::args_vector& args)
{
    std::set< engine::test_filter > filters;

    try {
        for (cmdline::args_vector::const_iterator iter = args.begin();
             iter != args.end(); iter++) {
            const engine::test_filter filter(engine::test_filter::parse(*iter));
            if (filters.find(filter) != filters.end())
                throw cmdline::error(F("Duplicate filter '%s'") % filter.str());
            filters.insert(filter);
        }
        check_disjoint_filters(filters);
    } catch (const std::runtime_error& e) {
        throw cmdline::error(e.what());
    }

    return filters;
}


/// Reports the filters that have not matched any tests as errors.
///
/// \param unused The collection of unused filters to report.
/// \param ui The user interface object through which errors are to be reported.
///
/// \return True if there are any unused filters.  The caller should report this
/// as an error to the user by means of a non-successful exit code.
bool
cli::report_unused_filters(const std::set< engine::test_filter >& unused,
                           cmdline::ui* ui)
{
    for (std::set< engine::test_filter >::const_iterator iter = unused.begin();
         iter != unused.end(); iter++) {
        cmdline::print_warning(ui, F("No test cases matched by the filter '%s'")
                               % (*iter).str());
    }

    return !unused.empty();
}


/// Formats a time delta for user presentation.
///
/// \param delta The time delta to format.
///
/// \return A user-friendly representation of the time delta.
std::string
cli::format_delta(const datetime::delta& delta)
{
    return F("%.3ss") % (delta.seconds + (delta.useconds / 1000000.0));
}


/// Formats a test case result for user presentation.
///
/// \param result The result to format.
///
/// \return A user-friendly representation of the result.
std::string
cli::format_result(const engine::test_result& result)
{
    std::string text;

    using engine::test_result;
    switch (result.type()) {
    case test_result::broken: text = "broken"; break;
    case test_result::expected_failure: text = "expected_failure"; break;
    case test_result::failed: text = "failed"; break;
    case test_result::passed: text = "passed"; break;
    case test_result::skipped: text = "skipped"; break;
    }
    INV(!text.empty());

    if (!result.reason().empty())
        text += ": " + result.reason();

    return text;
}


/// Formats the identifier of a test case for user presentation.
///
/// \param test_case The test case whose identifier to format.
///
/// \return A string representing the test case uniquely within a test suite.
std::string
cli::format_test_case_id(const engine::test_case& test_case)
{
    return F("%s:%s") % test_case.container_test_program().relative_path() %
        test_case.name();
}


/// Formats a filter using the same syntax of a test case.
///
/// \param test_filter The filter to format.
///
/// \return A string representing the test filter.
std::string
cli::format_test_case_id(const engine::test_filter& test_filter)
{
    return F("%s:%s") % test_filter.test_program % test_filter.test_case;
}
