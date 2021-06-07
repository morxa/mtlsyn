/***************************************************************************
 *  preorder_traversal.h - Preorder tree traversal
 *
 *  Created:   Fri Mar 12 20:15:57 2021 +0100
 *  Copyright  2021  Stefan Schupp <stefan.schupp@cs.rwth-aachen.de>
 ****************************************************************************/
/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.md file.
 */

#pragma once

#include <spdlog/spdlog.h>

#include <stdexcept>

namespace search {

/// Exception thrown if the tree is inconsistent
class InconsistentTreeException : public std::logic_error
{
public:
	/** Constructor.
	 * @param what The error message
	 */
	InconsistentTreeException(const std::string &what) : std::logic_error(what)
	{
	}
};

/// forward declaration of iterator class
template <class Node>
class preorder_iterator;
/// forward declaration of end iterator
template <typename Node>
preorder_iterator<Node> end(Node *root);
/// forward declaration of equality comparison operator
template <typename Node>
bool operator==(const preorder_iterator<Node> &lhs, const preorder_iterator<Node> &rhs);

/**
 * @brief Preorder iterator class.
 * @details Assumes that the tree is built via unique_ptr to child nodes and raw_ptr to parent node.
 * @tparam Node Node type
 */
template <class Node>
class preorder_iterator : public std::iterator<std::forward_iterator_tag, Node>
{
	friend preorder_iterator<Node> end<Node>(Node *root);
	friend bool                    operator==<Node>(const preorder_iterator<Node> &lhs,
                               const preorder_iterator<Node> &rhs);

public:
	/// Default constructor
	preorder_iterator() = default;
	/**
	 * @brief Construct a new preorder iterator object from a root node
	 * @param root
	 */
	preorder_iterator(Node *root) : root_(root), cur_(root)
	{
	}
	/// Destructor
	virtual ~preorder_iterator()
	{
	}
	/**
	 * @brief Postfix increment
	 * @return preorder_iterator<Node>
	 */
	preorder_iterator<Node>
	operator++(int)
	{
		auto tmp = cur_;
		increment();
		return tmp;
	}
	/**
	 * @brief Prefix increment
	 * @return preorder_iterator<Node>
	 */
	preorder_iterator<Node> &
	operator++()
	{
		increment();
		return *this;
	}
	/**
	 * @brief Dereference operator
	 * @return Node&
	 */
	Node &
	operator*()
	{
		return *cur_;
	}
	/**
	 * @brief Dereference operator
	 * @return Node*
	 */
	Node *
	operator->()
	{
		return cur_;
	}

private:
	/**
	 * @brief Implements forward preorder iteration. The end is reached when the root node is reached
	 * again and marked by setting cur_ to nullptr.
	 */
	void
	increment()
	{
		// do nothing if end has been reached (indicated by cur_ == nullptr)
		if (cur_ == nullptr) {
			return;
		}
		// descend through children
		if (!cur_->children.empty()) {
			cur_ = cur_->children[0].get();
			return;
		} else {
			// corner case: if only one node, cur_->parent == nullptr
			if (cur_->parents.empty()) {
				if (cur_ != root_) {
					throw InconsistentTreeException(
					  "Parent-child relation between current and parent node is not bidirectional");
				}
				cur_ = nullptr;
				return;
			}
			// if this is the last child, ascend until descending to a sibling is possible or the root is
			// reached
			assert(!cur_->parent.empty());
			if (cur_ == cur_->parent->children.back().get()) {
				// ascend as long as the node is the last in the parent's children list or until the root
				// has been reached
				while (cur_ != root_ && cur_ == cur_->parent->children.back().get()) {
					cur_ = cur_->parent;
					if (cur_->parent == nullptr && cur_ != root_) {
						throw InconsistentTreeException(
						  "Parent-child relation between current and parent node is not bidirectional");
					}
				}
				// reached root - end reached, terminate iteration
				if (cur_ == root_) {
					cur_ = nullptr;
					return;
				}
			}
			// descend into a sibling (can happen either after ascending or directly, if ascending
			// is not necessary). To do so, identify position of the current node in the parent's
			// children-vector and descend into the according next child.
			for (auto cPtrIt = cur_->parent->children.begin(); cPtrIt != cur_->parent->children.end();
			     ++cPtrIt) {
				// found position of child in the parent'S children-vector, increment to descend into next
				// sibling
				if (cPtrIt->get() == cur_) {
					cur_ = std::next(cPtrIt)->get();
					return;
				}
			}
			throw InconsistentTreeException(
			  "Parent-child relation between current and parent node is not bidirectional");
		}
	}

	Node *root_ = nullptr; ///< stored root node to determine end
	Node *cur_  = nullptr; ///< stored current node
};
/**
 * @brief Comparison for equality of lhs and rhs, uses underlying node comparator.
 * @param lhs Left-hand side iterator
 * @param rhs Right-hand side iterator
 * @return true If both nodes pointed to are equal
 * @return false Otherwise
 */
template <typename Node>
bool
operator==(const preorder_iterator<Node> &lhs, const preorder_iterator<Node> &rhs)
{
	return lhs.cur_ == rhs.cur_;
}
/**
 * @brief Comparison for inequality of lhs and rhs, uses underlying node comparator.
 * @param lhs Left-hand side iterator
 * @param rhs Right-hand side iterator
 * @return true If both nodes pointed to are not equal
 * @return false Otherwise
 */
template <typename Node>
bool
operator!=(const preorder_iterator<Node> &lhs, const preorder_iterator<Node> &rhs)
{
	return !(lhs == rhs);
}
/**
 * @brief Create begin-iterator from node for preorder traversal.
 * @tparam Node
 * @param root
 * @return preorder_iterator<Node>
 */
template <typename Node>
preorder_iterator<Node>
begin(Node *root)
{
	return preorder_iterator<Node>{root};
}
/**
 * @brief Create end-iterator from node for preorder traversal.
 * @tparam Node
 * @param root
 * @return preorder_iterator<Node>
 */
template <typename Node>
preorder_iterator<Node>
end(Node *root)
{
	preorder_iterator<Node> it{root};
	it.cur_ = nullptr;
	return it;
}

} // namespace search
