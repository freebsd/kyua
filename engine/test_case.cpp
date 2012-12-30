// Copyright 2010 Google Inc.
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

#include "engine/test_case.hpp"

extern "C" {
#include <signal.h>
}

#include <fstream>

#include "engine/exceptions.hpp"
#include "engine/isolation.ipp"
#include "engine/test_program.hpp"
#include "engine/test_result.hpp"
#include "engine/testers.hpp"
#include "engine/user_files/config.hpp"
#include "utils/config/exceptions.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/operations.hpp"
#include "utils/fs/auto_cleaners.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/process/children.ipp"
#include "utils/process/exceptions.hpp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace passwd = utils::passwd;
namespace process = utils::process;
namespace user_files = engine::user_files;

using utils::none;
using utils::optional;


namespace {


/// Extracts the value of 'unprivileged_user' from the configuration.
///
/// \param user_config The user configuration.
///
/// \return None if the user configuration does not define an unprivileged user,
/// or the unprivileged user itself if defined.
static optional< passwd::user >
get_unprivileged_user(const config::tree& user_config)
{
    if (!user_config.is_set("unprivileged_user"))
        return none;
    return utils::make_optional(
        user_config.lookup< user_files::user_node >("unprivileged_user"));
}


/// Converts a set of configuration variables to test program flags.
///
/// \param user_config The configuration variables provided by the user.
/// \param test_suite The name of the test suite.
/// \param args [out] The test program arguments in which to add the new flags.
static void
config_to_args(const config::tree& user_config,
               const std::string& test_suite,
               std::vector< std::string >& args)
{
    if (get_unprivileged_user(user_config))
        args.push_back(F("-vunprivileged-user=%s") %
                       get_unprivileged_user(user_config).get().name);

    try {
        const config::properties_map& properties = user_config.all_properties(
            F("test_suites.%s") % test_suite, true);
        for (config::properties_map::const_iterator iter = properties.begin();
             iter != properties.end(); iter++) {
            args.push_back(F("-v%s=%s") % (*iter).first % (*iter).second);
        }
    } catch (const config::unknown_key_error& unused_error) {
        // Ignore: not all test suites have entries in the configuration.
    }
}


/// Functor to execute a tester's run operation.
class run_test_case {
    /// Path to the tester binary.
    const fs::path _tester;

    /// Absolute path to the test program to run.
    const fs::path _program;

    /// Data of the test case to run.
    const engine::test_case& _test_case;

    /// Path to the result file to create.
    const fs::path _result_path;

    /// User-provided configuration variables.
    const config::tree& _user_config;

public:
    /// Constructor.
    ///
    /// \param interface Name of the interface of the tester.
    /// \param program Absolute path to the test program to run.
    /// \param test_case Data of the test case to run.
    /// \param result_path Path to the result file to create.
    /// \param user_config User-provided configuration variables.
    run_test_case(const std::string& interface, const fs::path& program,
                  const engine::test_case& test_case,
                  const fs::path& result_path,
                  const config::tree& user_config) :
        _tester(engine::tester_path(interface)), _program(program),
        _test_case(test_case), _result_path(result_path),
        _user_config(user_config)
    {
        PRE(_program.is_absolute());
    }

