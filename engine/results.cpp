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
#include <fstream>
#include <sstream>
#include <typeinfo>
#include <utility>

#include "engine/atf_test_case.hpp"
#include "engine/exceptions.hpp"
#include "engine/results.ipp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace process = utils::process;
namespace results = engine::results;

using utils::none;
using utils::optional;


namespace {


/// Reads a file and flattens its lines.
///
/// The main purpose of this function is to simplify the parsing of a file
/// containing the result of a test.  Therefore, the return value carries
/// several assumptions.
///
/// \param input The stream to read from.
///
/// \return A pair (line count, contents) detailing how many lines where read
/// and their contents.  If the file contains a single line with no newline
/// character, the line count is 0.  If the file includes more than one line,
/// the lines are merged together and separated by the magic string
/// '<<NEWLINE>>'.
static std::pair< size_t, std::string >
read_lines(std::istream& input)
{
    std::pair< size_t, std::string > ret = std::make_pair(0, "");

    do {
        std::string line;
        std::getline(input, line);
        if (input.eof() && !line.empty()) {
            if (ret.first == 0)
                ret.second = line;
            else {
                ret.second += "<<NEWLINE>>" + line;
                ret.first++;
            }
        } else if (input.good()) {
            if (ret.first == 0)
                ret.second = line;
            else
                ret.second += "<<NEWLINE>>" + line;
            ret.first++;
        }
    } while (input.good());

    return ret;
}


/// Parses a test result that does not accept a reason.
///
/// \param status The result status name.
/// \param rest The rest of the line after the status name.
///
/// \return An object representing the test result; results::broken if the
/// parsing failed.
///
/// \pre status must be "passed".
static results::result_ptr
parse_without_reason(const std::string& status, const std::string& rest)
{
    if (!rest.empty())
        return make_result(results::broken(F("%s cannot have a reason") %
                                           status));
    PRE(status == "passed");
    return make_result(results::passed());
}


/// Parses a test result that needs a reason.
///
/// \param status The result status name.
/// \param rest The rest of the line after the status name.
///
/// \return An object representing the test result; results::broken if the
/// parsing failed.
///
/// \pre status must be one of "expected_death", "expected_failure",
/// "expected_timeout", "failed" or "skipped".
static results::result_ptr
parse_with_reason(const std::string& status, const std::string& rest)
{
    if (rest.length() < 3 || rest.substr(0, 2) != ": ")
        return make_result(results::broken(F("%s must be followed by "
                                             "': <reason>'") % status));
    const std::string reason = rest.substr(2);
    INV(!reason.empty());

    if (status == "expected_death")
        return make_result(results::expected_death(reason));
    else if (status == "expected_failure")
        return make_result(results::expected_failure(reason));
    else if (status == "expected_timeout")
        return make_result(results::expected_timeout(reason));
    else if (status == "failed")
        return make_result(results::failed(reason));
    else if (status == "skipped")
        return make_result(results::skipped(reason));
    else
        PRE_MSG(false, "Unexpected status");
}


/// Converts a string to an integer.
///
/// \param str The string containing the integer to convert.
///
/// \return The converted integer; none if the parsing fails.
static optional< int >
parse_int(const std::string& str)
{
    if (str.empty()) {
        return none;
    } else {
        std::istringstream iss(str);

        int value;
        iss >> value;
        if (!iss.eof() || (!iss.eof() && !iss.good()))
            return none;
        else
            return utils::make_optional(value);
    }
}


/// Parses a test result that needs a reason and accepts an optional integer.
///
/// \param status The result status name.
/// \param rest The rest of the line after the status name.
///
/// \return An object representing the test result; results::broken if the
/// parsing failed.
///
/// \pre status must be one of "expected_exit" or "expected_signal".
static results::result_ptr
parse_with_reason_and_arg(const std::string& status, const std::string& rest)
{
    std::string::size_type delim = rest.find_first_of(":(");
    if (delim == std::string::npos)
        return make_result(results::broken(F("Invalid format for '%s' test "
                                             "case result; must be followed by "
                                             "'[(num)]: <reason>' but found "
                                             "'%s'") % status % rest));

    optional< int > arg;
    if (rest[delim] == '(') {
        const std::string::size_type delim2 = rest.find("):", delim);
        if (delim == std::string::npos)
            throw engine::format_error(F("Mismatched '(' in %s") % rest);

        const std::string argstr = rest.substr(delim + 1, delim2 - delim - 1);
        arg = parse_int(argstr);
        if (!arg)
            return make_result(results::broken(F("Invalid integer argument "
                                                 "'%s' to '%s' test case "
                                                 "result") % argstr % status));
        delim = delim2 + 1;
    }

    const std::string reason = rest.substr(delim + 2);

    if (status == "expected_exit")
        return make_result(results::expected_exit(arg, reason));
    else if (status == "expected_signal")
        return make_result(results::expected_signal(arg, reason));
    else
        PRE_MSG(false, "Unexpected status");
}


/// Formats the termination status of a process to be used with validate_result.
///
/// \param status The status to format.
///
/// \return A string describing the status.
static std::string
format_status(const process::status& status)
{
    if (status.exited())
        return F("exited with code %d") % status.exitstatus();
    else if (status.signaled())
        return F("received signal %d%s") % status.termsig() %
            (status.coredump() ? " (core dumped)" : "");
    else
        return F("terminated in an unknown manner");
}


}  // anonymous namespace


