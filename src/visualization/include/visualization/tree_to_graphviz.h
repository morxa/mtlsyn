/***************************************************************************
 *  tree_to_graphviz.h - Convert a search tree to a Graphviz graph
 *
 *  Created:   Thu 15 Apr 16:59:15 CEST 2021
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

#include <search/search_tree.h>
#include <utilities/graphviz/graphviz.h>

#include <optional>
#include <sstream>
#include <vector>

namespace visualization {

using search::LabelReason;

/** @brief Add a search tree node to a dot graph visualization of the search tree.
 * Add node as dot node to thegraph. Additionally, add all its children along
 * with edges from the given node to its children.
 * @param search_node The node to add to the graph
 * @param graph The graph to add the node to
 * @return The graphviz node, which can be used as reference for adding additional edges.
 */
template <typename LocationT, typename ActionT>
std::optional<utilities::graphviz::Node>
add_search_node_to_graph(const search::SearchTreeNode<LocationT, ActionT> *search_node,
                         utilities::graphviz::Graph *                      graph,
                         utilities::graphviz::Node *                       parent        = nullptr,
                         bool                                              skip_canceled = false)
{
	if (skip_canceled && search_node->label == search::NodeLabel::CANCELED) {
		return std::nullopt;
	}
	std::vector<std::string> words_labels;
	for (const auto &word : search_node->words) {
		std::vector<std::string> word_labels;
		for (const auto &word_partition : word) {
			std::stringstream str;
			str << word_partition;
			word_labels.push_back(str.str());
		}
		// Split the partitions into node sections (by using "|" as separator).
		// Put each word in its own group (with {}) so it is separated from the other words.
		words_labels.push_back(fmt::format("{{ {} }}", fmt::join(word_labels, "|")));
	}

	std::string label_reason;
	switch (search_node->label_reason) {
	case LabelReason::UNKNOWN: label_reason = "unknown"; break;
	case LabelReason::BAD_NODE: label_reason = "bad node"; break;
	case LabelReason::DEAD_NODE: label_reason = "dead node"; break;
	case LabelReason::NO_ATA_SUCCESSOR: label_reason = "no ATA successor"; break;
	case LabelReason::MONOTONIC_DOMINATION: label_reason = "monotonic domination"; break;
	case LabelReason::NO_BAD_ENV_ACTION: label_reason = "no bad env action"; break;
	case LabelReason::GOOD_CONTROLLER_ACTION_FIRST:
		label_reason = "good controller action first";
		break;
	case LabelReason::BAD_ENV_ACTION_FIRST: label_reason = "bad env action first"; break;
	}
	const std::string         node_id  = fmt::format("{}", fmt::join(words_labels, "|"));
	const bool                new_node = !graph->has_node(node_id);
	utilities::graphviz::Node node     = graph->get_node(node_id).value_or(
    graph->add_node(fmt::format("{{{}}}|{}", label_reason, fmt::join(words_labels, "|")), node_id));
	// Set the node color according to its label.
	if (search_node->label == search::NodeLabel::TOP) {
		node.set_property("color", "green");
	} else if (search_node->label == search::NodeLabel::BOTTOM) {
		node.set_property("color", "red");
	}
	if (parent) {
		graph->add_edge(*parent, node);
	}
	if (new_node) {
		// TODO fix edges
		for (const auto &[action, child] : search_node->children) {
			add_search_node_to_graph(child.get(), graph, &node, skip_canceled);
		}
	}
	return node;
}

/** @brief Generate a graphviz graph visualizing the search tree.
 * @param search_node The root node of the tree
 * @param skip_canceled If true, skip nodes that have been canceled
 * @return The search tree converted to a dot graph
 */
template <typename LocationT, typename ActionT>
utilities::graphviz::Graph
search_tree_to_graphviz(const search::SearchTreeNode<LocationT, ActionT> &search_node,
                        bool                                              skip_canceled = false)
{
	utilities::graphviz::Graph graph;
	graph.set_property("rankdir", "LR");
	graph.set_default_node_property("shape", "record");
	add_search_node_to_graph(&search_node, &graph, nullptr, skip_canceled);
	return graph;
}

} // namespace visualization
