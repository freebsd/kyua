// Copyright 2014 Google Inc.
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

/// \file engine/executor.hpp
/// Multiprogrammed test case executor.
///
/// See the documentation in utils/process/executor.hpp for details on
/// the expected workflow of these classes.

#if !defined(ENGINE_EXECUTOR_HPP)
#define ENGINE_EXECUTOR_HPP

#include "engine/executor_fwd.hpp"

#include <map>
#include <string>

#include "model/test_program_fwd.hpp"
#include "model/test_result_fwd.hpp"
#include "utils/config/tree_fwd.hpp"
#include "utils/datetime_fwd.hpp"
#include "utils/defs.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/optional_fwd.hpp"
#include "utils/process/status_fwd.hpp"
#include "utils/shared_ptr.hpp"

namespace engine {
namespace executor {


/// Abstract interface of a test program executor interface.
///
/// This interface defines the test program-specific operations that need to be
/// invoked at different points during the execution of a given test case.  The
/// executor internally instantiates one of these for every test case.
class interface {
public:
    /// Destructor.
    virtual ~interface() {}

    /// Executes a test case of the test program.
    ///
    /// This method is intended to be called within a subprocess and is expected
    /// to terminate execution either by exec(2)ing the test program or by
    /// exiting with a failure.
    ///
    /// \param test_program The test program to execute.
    /// \param test_case_name Name of the test case to invoke.
    /// \param vars User-provided variables to pass to the test program.
    /// \param control_directory Directory where the interface may place control
    ///     files.
    virtual void exec_test(const model::test_program& test_program,
                           const std::string& test_case_name,
                           const std::map< std::string, std::string >& vars,
                           const utils::fs::path& control_directory)
        const UTILS_NORETURN = 0;

    /// Computes the result of a test case based on its termination status.
    ///
    /// \param status The termination status of the subprocess used to execute
    ///     the exec_test() method or none if the test timed out.
    /// \param control_directory Directory where the interface may have placed
    ///     control files.
    /// \param stdout_path Path to the file containing the stdout of the test.
    /// \param stderr_path Path to the file containing the stderr of the test.
    ///
    /// \return A test result.
    virtual model::test_result compute_result(
        const utils::optional< utils::process::status >& status,
        const utils::fs::path& control_directory,
        const utils::fs::path& stdout_path,
        const utils::fs::path& stderr_path) const = 0;
};


/// Container for all test termination data and accessor to cleanup operations.
class result_handle {
    struct impl;
    /// Pointer to internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend class executor_handle;
    result_handle(std::shared_ptr< impl >);

public:
    ~result_handle(void);

    void cleanup(void);

    exec_handle original_exec_handle(void) const;
    const model::test_program_ptr test_program(void) const;
    const std::string& test_case_name(void) const;
    const model::test_result& test_result(void) const;
    const utils::datetime::timestamp& start_time() const;
    const utils::datetime::timestamp& end_time() const;
    utils::fs::path work_directory(void) const;
    const utils::fs::path& stdout_file(void) const;
    const utils::fs::path& stderr_file(void) const;
};


/// Stateful interface to the multiprogrammed execution of tests.
class executor_handle {
    struct impl;
    /// Pointer to internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend executor_handle setup(void);
    executor_handle(void) throw();

public:
    ~executor_handle(void);

    const utils::fs::path& root_work_directory(void) const;

    void cleanup(void);

    exec_handle spawn_test(const model::test_program_ptr, const std::string&,
                           const utils::config::tree&);
    result_handle wait_any_test(void);

    void check_interrupt(void) const;
};


void register_interface(const std::string&, const std::shared_ptr< interface >);
executor_handle setup(void);


}  // namespace executor
}  // namespace engine


#endif  // !defined(ENGINE_EXECUTOR_HPP)
