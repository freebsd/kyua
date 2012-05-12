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

#include "utils/config/tree.hpp"

#if !defined(UTILS_CONFIG_TREE_IPP)
#define UTILS_CONFIG_TREE_IPP

#include <stdexcept>

#include "utils/config/exceptions.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/text/exceptions.hpp"
#include "utils/text/operations.ipp"
#include "utils/sanity.hpp"

namespace utils {


namespace config {
namespace detail {


std::string flatten_key(const tree_key&);
tree_key parse_key(const std::string&);


/// Internal node of the tree.
///
/// This abstract base class provides the mechanism to implement both static and
/// dynamic nodes.  Ideally, the implementation would be split in subclasses and
/// this class would not include the knowledge of whether the node is dynamic or
/// not.  However, because the static/dynamic difference depends on the leaf
/// types, we need to declare template functions and these cannot be virtual.
class inner_node : public base_node {
    /// Whether the node is dynamic or not.
    bool _dynamic;

protected:
    /// Type to represent the collection of children of this node.
    ///
    /// Note that these are one-level keys.  They cannot contain dots, and thus
    /// is why we use a string rather than a tree_key.
    typedef std::map< std::string, base_node* > children_map;

    /// Mapping of keys to values that are descendants of this node.
    children_map _children;

public:
    inner_node(const bool);
    virtual ~inner_node(void) = 0;

    const base_node* lookup_node(const tree_key&,
                                 const tree_key::size_type) const;

    void all_properties(properties_map&, const tree_key&) const;
    bool is_set(void) const;

    template< class LeafType >
    const typename LeafType::value_type& lookup(
        const tree_key&, const tree_key::size_type) const;

    template< class LeafType >
    void set(const tree_key&, const tree_key::size_type,
             const typename LeafType::value_type&);

    void set_string(const tree_key&, const tree_key::size_type,
                    const std::string&);
};


/// Static internal node of the tree.
///
/// The direct children of this node must be pre-defined by calls to define().
/// Attempts to traverse this node and resolve a key that is not a pre-defined
/// children will result in an "unknown key" error.
class static_inner_node : public config::detail::inner_node {
public:
    static_inner_node(void);

