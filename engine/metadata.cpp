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

#include "engine/exceptions.hpp"
#include "utils/config/exceptions.hpp"
#include "utils/config/nodes.ipp"
#include "utils/config/tree.ipp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/path.hpp"
#include "utils/sanity.hpp"
#include "utils/units.hpp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace units = utils::units;


namespace {


/// A leaf node that holds a bytes quantity.
class bytes_node : public config::native_leaf_node< units::bytes > {
public:
    /// Pushes the node's value onto the Lua stack.
    ///
    /// \param unused_state The Lua state onto which to push the value.
    void
    push_lua(lutok::state& UTILS_UNUSED_PARAM(state)) const
    {
        UNREACHABLE;
    }

    /// Sets the value of the node from an entry in the Lua stack.
    ///
    /// \param unused_state The Lua state from which to get the value.
    /// \param unused_index The stack index in which the value resides.
    void
    set_lua(lutok::state& UTILS_UNUSED_PARAM(state),
            const int UTILS_UNUSED_PARAM(index))
    {
        UNREACHABLE;
    }
};


/// A leaf node that holds a "required user" property.
///
/// This node is just a string, but it provides validation of the only allowed
/// values.
class user_node : public config::string_node {
    /// Checks a given user textual representation for validity.
    ///
    /// \param user The value to validate.
    ///
    /// \throw config::value_error If the value is not valid.
    void
    validate(const value_type& user) const
    {
        if (!user.empty() && user != "root" && user != "unprivileged")
            throw config::value_error("Invalid required user value");
    }
};


/// A leaf node that holds a set of paths.
///
/// This node type is used to represent the value of the required files and
/// required programs, for example, and these do not allow relative paths.  We
/// check this here.
class paths_set_node : public config::base_set_node< fs::path > {
    /// Converts a single path to the native type.
    ///
    /// \param raw_value The value to parse.
    ///
    /// \return The parsed value.
    ///
    /// \throw config::value_error If the value is invalid.
    fs::path
    parse_one(const std::string& raw_value) const
    {
        try {
            return fs::path(raw_value);
        } catch (const fs::error& e) {
            throw config::value_error(e.what());
        }
    }

    /// Checks a collection of paths for validity.
    ///
    /// \param paths The value to validate.
    ///
    /// \throw config::value_error If the value is not valid.
    void
    validate(const value_type& paths) const
    {
        for (value_type::const_iterator iter = paths.begin();
             iter != paths.end(); ++iter) {
            const fs::path& path = *iter;
            if (!path.is_absolute() && path.ncomponents() > 1)
                throw config::value_error(F("Relative path '%s' not allowed") %
                                          *iter);
        }
    }
};


/// Initializes a tree to hold test case requirements.
///
/// \param [in,out] tree The tree to initialize.
static void
init_reqs_tree(config::tree& tree)
{
    tree.define< config::strings_set_node >("allowed_architectures");
    tree.set< config::strings_set_node >("allowed_architectures",
                                         engine::strings_set());

    tree.define< config::strings_set_node >("allowed_platforms");
    tree.set< config::strings_set_node >("allowed_platforms",
                                         engine::strings_set());

    tree.define< config::strings_set_node >("required_configs");
    tree.set< config::strings_set_node >("required_configs",
                                         engine::strings_set());

    tree.define< paths_set_node >("required_files");
    tree.set< paths_set_node >("required_files", engine::paths_set());

    tree.define< bytes_node >("required_memory");
    tree.set< bytes_node >("required_memory", units::bytes(0));

    tree.define< paths_set_node >("required_programs");
    tree.set< paths_set_node >("required_programs", engine::paths_set());

    tree.define< user_node >("required_user");
    tree.set< user_node >("required_user", "");
}


/// Looks up a value in a tree with error rewriting.
///
/// \tparam NodeType The type of the node.
/// \param tree The tree in which to insert the value.
/// \param key The key to set.
///
/// \return A read-write reference to the value in the node.
///
/// \throw engine::error If the key is not known or if the value is not valid.
template< class NodeType >
typename NodeType::value_type&
lookup_rw(config::tree& tree, const std::string& key)
{
    try {
        return tree.lookup_rw< NodeType >(key);
    } catch (const config::unknown_key_error& e) {
        throw engine::error(F("Unknown metadata property %s") % key);
    } catch (const config::value_error& e) {
        throw engine::error(F("Invalid value for metadata property %s: %s") %
                            key % e.what());
    }
}


/// Sets a value in a tree with error rewriting.
///
/// \tparam NodeType The type of the node.
/// \param tree The tree in which to insert the value.
/// \param key The key to set.
/// \param value The value to set the node to.
///
/// \throw engine::error If the key is not known or if the value is not valid.
template< class NodeType >
void
set(config::tree& tree, const std::string& key,
    const typename NodeType::value_type& value)
{
    try {
        tree.set< NodeType >(key, value);
    } catch (const config::unknown_key_error& e) {
        throw engine::error(F("Unknown metadata property %s") % key);
    } catch (const config::value_error& e) {
        throw engine::error(F("Invalid value for metadata property %s: %s") %
                            key % e.what());
    }
}


}  // anonymous namespace


