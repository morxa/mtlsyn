/***************************************************************************
 *  search_tree.h - Search tree data structure for the AB search tree
 *
 *  Created:   Mon  1 Feb 15:58:24 CET 2021
 *  Copyright  2021  Till Hofmann <hofmann@kbsg.rwth-aachen.de>
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

#include "automata/ta_regions.h"
#include "canonical_word.h"
//#include "preorder_traversal.h"
#include "reg_a.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <limits>
#include <memory>

namespace search {

/** The state of the search node */
enum class NodeState {
	UNKNOWN, /**< The node is not explored yet */
	GOOD,    /**< No undesired behavior is possible */
	BAD,     /**< Undesired behavior, i.e., the specification is violated */
	DEAD,    /**< The node does not have any successors */
};

/** The label of the search node */
enum class NodeLabel {
	UNLABELED,
	BOTTOM,
	TOP,
	CANCELED,
};

/** The reason for the current label, used for more detailed output */
enum class LabelReason {
	UNKNOWN,
	BAD_NODE,
	DEAD_NODE,
	NO_ATA_SUCCESSOR,
	MONOTONIC_DOMINATION,
	NO_BAD_ENV_ACTION,
	GOOD_CONTROLLER_ACTION_FIRST,
	BAD_ENV_ACTION_FIRST
};

/** A node in the search tree
 * @see TreeSearch */
template <typename Location, typename ActionType>
struct SearchTreeNode
{
	/** Construct a node.
	 * @param words The CanonicalABWords of the node (being of the same reg_a class)
	 * @param parent The parent of this node, nullptr is this is the root
	 */
	SearchTreeNode(const std::set<CanonicalABWord<Location, ActionType>> &words,
	               SearchTreeNode *                                       parent = nullptr,
	               const std::set<std::pair<RegionIndex, ActionType>> &          = {})
	: words(words)
	{
		if (parent != nullptr) {
			parents.push_back(parent);
		}
		assert(std::all_of(std::begin(words), std::end(words), [&words](const auto &word) {
			return words.empty() || reg_a(*std::begin(words)) == reg_a(word);
		}));
	}

	/** @brief Set the node label.
	 * @param new_label The new node label
	 * @param cancel_children If true, cancel children after setting the node label
	 */
	void
	set_label(NodeLabel new_label, bool cancel_children = false)
	{
		assert(new_label != NodeLabel::UNLABELED);
		if (label != NodeLabel::UNLABELED) {
			// Node already labeled, cancel.
			return;
		}
		label = new_label;
		if (cancel_children && is_expanded) {
			for (const auto &[action, child] : children) {
				child->set_label(NodeLabel::CANCELED, true);
			}
		}
	}

