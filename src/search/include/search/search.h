/***************************************************************************
 *  search.h - Construct the search tree over ABConfigurations
 *
 *  Created:   Mon  1 Feb 16:21:53 CET 2021
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

#include "automata/ata.h"
#include "automata/ta.h"
#include "canonical_word.h"
#include "heuristics.h"
#include "mtl/MTLFormula.h"
#include "operators.h"
#include "reg_a.h"
#include "search_tree.h"
#include "synchronous_product.h"
#include "utilities/priority_thread_pool.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <queue>
#include <variant>

namespace search {

/** @brief Check if the node has a satisfiable ATA configuration.
 * If every word in the node contains an ATA sink location, than none of those configurations is
 * satisfiable.
 * @return false if every word contains an ATA sink location
 */
template <typename Location, typename ActionType>
bool
has_satisfiable_ata_configuration(const SearchTreeNode<Location, ActionType> &node)
{
	return !std::all_of(std::begin(node.words), std::end(node.words), [](const auto &word) {
		return std::any_of(std::begin(word), std::end(word), [](const auto &component) {
			return std::find_if(std::begin(component),
			                    std::end(component),
			                    [](const auto &region_symbol) {
				                    return std::holds_alternative<ATARegionState<ActionType>>(region_symbol)
				                           && std::get<ATARegionState<ActionType>>(region_symbol).formula
				                                == logic::MTLFormula<ActionType>{
				                                  logic::AtomicProposition<ActionType>{"sink"}};
			                    })
			       != std::end(component);
		});
	});
}

/** Search the configuration tree for a valid controller. */
template <typename Location, typename ActionType>
class TreeSearch
{
	using Node = SearchTreeNode<Location, ActionType>;

public:
	/** Initialize the search.
	 * @param ta The plant to be controlled
	 * @param ata The specification of undesired behaviors
	 * @param controller_actions The actions that the controller may decide to take
	 * @param environment_actions The actions controlled by the environment
	 * @param K The maximal constant occurring in a clock constraint
	 * @param incremental_labeling True, if incremental labeling should be used (default=false)
	 * @param terminate_early If true, cancel the children of a node that has already been labeled
	 * @param heuristic The heuristic to use during tree expansion
	 */
	TreeSearch(const automata::ta::TimedAutomaton<Location, ActionType> *                      ta,
	           automata::ata::AlternatingTimedAutomaton<logic::MTLFormula<ActionType>,
	                                                    logic::AtomicProposition<ActionType>> *ata,
	           std::set<ActionType>                                   controller_actions,
	           std::set<ActionType>                                   environment_actions,
	           RegionIndex                                            K,
	           bool                                                   incremental_labeling = false,
	           bool                                                   terminate_early      = false,
	           std::unique_ptr<Heuristic<long, Location, ActionType>> heuristic =
	             std::make_unique<BfsHeuristic<long, Location, ActionType>>())
	: ta_(ta),
	  ata_(ata),
	  controller_actions_(controller_actions),
	  environment_actions_(environment_actions),
	  K_(K),
	  incremental_labeling_(incremental_labeling),
	  terminate_early_(terminate_early),
	  tree_root_(std::make_unique<Node>(std::set<CanonicalABWord<Location, ActionType>>{
	    get_canonical_word(ta->get_initial_configuration(), ata->get_initial_configuration(), K)})),
	  heuristic(std::move(heuristic))
	{
		// Assert that the two action sets are disjoint.
		assert(
		  std::all_of(controller_actions_.begin(), controller_actions_.end(), [this](const auto &a) {
			  return environment_actions_.find(a) == environment_actions_.end();
		  }));
		assert(
		  std::all_of(environment_actions_.begin(), environment_actions_.end(), [this](const auto &a) {
			  return controller_actions_.find(a) == controller_actions_.end();
		  }));
		add_node_to_queue(tree_root_.get());
	}

	/** Get the root of the search tree.
	 * @return A pointer to the root, only valid as long as the TreeSearch object has not been
	 * destroyed
	 */
	Node *
	get_root() const
	{
		return tree_root_.get();
	}

	/** Check if a node is bad, i.e., if it violates the specification.
	 * @param node A pointer to the node to check
	 * @return true if the node is bad
	 */
	bool
	is_bad_node(Node *node) const
	{
		return std::any_of(node->words.begin(), node->words.end(), [this](const auto &word) {
			const auto candidate = get_candidate(word);
			return ta_->is_accepting_configuration(candidate.first)
			       && ata_->is_accepting_configuration(candidate.second);
		});
	}