    /// Executes the tester.
    void
    operator()(void)
    {
        // We rely on parsing the output of the tester verbatim.  Disable any of
        // our own log messages so that they do not end up intermixed with such
        // output.
        logging::set_inmemory();

        std::vector< std::string > args;

        INV(_test_case.get_metadata().timeout().useconds == 0);
        args.push_back(F("-t%s") % _test_case.get_metadata().timeout().seconds);

        optional< passwd::user > unprivileged_user = get_unprivileged_user(
            _user_config);
        if (_test_case.get_metadata().required_user() == "unprivileged" &&
            unprivileged_user) {
            args.push_back(F("-u%s") % unprivileged_user.get().uid);
            args.push_back(F("-g%s") % unprivileged_user.get().gid);
        }

        args.push_back("test");
        config_to_args(_user_config,
                       _test_case.container_test_program().test_suite_name(),
                       args);

        // TODO(jmmv): This is an ugly hack to cope with an atf-specific
        // property.  We should not be doing this at all, so just consider this
        // a temporary optimization...
        if (_test_case.get_metadata().has_cleanup())
            args.push_back("-vhas.cleanup=true");
        else
            args.push_back("-vhas.cleanup=false");

        args.push_back(_program.str());
        args.push_back(_test_case.name());
        args.push_back(_result_path.str());
        process::exec(_tester, args);
    }
};


/// Functor to execute run_test_case() in a protected environment.
class run_test_case_safe {
    /// Data of the test case to run.
    const engine::test_case* _test_case;

    /// User-provided configuration variables.
    const config::tree& _user_config;

    /// Caller-provided runtime hooks.
    engine::test_case_hooks& _hooks;

    /// The file into which to store the test case's stdout.  If none, use a
    /// temporary file within the work directory.
    const optional< fs::path > _stdout_path;

    /// The file into which to store the test case's stderr.  If none, use a
    /// temporary file within the work directory.
    const optional< fs::path > _stderr_path;

public:
    /// Constructor.
    ///
    /// \param test_case Data of the test case to run.
    /// \param user_config User-provided configuration variables.
    /// \param hooks Caller-provided runtime hooks.
    /// \param stdout_path The file into which to store the test case's stdout.
    ///     If none, use a temporary file within the work directory.
    /// \param stderr_path The file into which to store the test case's stderr.
    ///     If none, use a temporary file within the work directory.
    run_test_case_safe(const engine::test_case* test_case,
                       const config::tree& user_config,
                       engine::test_case_hooks& hooks,
                       const optional< fs::path >& stdout_path,
                       const optional< fs::path >& stderr_path) :
        _test_case(test_case), _user_config(user_config), _hooks(hooks),
        _stdout_path(stdout_path), _stderr_path(stderr_path)
    {
    }

    /// Executes the test case.
    ///
    /// \param workdir Directory in which we are running.
    ///
    /// \return The result of the executed test case.
    engine::test_result
    operator()(const fs::path& workdir) const
    {
        const fs::path stdout_path =
            _stdout_path.get_default(workdir / "stdout.txt");
        const fs::path stderr_path =
            _stderr_path.get_default(workdir / "stderr.txt");

        const fs::path result_path = workdir / "result.txt";

        std::auto_ptr< process::child > child;
        process::status status = process::status::fake_exited(1);  // XXX
        try {
            child = process::child::fork_files(::run_test_case(
                _test_case->container_test_program().interface_name(),
                _test_case->container_test_program().absolute_path(),
                *_test_case, result_path, _user_config),
                stdout_path, stderr_path);

            status = child->wait();
        } catch (const process::error& e) {
            // TODO(jmmv): This really is horrible.  We ought to redo all the
            // signal handling, as this check_interrupt() aberration is racy and
            // ugly...
            //
            // We use SIGTERM because we assume the tester process is
            // well-behaved and this will cause the proper cleanup of the
            // environment.
            ::kill(child->pid(), SIGTERM);
            (void)child->wait();
            engine::check_interrupt();
            throw;
        }

        if (status.exited()) {
            if (status.exitstatus() == EXIT_SUCCESS) {
                // OK; the tester exited cleanly.
                // TODO(jmmv): We should validate that the result file encodes a
                // postive result.
            } else if (status.exitstatus() == EXIT_FAILURE) {
                // OK; the tester reported that the test itself failed and we
                // have the result file to indicate this.  TODO(jmmv): We should
                // validate that the result file encodes a negative result.
            } else {
                throw engine::error(F("Tester failed with code %s; that's "
                                      "really bad") % status.exitstatus());
            }
        } else {
            INV(status.signaled());
            throw engine::error("Tester received a signal; that's really bad");
        }

        _hooks.got_stdout(stdout_path);
        _hooks.got_stderr(stderr_path);

        std::ifstream result_file(result_path.c_str());
        const engine::test_result result = engine::test_result::parse(
            result_file);

        return result;
    }
};


}  // anonymous namespace


