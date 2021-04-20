/***************************************************************************
 *  create_controller.h - Create a controller from a search tree
 *
 *  Created:   Tue 06 Apr 17:09:55 CEST 2021
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

#include "automata/automata.h"
#include "automata/ta.h"
#include "automata/ta_regions.h"
#include "search_tree.h"
#include "synchronous_product/canonical_word.h"
#include "synchronous_product/preorder_traversal.h"

#include <stdexcept>
namespace controller_synthesis {

/** Construct a set of constraints from a time successor CanonicalABWord.
 */
template <typename LocationT, typename ActionT>
std::multimap<std::string, automata::ClockConstraint>
get_constraints_from_time_successor(
  const synchronous_product::CanonicalABWord<LocationT, ActionT> &word)
{
	using TARegionState = synchronous_product::TARegionState<LocationT>;
	std::multimap<std::string, automata::ClockConstraint> res;
	for (const auto &symbol : word) {
		for (const auto &region_state : symbol) {
			assert(std::holds_alternative<TARegionState>(region_state));
			const TARegionState state = std::get<TARegionState>(region_state);
			for (const auto &constraint :
			     automata::ta::get_clock_constraints_from_region_index(state.region_index)) {
				res.insert({{state.clock, constraint}});
			}
		}
	}
	return res;
}

template <typename LocationT, typename ActionT>
automata::ta::TimedAutomaton<std::set<synchronous_product::CanonicalABWord<LocationT, ActionT>>,
                             ActionT>
create_controller(const synchronous_product::SearchTreeNode<LocationT, ActionT> *const root)
{
	using synchronous_product::NodeLabel;
	using Transition =
	  automata::ta::Transition<std::set<synchronous_product::CanonicalABWord<LocationT, ActionT>>,
	                           ActionT>;
	using Location =
	  automata::ta::Location<std::set<synchronous_product::CanonicalABWord<LocationT, ActionT>>>;
	auto               node = root;
	std::set<Location> locations;
	locations.emplace(root->words);
	std::set<ActionT>       actions;
	std::vector<Transition> transitions;
	std::set<std::string>   clocks;
	while (!node->children.empty()) {
		if (node->label != NodeLabel::TOP) {
			throw std::invalid_argument(
			  "Cannot create a controller for a search tree that is not labeled with TOP");
		}
		auto successor_it =
		  std::find_if(begin(node->children), end(node->children), [](const auto &child) {
			  return child->label == NodeLabel::TOP;
		  });
		if (successor_it == std::end(node->children)) {
			throw synchronous_product::InconsistentTreeException(
			  fmt::format("Inconsistent tree labeling: TOP-labeled node {} has no TOP-labeled child",
			              *node));
		}
		auto successor = successor_it->get();
		locations.emplace(successor->words);
		for (const auto &[region_increment, action] : successor->incoming_actions) {
			// TODO source/target location and action are done. What about clock constraints and resets?
			// We need to reconstruct them from the node's words and the child's words.
			// Problem: We do not know which node word matches which child's node.
			// Is it sufficient to consider the reg_a components of the words?
			actions.insert(action);
			auto constraints = get_constraints_from_time_successor(
			  get_time_successor(reg_a(*std::begin(node->words)), region_increment));
			for (const auto &[clock_name, constraint] : constraints) {
				clocks.insert(clock_name);
			}
			transitions.push_back(
			  Transition{Location{node->words}, action, Location{successor->words}, constraints, {}});
		}
		node = successor;
	}
	return automata::ta::TimedAutomaton<
	  std::set<synchronous_product::CanonicalABWord<LocationT, ActionT>>,
	  ActionT>{locations, actions, Location{root->words}, locations, clocks, transitions};
}

} // namespace controller_synthesis
