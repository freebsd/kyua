// Copyright 2012 Google Inc.
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

#include "engine/testers.hpp"

#include <iostream>
#include <sstream>

#include "engine/exceptions.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/process/children.ipp"
#include "utils/process/status.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace passwd = utils::passwd;
namespace process = utils::process;

using utils::none;
using utils::optional;


namespace {


/// Functor to execute a tester.
class run_tester {
    /// Absolute path to the tester.
    fs::path _tester_path;

    /// Arguments to the tester, without the program name.
    std::vector< std::string > _args;

public:
    /// Constructor.
    ///
    /// \param tester_path Absolute path to the tester.
    /// \param args Arguments to the tester, without the program name.
    run_tester(const fs::path& tester_path,
               const std::vector< std::string >& args) :
        _tester_path(tester_path), _args(args)
    {
    }

    /// Executes the tester.
    void
    operator()(void)
    {
        // Prevent any of our own log messages from leaking into the tester's
        // output.
        logging::set_inmemory();

        try {
            process::exec(_tester_path, _args);
        } catch (const engine::error& e) {
            // If we fail for some reason, consider this a failure on our side
            // and report it accordingly.
            std::cerr << F("Failed to execute the tester %s: %s\n") %
                _tester_path % e.what();
            ::exit(3);
        }
    }
};


/// Reads a stream to the end and records the output in a string.
///
/// \param input The stream to read from.
///
/// \return The text of the stream.
static std::string
read_all(std::istream& input)
{
    std::ostringstream buffer;

    char tmp[1024];
    while (input.good()) {
        input.read(tmp, sizeof(tmp));
        if (input.good() || input.eof()) {
            buffer.write(tmp, input.gcount());
        }
    }

    return buffer.str();
}


/// Drops the trailing newline in a string and replaces others with a literal.
///
/// \param input The string in which to perform the replacements.
///
/// \return The modified string.
static std::string
replace_newlines(const std::string input)
{
    std::string output = input;

    while (output.length() > 0 && output[output.length() - 1] == '\n') {
        output.erase(output.end() - 1);
    }

    std::string::size_type newline = output.find('\n', 0);
    while (newline != std::string::npos) {
        output.replace(newline, 1, "<<NEWLINE>>");
        newline = output.find('\n', newline + 1);
    }

    return output;
}


}  // anonymous namespace


/// Returns the path to a tester binary.
///
/// \param interface Name of the interface of the tester being looked for.
///
/// \return Absolute path to the tester.
fs::path
engine::tester_path(const std::string& interface)
{
    const fs::path testersdir(utils::getenv_with_default(
        "KYUA_TESTERSDIR", KYUA_TESTERSDIR));

    const fs::path tester = testersdir / ("kyua-" + interface + "-tester");
    if (!fs::exists(tester))
        throw engine::error("Unknown interface " + interface);

    if (tester.is_absolute())
        return tester;
    else
        return tester.to_absolute();
}


/// Constructs a tester.
///
/// \param interface Name of the interface to use.
/// \param unprivileged_user If not none, the user to switch to when running
///     the tester.
/// \param timeout If not none, the timeout to pass to the tester.
engine::tester::tester(const std::string& interface,
                       const optional< passwd::user >& unprivileged_user,
                       const optional< datetime::delta >& timeout) :
    _interface(interface)
{
    if (unprivileged_user) {
        _common_args.push_back(F("-u%s") % unprivileged_user.get().uid);
        _common_args.push_back(F("-g%s") % unprivileged_user.get().gid);
    }
    if (timeout) {
        PRE(timeout.get().useconds == 0);
        _common_args.push_back(F("-t%s") % timeout.get().seconds);
    }
}


/// Destructor.
engine::tester::~tester(void)
{
}


/// Executes a list operation on a test program.
///
/// \param program Path to the test program.
///
/// \return The output of the tester, which represents a valid list of test
/// cases.
///
/// \throw error If the tester returns with an unsuccessful exit code.
std::string
engine::tester::list(const fs::path& program) const
{
    std::vector< std::string > args = _common_args;
    args.push_back("list");
    args.push_back(program.str());

    const fs::path tester_path = engine::tester_path(_interface);
    std::auto_ptr< process::child > child = process::child::fork_capture(
        run_tester(tester_path, args));

    const std::string output = read_all(child->output());

    const process::status status = child->wait();
    if (!status.exited() || status.exitstatus() != EXIT_SUCCESS)
        throw engine::error("Tester did not exit cleanly: " +
                            replace_newlines(output));
    return output;
}


/// Executes a test operation on a test case.
///
/// \param program Path to the test program.
/// \param test_case_name Name of the test case to execute.
/// \param result_file Path to the file in which to leave the result of the
///     tester invocation.
/// \param stdout_file Path to the file in which to store the stdout.
/// \param stderr_file Path to the file in which to store the stderr.
/// \param vars Collection of configuration variables.
///
/// \throw error If the tester returns with an unsuccessful exit code.
void
engine::tester::test(const fs::path& program, const std::string& test_case_name,
                     const fs::path& result_file, const fs::path& stdout_file,
                     const fs::path& stderr_file,
                     const std::map< std::string, std::string >& vars) const
{
    std::vector< std::string > args = _common_args;
    args.push_back("test");
    for (std::map< std::string, std::string >::const_iterator i = vars.begin();
         i != vars.end(); ++i) {
        args.push_back(F("-v%s=%s") % (*i).first % (*i).second);
    }
    args.push_back(program.str());
    args.push_back(test_case_name);
    args.push_back(result_file.str());

    const fs::path tester_path = engine::tester_path(_interface);
    std::auto_ptr< process::child > child = process::child::fork_files(
        run_tester(tester_path, args), stdout_file, stderr_file);
    const process::status status = child->wait();

    if (status.exited()) {
        if (status.exitstatus() == EXIT_SUCCESS) {
            // OK; the tester exited cleanly.
        } else if (status.exitstatus() == EXIT_FAILURE) {
            // OK; the tester reported that the test itself failed and we have
            // the result file to indicate this.
        } else {
            throw engine::error(F("Tester failed with code %s; this is a bug") %
                                status.exitstatus());
        }
    } else {
        INV(status.signaled());
        throw engine::error("Tester received a signal; this is a bug");
    }
}
