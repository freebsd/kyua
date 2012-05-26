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

/// \file utils/config/tree.hpp
/// Data type to represent a tree of arbitrary values with string keys.

#if !defined(UTILS_CONFIG_TREE_HPP)
#define UTILS_CONFIG_TREE_HPP

#include <map>
#include <string>
#include <vector>

#include "utils/noncopyable.hpp"
#include "utils/optional.hpp"

namespace utils {
namespace config {


/// Flat representation of all properties as strings.
typedef std::map< std::string, std::string > properties_map;


namespace detail {


/// Representation of a valid, tokenized key.
typedef std::vector< std::string > tree_key;


/// Base representation of a node.
///
/// This abstract class provides the base type for every node in the tree.  Due
/// to the dynamic nature of our trees (each leaf being able to hold arbitrary
/// data types), this base type is a necessity.
class base_node : noncopyable {
public:
    virtual ~base_node(void) = 0;

    /// Extracts a textual representation of the node as key/value string pairs.
    ///
    /// \param [out] properties The accumulator for the generated properties.
    ///     The contents of the map are only extended.
    /// \param key The path to the current node.
    virtual void all_properties(properties_map& properties,
                                const tree_key& key) const = 0;
};


class static_inner_node;


}  // namespace detail


/// Abstract leaf node without any specified type.
///
/// This base abstract type is necessary to have a common pointer type to which
/// to cast any leaf.  We later provide templated derivates of this class, and
/// those cannot act in this manner.
///
/// It is important to understand that a leaf can exist without actually holding
/// a value.  Our trees are "strictly keyed": keys must have been pre-defined
/// before a value can be set on them.  This is to ensure that the end user is
/// using valid key names and not making mistakes due to typos, for example.  To
/// represent this condition, we define an "empty" key in the tree to denote
/// that the key is valid, yet it has not been set by the user.  Only when an
/// explicit set is performed on the key, it gets a value.
class leaf_node : public detail::base_node {
public:
    virtual ~leaf_node(void);

    /// Checks whether the node has been set by the user.
    ///
    /// Nodes of the tree are predefined by the caller to specify the valid
    /// types of the leaves.  Such predefinition results in the creation of
    /// nodes within the tree, but these nodes have not yet been set.
    /// Traversing these nodes is invalid and should result in an "unknown key"
    /// error.
    ///
    /// \return True if a value has been set in the node.
    virtual bool is_set(void) const = 0;

    /// Sets the value of the node from a raw string representation.
    ///
    /// \param raw_value The value to set the node to.
    ///
    /// \throw value_error If the value is invalid.
    virtual void set_string(const std::string& raw_value) = 0;

    /// Converts the contents of the node to a string.
    ///
    /// \pre The node must have a value.
    ///
    /// \return A string representation of the value held by the node.
    virtual std::string to_string(void) const = 0;
};


/// Base leaf node for a single arbitrary type.
///
/// This templated leaf node holds a single object of any type.  The conversion
/// to/from string representations is undefined, as that depends on the
/// particular type being processed.  You should reimplement this class for any
/// type that needs additional processing/validation during conversion.
template< typename ValueType >
class typed_leaf_node : public leaf_node {
public:
    /// The type of the value held by this node.
    typedef ValueType value_type;

    typed_leaf_node(void);

    void all_properties(properties_map&, const detail::tree_key&) const;
    bool is_set(void) const;

    const value_type& value(void) const;
    void set(const value_type&);

private:
    /// The value held by this node.
    optional< value_type > _value;
};


/// Leaf node holding a native type.
///
/// This templated leaf node holds a native type.  The conversion to/from string
/// representations of the value happens by means of iostreams.
template< typename ValueType >
class native_leaf_node : public typed_leaf_node< ValueType > {
public:
    void set_string(const std::string&);
    std::string to_string(void) const;
};


/// Shorthand for a boolean node.
typedef native_leaf_node< bool > bool_node;


/// Shorthand for an integral node.
typedef native_leaf_node< int > int_node;


/// Shorthand for a string node.
typedef native_leaf_node< std::string > string_node;


/// Representation of a tree.
///
/// The string keys of the tree are in dotted notation and actually represent
/// path traversals through the nodes.
///
/// Our trees are "strictly-keyed": keys must be defined as "existent" before
/// their values can be set.  Defining a key is a separate action from setting
/// its value.  The rationale is that we want to be able to control what keys
/// get defined: because trees are used to hold configuration, we want to catch
/// typos as early as possible.  Also, users cannot set keys unless the types
/// are known in advance because our leaf nodes are strictly typed.
///
/// However, there is an exception to the strict keys: the inner nodes of the
/// tree can be static or dynamic.  Static inner nodes have a known subset of
/// children and attempting to set keys not previously defined will result in an
/// error.  Dynamic inner nodes do not have a predefined set of keys and can be
/// used to accept arbitrary user input.
///
/// For simplicity reasons, we force the root of the tree to be a static inner
/// node.  In other words, the root can never contain a value by itself and this
/// is not a problem because the root is not addressable by the key space.
/// Additionally, the root is strict so all of its direct children must be
/// explicitly defined.
///
/// This is, effectively, a simple wrapper around the node representing the
/// root.  Having a separate class aids in clearly representing the concept of a
/// tree and all of its public methods.  Also, the tree accepts dotted notations
/// for the keys while the internal structures do not.
class tree : noncopyable {
    /// The root of the tree.
    detail::static_inner_node* _root;

public:
    tree(void);
    ~tree(void);

    template< class LeafType >
    void define(const std::string&);

    void define_dynamic(const std::string&);

    bool is_set(const std::string&) const;

    template< class LeafType >
    const typename LeafType::value_type& lookup(const std::string&) const;

    template< class LeafType >
    void set(const std::string&, const typename LeafType::value_type&);

    std::string lookup_string(const std::string&) const;
    void set_string(const std::string&, const std::string&);

    properties_map all_properties(const std::string& = "") const;
};


}  // namespace config
}  // namespace utils

#endif  // !defined(UTILS_CONFIG_TREE_HPP)