/// Destructor for a test result.
results::base_result::~base_result(void)
{
}


/// Parses an input stream to extract a test result.
///
/// If the parsing fails for any reason, the test result becomes results::broken
/// and it contains the reason for the parsing failure.  Test cases that report
/// results in an inconsistent state cannot be trusted (e.g. the test program
/// code may have a bug), and thus why they are reported as broken instead of
/// just failed (which is a legitimate result for a test case).
///
/// \param input The stream to read from.
///
/// \return A dynamically-allocated instance of a class derived from
/// results::base_result representing the result of the test case.
results::result_ptr
results::parse(std::istream& input)
{
    const std::pair< size_t, std::string > data = read_lines(input);
    if (data.first == 0)
        return make_result(results::broken("Empty test result or no new line"));
    else if (data.first > 1)
        return make_result(results::broken("Test result contains multiple "
                                           "lines: " + data.second));
    else {
        const std::string::size_type delim = data.second.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyz_");
        const std::string status = data.second.substr(0, delim);
        const std::string rest = data.second.substr(status.length());

        if (status == "expected_death")
            return parse_with_reason(status, rest);
        else if (status == "expected_exit")
            return parse_with_reason_and_arg(status, rest);
        else if (status == "expected_failure")
            return parse_with_reason(status, rest);
        else if (status == "expected_signal")
            return parse_with_reason_and_arg(status, rest);
        else if (status == "expected_timeout")
            return parse_with_reason(status, rest);
        else if (status == "failed")
            return parse_with_reason(status, rest);
        else if (status == "passed")
            return parse_without_reason(status, rest);
        else if (status == "skipped")
            return parse_with_reason(status, rest);
        else
            return make_result(results::broken(F("Unknown test result '%s'") %
                                               status));
    }
}


/// Loads a test case result from a file.
///
/// \param file The file to parse.
///
/// \return The parsed test case result.  See the comments in results::parse()
/// for more details -- in particular, how errors are reported.
results::result_ptr
results::load(const fs::path& file)
{
    std::ifstream input(file.c_str());
    if (!input)
        return result_ptr(NULL);
    else
        return results::parse(input);
}


/// Adusts the raw result of a test case with its termination status.
///
/// Adjusting the result means ensuring that the termination conditions of the
/// program match what is expected of the paticular result is reported.  If such
/// conditions do not match, the test program is considered bogus.
///
/// \param raw_result The result as processed from the results file created by
///     the test case.
/// \param status The exit status of the test program.
///
/// \result The adjusted result.  The original result is transformed into broken
/// if the exit status of the program does not our expectations.
results::result_ptr
results::adjust_with_status(result_ptr raw_result,
                            const process::status& status)
{
    if (raw_result.get() == NULL)
        return make_result(broken(F("Premature exit: %s") %
                                  format_status(status)));

    if (typeid(*raw_result) == typeid(broken))
        return raw_result;

    if (typeid(*raw_result) == typeid(expected_death)) {
        return raw_result;

    } else if (typeid(*raw_result) == typeid(expected_exit)) {
        if (status.exited()) {
            const expected_exit* result =
                dynamic_cast< const expected_exit* >(raw_result.get());
            if (result->exit_status) {
                if (result->exit_status.get() == status.exitstatus())
                    return raw_result;
                else
                    return make_result(broken(F("Expected clean exit with code "
                                                "%d but got code %d") %
                                              result->exit_status.get() %
                                              status.exitstatus()));
            } else
                return raw_result;
        } else
            return make_result(broken("Expected clean exit but " +
                                      format_status(status)));

    } else if (typeid(*raw_result) == typeid(expected_failure)) {
        if (status.exited() && status.exitstatus() == EXIT_SUCCESS)
            return raw_result;
        else
            return make_result(broken("Expected failure should have reported "
                                      "success but " + format_status(status)));

    } else if (typeid(*raw_result) == typeid(expected_signal)) {
        if (status.signaled()) {
            const expected_signal* result =
                dynamic_cast< const expected_signal* >(
                    raw_result.get());
            if (result->signal_no) {
                if (result->signal_no.get() == status.termsig())
                    return raw_result;
                else
                    return make_result(broken(F("Expected signal %d but got "
                                                "%d") %
                                              result->signal_no.get() %
                                              status.termsig()));
            } else
                return raw_result;
        } else
            return make_result(broken("Expected signal but " +
                                      format_status(status)));

    } else if (typeid(*raw_result) == typeid(expected_timeout)) {
        return make_result(broken("Expected timeout but " +
                                  format_status(status)));

    } else if (typeid(*raw_result) == typeid(failed)) {
        if (status.exited() && status.exitstatus() == EXIT_FAILURE)
            return raw_result;
        else
            return make_result(broken("Failed test case should have reported "
                                      "failure but " + format_status(status)));

    } else if (typeid(*raw_result) == typeid(passed)) {
        if (status.exited() && status.exitstatus() == EXIT_SUCCESS)
            return raw_result;
        else
            return make_result(broken("Passed test case should have reported "
                                      "success but " + format_status(status)));

    } else if (typeid(*raw_result) == typeid(skipped)) {
        if (status.exited() && status.exitstatus() == EXIT_SUCCESS)
            return raw_result;
        else
            return make_result(broken("Skipped test case should have reported "
                                      "success but " + format_status(status)));

    } else {
        UNREACHABLE_MSG("Unhandled result type");
    }
}