	/**
	 * @brief Implements incremental labeling during search, bottom up. Nodes are labelled as soon
	 * as their label state can definitely be determined either because they are leaf-nodes or
	 * because the labeling of child nodes permits to determine a label (see details).
	 * @details Leaf-nodes can directly be labelled (in most cases?), the corresponding label pushed
	 * upwards in the search tree may allow for shortening the search significantly in the following
	 * cases: 1) A child is labelled "BAD" and there is no control-action which can be taken before
	 * that is labelled "GOOD" -> the node can be labelled as "BAD". 2) A child is labelled "GOOD"
	 * and came from a control-action and there is no non-"GOOD" environmental-action happening
	 * before -> the node can be labelled "GOOD". The call should be propagated to the parent node
	 * in case the labelling has been determined.
	 * @param controller_actions The set of controller actions
	 * @param environment_actions The set of environment actions
	 * @param cancel_children If true, cancel children if a node is labeled
	 */
	void
	label_propagate(const std::set<ActionType> &controller_actions,
	                const std::set<ActionType> &environment_actions,
	                bool                        cancel_children = false)
	{
		// SPDLOG_TRACE("Call propagate on node {}", *this);
		assert(is_expanded);
		// leaf-nodes should always be labelled directly
		assert(!children.empty() || label != NodeLabel::UNLABELED);
		// if not already happened: call recursively on parent node
		if (children.empty()) {
			assert(label != NodeLabel::UNLABELED);
			SPDLOG_TRACE("Node is a leaf, propagate labels.", *this);
			for (const auto &parent : parents) {
				parent->label_propagate(controller_actions, environment_actions, cancel_children);
			}
			return;
		}
		// do nothing if the node is already labelled
		if (label != NodeLabel::UNLABELED && !children.empty()) {
			SPDLOG_TRACE("Node is already labelled, abort.");
			return;
		}
		assert(!children.empty());

		// Find good and bad child nodes which are already labelled and determine their order (with
		// respect to time). Also keep track of yet unlabelled nodes (both cases, environmental and
		// controller action).
		constexpr auto max = std::numeric_limits<RegionIndex>::max();
		RegionIndex    first_good_controller_step{max};
		RegionIndex    first_non_bad_controller_step{max};
		RegionIndex    first_non_good_environment_step{max};
		RegionIndex    first_bad_environment_step{max};
		for (const auto &[timed_action, child] : children) {
			// Copy label to avoid races while checking the conditions below.
			const NodeLabel child_label = child->label;
			const auto &[step, action]  = timed_action;
			if (child_label == NodeLabel::TOP
			    && controller_actions.find(action) != std::end(controller_actions)) {
				first_good_controller_step = std::min(first_good_controller_step, step);
			} else if (child_label == NodeLabel::BOTTOM
			           && environment_actions.find(action) != std::end(environment_actions)) {
				first_bad_environment_step = std::min(first_bad_environment_step, step);
			} else if (child.get() != this && child_label == NodeLabel::UNLABELED
			           && environment_actions.find(action) != std::end(environment_actions)) {
				first_non_good_environment_step = std::min(first_non_good_environment_step, step);
			} else if (child.get() != this && child_label == NodeLabel::UNLABELED
			           && controller_actions.find(action) != std::end(controller_actions)) {
				first_non_bad_controller_step = std::min(first_non_bad_controller_step, step);
			}
		}
		SPDLOG_TRACE("First good controller step at {}, first non-good env. action step at {}, first "
		             "bad env. action at {}",
		             first_good_controller_step,
		             first_non_good_environment_step,
		             first_bad_environment_step);
		// cases in which incremental labelling can be applied and recursive calls should be issued
		if (first_non_good_environment_step == max && first_bad_environment_step == max) {
			label_reason = LabelReason::NO_BAD_ENV_ACTION;
			set_label(NodeLabel::TOP, cancel_children);
			SPDLOG_TRACE("{}: No non-good or bad environment action", NodeLabel::TOP);
		} else if (first_good_controller_step < first_non_good_environment_step
		           && first_good_controller_step < first_bad_environment_step) {
			label_reason = LabelReason::GOOD_CONTROLLER_ACTION_FIRST;
			set_label(NodeLabel::TOP, cancel_children);
			SPDLOG_TRACE("{}: Good controller action at {}, before first non-good env action at {}",
			             NodeLabel::TOP,
			             first_good_controller_step,
			             std::min(first_non_good_environment_step, first_bad_environment_step));
		} else if (first_bad_environment_step < max
		           && first_bad_environment_step <= first_good_controller_step
		           && first_bad_environment_step <= first_non_bad_controller_step) {
			label_reason = LabelReason::BAD_ENV_ACTION_FIRST;
			set_label(NodeLabel::BOTTOM, cancel_children);
			SPDLOG_TRACE("{}: Bad env action at {}, before first non-bad controller action at {}",
			             NodeLabel::BOTTOM,
			             first_bad_environment_step,
			             std::min(first_non_good_environment_step, first_bad_environment_step));
		}
		if (label != NodeLabel::UNLABELED) {
			for (const auto &parent : parents) {
				parent->label_propagate(controller_actions, environment_actions, cancel_children);
			}
		}
	}

	///**
	// * @brief Iterator to the beginning of the sequence induced by preorder traversal of the subtree
	// * induced by this node.
	// * @return preorder_iterator<SearchTreeNode<Location, ActionType>>
	// */
	// preorder_iterator<SearchTreeNode<Location, ActionType>>
	// begin()
	//{
	//	return search::begin(this);
	//}

