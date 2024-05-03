/*
 * Acknowledgement : Yuxuan Wang for Modifying the prototype of TrieStore class
 */

#ifndef SJTU_TRIE_HPP
#define SJTU_TRIE_HPP

#include <algorithm>
#include <cstddef>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sjtu {

// A TrieNode is a node in a Trie.
class TrieNode {
public:
    // Create a TrieNode with no children.
    TrieNode() = default;

    // Create a TrieNode with some children.
    explicit TrieNode(std::map<char, std::shared_ptr<const TrieNode>> children)
        : children_(std::move(children))
    {
    }

    virtual ~TrieNode() = default;

    // Clone returns a copy of this TrieNode. If the TrieNode has a value, the value is copied. The return
    // type of this function is a unique_ptr to a TrieNode.
    //
    // You cannot use the copy constructor to clone the node because it doesn't know whether a `TrieNode`
    // contains a value or not.
    //
    // Note: if you want to convert `unique_ptr` into `shared_ptr`, you can use `std::shared_ptr<T>(std::move(ptr))`.
    virtual auto Clone() const -> std::unique_ptr<TrieNode> { return std::make_unique<TrieNode>(children_); }

    // A map of children, where the key is the next character in the key, and the value is the next TrieNode.
    std::map<char, std::shared_ptr<const TrieNode>> children_;

    // Indicates if the node is the terminal node.
    bool is_value_node_ { false };

    // You can add additional fields and methods here. But in general, you don't need to add extra fields to
    // complete this project.
};

// A TrieNodeWithValue is a TrieNode that also has a value of type T associated with it.
template <class T>
class TrieNodeWithValue : public TrieNode {
public:
    // Create a trie node with no children and a value.
    explicit TrieNodeWithValue(std::shared_ptr<T> value)
        : value_(std::move(value))
    {
        this->is_value_node_ = true;
    }

    // Create a trie node with children and a value.
    TrieNodeWithValue(std::map<char, std::shared_ptr<const TrieNode>> children, std::shared_ptr<T> value)
        : TrieNode(std::move(children))
        , value_(std::move(value))
    {
        this->is_value_node_ = true;
    }

    // Override the Clone method to also clone the value.
    //
    // Note: if you want to convert `unique_ptr` into `shared_ptr`, you can use `std::shared_ptr<T>(std::move(ptr))`.
    auto Clone() const -> std::unique_ptr<TrieNode> override
    {
        return std::make_unique<TrieNodeWithValue<T>>(children_, value_);
    }

    // The value associated with this trie node.
    std::shared_ptr<T> value_;
};

// A Trie is a data structure that maps strings to values of type T. All operations on a Trie should not
// modify the trie itself. It should reuse the existing nodes as much as possible, and create new nodes to
// represent the new trie.
class Trie {
private:
    // The root of the trie.
    std::shared_ptr<const TrieNode> root_ { nullptr };

    // Create a new trie with the given root.
    explicit Trie(std::shared_ptr<const TrieNode> root)
        : root_(std::move(root))
    {
    }

public:
    // Create an empty trie.
    Trie() = default;

    // Get the value associated with the given key.
    // 1. If the key is not in the trie, return nullptr.
    // 2. If the key is in the trie but the type is mismatched, return nullptr.
    // 3. Otherwise, return the value.
    template <class T>
    auto Get(std::string_view key) const -> const T*
    {
        auto node = std::const_pointer_cast<TrieNode>(root_);
        if (node == nullptr)
            return nullptr;
        for (char c : key) {
            auto& children = node->children_;
            auto it = node->children_.find(c);
            if (it == node->children_.end())
                return nullptr;
            node = std::const_pointer_cast<TrieNode>(children[c]);
        }
        if (node->is_value_node_) {
            auto value_node = std::dynamic_pointer_cast<TrieNodeWithValue<T>>(node);
            return value_node->value_.get();
        }
        return nullptr;
    }

    // Put a new key-value pair into the trie. If the key already exists, overwrite the value.
    // Returns the new trie.
    template <class T>
    auto Put(std::string_view key, T value) const -> Trie
    {
        auto new_root = std::shared_ptr<TrieNode>(std::move(root_ ? root_->Clone() : std::make_unique<TrieNode>(TrieNode())));
        auto node = new_root;
        for (size_t i = 0; i < key.size(); i++) {
            char c = key[i];
            auto& children = node->children_;
            auto it = children.find(c);
            if (it == children.end()) {
                if (i == key.size() - 1)
                    children[c] = std::make_shared<TrieNodeWithValue<T>>(TrieNodeWithValue<T>(std::make_shared<T>(std::move(value))));
                else
                    children[c] = std::make_shared<TrieNode>(TrieNode());
            } else {
                if (i == key.size() - 1)
                    children[c] = std::make_shared<TrieNodeWithValue<T>>(TrieNodeWithValue<T>(std::move(children[c]->children_), std::make_shared<T>(std::move(value))));
                else
                    children[c] = std::shared_ptr<TrieNode>(std::move(children[c]->Clone()));
            }
            node = std::const_pointer_cast<TrieNode>(children[c]);
        }
        return Trie(std::move(new_root));
    }

