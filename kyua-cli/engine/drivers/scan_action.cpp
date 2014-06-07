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

#include "engine/drivers/scan_action.hpp"

#include "engine/action.hpp"
#include "engine/exceptions.hpp"
#include "engine/test_result.hpp"
#include "store/backend.hpp"
#include "store/exceptions.hpp"
#include "store/read_transaction.hpp"
#include "utils/defs.hpp"
#include "utils/optional.ipp"

namespace fs = utils::fs;
namespace scan_action = engine::drivers::scan_action;

using utils::optional;


namespace {


/// Gets an action from the store.
///
/// \param tx The open store transaction.
/// \param [in,out] action_id The specific action to get, or none to fetch the
///     latest available action.  This is updated to contain the action id of
///     the returned action.
///
/// \return The fetched action.
///
/// \throw error If there is any problem while loading the action.
static engine::action
get_action(store::read_transaction& tx, optional< int64_t >& action_id)
{
    try {
        if (action_id)
            return tx.get_action(action_id.get());
        else {
            const std::pair< int64_t, engine::action > latest_action =
                tx.get_latest_action();
            action_id = latest_action.first;
            return latest_action.second;
        }
    } catch (const store::error& e) {
        throw engine::error(e.what());
    }
}


}  // anonymous namespace


/// Pure abstract destructor.
scan_action::base_hooks::~base_hooks(void)
{
}


/// Callback executed before any operation is performed.
void
scan_action::base_hooks::begin(void)
{
}


/// Callback executed after all operations are performed.
///
/// \param unused_r A structure with all results computed by this driver.  Note
///     that this is also returned by the drive operation.
void
scan_action::base_hooks::end(const result& UTILS_UNUSED_PARAM(r))
{
}


/// Executes the operation.
///
/// \param store_path The path to the database store.
/// \param action_id The identifier of the action to scan; if none, scans the
///     latest action in the store.
/// \param hooks The hooks for this execution.
///
/// \returns A structure with all results computed by this driver.
scan_action::result
scan_action::drive(const fs::path& store_path,
                   optional< int64_t > action_id,
                   base_hooks& hooks)
{
    store::backend db = store::backend::open_ro(store_path);
    store::read_transaction tx = db.start_read();

    hooks.begin();

    const engine::action action = get_action(tx, action_id);
    hooks.got_action(action_id.get(), action);

    store::results_iterator iter = tx.get_action_results(action_id.get());
    while (iter) {
        hooks.got_result(iter);
        ++iter;
    }

    result r;
    hooks.end(r);
    return r;
}