	/** Check if there is an ancestor that monotonally dominates the given node
	 * @param node The node to check
	 */
	bool
	dominates_ancestor(Node *node) const
	{
		const Node *ancestor = node->parent;
		while (ancestor != nullptr) {
			if (is_monotonically_dominated(ancestor->words, node->words)) {
				return true;
			}
			ancestor = ancestor->parent;
		}
		return false;
	}

	/** Add a node the processing queue. This adds a new task to the thread pool that expands the node
	 * asynchronously.
	 * @param node The node to expand */
	void
	add_node_to_queue(Node *node)
	{
		pool_.add_job([this, node] { expand_node(node); }, -heuristic->compute_cost(node));
	}

	/** Build the complete search tree by expanding nodes recursively.
	 * @param multi_threaded If set to true, run the thread pool. Otherwise, process the jobs
	 * synchronously with a single thread. */
	void
	build_tree(bool multi_threaded = true)
	{
		if (multi_threaded) {
			pool_.start();
			pool_.wait();
		} else {
			while (step()) {}
		}
	}

	/** Compute the next iteration by taking the first item of the queue and expanding it.
	 * @return true if there was still an unexpanded node
	 */
	bool
	step()
	{
		utilities::QueueAccess queue_access{&pool_};
		if (queue_access.empty()) {
			return false;
		}
		auto step_function = std::get<1>(queue_access.top());
		queue_access.pop();
		step_function();
		return true;
	}

	/** Process and expand the given node.  */
	void
	expand_node(Node *node)
	{
		if (node->is_expanded || node->label != NodeLabel::UNLABELED) {
			// The node was already expanded or labeled, nothing to do.
			return;
		}
		SPDLOG_TRACE("Processing {}", *node);
		if (is_bad_node(node)) {
			node->label_reason = LabelReason::BAD_NODE;
			node->state        = NodeState::BAD;
			node->is_expanded  = true;
			if (incremental_labeling_) {
				node->set_label(NodeLabel::BOTTOM, terminate_early_);
				node->label_propagate(controller_actions_, environment_actions_, terminate_early_);
			}
			return;
		}
		if (!has_satisfiable_ata_configuration(*node)) {
			node->label_reason = LabelReason::NO_ATA_SUCCESSOR;
			node->state        = NodeState::GOOD;
			node->is_expanded  = true;
			if (incremental_labeling_) {
				node->set_label(NodeLabel::TOP, terminate_early_);
				node->label_propagate(controller_actions_, environment_actions_, terminate_early_);
			}
			return;
		}
		if (dominates_ancestor(node)) {
			node->label_reason = LabelReason::MONOTONIC_DOMINATION;
			node->state        = NodeState::GOOD;
			node->is_expanded  = true;
			if (incremental_labeling_) {
				node->set_label(NodeLabel::TOP, terminate_early_);
				node->label_propagate(controller_actions_, environment_actions_, terminate_early_);
			}
			return;
		}
		assert(node->children.empty());
		// Represent a set of configurations by their reg_a component so we can later partition the
		// set
		std::map<CanonicalABWord<Location, ActionType>, std::set<CanonicalABWord<Location, ActionType>>>
		  child_classes;
		// Store with which actions we reach each CanonicalABWord
		std::map<CanonicalABWord<Location, ActionType>, std::set<std::pair<RegionIndex, ActionType>>>
		  outgoing_actions;

		// Pre-compute time successors so we avoid re-computing them for each symbol.
		std::map<CanonicalABWord<Location, ActionType>,
		         std::vector<std::pair<RegionIndex, CanonicalABWord<Location, ActionType>>>>
		  time_successors;
		for (const auto &word : node->words) {
			time_successors[word] = get_time_successors(word, K_);
		}
		for (const auto &symbol : ta_->get_alphabet()) {
			std::set<std::pair<RegionIndex, CanonicalABWord<Location, ActionType>>> successors;
			for (const auto &word : node->words) {
				for (const auto &[increment, time_successor] : time_successors[word]) {
					for (const auto &successor :
					     get_next_canonical_words(*ta_, *ata_, get_candidate(time_successor), symbol, K_)) {
						successors.emplace(increment, successor);
					}
				}
			}

			// Partition the successors by their reg_a component.
			for (const auto &[increment, successor] : successors) {
				const auto word_reg = reg_a(successor);
				child_classes[word_reg].insert(successor);
				outgoing_actions[word_reg].insert(std::make_pair(increment, symbol));
			}
		}

		assert(child_classes.size() == outgoing_actions.size());

		// Create child nodes, where each child contains all successors words of
		// the same reg_a class.
		std::transform(std::begin(child_classes),
		               std::end(child_classes),
		               std::back_inserter(node->children),
		               [node, &outgoing_actions](auto &&map_entry) {
			               auto child =
			                 std::make_unique<Node>(std::move(map_entry.second),
			                                        node,
			                                        std::move(outgoing_actions[map_entry.first]));
			               return child;
		               });
		SPDLOG_TRACE("Finished processing sub tree:\n{}", node_to_string(*node, true));
		// Check if the node has been canceled in the meantime.
		if (node->label == NodeLabel::CANCELED) {
			node->children.clear();
			node->is_expanded = true;
			return;
		}
		node->is_expanded = true;
		for (const auto &child : node->children) {
			add_node_to_queue(child.get());
		}
		if (node->children.empty()) {
			node->state = NodeState::DEAD;
			if (incremental_labeling_) {
				node->label_reason = LabelReason::DEAD_NODE;
				node->set_label(NodeLabel::TOP, terminate_early_);
				node->label_propagate(controller_actions_, environment_actions_, terminate_early_);
			}
		}
	}

