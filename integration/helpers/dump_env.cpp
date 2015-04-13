// Copyright 2015 Google Inc.
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

// Dumps all environment variables.
//
// This helper program allows comparing the printed environment variables
// to what 'kyua report --verbose' may output.  It does so by sorting the
// variables and allowing the caller to customize how the output looks
// like (indentation for each line and for continuation lines).

#include <cstdlib>
#include <iostream>

#include "utils/env.hpp"
#include "utils/text/operations.ipp"

namespace text = utils::text;


int
main(const int argc, const char* const* const argv)
{
    if (argc != 3) {
        std::cerr << "Usage: dump_env <prefix> <continuation-prefix>\n";
        return EXIT_FAILURE;
    }
    const char* prefix = argv[1];
    const char* continuation_prefix = argv[2];

    const std::map< std::string, std::string > env = utils::getallenv();
    for (std::map< std::string, std::string >::const_iterator
             iter = env.begin(); iter != env.end(); ++iter) {
        const std::string& name = (*iter).first;
        const std::vector< std::string > value = text::split(
            (*iter).second, '\n');

        if (value.empty()) {
            std::cout << prefix << name << "=\n";
        } else {
            std::cout << prefix << name << '=' << value[0] << '\n';
            for (std::vector< std::string >::size_type i = 1;
                 i < value.size(); ++i) {
                std::cout << continuation_prefix << value[i] << '\n';
            }
        }
    }

    return EXIT_SUCCESS;
}