/// Adusts the raw result of a test case with its timeout.
///
/// Adjusting the result means ensuring that the test case is marked as broken
/// unless its status says that the timeout is expected.
///
/// \param result The result as processed from the results file created by
///     the test case.  Given that the test case timed out, it most likely did
///     not create a results file (unless for expected failures).  That's OK
///     because if the results file does not exist, we get a 'broken' type
///     result from load().
/// \param timeout The timeout for informative reasons.
///
/// \result The adjusted result.  The original result is transformed into broken
/// if the timeout was not expected.
results::result_ptr
results::adjust_with_timeout(results::result_ptr result,
                             const datetime::delta& timeout)
{
    if (result.get() == NULL)
        return make_result(broken(F("Test case timed out after %d seconds") %
                                  timeout.seconds));

    if (typeid(*result) == typeid(expected_timeout))
        return result;
    else
        return make_result(broken(F("Test case timed out after %d seconds") %
                                  timeout.seconds));
}


/// Calculates the final result of the execution of a test case.
///
/// \param test_case The test case for which the result is being adjusted.
/// \param body_status The exit status of the test case's body; none if it
///     timed out.
/// \param cleanup_status The exit status of the test case's cleanup; none if it
///     timed out.
/// \param result_from_file The result saved by the test case, if any, as
///     returned by the load() function.
///
/// \return The result of the test case as it should be reported to the user.
results::result_ptr
results::adjust(const engine::atf_test_case& test_case,
                const optional< process::status >& body_status,
                const optional< process::status >& cleanup_status,
                results::result_ptr result_from_file)
{
    results::result_ptr result;
    if (body_status)
        result = adjust_with_status(result_from_file, body_status.get());
    else
        result = adjust_with_timeout(result_from_file, test_case.timeout);

    if (result->good() && test_case.has_cleanup) {
        if (cleanup_status) {
            if (!cleanup_status.get().exited() ||
                cleanup_status.get().exitstatus() != EXIT_SUCCESS)
                result.reset(new results::broken("Test case cleanup did not "
                                                 "terminate successfully"));
        } else {
            result.reset(new results::broken(F("Test case cleanup timed out "
                                               "after %d seconds") %
                                             test_case.timeout.seconds));
        }
    }

    return result;
}


/// Constructs a new broken result.
///
/// \param reason_ The reason.
results::broken::broken(const std::string& reason_) :
    reason(reason_)
{
}


/// Equality comparator.
///
/// \param other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::broken::operator==(const results::broken& other)
    const
{
    return reason == other.reason;
}


/// Inquality comparator.
///
/// \param other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::broken::operator!=(const results::broken& other)
    const
{
    return reason != other.reason;
}


std::string
results::broken::format(void) const
{
    return F("broken: %s") % reason;
}


bool
results::broken::good(void) const
{
    return false;
}


/// Constructs a new expected_death result.
///
/// \param reason_ The reason.
results::expected_death::expected_death(const std::string& reason_) :
    reason(reason_)
{
}


/// Equality comparator.
///
/// \param other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::expected_death::operator==(const results::expected_death& other)
    const
{
    return reason == other.reason;
}


/// Inquality comparator.
///
/// \param other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::expected_death::operator!=(const results::expected_death& other)
    const
{
    return reason != other.reason;
}


std::string
results::expected_death::format(void) const
{
    return F("expected_death: %s") % reason;
}


bool
results::expected_death::good(void) const
{
    return true;
}


