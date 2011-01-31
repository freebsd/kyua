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
#include "engine/user_files/config.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/env.hpp"
#include "utils/optional.ipp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;
namespace user_files = engine::user_files;

using utils::optional;


namespace {


/// Path to the system-wide configuration files.
///
/// This is mutable so that tests can override it.  See set_confdir_for_testing.
static fs::path kyua_confdir(KYUA_CONFDIR);
#undef KYUA_CONFDIR


/// Basename of the user-specific configuration file.
static const char* user_config_basename = ".kyuarc";


/// Basename of the system-wide configuration file.
static const char* system_config_basename = "kyua.conf";


/// Textual description of the default configuration files.
///
/// This is just an auxiliary string required to define the option below, which
/// requires a pointer to a static C string.
static const std::string config_lookup_names = F("%s or %s") %
    (fs::path("~") / user_config_basename).str() %
    (kyua_confdir / system_config_basename).str();


}  // anonymous namespace


/// Standard definition of the option to specify a configuration file.
///
/// You must use load_config() to load a configuration file while honoring the
/// value of this flag.
const cmdline::path_option cli::config_option(
    'c', "config",
    "Path to the configuration file",
    "file", config_lookup_names.c_str());


/// Standard definition of the option to specify a Kyuafile.
///
/// You must use load_kyuafile() to load a configuration file while honoring the
/// value of this flag.
const cmdline::path_option cli::kyuafile_option(
    'k', "kyuafile",
    "Path to the test suite definition",
    "file", "Kyuafile");


/// Loads the configuration file for this session, if any.
///
/// The algorithm implemented here is as follows:
/// 1) If ~/.kyuarc exists, load it and return.
/// 2) If sysconfdir/kyua.conf exists, load it and return.
/// 3) Otherwise, return the built-in settings.
///
/// \param cmdline The parsed command line.
///
/// \throw engine::error If the parsing of the configuration file fails.
///     TODO(jmmv): I'm not sure if this is the raised exception.  And even if
///     it is, we should make it more accurate.
user_files::config
cli::load_config(const cmdline::parsed_cmdline& cmdline)
{
    // TODO(jmmv): We should really be able to use cmdline.has_option here to
    // detect whether the option was provided or not instead of checking against
    // the default value.
    const fs::path filename = cmdline.get_option< cmdline::path_option >(
        config_option.long_name());
    if (filename.str() != config_option.default_value())
        return user_files::config::load(filename);

    const optional< std::string > home = utils::getenv("HOME");
    if (home) {
        try {
            const fs::path path = fs::path(home.get()) / user_config_basename;
            if (fs::exists(path))
                return user_files::config::load(path);
        } catch (const fs::error& e) {
            // Fall through.  If we fail to load the user-specific configuration
            // file because it cannot be openend, we try to load the system-wide
            // one.

            // TODO(jmmv): Log this condition when we have a logging facility.
        }
    }

    const fs::path path = kyua_confdir / system_config_basename;
    if (fs::exists(path))
        return user_files::config::load(path);
    else
        return user_files::config::defaults();
}


/// Loads the Kyuafile for this session or generates a fake one.
///
/// The algorithm implemented here is as follows:
/// 1) If there are arguments on the command line that are supposed to override
///    the Kyuafile, the Kyuafile is not loaded and a fake one is generated.
/// 2) Otherwise, the user-provided Kyuafile is loaded.
///
/// \param cmdline The parsed command line.
///
/// \throw engine::error If the parsing of the configuration file fails.
///     TODO(jmmv): I'm not sure if this is the raised exception.  And even if
///     it is, we should make it more accurate.
user_files::kyuafile
cli::load_kyuafile(const cmdline::parsed_cmdline& cmdline)
{
    const fs::path filename = cmdline.get_option< cmdline::path_option >(
        kyuafile_option.long_name());

    if (cmdline.arguments().empty())
        return user_files::kyuafile::load(filename);
    else {
        // TODO(jmmv): Move the from_arguments functionality here.  We probably
        // don't want to generate the fake file from scratch because we should
        // inherit the Kyuafile from the directory.  Not sure how to do that
        // though.
        return user_files::kyuafile::from_arguments(cmdline.arguments());
    }
}


/// Sets the value of the system-wide configuration directory.
///
/// Only use this for testing purposes.
///
/// \param dir The new value of the configuration directory.
void
cli::set_confdir_for_testing(const utils::fs::path& dir)
{
    kyua_confdir = dir;
}
