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

#include <typeinfo>

#include "engine/atf_iface/test_program.hpp"
#include "engine/plain_iface/test_program.hpp"
#include "engine/test_program.hpp"
#include "store/dbtypes.hpp"
#include "store/exceptions.hpp"
#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"
#include "utils/sqlite/statement.hpp"

namespace atf_iface = engine::atf_iface;
namespace datetime = utils::datetime;
namespace plain_iface = engine::plain_iface;
namespace sqlite = utils::sqlite;


/// Determines the interface type of a given test program.
///
/// \param test_program The test program to determine the interface of.
///
/// \return The test program interface.
///
/// \todo It might make sense to make this a method of base_test_program and
/// make every subclass return its own type.  However, even doing this would not
/// free the storage layer from doing nasty 'switches' to act differently on
/// each interface.  Also the whole interface_type thing is only required by the
/// storage layer, so moving it into the engine may not be that appropriate.
store::detail::interface_type
store::guess_interface(const engine::base_test_program& test_program)
{
    if (typeid(test_program) == typeid(atf_iface::test_program)) {
        return detail::atf_interface;
    } else if (typeid(test_program) == typeid(plain_iface::test_program)) {
        return detail::plain_interface;
    } else
        UNREACHABLE_MSG("Unsupported test program interface");
}


/// Binds a boolean value to a statement parameter.
///
/// \param stmt The statement to which to bind the parameter.
/// \param field The name of the parameter; must exist.
/// \param value The value to bind.
void
store::bind_bool(sqlite::statement& stmt, const char* field, const bool value)
{
    stmt.bind_text(field, value ? "true" : "false");
}


/// Binds a time delta to a statement parameter.
///
/// \param stmt The statement to which to bind the parameter.
/// \param field The name of the parameter; must exist.
/// \param value The value to bind.
void
store::bind_delta(sqlite::statement& stmt, const char* field,
                  const datetime::delta& delta)
{
    stmt.bind_int64(field, delta.to_useconds());
}


/// Binds a test interface type to a statement parameter.
///
/// \param stmt The statement to which to bind the parameter.
/// \param field The name of the parameter; must exist.
/// \param value The value to bind.
void
store::bind_interface(sqlite::statement& stmt, const char* field,
                      const detail::interface_type interface)
{
    switch (interface) {
    case detail::atf_interface:
        stmt.bind_text(field, "atf");
        break;
    case detail::plain_interface:
        stmt.bind_text(field, "plain");
        break;
    default:
        UNREACHABLE_MSG("Unsupported test program interface");
    }
}


/// Binds a string to a statement parameter.
///
/// If the string is not empty, this binds the string itself.  Otherwise, it
/// binds a NULL value.
///
/// \param stmt The statement to which to bind the parameter.
/// \param stmt The statement to which to bind the field.
/// \param field The name of the parameter; must exist.
/// \param str The string to bind.
void
store::bind_optional_string(sqlite::statement& stmt, const char* field,
                            const std::string& str)
{
    if (str.empty())
        stmt.bind_null(field);
    else
        stmt.bind_text(field, str);
}


/// Queries a boolean value from a statement.
///
/// \param stmt The statement from which to get the column.
/// \param column The name of the column holding the value.
///
/// \return The parsed value if all goes well.
///
/// \throw integrity_error If the value in the specified column is invalid.
bool
store::column_bool(sqlite::statement& stmt, const char* column)
{
    const int id = stmt.column_id(column);
    if (stmt.column_type(id) != sqlite::type_text)
        throw store::integrity_error(F("Boolean value in column %s is not a "
                                       "string") % column);
    const std::string value = stmt.column_text(id);
    if (value == "true")
        return true;
    else if (value == "false")
        return false;
    else
        throw store::integrity_error(F("Unknown boolean value '%s'") % value);
}


/// Queries a time delta from a statement.
///
/// \param stmt The statement from which to get the column.
/// \param column The name of the column holding the value.
///
/// \return The parsed value if all goes well.
///
/// \throw integrity_error If the value in the specified column is invalid.
datetime::delta
store::column_delta(sqlite::statement& stmt, const char* column)
{
    const int id = stmt.column_id(column);
    if (stmt.column_type(id) != sqlite::type_integer)
        throw store::integrity_error(F("Time delta in column %s is not an "
                                       "integer") % column);
    return datetime::delta::from_useconds(stmt.column_int64(id));
}


/// Queries an interface type from a statement.
///
/// \param stmt The statement from which to get the column.
/// \param column The name of the column holding the value.
///
/// \return The parsed value if all goes well.
///
/// \throw integrity_error If the value in the specified column is invalid.
store::detail::interface_type
store::column_interface(sqlite::statement& stmt, const char* column)
{
    const int id = stmt.column_id(column);
    if (stmt.column_type(id) != sqlite::type_text)
        throw store::integrity_error(F("Interface name value in column %s is "
                                       "not a string") % column);
    const std::string value = stmt.column_text(id);
    if (value == "atf")
        return detail::atf_interface;
    else if (value == "plain")
        return detail::plain_interface;
    else
        throw store::integrity_error(F("Unknown interface name '%s'") % value);
}


/// Queries an optional string from a statement.
///
/// \param stmt The statement from which to get the column.
/// \param column The name of the column holding the value.
///
/// \return The parsed value if all goes well.
///
/// \throw integrity_error If the value in the specified column is invalid.
std::string
store::column_optional_string(sqlite::statement& stmt, const char* column)
{
    const int id = stmt.column_id(column);
    switch (stmt.column_type(id)) {
    case sqlite::type_text:
        return stmt.column_text(id);
    case sqlite::type_null:
        return "";
    default:
        throw integrity_error(F("Invalid string type in column %s") % column);
    }
}
