#pragma once

#include <memory>

#include <cassert>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/range/iterator_range.hpp>

template<typename Key, typename Value, typename KeyBitStringTraits>
class radix_tree
{
public:
	typedef Key key_t;
	typedef Value value_t;

	template<bool IsConst>
	class base_iterator;

	class node {
	private:
		friend class radix_tree;

		template<bool IsConst>
		friend class base_iterator;

		key_t m_key{};
		std::unique_ptr<value_t> m_value;
		std::unique_ptr<node> m_left;
		std::unique_ptr<node> m_right;
		node* m_parent{nullptr};

		explicit node(key_t const& key, node* parent)
		: m_key(key), m_parent(parent) {
		}
		node(node const& other) = delete;
		node(node const& other, node* parent)
		: m_key(other.m_key)
		, m_value(other.m_value ? new Value(*other.m_value) : nullptr)
		, m_left(other.m_left ? new node(*other.m_left, this) : nullptr)
		, m_right(other.m_right ? new node(*other.m_right, this) : nullptr)
		, m_parent(parent) {
		}

	public:
		key_t const& key() const { return m_key; }

		// user should only ever see nodes with value
		value_t const& value() const { return *m_value; }
		value_t& value() { return *m_value; }
	};

	template<bool IsConst>
	class base_iterator : public boost::iterator_facade<base_iterator<IsConst>, typename std::conditional<IsConst, node const, node>::type, boost::forward_traversal_tag> {
	public:
		base_iterator() = default;

		base_iterator(base_iterator const& other) = default;

		// always allow copying from mutable constructor
		template<bool IsConstArg, typename std::enable_if<!IsConstArg>::type* = nullptr>
		base_iterator(base_iterator<IsConstArg> const& other)
		: m_node(other.m_node), m_root(other.m_root) {
		}

		boost::iterator_range<base_iterator> subtree() const {
			return boost::make_iterator_range(
				base_iterator(m_node, m_node, no_init_walk{}),
				base_iterator(nullptr, m_node, no_init_walk{}));
		}

		explicit operator bool() const {
			return bool(m_node);
		}

	private:
		friend class radix_tree;
		friend class boost::iterator_core_access;
		typedef typename std::conditional<IsConst, node const, node>::type Node;

		node* m_node{nullptr};
		node* m_root{nullptr}; // can limit to subtree
		explicit base_iterator(node* pos, node* root)
		: m_node(pos), m_root(root) {
			// find first node with value
			if (m_node && !m_node->m_value) increment();
		}

		struct no_init_walk{};
		explicit base_iterator(node* pos, node* root, no_init_walk)
		: m_node(pos), m_root(root) {
		}

		void increment() {
			for (;;) {
				if (m_node->m_left) {
					m_node = m_node->m_left.get();
				} else if (m_node->m_right) {
					m_node = m_node->m_right.get();
				} else if (m_root == m_node) {
					m_node = nullptr;
					return; // reached end of tree
				} else {
					// reached bottom. go up again
					for (;;) {
						Node* prev = m_node;
						if (m_root == m_node) {
							m_node = nullptr;
							return; // reached end of tree
						}
						m_node = m_node->m_parent;
						// previous node wasn't root, so there must have been a parent
						assert(m_node);
						// when we walk up and came through the left link, and the right link has a node,
						// walk down the right link
						if (m_node->m_left.get() == prev && m_node->m_right) {
							m_node = m_node->m_right.get();
							break;
						}
						// otherwise keep walking up
					}
				}
				// found a node with value, return it
				if (m_node->m_value) return;
			}
		}

		// TODO: bidirectional_traversal_tag
		// void decrement() {
		// }

		template<bool IsConstArg>
		bool equal(base_iterator<IsConstArg> const& other) const
		{
			return m_node == other.m_node;
		}

		Node& dereference() const { return *m_node; }
	};

	typedef base_iterator<false> iterator;
	typedef base_iterator<true> const_iterator;

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

	std::unique_ptr<node> m_root;

