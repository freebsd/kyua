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

#include "utils/config/tree.ipp"

#include <typeinfo>

#include "utils/config/exceptions.hpp"
#include "utils/text/operations.hpp"

namespace config = utils::config;
namespace text = utils::text;


/// Converts a key to its textual representation.
///
/// \param key The key to convert.
std::string
config::detail::flatten_key(const tree_key& key)
{
    PRE(!key.empty());
    return text::join(key, ".");
}


/// Parses and validates a textual key.
///
/// \param str The key to process in dotted notation.
///
/// \return The tokenized key if valid.
///
/// \throw invalid_key_error If the input key is empty or invalid for any other
///     reason.  Invalid does NOT mean unknown though.
config::detail::tree_key
config::detail::parse_key(const std::string& str)
{
    const tree_key key = text::split(str, '.');
    if (key.empty())
        throw invalid_key_error("Empty key");
    for (tree_key::const_iterator iter = key.begin(); iter != key.end(); iter++)
        if ((*iter).empty())
            throw invalid_key_error("Empty component in key");
    return key;
}


/// Destructor.
config::detail::base_node::~base_node(void)
{
}


/// Constructor.
///
/// \param dynamic_ Whether the node is dynamic or not.
config::detail::inner_node::inner_node(const bool dynamic_) :
    _dynamic(dynamic_)
{
}


/// Destructor.
config::detail::inner_node::~inner_node(void)
{
    for (children_map::const_iterator iter = _children.begin();
         iter != _children.end(); ++iter)
        delete (*iter).second;
}


/// Converts the subtree to a collection of key/value string pairs.
///
/// \param [out] properties The accumulator for the generated properties.  The
///     contents of the map are only extended.
/// \param key The path to the current node.
void
config::detail::inner_node::get_all_properties(properties_map& properties,
                                               const tree_key& key) const
{
    for (children_map::const_iterator iter = _children.begin();
         iter != _children.end(); ++iter) {
        tree_key child_key = key;
        child_key.push_back((*iter).first);

        try {
            const leaf_node& node = dynamic_cast< const leaf_node& >(
                *(*iter).second);
            if (node.is_set())
                properties[flatten_key(child_key)] = node.to_string();
        } catch (const std::bad_cast& e) {
            try {
                const inner_node& node = dynamic_cast< const inner_node& >(
                    *(*iter).second);
                node.get_all_properties(properties, child_key);
            } catch (const std::bad_cast& e2) {
                UNREACHABLE_MSG("Node not inner nor leaf");
            }
        }
    }
}


/// Constructor.
config::detail::static_inner_node::static_inner_node(void) :
    inner_node(false)
{
}


/// Constructor.
config::detail::dynamic_inner_node::dynamic_inner_node(void) :
    inner_node(true)
{
}


/// Destructor.
config::leaf_node::~leaf_node(void)
{
}


/// Constructor.
config::tree::tree(void) :
    _root(new detail::static_inner_node())
{
}


/// Destructor.
config::tree::~tree(void)
{
    delete _root;
}


/// Registers a node as being dynamic.
///
/// This operation creates the given key as an inner node.  Further set
/// operations that trespass this node will automatically create any missing
/// keys.
///
/// This method does not raise errors on invalid/unknown keys or other
/// tree-related issues.  The reasons is that define() is a method that does not
/// depend on user input: it is intended to pre-populate the tree with a
/// specific structure, and that happens once at coding time.
///
/// \param dotted_key The key to be registered in dotted representation.
void
config::tree::define_dynamic(const std::string& dotted_key)
{
    try {
        const detail::tree_key key = detail::parse_key(dotted_key);
        _root->define< detail::dynamic_inner_node >(key, 0);
    } catch (const error& e) {
        UNREACHABLE_MSG("define() failing due to key errors is a programming "
                        "mistake: " + std::string(e.what()));
    }
}


/// Converts the tree to a collection of key/value string pairs.
///
/// \return A map of keys to values in their textual representation.
config::properties_map
config::tree::all_properties(void) const
{
    properties_map properties;
    _root->get_all_properties(properties, detail::tree_key());
    return properties;
}
