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

#include "tap_parser.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <unistd.h>

#include "error.h"
#include "text.h"


/// Name of a regex error type.
static const char* const regex_error_type = "regex";


/// Representation of a regex error.
struct regex_error_data {
    /// Value of the error code captured during the error creation.
    int original_code;

    /// Value of the regex captured during the error creation.
    regex_t *original_preg;

    /// Explanation of the problem that lead to the error.
    char description[4096];
};
/// Shorthand for a regex_error_data structure.
typedef struct regex_error_data regex_error_data_t;


/// Generates a user-friendly representation of the error.
///
/// \pre The error must be set.
///
/// \param error Error for which to generate a message.
/// \param output_buffer Buffer to hold the generated message.
/// \param output_size Length of output_buffer.
///
/// \return The number of bytes written to output_buffer, or a negative value if
/// there was an error.
static int
regex_format(const kyua_error_t error, char* const output_buffer,
            const size_t output_size)
{
    assert(kyua_error_is_type(error, regex_error_type));

    const regex_error_data_t* data = kyua_error_data(error);
    int prefix_length = snprintf(output_buffer, output_size, "%s: ",
                                 data->description);
    (void)regerror(data->original_code, data->original_preg,
                   output_buffer + prefix_length, output_size - prefix_length);
    return strlen(output_buffer);
}


/// Releases the contents of the error.
///
/// \param opaque_data Internal data of a regex error.
static void
regex_free(void* opaque_data)
{
    regex_error_data_t* data = opaque_data;
    regfree(data->original_preg);
    free(data->original_preg);
}


/// Constructs a new regex error.
///
/// \param original_code regex error code for this error.
/// \param original_preg Original regex for this error.  Takes ownership.
/// \param description Textual description of the problem.
/// \param ... Positional arguments for the description.
///
/// \return The generated error.
static kyua_error_t
regex_error_new(const int original_code, regex_t* original_preg,
                const char* description, ...)
{
    va_list ap;

    const size_t data_size = sizeof(regex_error_data_t);
    regex_error_data_t* data = (regex_error_data_t*)calloc(1, data_size);
    if (data == NULL) {
        regfree(original_preg);
        return kyua_oom_error_new();
    }

    data->original_code = original_code;
    data->original_preg = malloc(sizeof(regex_t));
    if (data->original_preg == NULL) {
        regfree(original_preg);
        return kyua_oom_error_new();
    }
    memcpy(data->original_preg, original_preg, sizeof(regex_t));
    va_start(ap, description);
    (void)vsnprintf(data->description, sizeof(data->description),
                    description, ap);
    va_end(ap);

    return kyua_error_new(regex_error_type, data, data_size, regex_format,
                          regex_free);
}


/// Extracts a regex match as a long.
///
/// \param line Input line in which the match lives.
/// \param match Match as returned by regexec.
/// \param [out] output Pointer to the parsed long value.
///
/// \return NULL if all is OK; a pointer to a constant string to an error
/// message otherwise.
static const char*
regex_match_to_long(const char* line, const regmatch_t* match, long* output)
{
    char buffer[16];
    const size_t length = match->rm_eo - match->rm_so;
    if (length > sizeof(buffer) - 1)
        return "Plan line too long";
    memcpy(buffer, line + match->rm_so, length);
    buffer[length] = '\0';

    errno = 0;
    char *endptr;
    const long tmp = strtol(buffer, &endptr, 10);
    assert(buffer[0] != '\0' && *endptr == '\0');  // Input is a number.
    if (errno == ERANGE || (tmp == LONG_MAX || tmp == LONG_MIN))
        return "Plan line includes out of range numbers";
    else {
        *output = tmp;
        return NULL;
    }
}


/// Initializes the contents of a kyua_tap_summary_t object with default values.
///
/// \param summary The object to initialize.
void
kyua_tap_summary_init(kyua_tap_summary_t* summary)
{
    memset(summary, 0, sizeof(*summary));
    summary->parse_error = NULL;
    summary->bail_out = false;
    summary->first_index = 0;
    summary->last_index = 0;
    summary->all_skipped_reason = NULL;
    summary->ok_count = 0;
    summary->not_ok_count = 0;
}


/// Releases the contents of a kyua_tap_summary_t object.
///
/// \param summary The object to release.
void
kyua_tap_summary_fini(kyua_tap_summary_t* summary)
{
    if (summary->all_skipped_reason != NULL)
        free(summary->all_skipped_reason);
}


