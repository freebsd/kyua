// Copyright 2013 Google Inc.
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

/// \file tap_parser.h
/// Utilities to parse the output of a TAP test program.

#if !defined(KYUA_TAP_PARSER_H)
#define KYUA_TAP_PARSER_H

#include <stdbool.h>
#include <stdio.h>

#include "error_fwd.h"


/// Results of the parsing of a TAP test output.
struct kyua_tap_summary {
    /// If not NULL, describes the reason for a parse failure.  In this case,
    /// none of the other files should be checked.
    const char* parse_error;

    /// Set to true if the program asked to bail out.  In this case, the
    /// remaining fields may be inconsistent.
    bool bail_out;

    /// Index of the first test as reported by the test plan.
    long first_index;

    /// Index of the last test as reported by the test plan.
    long last_index;

    /// Total number of "ok" tests.
    long ok_count;

    /// Total number of "not ok" tests.
    long not_ok_count;
};
/// Shorthand for a kyua_tap_summary structure.
typedef struct kyua_tap_summary kyua_tap_summary_t;


void kyua_tap_summary_init(kyua_tap_summary_t*);
void kyua_tap_summary_fini(kyua_tap_summary_t*);

kyua_error_t kyua_tap_try_parse_plan(const char*, kyua_tap_summary_t*);

kyua_error_t kyua_tap_parse(const int, FILE*, kyua_tap_summary_t*);


#endif  // !defined(KYUA_TAP_PARSER_H)