/// Destructor.
engine::test_case_hooks::~test_case_hooks(void)
{
}


/// Called once the test case's stdout is ready for processing.
///
/// It is important to note that this file is only available within this
/// callback.  Attempting to read the file once the execute function has
/// returned will result in an error because the file might have been deleted.
///
/// \param unused_file The path to the file containing the stdout.
void
engine::test_case_hooks::got_stdout(const fs::path& UTILS_UNUSED_PARAM(file))
{
}


/// Called once the test case's stderr is ready for processing.
///
/// It is important to note that this file is only available within this
/// callback.  Attempting to read the file once the execute function has
/// returned will result in an error because the file might have been deleted.
///
/// \param unused_file The path to the file containing the stderr.
void
engine::test_case_hooks::got_stderr(const fs::path& UTILS_UNUSED_PARAM(file))
{
}


/// Internal implementation for a test_case.
struct engine::test_case::impl {
    /// Name of the interface implemented by the test program.
    const std::string interface_name;

    /// Test program this test case belongs to.
    const test_program& _test_program;

    /// Name of the test case; must be unique within the test program.
    std::string name;

    /// Test case metadata.
    metadata md;

    /// Fake result to return instead of running the test case.
    optional< test_result > fake_result;

    /// Constructor.
    ///
    /// \param interface_name_ Name of the interface implemented by the test
    ///     program.
    /// \param test_program_ The test program this test case belongs to.
    /// \param name_ The name of the test case within the test program.
    /// \param md_ Metadata of the test case.
    /// \param fake_result_ Fake result to return instead of running the test
    ///     case.
    impl(const std::string& interface_name_,
         const test_program& test_program_,
         const std::string& name_,
         const metadata& md_,
         const optional< test_result >& fake_result_) :
        interface_name(interface_name_),
        _test_program(test_program_),
        name(name_),
        md(md_),
        fake_result(fake_result_)
    {
    }
};


/// Constructs a new test case.
///
/// \param interface_name_ Name of the interface implemented by the test
///     program.
/// \param test_program_ The test program this test case belongs to.  This is a
///     static reference (instead of a test_program_ptr) because the test
///     program must exist in order for the test case to exist.
/// \param name_ The name of the test case within the test program.  Must be
///     unique.
/// \param md_ Metadata of the test case.
engine::test_case::test_case(const std::string& interface_name_,
                             const test_program& test_program_,
                             const std::string& name_,
                             const metadata& md_) :
    _pimpl(new impl(interface_name_, test_program_, name_, md_, none))
{
}



/// Constructs a new fake test case.
///
/// A fake test case is a test case that is not really defined by the test
/// program.  Such test cases have a name surrounded by '__' and, when executed,
/// they return a fixed, pre-recorded result.
///
/// This is necessary for the cases where listing the test cases of a test
/// program fails.  In this scenario, we generate a single test case within
/// the test program that unconditionally returns a failure.
///
/// TODO(jmmv): Need to get rid of this.  We should be able to report the
/// status of test programs independently of test cases, as some interfaces
/// don't know about the latter at all.
///
/// \param interface_name_ Name of the interface implemented by the test
///     program.
/// \param test_program_ The test program this test case belongs to.
/// \param name_ The name to give to this fake test case.  This name has to be
///     prefixed and suffixed by '__' to clearly denote that this is internal.
/// \param description_ The description of the test case, if any.
/// \param test_result_ The fake result to return when this test case is run.
engine::test_case::test_case(
    const std::string& interface_name_,
    const test_program& test_program_,
    const std::string& name_,
    const std::string& description_,
    const engine::test_result& test_result_) :
    _pimpl(new impl(interface_name_, test_program_, name_,
                    metadata_builder().set_description(description_).build(),
                    utils::make_optional(test_result_)))
{
    PRE_MSG(name_.length() > 4 && name_.substr(0, 2) == "__" &&
            name_.substr(name_.length() - 2) == "__",
            "Invalid fake name provided to fake test case");
}