	///**
	// * @brief Iterator to the end of the sequence induced by preorder traversal of the subtree
	// induced
	// * by this node.
	// * @return preorder_iterator<SearchTreeNode<Location, ActionType>>
	// */
	// preorder_iterator<SearchTreeNode<Location, ActionType>>
	// end()
	//{
	//	return search::end(this);
	//}

	/**
	 * @brief Compares two nodes for equality.
	 * @param other The other node
	 * @return true If both nodes are the same (without considering subtrees)
	 * @return false Otherwise
	 */
	bool
	operator==(const SearchTreeNode<Location, ActionType> &other) const
	{
		return this->words == other.words && this->state == other.state && this->label == other.label
		  //&& this->parent == other.parent && this->children == other.children
		  ;
	}

	/** The words of the node */
	std::set<CanonicalABWord<Location, ActionType>> words;
	/** The state of the node */
	std::atomic<NodeState> state = NodeState::UNKNOWN;
	/** Whether we have a successful strategy in the node */
	std::atomic<NodeLabel> label = NodeLabel::UNLABELED;
	/** The parent of the node, this node was directly reached from the parent */
	std::vector<SearchTreeNode *> parents = {};
	/** Whether the node has been expanded. This is used for multithreading, in particular to check
	 * whether we can access the children already. */
	std::atomic_bool is_expanded{false};
	/** A list of the children of the node, which are reachable by a single transition */
	// TODO change container with custom comparator to set to avoid duplicates (also better
	// performance)
	std::map<std::pair<RegionIndex, ActionType>, std::shared_ptr<SearchTreeNode>> children = {};
	/** A more detailed description for the node that explains the current label. */
	LabelReason label_reason = LabelReason::UNKNOWN;
};

/** Print a node state. */
std::ostream &operator<<(std::ostream &os, const search::NodeState &node_state);
/** Print a node label. */
std::ostream &operator<<(std::ostream &os, const search::NodeLabel &node_label);

/** @brief Print a SearchTreeNode, optionally the whole tree.
 * By default, just print information about the node itself on a single line. Optionally, also print
 * all its children, effectively printing the whole sub-tree.
 * @param os The stream to print to
 * @param node The node to print
 * @param print_children If true, also print the node's children
 * @param indent The indentation to insert before this node, should be the distance to the root node
 */
template <typename Location, typename ActionType>
void
print_to_ostream(std::ostream &                                      os,
                 const search::SearchTreeNode<Location, ActionType> &node,
                 bool                                                print_children = false,
                 unsigned int                                        indent         = 0)
{
	os << "(" << indent << ") -> { ";
	os << "} -> " << node.words << ": " << node.state << " " << node.label;
	if (false && print_children) {
		os << '\n';
		for (const auto &[action, child] : node.children) {
			for (unsigned int i = 0; i < indent; i++) {
				os << "  ";
			}
			os << "(" << action.first << ", " << action.second << ")"
			   << " -> ";
			print_to_ostream(os, *child, true, indent + 1);
		}
	}
}

/** Print a node
 * @param os The ostream to print to
 * @param node The node to print
 * @return A reference to the ostream
 */
template <typename Location, typename ActionType>
std::ostream &
operator<<(std::ostream &os, const search::SearchTreeNode<Location, ActionType> &node)
{
	print_to_ostream(os, node);
	return os;
}

/** Print a node to a string.
 * @param node The node to print
 * @param print_children If true, also print the node's children
 * @return A string representation of the node
 */
template <typename Location, typename ActionType>
std::string
node_to_string(const search::SearchTreeNode<Location, ActionType> &node,
               bool                                                print_children = false)
{
	std::stringstream str;
	print_to_ostream(str, node, print_children);
	return str.str();
}

/** Print a vector of node pointers
 * @param os The ostream to print to
 * @param nodes The node pointers to print
 * @return A reference to the ostream
 */
template <typename Location, typename ActionType>
std::ostream &
operator<<(std::ostream &                                                                    os,
           const std::vector<std::shared_ptr<search::SearchTreeNode<Location, ActionType>>> &nodes)
{
	for (const auto &node : nodes) {
		os << *node;
	}
	return os;
}

} // namespace search