/// Internal implementation of the metadata class.
struct engine::metadata::impl {
    /// Collection of requirements.
    config::tree reqs;

    /// Constructor.
    ///
    /// \param reqs_ Requirements of the test.
    impl(const utils::config::tree& reqs_) :
        reqs(reqs_)
    {
    }
};


/// Constructor.
///
/// \param reqs Requirements of the test.
engine::metadata::metadata(const utils::config::tree& reqs) :
    _pimpl(new impl(reqs))
{
}


/// Destructor.
engine::metadata::~metadata(void)
{
}


/// Returns the architectures allowed by the test.
///
/// \return Set of architectures, or empty if this does not apply.
const engine::strings_set&
engine::metadata::allowed_architectures(void) const
{
    return _pimpl->reqs.lookup< config::strings_set_node >(
        "allowed_architectures");
}


/// Returns the platforms allowed by the test.
///
/// \return Set of platforms, or empty if this does not apply.
const engine::strings_set&
engine::metadata::allowed_platforms(void) const
{
    return _pimpl->reqs.lookup< config::strings_set_node >("allowed_platforms");
}


/// Returns the list of configuration variables needed by the test.
///
/// \return Set of configuration variables.
const engine::strings_set&
engine::metadata::required_configs(void) const
{
    return _pimpl->reqs.lookup< config::strings_set_node >("required_configs");
}


/// Returns the list of files needed by the test.
///
/// \return Set of paths.
const engine::paths_set&
engine::metadata::required_files(void) const
{
    return _pimpl->reqs.lookup< paths_set_node >("required_files");
}


/// Returns the amount of memory required by the test.
///
/// \return Number of bytes, or 0 if this does not apply.
const units::bytes&
engine::metadata::required_memory(void) const
{
    return _pimpl->reqs.lookup< bytes_node >("required_memory");
}


/// Returns the list of programs needed by the test.
///
/// \return Set of paths.
const engine::paths_set&
engine::metadata::required_programs(void) const
{
    return _pimpl->reqs.lookup< paths_set_node >("required_programs");
}


/// Returns the user required by the test.
///
/// \return One of unprivileged, root or empty.
const std::string&
engine::metadata::required_user(void) const
{
    return _pimpl->reqs.lookup< user_node >("required_user");
}


/// Externalizes the metadata to a set of key/value textual pairs.
///
/// \return A key/value representation of the metadata.
engine::properties_map
engine::metadata::to_properties(void) const
{
    return _pimpl->reqs.all_properties();
}


/// Internal implementation of the metadata_builder class.
struct engine::metadata_builder::impl {
    /// Collection of requirements.
    config::tree reqs;

    /// Whether we have created a metadata object or not.
    bool built;

    /// Constructor.
    impl(void) :
        built(false)
    {
        init_reqs_tree(reqs);
    }
};


/// Constructor.
engine::metadata_builder::metadata_builder(void) :
    _pimpl(new impl())
{
}


/// Destructor.
engine::metadata_builder::~metadata_builder(void)
{
}


