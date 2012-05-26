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

#include "utils/config/exceptions.hpp"
#include "utils/format/macros.hpp"
#include "utils/text/operations.hpp"

namespace config = utils::config;
namespace text = utils::text;


/// Converts a key to its textual representation.
///
/// \param key The key to convert.
std::string
utils::config::detail::flatten_key(const tree_key& key)
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
utils::config::detail::tree_key
utils::config::detail::parse_key(const std::string& str)
{
    const tree_key key = text::split(str, '.');
    if (key.empty())
        throw invalid_key_error("Empty key");
    for (tree_key::const_iterator iter = key.begin(); iter != key.end(); iter++)
        if ((*iter).empty())
            throw invalid_key_error(F("Empty component in key '%s'") % str);
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


/// Finds a node without creating it if not found.
///
/// This recursive algorithm traverses the tree searching for a particular key.
/// The returned node is constant, so this can only be used for querying
/// purposes.  For this reason, this algorithm does not create intermediate
/// nodes if they don't exist (as would be necessary to set a new node).
///
/// \param key The key to be queried.
/// \param key_pos The current level within the key to be examined.
///
/// \return A reference to the located node, if successful.
///
/// \throw unknown_key_error If the provided key is unknown.
const config::detail::base_node*
config::detail::inner_node::lookup_ro(const tree_key& key,
                                      const tree_key::size_type key_pos) const
{
    if (key_pos == key.size())
        throw unknown_key_error(key);

    const children_map::const_iterator child_iter = _children.find(
        key[key_pos]);
    if (child_iter == _children.end())
        throw unknown_key_error(key);

    if (key_pos == key.size() - 1) {
        return (*child_iter).second;
    } else {
        PRE(key_pos < key.size() - 1);
        try {
            const inner_node& child = dynamic_cast< const inner_node& >(
                *(*child_iter).second);
            return child.lookup_ro(key, key_pos + 1);
        } catch (const std::bad_cast& e) {
            throw unknown_key_error(key);
        }
    }
}


/// Finds a node and creates it if not found.
///
/// This recursive algorithm traverses the tree searching for a particular key,
/// creating any intermediate nodes if they do not already exist (for the case
/// of dynamic inner nodes).  The returned node is non-constant, so this can be
/// used by the algorithms that set key values.
///
/// \param key The key to be queried.
/// \param key_pos The current level within the key to be examined.
/// \param new_node A function that returns a new leaf node of the desired
///     type.  This is only called if the leaf cannot be found, but it has
///     already been defined.
///
/// \return A reference to the located node, if successful.
///
/// \throw unknown_key_error If the provided key is unknown.
/// \throw value_error If the resulting node of the search would be an inner
///     node.
config::leaf_node*
config::detail::inner_node::lookup_rw(const tree_key& key,
                                      const tree_key::size_type key_pos,
                                      new_node_hook new_node)
{
    if (key_pos == key.size())
        throw unknown_key_error(key);

    children_map::const_iterator child_iter = _children.find(key[key_pos]);
    if (child_iter == _children.end()) {
        if (_dynamic) {
            base_node* const child = (key_pos == key.size() - 1) ?
                static_cast< base_node* >(new_node()) :
                static_cast< base_node* >(new dynamic_inner_node());
            _children.insert(children_map::value_type(key[key_pos], child));
            child_iter = _children.find(key[key_pos]);
        } else {
            throw unknown_key_error(key);
        }
    }

    if (key_pos == key.size() - 1) {
        try {
            leaf_node& child = dynamic_cast< leaf_node& >(
                *(*child_iter).second);
            return &child;
        } catch (const std::bad_cast& unused_error) {
            throw value_error(F("Invalid value for key '%s'") %
                              flatten_key(key));
        }
    } else {
        PRE(key_pos < key.size() - 1);
        try {
            inner_node& child = dynamic_cast< inner_node& >(
                *(*child_iter).second);
            return child.lookup_rw(key, key_pos + 1, new_node);
        } catch (const std::bad_cast& e) {
            throw unknown_key_error(key);
        }
    }
}


/// Converts the subtree to a collection of key/value string pairs.
///
/// \param [out] properties The accumulator for the generated properties.  The
///     contents of the map are only extended.
/// \param key The path to the current node.
void
config::detail::inner_node::all_properties(properties_map& properties,
                                           const tree_key& key) const
{
    for (children_map::const_iterator iter = _children.begin();
         iter != _children.end(); ++iter) {
        tree_key child_key = key;
        child_key.push_back((*iter).first);
        try {
            (*iter).second->all_properties(properties, child_key);
        } catch (const unknown_key_error& unused_error) {
            // Ignore.  This can happen when we invoke all_properties on a
            // defined but unset leaf node, and we don't want to consider this
            // an error.
        }
    }
}


/// Constructor.
config::detail::static_inner_node::static_inner_node(void) :
    inner_node(false)
{
}


/// Registers a key as valid and having a specific type.
///
/// This method does not raise errors on invalid/unknown keys or other
/// tree-related issues.  The reasons is that define() is a method that does not
/// depend on user input: it is intended to pre-populate the tree with a
/// specific structure, and that happens once at coding time.
///
/// \param key The key to be registered.
/// \param key_pos The current level within the key to be examined.
/// \param new_node A function that returns a new leaf node of the desired
///     type.
void
config::detail::static_inner_node::define(const tree_key& key,
                                          const tree_key::size_type key_pos,
                                          new_node_hook new_node)
{
    if (key_pos == key.size() - 1) {
        PRE_MSG(_children.find(key[key_pos]) == _children.end(),
                "Key already defined");
        _children.insert(children_map::value_type(key[key_pos], new_node()));
    } else {
        PRE(key_pos < key.size() - 1);
        const children_map::const_iterator child_iter = _children.find(
            key[key_pos]);

        if (child_iter == _children.end()) {
            static_inner_node* const child_ptr = new static_inner_node();
            _children.insert(children_map::value_type(key[key_pos], child_ptr));
            child_ptr->define(key, key_pos + 1, new_node);
        } else {
            try {
                static_inner_node& child = dynamic_cast< static_inner_node& >(
                    *(*child_iter).second);
                child.define(key, key_pos + 1, new_node);
            } catch (const std::bad_cast& e) {
                UNREACHABLE;
            }
        }
    }
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
        _root->define(key, 0, detail::new_node< detail::dynamic_inner_node >);
    } catch (const error& e) {
        UNREACHABLE_MSG("define() failing due to key errors is a programming "
                        "mistake: " + std::string(e.what()));
    }
}