    template< class LeafType >
    void define(const tree_key&, const tree_key::size_type);
};


/// Dynamic internal node of the tree.
///
/// The children of this node need not be pre-defined.  Attempts to traverse
/// this node and resolve a key will result in such key being created.  Any
/// intermediate non-existent nodes of the traversal will be created as dynamic
/// inner nodes as well.
class dynamic_inner_node : public config::detail::inner_node {
public:
    dynamic_inner_node(void);
};


}  // namespace detail
}  // namespace config


/// Gets the value of a leaf addressed by its key.
///
/// \tparam LeafType The node type of the leaf we are querying.
/// \param key The key to be queried.
/// \param key_pos The current level within the key to be examined.
///
/// \return A reference to the value in the located leaf, if successful.
///
/// \throw unknown_key_error If the provided key is unknown.
template< class LeafType >
const typename LeafType::value_type&
config::detail::inner_node::lookup(const tree_key& key,
                                   const tree_key::size_type key_pos) const
{
    const base_node* base_child = lookup_node(key, key_pos);

    try {
        const LeafType& child = dynamic_cast< const LeafType& >(*base_child);
        if (child.is_set())
            return child.value();
        else
            throw unknown_key_error(F("Unknown key '%s'") % flatten_key(key));
    } catch (const std::bad_cast& e) {
        try {
            (void)dynamic_cast< const inner_node& >(*base_child);
            throw unknown_key_error(F("Unknown key '%s'") % flatten_key(key));
        } catch (const std::bad_cast& e) {
            UNREACHABLE_MSG("Invalid type for node");
        }
    }
}


/// Sets the value of a leaf addressed by its key.
///
/// \tparam LeafType The node type of the leaf we are setting.
/// \param key The key to be set.
/// \param key_pos The current level within the key to be examined.
/// \param value The value to set into the node.
///
/// \throw unknown_key_error If the provided key is unknown.
/// \throw value_error If the value mismatches the node type.
template< class LeafType >
void
config::detail::inner_node::set(const tree_key& key,
                                const tree_key::size_type key_pos,
                                const typename LeafType::value_type& value)
{
    if (key_pos == key.size())
        throw unknown_key_error(F("Unknown key '%s'") % flatten_key(key));

    children_map::const_iterator child_iter = _children.find(key[key_pos]);
    if (child_iter == _children.end()) {
        if (_dynamic) {
            base_node* const child = (key_pos == key.size() - 1) ?
                static_cast< base_node* >(new LeafType()) :
                static_cast< base_node* >(new dynamic_inner_node());
            _children.insert(children_map::value_type(key[key_pos], child));
            child_iter = _children.find(key[key_pos]);
        } else {
            throw unknown_key_error(F("Unknown key '%s'") % flatten_key(key));
        }
    }

    if (key_pos == key.size() - 1) {
        try {
            LeafType& child = dynamic_cast< LeafType& >(*(*child_iter).second);
            child.set(value);
        } catch (const std::bad_cast& e) {
            throw value_error(F("Invalid value for key '%s'") %
                              flatten_key(key));
        }
    } else {
        PRE(key_pos < key.size() - 1);
        try {
            inner_node& child = dynamic_cast< inner_node& >(
                *(*child_iter).second);
            child.set< LeafType >(key, key_pos + 1, value);
        } catch (const std::bad_cast& e) {
            throw unknown_key_error(F("Unknown key '%s'") % flatten_key(key));
        }
    }
}


/// Registers a key as valid and having a specific type.
///
/// This method does not raise errors on invalid/unknown keys or other
/// tree-related issues.  The reasons is that define() is a method that does not
/// depend on user input: it is intended to pre-populate the tree with a
/// specific structure, and that happens once at coding time.
///
/// \tparam LeafType The node type of the leaf we are defining.
/// \param key The key to be registered.
/// \param key_pos The current level within the key to be examined.
template< class LeafType >
void
config::detail::static_inner_node::define(const tree_key& key,
                                          const tree_key::size_type key_pos)
{
    if (key_pos == key.size() - 1) {
        PRE_MSG(_children.find(key[key_pos]) == _children.end(),
                "Key already defined");
        _children.insert(children_map::value_type(key[key_pos],
                                                  new LeafType()));
    } else {
        PRE(key_pos < key.size() - 1);
        const children_map::const_iterator child_iter = _children.find(
            key[key_pos]);

        if (child_iter == _children.end()) {
            static_inner_node* const child_ptr = new static_inner_node();
            _children.insert(children_map::value_type(key[key_pos], child_ptr));
            child_ptr->define< LeafType >(key, key_pos + 1);
        } else {
            try {
                static_inner_node& child = dynamic_cast< static_inner_node& >(
                    *(*child_iter).second);
                child.define< LeafType >(key, key_pos + 1);
            } catch (const std::bad_cast& e) {
                UNREACHABLE;
            }
        }
    }
}


/// Constructor for a node with an undefined value.
///
/// This should only be called by the tree's define() method as a way to
/// register a node as known but undefined.  The node will then serve as a
/// placeholder for future values.
template< typename ValueType >
config::typed_leaf_node< ValueType >::typed_leaf_node(void) :
    _value(none)
{
}


/// Extracts a textual representation of the node as key/value string pair.
///
/// \param [out] properties The accumulator for the generated properties.
///     The contents of the map are only extended.
/// \param key The path to the current node.
///
/// \throw unknown_key_error If the node has been defined but not set yet.  The
///     caller should use is_set() on the node before calling this method if it
///     desires to avoid this exception.
template< typename ValueType >
void
config::typed_leaf_node< ValueType >::all_properties(
    properties_map& properties, const detail::tree_key& key) const
{
    if (is_set())
        properties[detail::flatten_key(key)] = to_string();
    else
        throw unknown_key_error(F("Unknown key '%s'") %
                                detail::flatten_key(key));
}


/// Checks whether the node has been set.
///
/// Remember that a node can exist before holding a value (i.e. when the node
/// has been defined as "known" but not yet set by the user).  This function
/// checks whether the node laready holds a value.
///
/// \return True if a value has been set in the node.
template< typename ValueType >
bool
config::typed_leaf_node< ValueType >::is_set(void) const
{
    return static_cast< bool >(_value);
}


/// Sets the value of the node from a raw string representation.
///
/// \param raw_value The value to set the node to.
///
/// \throw value_error If the value is invalid.
template< typename ValueType >
void
config::typed_leaf_node< ValueType >::set_string(const std::string& raw_value)
{
    try {
        _value = text::to_type< ValueType >(raw_value);
    } catch (const text::value_error& e) {
        throw value_error(e.what());
    }
}


/// Converts the contents of the node to a string.
///
/// \pre The node must have a value.
///
/// \return A string representation of the value held by the node.
template< typename ValueType >
std::string
config::typed_leaf_node< ValueType >::to_string(void) const
{
    PRE(is_set());
    return F("%s") % _value.get();
}


/// Gets the value stored in the node.
///
/// \pre The node must have a value.
///
/// \return The value in the node.
template< typename ValueType >
const typename config::typed_leaf_node< ValueType >::value_type&
config::typed_leaf_node< ValueType >::value(void) const
{
    PRE(is_set());
    return _value.get();
}


/// Sets the value of the node.
///
/// \param value_ The new value to set the node to.
template< typename ValueType >
void
config::typed_leaf_node< ValueType >::set(const value_type& value_)
{
    _value = optional< value_type >(value_);
}


/// Registers a key as valid and having a specific type.
///
/// This method does not raise errors on invalid/unknown keys or other
/// tree-related issues.  The reasons is that define() is a method that does not
/// depend on user input: it is intended to pre-populate the tree with a
/// specific structure, and that happens once at coding time.
///
/// \tparam LeafType The node type of the leaf we are defining.
/// \param dotted_key The key to be registered in dotted representation.
template< class LeafType >
void
config::tree::define(const std::string& dotted_key)
{
    try {
        const detail::tree_key key = detail::parse_key(dotted_key);
        _root->define< LeafType >(key, 0);
    } catch (const error& e) {
        UNREACHABLE_MSG(F("define() failing due to key errors is a programming "
                          "mistake: %s") % e.what());
    }
}


/// Gets the value of a leaf addressed by its key.
///
/// \tparam LeafType The node type of the leaf we are querying.
/// \param dotted_key The key to be registered in dotted representation.
///
/// \return A reference to the value in the located leaf, if successful.
///
/// \throw invalid_key_error If the provided key has an invalid format.
/// \throw unknown_key_error If the provided key is unknown.
template< class LeafType >
const typename LeafType::value_type&
config::tree::lookup(const std::string& dotted_key) const
{
    const detail::tree_key key = detail::parse_key(dotted_key);
    return _root->lookup< LeafType >(key, 0);
}


/// Sets the value of a leaf addressed by its key.
///
/// \tparam LeafType The node type of the leaf we are setting.
/// \param dotted_key The key to be registered in dotted representation.
/// \param value The value to set into the node.
///
/// \throw invalid_key_error If the provided key has an invalid format.
/// \throw unknown_key_error If the provided key is unknown.
/// \throw value_error If the value mismatches the node type.
template< class LeafType >
void
config::tree::set(const std::string& dotted_key,
                  const typename LeafType::value_type& value)
{
    const detail::tree_key key = detail::parse_key(dotted_key);
    _root->set< LeafType >(key, 0, value);
}


}  // namespace utils

#endif  // !defined(UTILS_CONFIG_TREE_IPP)
