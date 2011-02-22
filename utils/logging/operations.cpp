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
#include <unistd.h>
}

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/operations.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;

using utils::none;
using utils::optional;


namespace {


/// First time recorded by the logging module.
static optional< datetime::timestamp > first_timestamp = none;


/// In-memory record of log entries before persistency is enabled.
static std::vector< std::string > backlog;


/// Stream to the currently open log file.
static std::auto_ptr< std::ofstream > logfile;


/// Constant string to strftime to format timestamps.
static const char* timestamp_format = "%Y%m%d-%H%M%S";


}  // anonymous namespace


/// Generates a standard log name.
///
/// This always adds the same timestamp to the log name for a particular run.
/// Also, the timestamp added to the file name corresponds to the first
/// timestamp recorded by the module; it does not necessarily contain the
/// current value of "now".
///
/// \param logdir The path to the directory in which to place the log.
/// \param progname The name of the program that is generating the log.
fs::path
logging::generate_log_name(const fs::path& logdir, const std::string& progname)
{
    if (!first_timestamp)
        first_timestamp = datetime::timestamp::now();
    return logdir / (F("%s.%s.log") % progname %
                     first_timestamp.get().strftime(timestamp_format));
}


/// Logs an entry to the log file.
///
/// If the log is not yet set to persistent mode, the entry is recorded in the
/// in-memory backlog.  Otherwise, it is just written to disk.
///
/// \param type The type of the entry.  Can be one of: D=debugging, E=error,
///     I=info, W=warning.
/// \param file The file from which the log message is generated.
/// \param line The line from which the log message is generated.
/// \param user_message The raw message to store.
void
logging::log(const char type, const char* file, const int line,
             const std::string& user_message)
{
    PRE(type == 'D' || type == 'E' || type == 'I' || type == 'W');

    const datetime::timestamp now = datetime::timestamp::now();
    if (!first_timestamp)
        first_timestamp = now;

    const std::string message = F("%s %c %d %s:%d: %s") %
        now.strftime(timestamp_format) % type % ::getpid() % file % line %
        user_message;
    if (logfile.get() == NULL)
        backlog.push_back(message);
    else {
        INV(backlog.empty());
        (*logfile) << message << '\n';
        (*logfile).flush();
    }
}


/// Makes the log persistent.
///
/// Calling this function flushes the in-memory log, if any, to disk and sets
/// the logging module to send log entries to disk from this point onwards.
/// There is no way back, and the caller program should execute this function as
/// early as possible to ensure that a crash at startup does not discard too
/// many useful log entries.
///
/// \param path The file to write the logs to.
///
/// \throw std::runtime_error If the given file cannot be created.
void
logging::set_persistency(const fs::path& path)
{
    PRE(logfile.get() == NULL);

    logfile.reset(new std::ofstream(path.c_str()));
    if (!(*logfile))
        throw std::runtime_error(F("Failed to create log file %s") % path);

    for (std::vector< std::string >::const_iterator iter = backlog.begin();
         iter != backlog.end(); iter++)
        (*logfile) << *iter << '\n';
    backlog.clear();
}