	/** Compute the final tree labels.
	 * @param node The node to start the labeling at (e.g., the root of the tree)
	 */
	void
	label(Node *node = nullptr)
	{
		// TODO test the label function separately.
		if (node == nullptr) {
			node = get_root();
		}
		if (node->state == NodeState::GOOD || node->state == NodeState::DEAD) {
			node->set_label(NodeLabel::TOP, terminate_early_);
		} else if (node->state == NodeState::BAD) {
			node->set_label(NodeLabel::BOTTOM, terminate_early_);
		} else {
			for (const auto &child : node->children) {
				label(child.get());
			}
			bool        found_bad = false;
			RegionIndex first_good_controller_step{std::numeric_limits<RegionIndex>::max()};
			RegionIndex first_bad_environment_step{std::numeric_limits<RegionIndex>::max()};
			for (const auto &child : node->children) {
				for (const auto &[step, action] : child->incoming_actions) {
					if (child->label == NodeLabel::TOP
					    && controller_actions_.find(action) != std::end(controller_actions_)) {
						first_good_controller_step = std::min(first_good_controller_step, step);
					} else if (child->label == NodeLabel::BOTTOM
					           && environment_actions_.find(action) != std::end(environment_actions_)) {
						found_bad                  = true;
						first_bad_environment_step = std::min(first_bad_environment_step, step);
					}
				}
			}
			if (!found_bad || first_good_controller_step < first_bad_environment_step) {
				node->set_label(NodeLabel::TOP, terminate_early_);
			} else {
				node->set_label(NodeLabel::BOTTOM, terminate_early_);
			}
		}
	}

	/** Get the size of the given sub-tree.
	 * @param node The sub-tree to get the size of, defaults to the whole tree if omitted
	 * @return The number of nodes in the sub-tree, including the node itself
	 */
	size_t
	get_size(Node *node = nullptr)
	{
		if (node == nullptr) {
			node = get_root();
		}
		size_t sum = 1;
		for (const auto &child : node->children) {
			sum += get_size(child.get());
		}
		return sum;
	}

private:
	const automata::ta::TimedAutomaton<Location, ActionType> *const                             ta_;
	const automata::ata::AlternatingTimedAutomaton<logic::MTLFormula<ActionType>,
	                                               logic::AtomicProposition<ActionType>> *const ata_;

	const std::set<ActionType> controller_actions_;
	const std::set<ActionType> environment_actions_;
	RegionIndex                K_;
	const bool                 incremental_labeling_;
	const bool                 terminate_early_{false};

	std::unique_ptr<Node>       tree_root_;
	utilities::ThreadPool<long> pool_{utilities::ThreadPool<long>::StartOnInit::NO};
	std::unique_ptr<Heuristic<long, Location, ActionType>> heuristic;
};

} // namespace search
