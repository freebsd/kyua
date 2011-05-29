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

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

#include <cstdlib>
#include <fstream>

#include <atf-c++.hpp>

#include "cli/cmd_about.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/test_utils.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;

using cli::cmd_about;


namespace {


static void
create_fake_doc(const char* dirname, const char* docname)
{
    std::ofstream doc((fs::path(dirname) / docname).c_str());
    ATF_REQUIRE(doc);
    doc << "Content of " << docname << "\n";
    doc.close();
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(default);
ATF_TEST_CASE_BODY(default)
{
    cmdline::args_vector args;
    args.push_back("about");

    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args));
    ATF_REQUIRE(utils::grep_string(PACKAGE_NAME, ui.out_log()[0]));
    ATF_REQUIRE(utils::grep_string(PACKAGE_VERSION, ui.out_log()[0]));
    // Don't care if the documents are printed or not because we do not know if
    // they are available yet (may not have run 'make install').  Just ensure we
    // get to the right path with the default --what=all and documents location.
    ATF_REQUIRE(utils::grep_vector("Homepage", ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(show_all__ok);
ATF_TEST_CASE_BODY(show_all__ok)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("--show=all");

    fs::mkdir(fs::path("fake-docs"), 0755);
    create_fake_doc("fake-docs", "AUTHORS");
    create_fake_doc("fake-docs", "COPYING");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args));
    ATF_REQUIRE(utils::grep_string(PACKAGE_NAME, ui.out_log()[0]));
    ATF_REQUIRE(utils::grep_string(PACKAGE_VERSION, ui.out_log()[0]));
    ATF_REQUIRE(utils::grep_vector("Content of AUTHORS", ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("Content of COPYING", ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("Homepage", ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(show_all__missing_docs);
ATF_TEST_CASE_BODY(show_all__missing_docs)
{
    cmdline::args_vector args;
    args.push_back("about");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cmd.main(&ui, args));

    ATF_REQUIRE(utils::grep_string(PACKAGE_NAME, ui.out_log()[0]));
    ATF_REQUIRE(utils::grep_string(PACKAGE_VERSION, ui.out_log()[0]));

    ATF_REQUIRE(utils::grep_vector("Homepage", ui.out_log()));

    ATF_REQUIRE(utils::grep_vector("Failed to open.*AUTHORS", ui.err_log()));
    ATF_REQUIRE(utils::grep_vector("Failed to open.*COPYING", ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(show_authors__ok);
ATF_TEST_CASE_BODY(show_authors__ok)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("--show=authors");

    fs::mkdir(fs::path("fake-docs"), 0755);
    create_fake_doc("fake-docs", "AUTHORS");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args));
    ATF_REQUIRE(!utils::grep_string(PACKAGE_NAME, ui.out_log()[0]));
    ATF_REQUIRE(utils::grep_vector("Content of AUTHORS", ui.out_log()));
    ATF_REQUIRE(!utils::grep_vector("COPYING", ui.out_log()));
    ATF_REQUIRE(!utils::grep_vector("Homepage", ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(show_authors__missing_doc);
ATF_TEST_CASE_BODY(show_authors__missing_doc)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("--show=authors");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cmd.main(&ui, args));

    ATF_REQUIRE_EQ(0, ui.out_log().size());

    ATF_REQUIRE(utils::grep_vector("Failed to open.*AUTHORS", ui.err_log()));
    ATF_REQUIRE(!utils::grep_vector("Failed to open.*COPYING", ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(show_license__ok);
ATF_TEST_CASE_BODY(show_license__ok)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("--show=license");

    fs::mkdir(fs::path("fake-docs"), 0755);
    create_fake_doc("fake-docs", "COPYING");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args));
    ATF_REQUIRE(!utils::grep_string(PACKAGE_NAME, ui.out_log()[0]));
    ATF_REQUIRE(!utils::grep_vector("AUTHORS", ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("Content of COPYING", ui.out_log()));
    ATF_REQUIRE(!utils::grep_vector("Homepage", ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(show_license__missing_doc);
ATF_TEST_CASE_BODY(show_license__missing_doc)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("--show=license");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cmd.main(&ui, args));

    ATF_REQUIRE_EQ(0, ui.out_log().size());

    ATF_REQUIRE(!utils::grep_vector("Failed to open.*AUTHORS", ui.err_log()));
    ATF_REQUIRE(utils::grep_vector("Failed to open.*COPYING", ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(show_version__ok);
ATF_TEST_CASE_BODY(show_version__ok)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("--show=version");

    utils::setenv("KYUA_DOCDIR", "fake-docs");
    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args));
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE(utils::grep_string(PACKAGE_NAME, ui.out_log()[0]));
    ATF_REQUIRE(utils::grep_string(PACKAGE_VERSION, ui.out_log()[0]));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_args);
ATF_TEST_CASE_BODY(invalid_args)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("invalid");

    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Too many arguments",
                         cmd.main(&ui, args));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_what);
ATF_TEST_CASE_BODY(invalid_what)
{
    cmdline::args_vector args;
    args.push_back("about");
    args.push_back("--show=foo");

    cmd_about cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Invalid value.*--show: foo",
                         cmd.main(&ui, args));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, default);
    ATF_ADD_TEST_CASE(tcs, show_all__ok);
    ATF_ADD_TEST_CASE(tcs, show_all__missing_docs);
    ATF_ADD_TEST_CASE(tcs, show_authors__ok);
    ATF_ADD_TEST_CASE(tcs, show_authors__missing_doc);
    ATF_ADD_TEST_CASE(tcs, show_license__ok);
    ATF_ADD_TEST_CASE(tcs, show_license__missing_doc);
    ATF_ADD_TEST_CASE(tcs, show_version__ok);
    ATF_ADD_TEST_CASE(tcs, invalid_args);
    ATF_ADD_TEST_CASE(tcs, invalid_what);
}