	// find node which satisfies:
	// - node key is prefixed by searched key
	// - has the shortest key possible
	// NOTE: doesn't necessarily have a value, don't return directly in iterator!
	node* intern_lookup_parent(key_t const& key) const {
		node* current = m_root.get();
		bitstring const key_bs = key_to_bs(key);

		for (;;) {
			if (!current) return nullptr;
			bitstring const parent_key_bs = key_to_bs(current->m_key);
			if (is_prefix(parent_key_bs, key_bs)) {
				if (parent_key_bs == key_bs) {
					// found an exact match
					return current;
				}
				assert(key_bs.length() > parent_key_bs.length());
				if (key_bs[parent_key_bs.length()]) {
					current = current->m_right.get();
				} else {
					current = current->m_left.get();
				}
			} else if (is_prefix(key_bs, parent_key_bs)) {
				// first node which has a key prefixed by key_bs
				return current;
			} else {
				return nullptr;
			}
		}
	}

	// find node which satisfies:
	// - node key is a prefix of searched key
	// - has a value
	// - has the longest key possible
	node* intern_lookup(key_t const& key) const {
		node* last_value_node = nullptr;
		node* current = m_root.get();
		bitstring const key_bs = key_to_bs(key);

		for (;;) {
			if (!current) return last_value_node;
			bitstring const parent_key_bs = key_to_bs(current->m_key);
			if (is_prefix(key_to_bs(current->m_key), key_bs)) {
				if (current->m_value) last_value_node = current;
				if (parent_key_bs == key_bs) {
					// found an exact match
					return last_value_node;
				}
				assert(key_bs.length() > parent_key_bs.length());
				if (key_bs[parent_key_bs.length()]) {
					current = current->m_right.get();
				} else {
					current = current->m_left.get();
				}
			} else {
				return last_value_node;
			}
		}
	}