/// Accumulates an additional allowed architecture.
///
/// \param arch The architecture.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::add_allowed_architecture(const std::string& arch)
{
    lookup_rw< config::strings_set_node >(
        _pimpl->reqs, "allowed_architectures").insert(arch);
    return *this;
}


/// Accumulates an additional allowed platform.
///
/// \param platform The platform.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::add_allowed_platform(const std::string& platform)
{
    lookup_rw< config::strings_set_node >(
        _pimpl->reqs, "allowed_platforms").insert(platform);
    return *this;
}


/// Accumulates an additional required configuration variable.
///
/// \param var The name of the configuration variable.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::add_required_config(const std::string& var)
{
    lookup_rw< config::strings_set_node >(
        _pimpl->reqs, "required_configs").insert(var);
    return *this;
}


/// Accumulates an additional required file.
///
/// \param path The path to the file.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::add_required_file(const fs::path& path)
{
    lookup_rw< paths_set_node >(_pimpl->reqs, "required_files").insert(path);
    return *this;
}


/// Accumulates an additional required program.
///
/// \param path The path to the program.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::add_required_program(const fs::path& path)
{
    lookup_rw< paths_set_node >(_pimpl->reqs, "required_programs").insert(path);
    return *this;
}


/// Sets the architectures allowed by the test.
///
/// \param as Set of architectures.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_allowed_architectures(const strings_set& as)
{
    set< config::strings_set_node >(_pimpl->reqs, "allowed_architectures", as);
    return *this;
}


/// Sets the platforms allowed by the test.
///
/// \return ps Set of platforms.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_allowed_platforms(const strings_set& ps)
{
    set< config::strings_set_node >(_pimpl->reqs, "allowed_platforms", ps);
    return *this;
}


/// Sets the list of configuration variables needed by the test.
///
/// \param vars Set of configuration variables.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_required_configs(const strings_set& vars)
{
    set< config::strings_set_node >(_pimpl->reqs, "required_configs", vars);
    return *this;
}


/// Sets the list of files needed by the test.
///
/// \param files Set of paths.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_required_files(const paths_set& files)
{
    set< paths_set_node >(_pimpl->reqs, "required_files", files);
    return *this;
}


/// Sets the amount of memory required by the test.
///
/// \param bytes Number of bytes.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_required_memory(const units::bytes& bytes)
{
    set< bytes_node >(_pimpl->reqs, "required_memory", bytes);
    return *this;
}


/// Sets the list of programs needed by the test.
///
/// \param progs Set of paths.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_required_programs(const paths_set& progs)
{
    set< paths_set_node >(_pimpl->reqs, "required_programs", progs);
    return *this;
}


/// Sets the user required by the test.
///
/// \param user One of unprivileged, root or empty.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid.
engine::metadata_builder&
engine::metadata_builder::set_required_user(const std::string& user)
{
    set< user_node >(_pimpl->reqs, "required_user", user);
    return *this;
}


/// Sets a metadata property by name from its textual representation.
///
/// \param key The property to set.
/// \param value The value to set the property to.
///
/// \return A reference to this builder.
///
/// \throw engine::error If the value is invalid or the key does not exist.
engine::metadata_builder&
engine::metadata_builder::set_string(const std::string& key,
                                     const std::string& value)
{
    try {
        _pimpl->reqs.set_string(key, value);
    } catch (const config::unknown_key_error& e) {
        throw engine::format_error(F("Unknown metadata property %s") % key);
    } catch (const config::value_error& e) {
        throw engine::format_error(
            F("Invalid value for metadata property %s: %s") % key % e.what());
    }
    return *this;
}


/// Creates a new metadata object.
///
/// \pre This has not yet been called.  We only support calling this function
/// once due to the way the internal tree works: we pass around references, not
/// deep copies, so if we allowed a second build, we'd encourage reusing the
/// same builder to construct different metadata objects, and this could have
/// unintended consequences.
///
/// \return The constructed metadata object.
engine::metadata
engine::metadata_builder::build(void) const
{
    PRE(!_pimpl->built);
    _pimpl->built = true;

    return metadata(_pimpl->reqs);
}