/// Attempts to parse a TAP plan line.
///
/// \param line The line to parse from the output of the TAP test program.
/// \param [in,out] summary Summary of the current status of the parsing.
///     Updated if a new plan is encountered.
///
/// \return An error object if there are problems parsing the input line or if
/// a duplicate plan has been encountered (as described by summary).
/// OK otherwise.
kyua_error_t
kyua_tap_try_parse_plan(const char* line, kyua_tap_summary_t* summary)
{
    int code;

    regex_t preg;
    code = regcomp(&preg, "^([0-9]+)\\.\\.([0-9]+)", REG_EXTENDED);
    if (code != 0)
        return regex_error_new(code, &preg, "regcomp failed");

    regmatch_t matches[3];
    code = regexec(&preg, line, 3, matches, 0);
    if (code != 0) {
        if (code == REG_NOMATCH)
            goto end;
        else
            return regex_error_new(code, &preg, "regexec failed");
    }

    if (summary->first_index != 0 || summary->last_index != 0 ||
        summary->all_skipped_reason != NULL) {
        summary->parse_error = "Output includes two test plans";
        goto end;
    }

    const char* error;

    long first_index;
    error = regex_match_to_long(line, &matches[1], &first_index);
    if (error != NULL) {
        summary->parse_error = error;
        goto end;
    }

    long last_index;
    error = regex_match_to_long(line, &matches[2], &last_index);
    if (error != NULL) {
        summary->parse_error = error;
        goto end;
    }

    const char* skip_start = strcasestr(line, "SKIP");
    if (skip_start != NULL) {
        const char *reason = skip_start + strlen("SKIP");
        while (*reason != '\0' && isspace(*reason))
            ++reason;
        if (*reason == '\0') {
            summary->all_skipped_reason = strdup("No reason specified");
        } else {
            summary->all_skipped_reason = strdup(reason);
        }
    }

    if (summary->all_skipped_reason != NULL) {
        if (first_index != 1 || last_index != 0) {
            summary->parse_error = "Skipped test plan has invalid range";
        } else {
            summary->first_index = first_index;
            summary->last_index = last_index;
        }
    } else if (last_index < first_index) {
        summary->parse_error = "Test plan is reversed";
    } else {
        summary->first_index = first_index;
        summary->last_index = last_index;
    }

end:
    regfree(&preg);
    return kyua_error_ok();
}


/// Parses the output of a TAP test program.
///
/// \param fd Descriptor from which to read the output.  Grabs ownership.
/// \param output Stream to which to print the output as it is read.
/// \param [out] summary Filled in with the details of the parsing.  Only valid
///     if the function returns OK.
///
/// \return OK if the parsing succeeds (regardless of whether the test program
/// exits successfully or not); an error otherwise.
kyua_error_t
kyua_tap_parse(const int fd, FILE* output, kyua_tap_summary_t* summary)
{
    kyua_error_t error;
    char line[1024];  // It's ugly to have a limit, but it's easier this way.

    FILE* input = fdopen(fd, "r");
    if (input == NULL) {
        close(fd);
        return kyua_libc_error_new(errno, "fdopen(3) failed");
    }

    kyua_tap_summary_init(summary);

    error = kyua_error_ok();
    while (!kyua_error_is_set(error) &&
           summary->parse_error == NULL && !summary->bail_out &&
           kyua_text_fgets_no_newline(line, sizeof(line), input) != NULL &&
           strcmp(line, "") != 0) {
        fprintf(output, "%s\n", line);

        error = kyua_tap_try_parse_plan(line, summary);

        if (strstr(line, "Bail out!") == line)
            summary->bail_out = true;
        else if (strstr(line, "ok") == line)
            summary->ok_count++;
        else if (strstr(line, "not ok") == line) {
            if (strstr(line, "TODO") != NULL || strstr(line, "SKIP") != NULL)
                summary->ok_count++;
            else
                summary->not_ok_count++;
        }
    }
    fclose(input);

    if (summary->parse_error == NULL &&
        !summary->bail_out &&
        summary->all_skipped_reason == NULL) {
        const long exp_count = summary->last_index - summary->first_index + 1;
        const long actual_count = summary->ok_count + summary->not_ok_count;
        if (exp_count != actual_count) {
            summary->parse_error = "Reported plan differs from actual executed "
                "tests";
        }
    }
    return kyua_error_ok();
}