	// find node which satisfies:
	// - has a key equal to searched key
	// - has a value
	node* intern_exact_lookup(key_t const& key) const {
		node* current = m_root.get();
		bitstring const key_bs = key_to_bs(key);

		for (;;) {
			if (!current) return nullptr;
			bitstring const parent_key_bs = key_to_bs(current->m_key);
			if (is_prefix(key_to_bs(current->m_key), key_bs)) {
				if (parent_key_bs == key_bs) {
					// found an exact match; check whether it has a value
					return current->m_value ? current : nullptr;
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
		node* parent{nullptr};
		std::unique_ptr<node>* insert_pos = &m_root;
		bitstring const key_bs = key_to_bs(key);

		for (;;) {
			if (!*insert_pos) {
				insert_pos->reset(new node(key, parent));
				return insert_pos->get();
			}
			bitstring const insert_pos_key_bs = key_to_bs((*insert_pos)->m_key);
			if (is_prefix(insert_pos_key_bs, key_bs)) {
				if (insert_pos_key_bs == key_bs) {
					// found an exact match
					return insert_pos->get();
				}
				assert(key_bs.length() > insert_pos_key_bs.length());
				parent = insert_pos->get();
				if (key_bs[insert_pos_key_bs.length()]) {
					insert_pos = &(*insert_pos)->m_right;
				} else {
					insert_pos = &(*insert_pos)->m_left;
				}
			} else {
				// need to split insert_pos
				bitstring common_prefix_bs = longest_common_prefix(insert_pos_key_bs, key_bs);
				assert(is_prefix(common_prefix_bs, insert_pos_key_bs));
				assert(is_prefix(common_prefix_bs, key_bs));
				assert(common_prefix_bs.length() < insert_pos_key_bs.length());
				if (common_prefix_bs.length() == key_bs.length()) {
					// key_bs is a prefix of insert_pos_key_bs, insert between
					std::unique_ptr<node> new_node{new node(key, parent)};
					parent = new_node.get();
					if (insert_pos_key_bs[common_prefix_bs.length()]) {
						new_node->m_right = std::move(*insert_pos);
						new_node->m_right->m_parent = parent;
					} else {
						new_node->m_left = std::move(*insert_pos);
						new_node->m_left->m_parent = parent;
					}
					*insert_pos = std::move(new_node);
					return insert_pos->get();
				} else {
					assert(common_prefix_bs.length() < key_bs.length());
					// need a new node which forks to insert_pos_key_bs and key_bs; need to copy common_prefix into key
					if (insert_pos_key_bs[common_prefix_bs.length()]) {
						assert(!key_bs[common_prefix_bs.length()]);
						std::unique_ptr<node> new_node{new node(bs_to_key(common_prefix_bs), parent)};
						parent = new_node.get();
						new_node->m_right = std::move(*insert_pos);
						new_node->m_right->m_parent = parent;
						new_node->m_left.reset(new node(key, parent));
						*insert_pos = std::move(new_node);
						return (*insert_pos)->m_left.get();
					} else {
						assert(key_bs[common_prefix_bs.length()]);
						std::unique_ptr<node> new_node{new node(bs_to_key(common_prefix_bs), parent)};
						parent = new_node.get();
						new_node->m_left = std::move(*insert_pos);
						new_node->m_left->m_parent = parent;
						new_node->m_right.reset(new node(key, parent));
						*insert_pos = std::move(new_node);
						return (*insert_pos)->m_right.get();
					}
				}
			}
		}
	}

	void merge(node* pos) {
		// when copying a node pointer up make sure to create an intermediate pointer
		// to keep the sub-object alive, as the assignment might kill it otherwise
		if (!pos->m_value) {
			std::unique_ptr<node> merge_up;
			if (!pos->m_right) {
				// delete "pos", replace with "pos->m_left":
				std::unique_ptr<node> merge_up = std::move(pos->m_left);
			} else if (!pos->m_left) {
				// delete "pos", replace with "pos->m_left":
				std::unique_ptr<node> merge_up = std::move(pos->m_left);
			} else {
				// both forks still in use, not merging
				return;
			}

			node* parent = pos->m_parent;
			if (merge_up) merge_up->m_parent = parent;

			if (!parent) {
				assert(pos == m_root.get());
				m_root = std::move(merge_up);
			} else {
				if (parent->m_left.get() == pos) {
					parent->m_left = std::move(merge_up);
				} else {
					assert(parent->m_right.get() == pos);
					parent->m_right = std::move(merge_up);
				}
				merge(parent);
			}
		}
	}

	void intern_remove(node* pos) {
		if (pos->m_value) --m_size;
		pos->m_value.reset();
		merge(pos);
	}

	size_t intern_remove(key_t const& key) {
		node* remove_pos = intern_exact_lookup(key);
		if (!remove_pos) return 0;
		intern_remove(remove_pos);
		return 1;
	}

	boost::iterator_range<const_iterator> subtree(node* n) const {
		return boost::make_iterator_range(const_iterator(n, n), const_iterator(nullptr, n));
	}

	boost::iterator_range<iterator> subtree(node* n) {
		return boost::make_iterator_range(iterator(n, n), iterator(nullptr, n));
	}

	// implement copy + move semantics for size_t: move resets source size
	struct size_wrapper_type {
		size_t m_value{0};

		size_wrapper_type() = default;
		size_wrapper_type(size_wrapper_type const& other) = default;
		size_wrapper_type(size_wrapper_type&& other)
		: m_value(other.m_value) {
			other.m_value = 0;
		}

		size_wrapper_type& operator=(size_wrapper_type const& other) = default;
		size_wrapper_type& operator=(size_wrapper_type&& other) {
			m_value = other.m_value;
			other.m_value = 0;
			return *this;
		}

		size_wrapper_type& operator++() {
			++m_value;
			return *this;
		}

		size_wrapper_type& operator--() {
			--m_value;
			return *this;
		}
	};

	size_wrapper_type m_size;

public:
	radix_tree() = default;
	radix_tree(radix_tree const& other)
	: m_root(other.m_root ? new node(*other.m_root, nullptr) : nullptr) {
	}
	radix_tree(radix_tree&& other) = default;
	radix_tree& operator=(radix_tree const& other) {
		if (this != &other) m_root.reset(other.m_root ? new node(*other.m_root, nullptr) : nullptr);
		return *this;
	}
	radix_tree& operator=(radix_tree&& other) = default;

	template<typename ValueArg>
	std::pair<iterator, bool> insert(key_t const& key, ValueArg&& value) {
		node* n = intern_insert(key);
		if (n->m_value) return std::make_pair(iterator(n, m_root.get()), false);
		n->m_value.reset(new value_t(std::forward<ValueArg>(value)));
		++m_size;
		return std::make_pair(iterator(n, m_root.get()), true);
	}

	template<typename ValueArg>
	std::pair<iterator, bool> insert_or_assign(key_t const& key, ValueArg&& value) {
		node* n = intern_insert(key);
		if (n->m_value) {
			*n->m_value = std::forward<ValueArg>(value);
			return std::make_pair(iterator(n, m_root.get()), false);
		} else {
			n->m_value.reset(new value_t(std::forward<ValueArg>(value)));
			++m_size;
			return std::make_pair(iterator(n, m_root.get()), true);
		}
	}

	const_iterator find(key_t const& key) const {
		return const_iterator(intern_lookup(key), m_root.get());
	}

	iterator find(key_t const& key) {
		return iterator(intern_lookup(key), m_root.get());
	}

	const_iterator find_exact(key_t const& key) const {
		return const_iterator(intern_exact_lookup(key), m_root.get());
	}

	iterator find_exact(key_t const& key) {
		return iterator(intern_exact_lookup(key), m_root.get());
	}

	boost::iterator_range<const_iterator> find_all(key_t const& key) const {
		return subtree(intern_lookup_parent(key));
	}

	boost::iterator_range<iterator> find_all(key_t const& key) {
		return subtree(intern_lookup_parent(key));
	}

	const value_t* value(key_t const& key) const {
		node *n = intern_lookup(key);
		return n ? n->m_value.get() : nullptr;
	}

	value_t* value(key_t const& key) {
		node *n = intern_lookup(key);
		return n ? n->m_value.get() : nullptr;
	}

	const value_t* value_exact(key_t const& key) const {
		node *n = intern_exact_lookup(key);
		return n ? n->m_value.get() : nullptr;
	}

	value_t* value_exact(key_t const& key) {
		node *n = intern_exact_lookup(key);
		return n ? n->m_value.get() : nullptr;
	}

	size_t erase(key_t const& key) {
		return intern_remove(key);
	}

	iterator erase(const_iterator const& pos) {
		// copy iterator to remove "const":
		iterator next(pos.m_node, pos.m_root, typename iterator::no_init_walk{});

		// remember next child in iteration order
		node* next_child = pos.m_node->m_left ? pos.m_node->m_left.get() : pos.m_node->m_right.get();
		++next;
		// "next.m_node" doesn't have to be next_child if next_child didn't have a value!

		intern_remove(pos.m_node);
		if (pos.m_node == pos.m_root) {
			// possibly just killed the "root" in the iterator, fix it.
			if (next_child && next_child->m_parent != pos.m_node) {
				// the parent changed, the old node was merged, i.e. had only one child, which is "next_child"
				next.m_root = next_child;
			} else {
				// there was no child, so the node is definitively dead,
				// and there is no remaining subtree
				next = iterator();
			}
		}
		return next;
	}

	void clear() {
		*this = radix_tree();
	}

	bool empty() const {
		return 0 == m_size.m_value;
	}

	size_t size() const {
		return m_size.m_value;
	}

	iterator begin() { return iterator(m_root.get(), m_root.get()); }
	iterator end() { return iterator(nullptr, m_root.get()); }
	const_iterator begin() const { return const_iterator(m_root.get(), m_root.get()); }
	const_iterator end() const { return const_iterator(nullptr, m_root.get()); }
	const_iterator cbegin() const { return const_iterator(m_root.get(), m_root.get()); }
	const_iterator cend() const { return const_iterator(nullptr, m_root.get()); }

	/** swap content of two trees */
	friend void swap(radix_tree& a, radix_tree& b) {
		using std::swap;
		swap(a.m_size, b.m_size);
		swap(a.m_root, b.m_root);
	}
};