/// Constructs a new expected_exit result.
///
/// \param exit_status_ The expected exit status; none for any.
/// \param reason_ The reason.
results::expected_exit::expected_exit(const optional< int >& exit_status_,
                                      const std::string& reason_) :
    exit_status(exit_status_),
    reason(reason_)
{
}


/// Equality comparator.
///
/// \param other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::expected_exit::operator==(const results::expected_exit& other)
    const
{
    return exit_status == other.exit_status && reason == other.reason;
}


/// Inquality comparator.
///
/// \param other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::expected_exit::operator!=(const results::expected_exit& other)
    const
{
    return exit_status != other.exit_status || reason != other.reason;
}


std::string
results::expected_exit::format(void) const
{
    if (exit_status)
        return F("expected_exit(%d): %s") % exit_status.get() % reason;
    else
        return F("expected_exit: %s") % reason;
}


bool
results::expected_exit::good(void) const
{
    return true;
}


/// Constructs a new expected_failure result.
///
/// \param reason_ The reason.
results::expected_failure::expected_failure(const std::string& reason_) :
    reason(reason_)
{
}


/// Equality comparator.
///
/// \param other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::expected_failure::operator==(const results::expected_failure& other)
    const
{
    return reason == other.reason;
}


/// Inquality comparator.
///
/// \param other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::expected_failure::operator!=(const results::expected_failure& other)
    const
{
    return reason != other.reason;
}


std::string
results::expected_failure::format(void) const
{
    return F("expected_failure: %s") % reason;
}


bool
results::expected_failure::good(void) const
{
    return true;
}


/// Constructs a new expected_signal result.
///
/// \param signal_no_ The expected signal number; none for any.
/// \param reason_ The reason.
results::expected_signal::expected_signal(const optional< int >& signal_no_,
                                          const std::string& reason_) :
    signal_no(signal_no_),
    reason(reason_)
{
}


/// Equality comparator.
///
/// \param other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::expected_signal::operator==(const results::expected_signal& other)
    const
{
    return signal_no == other.signal_no && reason == other.reason;
}


/// Inquality comparator.
///
/// \param other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::expected_signal::operator!=(const results::expected_signal& other)
    const
{
    return signal_no != other.signal_no || reason != other.reason;
}


std::string
results::expected_signal::format(void) const
{
    if (signal_no)
        return F("expected_signal(%d): %s") % signal_no.get() % reason;
    else
        return F("expected_signal: %s") % reason;
}


bool
results::expected_signal::good(void) const
{
    return true;
}


/// Constructs a new expected_timeout result.
///
/// \param reason_ The reason.
results::expected_timeout::expected_timeout(const std::string& reason_) :
    reason(reason_)
{
}


/// Equality comparator.
///
/// \param other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::expected_timeout::operator==(const results::expected_timeout& other)
    const
{
    return reason == other.reason;
}


/// Inquality comparator.
///
/// \param other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::expected_timeout::operator!=(const results::expected_timeout& other)
    const
{
    return reason != other.reason;
}


std::string
results::expected_timeout::format(void) const
{
    return F("expected_timeout: %s") % reason;
}


bool
results::expected_timeout::good(void) const
{
    return true;
}


/// Constructs a new failed result.
///
/// \param reason_ The reason.
results::failed::failed(const std::string& reason_) :
    reason(reason_)
{
}


/// Equality comparator.
///
/// \param other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::failed::operator==(const results::failed& other)
    const
{
    return reason == other.reason;
}


/// Inquality comparator.
///
/// \param other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::failed::operator!=(const results::failed& other)
    const
{
    return reason != other.reason;
}


std::string
results::failed::format(void) const
{
    return F("failed: %s") % reason;
}


bool
results::failed::good(void) const
{
    return false;
}


/// Constructs a new passed result.
results::passed::passed(void)
{
}


/// Equality comparator.
///
/// \param other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::passed::operator==(const results::passed& other)
    const
{
    return true;
}


/// Inquality comparator.
///
/// \param other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::passed::operator!=(const results::passed& other)
    const
{
    return false;
}


std::string
results::passed::format(void) const
{
    return "passed";
}


bool
results::passed::good(void) const
{
    return true;
}


/// Constructs a new skipped result.
///
/// \param reason_ The reason.
results::skipped::skipped(const std::string& reason_) :
    reason(reason_)
{
}


/// Equality comparator.
///
/// \param other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::skipped::operator==(const results::skipped& other)
    const
{
    return reason == other.reason;
}


/// Inquality comparator.
///
/// \param other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::skipped::operator!=(const results::skipped& other)
    const
{
    return reason != other.reason;
}


std::string
results::skipped::format(void) const
{
    return F("skipped: %s") % reason;
}


bool
results::skipped::good(void) const
{
    return true;
}
