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

extern "C" {
#include <time.h>
}

#include "utils/datetime.hpp"
#include "utils/sanity.hpp"

namespace datetime = utils::datetime;


/// Creates a zero time delta.
datetime::delta::delta(void) :
    seconds(0),
    useconds(0)
{
}


/// Creates a time delta.
///
/// \param seconds_ The seconds in the delta.
/// \param useconds_ The microseconds in the delta.
datetime::delta::delta(const unsigned int seconds_,
                       const unsigned long useconds_) :
    seconds(seconds_),
    useconds(useconds_)
{
}


/// Checks if two time deltas are equal.
///
/// \param other The object to compare to.
///
/// \return True if the two time deltas are equals; false otherwise.
bool
datetime::delta::operator==(const datetime::delta& other) const
{
    return seconds == other.seconds && useconds == other.useconds;
}


namespace utils {
namespace datetime {


/// Internal representation for datetime::timestamp.
struct timestamp::impl {
    /// The raw timestamp as provided by libc.
    ::tm data;

    /// Constructs an impl object from initialized data.
    ///
    /// \param data_ The raw timestamp to use.
    impl(const ::tm& data_) : data(data_)
    {
    }
};


}  // namespace datetime
}  // namespace utils


/// Constructs a new timestamp.
///
/// \param pimpl_ An existing impl representation.
datetime::timestamp::timestamp(std::tr1::shared_ptr< impl > pimpl_) :
    _pimpl(pimpl_)
{
}


/// Constructs a timestamp based on user-friendly values.
///
/// \param year The year in the [1900,inf) range.
/// \param month The month in the [1,12] range.
/// \param day The day in the [1,30] range.
/// \param hour The hour in the [0,23] range.
/// \param minute The minute in the [0,59] range.
/// \param second The second in the [0,59] range.
///
/// \return A new timestamp.
datetime::timestamp
datetime::timestamp::from_values(const int year, const int month,
                                 const int day, const int hour,
                                 const int minute, const int second)
{
    PRE(year >= 1900);
    PRE(month >= 1 && month <= 12);
    PRE(day >= 1 && day <= 30);
    PRE(hour >= 0 && hour <= 23);
    PRE(minute >= 0 && minute <= 59);
    PRE(second >= 0 && second <= 59);

    // The code below is quite convoluted.  The problem is that we can't assume
    // that some fields (like tm_zone) of ::tm exist, and thus we can't blindly
    // set them from the code.  Instead of detecting their presence in the
    // configure script, we just query the current time to initialize such
    // fields and then we override the ones we are interested in.  (There might
    // be some better way to do this, but I don't know it and the documentation
    // does not shed much light into how to create your own fake date.)

    const time_t current_time = ::time(NULL);

    ::tm data;
    if (::gmtime_r(&current_time, &data) == NULL)
        UNREACHABLE;

    data.tm_sec = second;
    data.tm_min = minute;
    data.tm_hour = hour;
    data.tm_mday = day;
    data.tm_mon = month - 1;
    data.tm_year = year - 1900;
    // Ignored: data.tm_wday
    // Ignored: data.tm_yday

    const time_t mock_time = ::mktime(&data);
    if (::gmtime_r(&mock_time, &data) == NULL)
        UNREACHABLE;

    return timestamp(std::tr1::shared_ptr< impl >(new impl(data)));
}


/// Constructs a new timestamp representing the current time in UTC.
///
/// \return A new timestamp.
datetime::timestamp
datetime::timestamp::now(void)
{
    ::tm data;
    {
        const time_t current_time = ::time(NULL);
        if (::gmtime_r(&current_time, &data) == NULL)
            UNREACHABLE_MSG("gmtime_r(3) did not accept the value returned by "
                            "time(3); this cannot happen");
    }

    return timestamp(std::tr1::shared_ptr< impl >(new impl(data)));
}


/// Formats a timestamp.
///
/// \param format The format string to use as consumed by strftime(3).
///
/// \return The formatted time.
std::string
datetime::timestamp::strftime(const std::string& format) const
{
    char buf[128];
    if (::strftime(buf, sizeof(buf), format.c_str(), &_pimpl->data) == 0)
        UNREACHABLE_MSG("Arbitrary-long format strings are unimplemented");
    return buf;
}
