#pragma once

#include <memory>

#include <cassert>

template<typename Key, typename Value, typename KeyBitStringTraits>
class radix_tree
{
public:
	typedef Key key_t;
	typedef Value value_t;

private:
	typedef typename KeyBitStringTraits::bitstring bitstring;
	static bitstring key_to_bs(key_t const& key) {
		KeyBitStringTraits keyBitStringTraits{};
		return keyBitStringTraits.value_to_bitstring(key);
	}
	static key_t bs_to_key(bitstring bs) {
		KeyBitStringTraits keyBitStringTraits{};
		return keyBitStringTraits.bitstring_to_value(bs);
	}

	struct node {
		key_t m_key{};
		std::unique_ptr<value_t> m_value;
		std::unique_ptr<node> m_left;
		std::unique_ptr<node> m_right;

		explicit node(key_t const& key)
		: m_key(key) {
		}
		node(node const& other)
		: m_key(other.m_key)
		, m_value(other.m_value ? new Value(*other.m_value) : nullptr)
		, m_left(other.m_left ? new node(*other.m_left) : nullptr)
		, m_right(other.m_right ? new node(*other.m_right) : nullptr) {
		}
	};

	std::unique_ptr<node> m_root;

	value_t* intern_lookup(key_t const& key) const {
		value_t* last_value = nullptr;
		node const* current = m_root.get();
		bitstring const key_bs = key_to_bs(key);

		for (;;) {
			if (!current) return last_value;
			bitstring const parent_key_bs = key_to_bs(current->m_key);
			if (is_prefix(key_to_bs(current->m_key), key_bs)) {
				if (current->m_value) last_value = current->m_value.get();
				if (parent_key_bs == key_bs) {
					// found an exact match
					return last_value;
				}
				assert(key_bs.length() > parent_key_bs.length());
				if (key_bs[parent_key_bs.length()]) {
					current = current->m_right.get();
				} else {
					current = current->m_left.get();
				}
			} else {
				return last_value;
			}
		}
	}

	value_t* intern_exact_lookup(key_t const& key) const {
		node const* current = m_root.get();
		bitstring const key_bs = key_to_bs(key);

		for (;;) {
			if (!current) return nullptr;
			bitstring const parent_key_bs = key_to_bs(current->m_key);
			if (is_prefix(key_to_bs(current->m_key), key_bs)) {
				if (parent_key_bs == key_bs) {
					// found an exact match
					return current->m_value.get();
				}
				assert(key_bs.length() > parent_key_bs.length());
				if (key_bs[parent_key_bs.length()]) {
					current = current->m_right.get();
				} else {
					current = current->m_left.get();
				}
			} else {
				return nullptr;
			}
		}
	}

	node* intern_insert(key_t const& key) {
		std::unique_ptr<node>* parent = &m_root;
		bitstring const key_bs = key_to_bs(key);

		for (;;) {
			if (!*parent) {
				parent->reset(new node(key));
				return parent->get();
			}
			bitstring const parent_key_bs = key_to_bs((*parent)->m_key);
			if (is_prefix(key_to_bs((*parent)->m_key), key_bs)) {
				if (parent_key_bs == key_bs) {
					// found an exact match
					return parent->get();
				}
				assert(key_bs.length() > parent_key_bs.length());
				if (key_bs[parent_key_bs.length()]) {
					parent = &(*parent)->m_right;
				} else {
					parent = &(*parent)->m_left;
				}
			} else {
				// need to split parent
				bitstring common_prefix_bs = longest_common_prefix(parent_key_bs, key_bs);
				assert(common_prefix_bs.length() < parent_key_bs.length());
				if (common_prefix_bs.length() == key_bs.length()) {
					// key_bs is a prefix of parent_key_bs, insert between
					std::unique_ptr<node> new_node{new node(key)};
					if (parent_key_bs[common_prefix_bs.length()]) {
						new_node->m_right = std::move(*parent);
					} else {
						new_node->m_left = std::move(*parent);
					}
					*parent = std::move(new_node);
					return parent->get();
				} else {
					assert(common_prefix_bs.length() < key_bs.length());
					// need a new node which forks to parent_key_bs and key_bs; need to copy common_prefix into key
					if (parent_key_bs[common_prefix_bs.length()]) {
						assert(!key_bs[common_prefix_bs.length()]);
						std::unique_ptr<node> new_node{new node(bs_to_key(common_prefix_bs))};
						new_node->m_right = std::move(*parent);
						new_node->m_left.reset(new node(key));
						*parent = std::move(new_node);
						return (*parent)->m_left.get();
					} else {
						assert(key_bs[common_prefix_bs.length()]);
						std::unique_ptr<node> new_node{new node(bs_to_key(common_prefix_bs))};
						new_node->m_left = std::move(*parent);
						new_node->m_right.reset(new node(key));
						*parent = std::move(new_node);
						return (*parent)->m_right.get();
					}
				}
			}
		}
	}

	size_t intern_remove(key_t const& key) {
		std::unique_ptr<node>* parent = &m_root;
		bitstring const key_bs = key_to_bs(key);

		for (;;) {
			if (!*parent) return 0;
			bitstring const parent_key_bs = key_to_bs((*parent)->m_key);
			if (is_prefix(key_to_bs((*parent)->m_key), key_bs)) {
				if (parent_key_bs == key_bs) {
					// found an exact match
					// first delete value
					(*parent)->m_value.reset();
					// now check if we can merge node:
					if (!(*parent)->m_right) {
						std::unique_ptr<node> tmp = std::move((*parent)->m_left);
						*parent = std::move(tmp);
					} else if (!(*parent)->m_left) {
						std::unique_ptr<node> tmp = std::move((*parent)->m_right);
						*parent = std::move(tmp);
					}
					return 1;
				}
				assert(key_bs.length() > parent_key_bs.length());
				if (key_bs[parent_key_bs.length()]) {
					parent = &(*parent)->m_right;
				} else {
					parent = &(*parent)->m_left;
				}
			} else {
				return 0;
			}
		}
	}

public:
	radix_tree() = default;
	radix_tree(radix_tree const& other)
	: m_root(other.m_root ? new node(*other.m_root) : nullptr) {
	}
	radix_tree(radix_tree&& other) = default;
	radix_tree& operator=(radix_tree const& other) {
		if (this != &other) m_root.reset(other.m_root ? new node(*other.m_root) : nullptr);
		return *this;
	}
	radix_tree& operator=(radix_tree&& other) = default;

	template<typename ValueArg>
	bool insert(key_t const& key, ValueArg&& value) {
		node* n = intern_insert(key);
		if (n->m_value) return false;
		n->m_value.reset(new value_t(std::forward<ValueArg>(value)));
	}

	template<typename ValueArg>
	bool insert_or_assign(key_t const& key, ValueArg&& value) {
		node* n = intern_insert(key);
		if (n->m_value) {
			*n->m_value = std::forward<ValueArg>(value);
			return false;
		} else {
			n->m_value.reset(new value_t(std::forward<ValueArg>(value)));
			return true;
		}
	}

	const value_t* value(key_t const& key) const {
		return intern_lookup(key);
	}

	value_t* value(key_t const& key) {
		return intern_lookup(key);
	}

	const value_t* value_exact(key_t const& key) const {
		return intern_exact_lookup(key);
	}

	value_t* value_exact(key_t const& key) {
		return intern_exact_lookup(key);
	}

	size_t erase(key_t const& key) {
		return intern_remove(key);
	}
};