    // Remove the key from the trie. If the key does not exist, return the original trie.
    // Otherwise, returns the new trie.
    auto Remove(std::shared_ptr<TrieNode> node, size_t pos, std::string_view key) const -> std::shared_ptr<TrieNode>
    {
        if (pos == key.size()) {
            if (!node->is_value_node_)
                return node;
            if (pos != 0 && node->children_.empty())
                return nullptr;
            auto new_node = std::make_shared<TrieNode>(TrieNode(std::move(node->children_)));
            return new_node;
        }
        char c = key[pos];
        auto& children = node->children_;
        auto it = children.find(c);
        if (it == children.end())
            return node;
        auto new_child = Remove(std::shared_ptr<TrieNode>(std::move(children[c]->Clone())), pos + 1, key);
        if (new_child == nullptr)
            children.erase(it);
        else
            children[c] = new_child;
        if (pos != 0 && children.empty() && !node->is_value_node_)
            return nullptr;
        return node;
    }
    auto Remove(std::string_view key) const -> Trie
    {
        auto new_root = std::shared_ptr<TrieNode>(std::move(root_ ? root_->Clone() : std::make_unique<TrieNode>(TrieNode())));
        auto node = new_root;
        auto new_node = Remove(node, 0, key);
        return Trie(std::move(new_node));
    }

    bool operator==(const Trie& temp) const
    {
        return root_ == temp.root_;
    }
};

// This class is used to guard the value returned by the trie. It holds a reference to the root so
// that the reference to the value will not be invalidated.
template <class T>
class ValueGuard {
public:
    ValueGuard(Trie root, const T& value)
        : root_(std::move(root))
        , value_(value)
    {
    }
    auto operator*() const -> const T& { return value_; }

private:
    Trie root_;
    const T& value_;
};

// This class is a thread-safe wrapper around the Trie class. It provides a simple interface for
// accessing the trie. It should allow concurrent reads and a single write operation at the same
// time.
class TrieStore {
public:
    // This function returns a ValueGuard object that holds a reference to the value in the trie of the given version (default: newest version). If
    // the key does not exist in the trie, it will return std::nullopt.
    template <class T>
    auto Get(std::string_view key, size_t version = -1) -> std::optional<ValueGuard<T>>
    {
        if (version == -1)
            version = get_version();
        std::shared_lock lock1(snapshots_lock_);
        if (version >= snapshots_.size())
            return std::nullopt;
        auto& trie = snapshots_[version];
        auto value = trie.Get<T>(key);
        if (value == nullptr)
            return std::nullopt;
        return ValueGuard<T>(trie, *value);
    }

    // This function will insert the key-value pair into the trie. If the key already exists in the
    // trie, it will overwrite the value
    // return the version number after operation
    // Hint: new version should only be visible after the operation is committed(completed)
    template <class T>
    size_t Put(std::string_view key, T value)
    {
        std::unique_lock lock1(write_lock_);
        auto new_trie = snapshots_.back().Put(key, std::move(value));
        std::unique_lock lock2(snapshots_lock_);
        snapshots_.push_back(new_trie);
        return get_version();
    }

    // This function will remove the key-value pair from the trie.
    // return the version number after operation
    // if the key does not exist, version number should not be increased
    size_t Remove(std::string_view key)
    {
        std::unique_lock lock1(write_lock_);
        auto new_version = get_version() + 1;
        auto new_trie = snapshots_.back().Remove(key);
        if (new_trie == snapshots_.back())
            return get_version();
        std::unique_lock lock2(snapshots_lock_);
        snapshots_.push_back(new_trie);
        return get_version();
    }

    // This function return the newest version number
    size_t get_version()
    {
        return snapshots_.size() - 1;
    }

private:
    // This mutex sequences all writes operations and allows only one write operation at a time.
    // Concurrent modifications should have the effect of applying them in some sequential order
    std::shared_mutex write_lock_;
    std::shared_mutex snapshots_lock_;

    // Stores all historical versions of trie
    // version number ranges from [0, snapshots_.size())
    std::vector<Trie> snapshots_ { 1 };
};

} // namespace sjtu

#endif // SJTU_TRIE_HPP