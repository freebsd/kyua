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

#include "utils/fs/auto_cleaners.hpp"

extern "C" {
#include <unistd.h>
}

#include <cerrno>

#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/logging/macros.hpp"

namespace fs = utils::fs;


/// Constructs a new auto_directory and grabs ownership of a directory.
///
/// \param directory_ The directory to grab the ownership of.
fs::auto_directory::auto_directory(const path& directory_) :
    _directory(directory_),
    _cleaned(false)
{
}


/// Recursively deletes the managed directory.
///
/// This should not be relied on because it cannot provide proper error
/// reporting.  Instead, the caller should use the cleanup() method.
fs::auto_directory::~auto_directory(void)
{
    try {
        this->cleanup();
    } catch (const fs::error& e) {
        LW(F("Failed to auto-cleanup directory '%s': %s") % _directory %
           e.what());
    }
}


/// Gets the directory managed by this auto_directory.
///
/// \return The path to the managed directory.
const fs::path&
fs::auto_directory::directory(void) const
{
    return _directory;
}


/// Recursively deletes the managed directory.
///
/// This operation is idempotent.
///
/// \throw fs::error If there is a problem removing any directory or file.
void
fs::auto_directory::cleanup(void)
{
    if (!_cleaned) {
        // Mark this as cleaned first so that, in case of failure, we don't
        // reraise the error from the destructor.
        _cleaned = true;

        fs::cleanup(_directory);
    }
}


/// Constructs a new auto_file and grabs ownership of a file.
///
/// \param file_ The file to grab the ownership of.
fs::auto_file::auto_file(const path& file_) :
    _file(file_),
    _removed(false)
{
}


/// Deletes the managed file.
///
/// This should not be relied on because it cannot provide proper error
/// reporting.  Instead, the caller should use the remove() method.
fs::auto_file::~auto_file(void)
{
    try {
        this->remove();
    } catch (const fs::error& e) {
        LW(F("Failed to auto-cleanup file '%s': %s") % _file %
           e.what());
    }
}


/// Gets the file managed by this auto_file.
///
/// \return The path to the managed file.
const fs::path&
fs::auto_file::file(void) const
{
    return _file;
}


/// Deletes the managed file.
///
/// This operation is idempotent.
///
/// \throw fs::error If there is a problem removing the file.
void
fs::auto_file::remove(void)
{
    if (!_removed) {
        // Mark this as cleaned first so that, in case of failure, we don't
        // reraise the error from the destructor.
        _removed = true;

        if (::unlink(_file.c_str()) == -1) {
            const int original_errno = errno;
            throw fs::system_error(F("Removal of %s failed") % _file,
                                   original_errno);
        }
    }
}
