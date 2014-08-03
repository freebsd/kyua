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

#include "engine/context.hpp"
#include "engine/test_result.hpp"
#include "store/read_backend.hpp"
#include "store/read_transaction.hpp"
#include "utils/defs.hpp"

namespace fs = utils::fs;
namespace scan_results = engine::drivers::scan_results;


/// Pure abstract destructor.
scan_results::base_hooks::~base_hooks(void)
{
}


/// Callback executed before any operation is performed.
void
scan_results::base_hooks::begin(void)
{
}


/// Callback executed after all operations are performed.
///
/// \param unused_r A structure with all results computed by this driver.  Note
///     that this is also returned by the drive operation.
void
scan_results::base_hooks::end(const result& UTILS_UNUSED_PARAM(r))
{
}


/// Executes the operation.
///
/// \param store_path The path to the database store.
/// \param hooks The hooks for this execution.
///
/// \returns A structure with all results computed by this driver.
scan_results::result
scan_results::drive(const fs::path& store_path, base_hooks& hooks)
{
    store::read_backend db = store::read_backend::open_ro(store_path);
    store::read_transaction tx = db.start_read();

    hooks.begin();

    const engine::context context = tx.get_context();
    hooks.got_context(context);

    store::results_iterator iter = tx.get_results();
    while (iter) {
        hooks.got_result(iter);
        ++iter;
    }

    result r;
    hooks.end(r);
    return r;
}
