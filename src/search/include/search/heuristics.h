/***************************************************************************
 *  heuristics.h - Heuristics to evaluate search tree nodes
 *
 *  Created:   Tue 23 Mar 09:05:55 CET 2021
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

#ifndef SRC_SYNCHRONOUS_PRODUCT_INCLUDE_SYNCHRONOUS_PRODUCT_HEURISTICS_H
#define SRC_SYNCHRONOUS_PRODUCT_INCLUDE_SYNCHRONOUS_PRODUCT_HEURISTICS_H

#include "search_tree.h"

#include <limits>

namespace search {

/** The heuristics interface.
 * @tparam ValueT The value type of the heuristic function
 * @tparam LocationT The type of the location of an automaton
 * @tparam ActionT The type of an action of an automaton
 */
template <typename ValueT, typename LocationT, typename ActionT>
class Heuristic
{
public:
	/** @brief Compute the cost of the given node.
	 * The higher the cost, the lower the priority.
	 * @param node The node to compute the cost for
	 * @return The cost of the node
	 */
	virtual ValueT compute_cost(SearchTreeNode<LocationT, ActionT> *node) = 0;
	/** Virtual destructor. */
	virtual ~Heuristic()
	{
	}
};

/** @brief The BFS heuristic.
 * The BFS heuristic simply increases the cost with every evaluated node and therefore
 * processes them just like a FIFO queue, resulting in breadth-first sarch.
 * @tparam ValueT The value type of the heuristic function
 * @tparam LocationT The type of the location of an automaton
 * @tparam ActionT The type of an action of an automaton
 */
template <typename ValueT, typename LocationT, typename ActionT>
class BfsHeuristic : public Heuristic<ValueT, LocationT, ActionT>
{
public:
	/** @brief Compute the cost of the given node.
	 * The cost will strictly monotonically increase for each node, thereby emulating breadth-first
	 * search.
	 * @return The cost of the node
	 */
	ValueT
	compute_cost(SearchTreeNode<LocationT, ActionT> *) override
	{
		return ++node_counter;
	}

private:
	std::atomic_size_t node_counter{0};
};

/** @brief The DFS heuristic.
 * The BFS heuristic simply decreases the cost with every evaluated node and therefore
 * processes them just like a LIFO queue, resulting in depth-first sarch.
 */
template <typename ValueT, typename LocationT, typename ActionT>
class DfsHeuristic : public Heuristic<ValueT, LocationT, ActionT>
{
public:
	/** @brief Compute the cost of the given node.
	 * The cost will strictly monotonically increase for each node, thereby emulating breadth-first
	 * search.
	 * @return The cost of the node
	 */
	ValueT
	compute_cost(SearchTreeNode<LocationT, ActionT> *) override
	{
		return -(++node_counter);
	}

private:
	std::atomic_size_t node_counter{0};
};

/** @brief The Time heuristic, which prefers early actions.
 * This heuristic computes the accumulated time from the root node to the current node and
 * prioritizes nodes that occur early.
 * */
template <typename ValueT, typename LocationT, typename ActionT>
class TimeHeuristic : public Heuristic<ValueT, LocationT, ActionT>
{
public:
	/** @brief Compute the cost of the given node.
	 * The cost will strictly monotonically increase for each node, thereby emulating breadth-first
	 * search.
	 * @param node The node to compute the cost for
	 * @return The cost of the node
	 */
	ValueT
	compute_cost(SearchTreeNode<LocationT, ActionT> *node) override
	{
		if (node->parents.empty()) {
			return 0;
		}
		ValueT parent_cost = std::numeric_limits<ValueT>::max();
		for (const auto &parent : node->parents) {
			parent_cost = std::min(parent_cost, compute_cost(parent));
		}
		ValueT node_cost = std::numeric_limits<ValueT>::max();
		for (const auto &action : node->incoming_actions) {
			node_cost = std::min(node_cost, ValueT{action.first});
		}
		return parent_cost + node_cost;
	}
};

/** @brief Prefer environment actions over controller actions.
 * This heuristic assigns a cost of 0 to every node that has at least one environment action as
 * incoming action. Otherwise, it assigns the cost 1.
 */
template <typename ValueT, typename LocationT, typename ActionT>
class PreferEnvironmentActionHeuristic : public Heuristic<ValueT, LocationT, ActionT>
{
public:
	/** Initialize the heuristic.
	 * @param environment_actions The environment actions that may occur
	 */
	PreferEnvironmentActionHeuristic(const std::set<ActionT> &environment_actions)
	: environment_actions(environment_actions)
	{
	}

	/** Compute the cost of a node.
	 * @param node The node to compute the cost for
	 * @return 0 if the node contains an environment action as incoming action, 1 otherwise.
	 */
	ValueT
	compute_cost(SearchTreeNode<LocationT, ActionT> *node) override
	{
		if (std::find_if(std::begin(node->incoming_actions),
		                 std::end(node->incoming_actions),
		                 [this](const auto &action) {
			                 return environment_actions.find(action.second)
			                        != std::end(environment_actions);
		                 })
		    != std::end(node->incoming_actions)) {
			return 0;
		} else {
			return 1;
		}
	}

private:
	std::set<ActionT> environment_actions;
};

/** @brief Prefer nodes with a low number of canonical words. */
template <typename ValueT, typename LocationT, typename ActionT>
class NumCanonicalWordsHeuristic : public Heuristic<ValueT, LocationT, ActionT>
{
public:
	/** Compute the cost of a node.
	 * @param node The node to compute the cost for
	 * @return The number of canonical worrds in the node
	 */
	ValueT
	compute_cost(SearchTreeNode<LocationT, ActionT> *node) override
	{
		return node->words.size();
	}
};

/** @brief Compose multiple heuristics.
 * This heuristic computes a weighted sum over a set of heuristics.
 */
template <typename ValueT, typename LocationT, typename ActionT>
class CompositeHeuristic : public Heuristic<ValueT, LocationT, ActionT>
{
public:
	/** Initialize the heuristic.
	 * @param heuristics A set of pairs (weight, heuristic) to use for the weighted sum
	 */
	CompositeHeuristic(
	  std::vector<std::pair<ValueT, std::unique_ptr<Heuristic<ValueT, LocationT, ActionT>>>>
	    heuristics)
	: heuristics(std::move(heuristics))
	{
	}

	/** Compute the cost of a node.
	 * @param node The node to compute the cost for
	 * @return The weighted sum over all the heuristics
	 */
	ValueT
	compute_cost(SearchTreeNode<LocationT, ActionT> *node) override
	{
		ValueT res = 0;
		for (auto &&[weight, heuristic] : heuristics) {
			res += weight * heuristic->compute_cost(node);
		}
		return res;
	}

private:
	std::vector<std::pair<ValueT, std::unique_ptr<Heuristic<ValueT, LocationT, ActionT>>>> heuristics;
};

} // namespace search

#endif /* ifndef SRC_SYNCHRONOUS_PRODUCT_INCLUDE_SYNCHRONOUS_PRODUCT_HEURISTICS_H */