/// Destroys a test case.
engine::test_case::~test_case(void)
{
}


/// Gets the name of the interface implemented by the test program.
///
/// \return An interface name.
const std::string&
engine::test_case::interface_name(void) const
{
    return _pimpl->interface_name;
}


/// Gets the test program this test case belongs to.
///
/// \return A reference to the container test program.
const engine::test_program&
engine::test_case::container_test_program(void) const
{
    return _pimpl->_test_program;
}


/// Gets the test case name.
///
/// \return The test case name, relative to the test program.
const std::string&
engine::test_case::name(void) const
{
    return _pimpl->name;
}


/// Gets the test case metadata.
///
/// \return The test case metadata.
const engine::metadata&
engine::test_case::get_metadata(void) const
{
    return _pimpl->md;
}


/// Gets the fake result pre-stored for this test case.
///
/// \return A fake result, or none if not defined.
optional< engine::test_result >
engine::test_case::fake_result(void) const
{
    return _pimpl->fake_result;
}


/// Runs the test case in debug mode.
///
/// Debug mode gives the caller more control on the execution of the test.  It
/// should not be used for normal execution of tests; instead, call run().
///
/// \param test_case The test case to debug.
/// \param user_config The user configuration that defines the execution of this
///     test case.
/// \param hooks Hooks to introspect the execution of the test case.
/// \param stdout_path The file to which to redirect the stdout of the test.
///     For interactive debugging, '/dev/stdout' is probably a reasonable value.
/// \param stderr_path The file to which to redirect the stdout of the test.
///     For interactive debugging, '/dev/stderr' is probably a reasonable value.
///
/// \return The result of the execution of the test case.
engine::test_result
engine::debug_test_case(const test_case* test_case,
                        const config::tree& user_config,
                        test_case_hooks& hooks,
                        const fs::path& stdout_path,
                        const fs::path& stderr_path)
{
    if (test_case->fake_result())
        return test_case->fake_result().get();

    const std::string skip_reason = check_reqs(
        test_case->get_metadata(), user_config,
        test_case->container_test_program().test_suite_name());
    if (!skip_reason.empty())
        return test_result(test_result::skipped, skip_reason);

    if (!fs::exists(test_case->container_test_program().absolute_path()))
        return test_result(test_result::broken, "Test program does not exist");

    return protected_run(run_test_case_safe(test_case, user_config, hooks,
                                            utils::make_optional(stdout_path),
                                            utils::make_optional(stderr_path)));
}


/// Runs the test case.
///
/// \param test_case The test case to run.
/// \param user_config The user configuration that defines the execution of this
///     test case.
/// \param hooks Hooks to introspect the execution of the test case.
///
/// \return The result of the execution of the test case.
engine::test_result
engine::run_test_case(const test_case* test_case,
                      const config::tree& user_config,
                      test_case_hooks& hooks)
{
    if (test_case->fake_result())
        return test_case->fake_result().get();

    const std::string skip_reason = check_reqs(
        test_case->get_metadata(), user_config,
        test_case->container_test_program().test_suite_name());
    if (!skip_reason.empty())
        return test_result(test_result::skipped, skip_reason);

    if (!fs::exists(test_case->container_test_program().absolute_path()))
        return test_result(test_result::broken, "Test program does not exist");

    return protected_run(run_test_case_safe(test_case, user_config, hooks,
                                            none, none));
}
