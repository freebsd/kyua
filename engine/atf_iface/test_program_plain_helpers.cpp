// Copyright 2010, 2011 Google Inc.
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

#include <cstdlib>
#include <cstring>
#include <iostream>


namespace {


int
helper_abort_test_cases_list(int argc, char** argv)
{
    for (const char* const* arg = argv; *arg != NULL; arg++) {
        if (std::strcmp(*arg, "-l") == 0) {
            std::cout << "Content-Type: application/X-atf-tp; "
                "version=\"1\"\n\n";
            std::cout << "ident: foo\n";
        }
    }
    std::abort();
}


int
helper_empty_test_cases_list(int argc, char** argv)
{
    return EXIT_SUCCESS;
}


int
helper_zero_test_cases(int argc, char** argv)
{
    for (const char* const* arg = argv; *arg != NULL; arg++) {
        if (std::strcmp(*arg, "-l") == 0)
            std::cout << "Content-Type: application/X-atf-tp; "
                "version=\"1\"\n\n";
    }
    return EXIT_SUCCESS;
}


static const struct helper {
    const char* name;
    int (*hook)(int, char**);
} helpers[] = {
    { "abort_test_cases_list", helper_abort_test_cases_list, },
    { "empty_test_cases_list", helper_empty_test_cases_list, },
    { "zero_test_cases", helper_zero_test_cases, },
    { NULL, NULL, },
};


}  // anonymous namespace


int
main(int argc, char** argv)
{
    const char* command = std::getenv("HELPER");
    if (command == NULL) {
        std::cerr << "Usage error: HELPER must be set to a helper name\n";
        std::exit(EXIT_FAILURE);
    }

    const struct helper* iter = helpers;
    for (; iter->name != NULL && std::strcmp(iter->name, command) != 0; iter++)
        ;
    if (iter->name == NULL) {
        std::cerr << "Usage error: unknown command " << command << "\n";
        std::exit(EXIT_FAILURE);
    }

    return iter->hook(argc, argv);
}
