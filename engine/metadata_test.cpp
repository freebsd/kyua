// Copyright 2012 Google Inc.
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

#include "engine/metadata.hpp"

#include <atf-c++.hpp>

#include "utils/fs/path.hpp"
#include "utils/units.hpp"

namespace fs = utils::fs;
namespace units = utils::units;


ATF_TEST_CASE_WITHOUT_HEAD(defaults);
ATF_TEST_CASE_BODY(defaults)
{
    const engine::metadata md = engine::metadata_builder().build();
    ATF_REQUIRE(md.allowed_architectures().empty());
    ATF_REQUIRE(md.allowed_platforms().empty());
    ATF_REQUIRE(md.required_configs().empty());
    ATF_REQUIRE(md.required_files().empty());
    ATF_REQUIRE_EQ(units::bytes(0), md.required_memory());
    ATF_REQUIRE(md.required_programs().empty());
    ATF_REQUIRE(md.required_user().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(add);
ATF_TEST_CASE_BODY(add)
{
    engine::strings_set architectures;
    architectures.insert("1-architecture");
    architectures.insert("2-architecture");

    engine::strings_set platforms;
    platforms.insert("1-platform");
    platforms.insert("2-platform");

    engine::strings_set configs;
    configs.insert("1-config");
    configs.insert("2-config");

    engine::paths_set files;
    files.insert(fs::path("1-file"));
    files.insert(fs::path("2-file"));

    engine::paths_set programs;
    programs.insert(fs::path("1-program"));
    programs.insert(fs::path("2-program"));

    const engine::metadata md = engine::metadata_builder()
        .add_allowed_architecture("1-architecture")
        .add_allowed_platform("1-platform")
        .add_required_config("1-config")
        .add_required_file(fs::path("1-file"))
        .add_required_program(fs::path("1-program"))
        .add_allowed_architecture("2-architecture")
        .add_allowed_platform("2-platform")
        .add_required_config("2-config")
        .add_required_file(fs::path("2-file"))
        .add_required_program(fs::path("2-program"))
        .build();

    ATF_REQUIRE(architectures == md.allowed_architectures());
    ATF_REQUIRE(platforms == md.allowed_platforms());
    ATF_REQUIRE(configs == md.required_configs());
    ATF_REQUIRE(files == md.required_files());
    ATF_REQUIRE(programs == md.required_programs());
}


ATF_TEST_CASE_WITHOUT_HEAD(override_all_with_setters);
ATF_TEST_CASE_BODY(override_all_with_setters)
{
    engine::strings_set architectures;
    architectures.insert("the-architecture");

    engine::strings_set platforms;
    platforms.insert("the-platforms");

    engine::strings_set configs;
    configs.insert("the-configs");

    engine::paths_set files;
    files.insert(fs::path("the-files"));

    const units::bytes memory(12345);

    engine::paths_set programs;
    programs.insert(fs::path("the-programs"));

    const std::string user = "root";

    const engine::metadata md = engine::metadata_builder()
        .set_allowed_architectures(architectures)
        .set_allowed_platforms(platforms)
        .set_required_configs(configs)
        .set_required_files(files)
        .set_required_memory(memory)
        .set_required_programs(programs)
        .set_required_user(user)
        .build();

    ATF_REQUIRE(architectures == md.allowed_architectures());
    ATF_REQUIRE(platforms == md.allowed_platforms());
    ATF_REQUIRE(configs == md.required_configs());
    ATF_REQUIRE(files == md.required_files());
    ATF_REQUIRE_EQ(memory, md.required_memory());
    ATF_REQUIRE(programs == md.required_programs());
    ATF_REQUIRE_EQ(user, md.required_user());
}


ATF_TEST_CASE_WITHOUT_HEAD(override_all_with_set_string);
ATF_TEST_CASE_BODY(override_all_with_set_string)
{
    engine::strings_set architectures;
    architectures.insert("a1");
    architectures.insert("a2");

    engine::strings_set platforms;
    platforms.insert("p1");
    platforms.insert("p2");

    engine::strings_set configs;
    configs.insert("config-var");

    engine::paths_set files;
    files.insert(fs::path("plain"));
    files.insert(fs::path("/absolute/path"));

    const units::bytes memory(1024 * 1024);

    engine::paths_set programs;
    programs.insert(fs::path("program"));
    programs.insert(fs::path("/absolute/prog"));

    const std::string user = "unprivileged";

    const engine::metadata md = engine::metadata_builder()
        .set_string("allowed_architectures", "a1 a2")
        .set_string("allowed_platforms", "p1 p2")
        .set_string("required_configs", "config-var")
        .set_string("required_files", "plain /absolute/path")
        .set_string("required_memory", "1M")
        .set_string("required_programs", "program /absolute/prog")
        .set_string("required_user", "unprivileged")
        .build();

    ATF_REQUIRE(architectures == md.allowed_architectures());
    ATF_REQUIRE(platforms == md.allowed_platforms());
    ATF_REQUIRE(configs == md.required_configs());
    ATF_REQUIRE(files == md.required_files());
    ATF_REQUIRE_EQ(memory, md.required_memory());
    ATF_REQUIRE(programs == md.required_programs());
    ATF_REQUIRE_EQ(user, md.required_user());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, defaults);
    ATF_ADD_TEST_CASE(tcs, add);
    ATF_ADD_TEST_CASE(tcs, override_all_with_setters);
    ATF_ADD_TEST_CASE(tcs, override_all_with_set_string);

    // TODO(jmmv): Add tests for error conditions (invalid keys and invalid
    // values).
}
