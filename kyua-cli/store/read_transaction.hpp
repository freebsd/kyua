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

/// \file store/read_transaction.hpp
/// Implementation of read-only transactions on the backend.

#if !defined(STORE_READ_TRANSACTION_HPP)
#define STORE_READ_TRANSACTION_HPP

extern "C" {
#include <stdint.h>
}

#include <string>

#include "engine/test_program.hpp"
#include "utils/datetime.hpp"
#include "utils/shared_ptr.hpp"

namespace engine {
class context;
class test_result;
}  // namespace engine

namespace store {


class read_backend;
class read_transaction;


namespace detail {


engine::test_program_ptr get_test_program(read_backend&, const int64_t);


}  // namespace detail


/// Iterator for the set of test case results that are part of an action.
///
/// \todo Note that this is not a "standard" C++ iterator.  I have chosen to
/// implement a different interface because it makes things easier to represent
/// an SQL statement state.  Rewrite as a proper C++ iterator, inheriting from
/// std::iterator.
class results_iterator {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend class read_transaction;
    results_iterator(std::shared_ptr< impl >);

public:
    ~results_iterator(void);

    results_iterator& operator++(void);
    operator bool(void) const;

    const engine::test_program_ptr test_program(void) const;
    std::string test_case_name(void) const;
    engine::test_result result(void) const;
    utils::datetime::delta duration(void) const;

    std::string stdout_contents(void) const;
    std::string stderr_contents(void) const;
};


/// Representation of a read-only transaction.
///
/// Transactions are the entry place for high-level calls that access the
/// database.
class read_transaction {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend class read_backend;
    read_transaction(read_backend&);

public:
    ~read_transaction(void);

    void finish(void);

    engine::context get_context(const int64_t);
    results_iterator get_results(void);
};


}  // namespace store

#endif  // !defined(STORE_READ_TRANSACTION_HPP)
