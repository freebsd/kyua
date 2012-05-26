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

#include <typeinfo>

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


/// Type of the new_node() family of functions.
typedef base_node* (*new_node_hook)(void);


/// Creates a new leaf node of a given type.
///
/// \tparam NodeType The type of the leaf node to create.
///
/// \return A pointer to the newly-created node.
template< class NodeType >
base_node*
new_node(void)
{
    return new NodeType();
}


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

    const base_node* lookup_ro(const tree_key&,
                               const tree_key::size_type) const;
    leaf_node* lookup_rw(const tree_key&, const tree_key::size_type,
                         new_node_hook);

    void all_properties(properties_map&, const tree_key&) const;
};


/// Static internal node of the tree.
///
/// The direct children of this node must be pre-defined by calls to define().
/// Attempts to traverse this node and resolve a key that is not a pre-defined
/// children will result in an "unknown key" error.
class static_inner_node : public config::detail::inner_node {
public:
    static_inner_node(void);

    void define(const tree_key&, const tree_key::size_type, new_node_hook);
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
        throw unknown_key_error(key);
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


/// Sets the value of the node from a raw string representation.
///
/// \param raw_value The value to set the node to.
///
/// \throw value_error If the value is invalid.
template< typename ValueType >
void
config::native_leaf_node< ValueType >::set_string(const std::string& raw_value)
{
    try {
        typed_leaf_node< ValueType >::set(text::to_type< ValueType >(
            raw_value));
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
config::native_leaf_node< ValueType >::to_string(void) const
{
    PRE(typed_leaf_node< ValueType >::is_set());
    return F("%s") % typed_leaf_node< ValueType >::value();
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
        _root->define(key, 0, detail::new_node< LeafType >);
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
    const detail::base_node* raw_node = _root->lookup_ro(key, 0);
    try {
        const LeafType& child = dynamic_cast< const LeafType& >(*raw_node);
        if (child.is_set())
            return child.value();
        else
            throw unknown_key_error(key);
    } catch (const std::bad_cast& unused_error) {
        throw unknown_key_error(key);
    }
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
    leaf_node* raw_node = _root->lookup_rw(key, 0,
                                           detail::new_node< LeafType >);
    try {
        LeafType& child = dynamic_cast< LeafType& >(*raw_node);
        child.set(value);
    } catch (const std::bad_cast& unused_error) {
        throw value_error(F("Invalid value for key '%s'") %
                          detail::flatten_key(key));
    }
}


}  // namespace utils

#endif  // !defined(UTILS_CONFIG_TREE_IPP)
