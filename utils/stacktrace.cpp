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

#include "utils/stacktrace.hpp"

extern "C" {
#include <sys/param.h>
#include <sys/resource.h>

#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/process/children.ipp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace process = utils::process;

using utils::none;
using utils::optional;


/// Built-in path to GDB.
///
/// This is the value that should be passed to the find_gdb() function.  If this
/// is an absolute path, then we use the binary specified by the variable; if it
/// is a relative path, we look for the binary in the path.
///
/// Test cases can override the value of this built-in constant to unit-test the
/// behavior of the functions below.
const char* utils::builtin_gdb = GDB;


namespace {


/// Maximum length of the core file name, if known.
///
/// Some operating systems impose a maximum length on the basename of the core
/// file.  If MAXCOMLEN is defined, then we need to truncate the program name to
/// this length before searching for the core file.  If no such limit is known,
/// this is infinite.
static const std::string::size_type max_core_name_length =
#if defined(MAXCOMLEN)
    MAXCOMLEN
#else
    std::string::npos
#endif
    ;


/// Time to give to the external GDB process to produce a stack trace.
static const datetime::delta gdb_timeout(300, 0);


/// Functor to execute GDB in a subprocess.
class run_gdb {
    /// Path to the GDB binary to use.
    const fs::path& _gdb;

    /// Path to the program being debugged.
    const fs::path& _program;

    /// Path to the dumped core.
    const fs::path& _core_name;

    /// Directory from where to run GDB.
    const fs::path& _work_directory;

public:
    /// Constructs the functor.
    ///
    /// \param gdb_ Path to the GDB binary to use.
    /// \param program_ Path to the program being debugged.  Can be relative to
    ///     the given work directory.
    /// \param core_name_ Path to the dumped core.  Use find_core() to deduce
    ///     a valid candidate.  Can be relative to the given work directory.
    /// \param work_directory_ Directory from where to run GDB.
    run_gdb(const fs::path& gdb_, const fs::path& program_,
            const fs::path& core_name_, const fs::path& work_directory_) :
        _gdb(gdb_), _program(program_), _core_name(core_name_),
        _work_directory(work_directory_)
    {
    }

    /// Executes GDB.
    void
    operator()(void)
    {
        if (::chdir(_work_directory.c_str()) == 1) {
            const int original_errno = errno;
            std::cerr << F("Failed to chdir to %s: %s") % _work_directory
                % std::strerror(original_errno);
            std::exit(EXIT_FAILURE);
        }

        utils::unsetenv("TERM");

        std::vector< std::string > args;
        args.push_back("-batch");
        args.push_back("-q");
        args.push_back("-ex");
        args.push_back("bt");
        args.push_back(_program.str());
        args.push_back(_core_name.str());

        process::exec(_gdb, args);
    }
};


/// Reads a file and appends it to a stream.
///
/// \param file The file to be read.
/// \param output The stream into which to write the file.
/// \param line_prefix A string to be prepended to all the written lines.
///
/// \post If the reading of the file fails, an error is written to the output.
static void
dump_file_into_stream(const fs::path& file, std::ostream& output,
                      const char* line_prefix)
{
    std::ifstream input(file.c_str());
    if (!input)
        output << F("Failed to open %s\n") % file;
    else {
        std::string line;
        while (std::getline(input, line).good())
            output << F("%s%s\n") % line_prefix % line;
    }
}


}  // anonymous namespace


/// Looks for the path to the GDB binary.
///
/// \return The absolute path to the GDB binary if any, otherwise none.  Note
/// that the returned path may or may not be valid: there is no guarantee that
/// the path exists and is executable.
optional< fs::path >
utils::find_gdb(void)
{
    if (std::strlen(builtin_gdb) == 0) {
        LW("The builtin path to GDB is bogus, which probably indicates a bug "
           "in the build system; cannot gather stack traces");
        return none;
    }

    const fs::path gdb(builtin_gdb);
    if (gdb.is_absolute())
        return utils::make_optional(gdb);
    else
        return fs::find_in_path(gdb.c_str());
}


/// Looks for a core file for the given program.
///
/// \param program The name of the binary that generated the core file.  Can be
///     either absolute or relative.
/// \param status The exit status of the program.  This is necessary to gather
///     the PID.
/// \param work_directory The directory from which the program was run.
///
/// \return The path to the core file, if found; otherwise none.
optional< fs::path >
utils::find_core(const fs::path& program, const process::status& status,
                 const fs::path& work_directory)
{
    std::vector< fs::path > candidates;

    candidates.push_back(work_directory /
        (program.leaf_name().substr(0, max_core_name_length) + ".core"));
    if (program.is_absolute()) {
        candidates.push_back(program.branch_path() /
            (program.leaf_name().substr(0, max_core_name_length) + ".core"));
    }
    candidates.push_back(work_directory / (F("core.%s") % status.dead_pid()));
    candidates.push_back(fs::path("/cores") /
                         (F("core.%s") % status.dead_pid()));

    for (std::vector< fs::path >::const_iterator iter = candidates.begin();
         iter != candidates.end(); ++iter) {
        if (fs::exists(*iter)) {
            LD(F("Attempting core file candidate %s: found") % *iter);
            return utils::make_optional(*iter);
        } else {
            LD(F("Attempting core file candidate %s: not found") % *iter);
        }
    }
    return none;
}


/// Raises core size limit to its possible maximum.
///
/// This is a best-effort operation.  There is no guarantee that the operation
/// will yield a large-enough limit to generate any possible core file.
void
utils::unlimit_core_size(void)
{
    struct rlimit rl;
    {
        const int ret = ::getrlimit(RLIMIT_CORE, &rl);
        INV(ret != -1);
    }
    rl.rlim_cur = rl.rlim_max;
    LD(F("Raising soft core size limit to %s (hard value)") % rl.rlim_cur);
    const int ret = ::setrlimit(RLIMIT_CORE, &rl) != -1;
    INV(ret != -1);
}


/// Gathers a stacktrace of a crashed program.
///
/// \param program The name of the binary that crashed and dumped a core file.
///     Can be either absolute or relative.
/// \param status The exit status of the program.
/// \param work_directory The directory from which the program was run.
/// \param output Stream into which to dump the stack trace and any additional
///     information.
///
/// \post If anything goes wrong, the diagnostic messages are written to the
/// output.  This function should not throw.
void
utils::dump_stacktrace(const fs::path& program, const process::status& status,
                       const fs::path& work_directory, std::ostream& output)
{
    PRE(status.signaled() && status.coredump());

    output << F("Process with PID %s exited with signal %s and dumped core; "
                "attempting to gather stack trace\n") %
        status.dead_pid() % status.termsig();

    const optional< fs::path > gdb = utils::find_gdb();
    if (!gdb) {
        output << F("Cannot find GDB binary; builtin was '%s'\n") % builtin_gdb;
        return;
    }

    const optional< fs::path > core_file = find_core(program, status,
                                                     work_directory);
    if (!core_file) {
        output << F("Cannot find any core file\n");
        return;
    }

    const fs::path gdb_out = work_directory / "gdb.out";
    const fs::path gdb_err = work_directory / "gdb.err";

    std::auto_ptr< process::child_with_files > gdb_child =
        process::child_with_files::fork(
            run_gdb(gdb.get(), program, core_file.get(), work_directory),
            gdb_out, gdb_err);
    const process::status gdb_status = gdb_child->wait(gdb_timeout);

    dump_file_into_stream(gdb_out, output, "gdb stdout: ");
    dump_file_into_stream(gdb_err, output, "gdb stderr: ");
    if (gdb_status.exited() && gdb_status.exitstatus() == EXIT_SUCCESS)
        output << "GDB exited successfully\n";
    else
        output << "GDB failed; see output above for details\n";
}


/// Gathers a stacktrace of a program if it crashed.
///
/// This is just a convenience function to allow appending the stacktrace to an
/// existing file and to permit reusing the status as returned by auxiliary
/// process-spawning functions.
///
/// \param program The name of the binary that crashed and dumped a core file.
///     Can be either absolute or relative.
/// \param status The exit status of the program if available; may be none when
///     the program timed out.
/// \param work_directory The directory from which the program was run.
/// \param output_file File into which to dump the stack trace and any
///     additional information.
///
/// \throw std::runtime_error If the output file cannot be opened.
///
/// \post If anything goes wrong with the stack gatheringq, the diagnostic
/// messages are written to the output.
void
utils::dump_stacktrace_if_available(const fs::path& program,
                                    const optional< process::status >& status,
                                    const fs::path& work_directory,
                                    const fs::path& output_file)
{
    if (!status || !status.get().signaled() || !status.get().coredump())
        return;

    std::ofstream output(output_file.c_str(), std::ios_base::app);
    if (!output)
        throw std::runtime_error(F("Cannot append stacktrace to file %s") %
                                 output_file);

    dump_stacktrace(program, status.get(), work_directory, output);
}