/// Checks if a given node is set.
///
/// \param dotted_key The key to be checked.
///
/// \return True if the key is set to a specific value (not just defined).
/// False if the key is not set or if the key does not exist.
///
/// \throw invalid_key_error If the provided key has an invalid format.
bool
config::tree::is_set(const std::string& dotted_key) const
{
    const detail::tree_key key = detail::parse_key(dotted_key);
    try {
        const detail::base_node* raw_node = _root->lookup_ro(key, 0);
        try {
            const leaf_node& child = dynamic_cast< const leaf_node& >(
                *raw_node);
            return child.is_set();
        } catch (const std::bad_cast& unused_error) {
            return false;
        }
    } catch (const unknown_key_error& unused_error) {
        return false;
    }
}


/// Gets the value of a node as a plain string.
///
/// \param dotted_key The key to be looked up.
///
/// \return The value of the located node as a string.
///
/// \throw invalid_key_error If the provided key has an invalid format.
/// \throw unknown_key_error If the provided key is unknown.
std::string
config::tree::lookup_string(const std::string& dotted_key) const
{
    const detail::tree_key key = detail::parse_key(dotted_key);
    const detail::base_node* raw_node = _root->lookup_ro(key, 0);
    try {
        const leaf_node& child = dynamic_cast< const leaf_node& >(*raw_node);
        return child.to_string();
    } catch (const std::bad_cast& unused_error) {
        throw unknown_key_error(key);
    }
}


/// Sets the value of a leaf addressed by its key from a string value.
///
/// This respects the native types of all the nodes that have been predefined.
/// For new nodes under a dynamic subtree, this has no mechanism of determining
/// what type they need to have, so they are created as plain string nodes.
///
/// \param dotted_key The key to be registered in dotted representation.
/// \param raw_value The string representation of the value to set the node to.
///
/// \throw invalid_key_error If the provided key has an invalid format.
/// \throw unknown_key_error If the provided key is unknown.
/// \throw value_error If the value mismatches the node type.
void
config::tree::set_string(const std::string& dotted_key,
                         const std::string& raw_value)
{
    const detail::tree_key key = detail::parse_key(dotted_key);
    detail::base_node* raw_node = _root->lookup_rw(
        key, 0, detail::new_node< string_node >);
    try {
        leaf_node& child = dynamic_cast< leaf_node& >(*raw_node);
        child.set_string(raw_value);
    } catch (const std::bad_cast& unused_error) {
        throw value_error(F("Invalid value for key '%s'") %
                          detail::flatten_key(key));
    }
}


/// Converts the tree to a collection of key/value string pairs.
///
/// \return A map of keys to values in their textual representation.
///
/// \throw invalid_key_error If the provided key has an invalid format.
/// \throw unknown_key_error If the provided key is unknown.
config::properties_map
config::tree::all_properties(const std::string& dotted_key) const
{
    properties_map properties;

    detail::tree_key key;
    const detail::base_node* raw_node;
    if (dotted_key.empty()) {
        raw_node = _root;
    } else {
        key = detail::parse_key(dotted_key);
        raw_node = _root->lookup_ro(key, 0);
    }
    raw_node->all_properties(properties, key);

    return properties;
}
