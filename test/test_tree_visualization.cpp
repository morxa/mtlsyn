/***************************************************************************
 *  test_tree_visualization.cpp - Test search tree visualization
 *
 *  Created:   Thu 15 Apr 20:54:49 CEST 2021
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

#include "automata/ta.h"
#include "mtl/MTLFormula.h"

#include <search/canonical_word.h>
#include <search/search_tree.h>
#include <visualization/tree_to_graphviz.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

namespace {

using ATARegionState  = search::ATARegionState<std::string>;
using CanonicalABWord = search::CanonicalABWord<std::string, std::string>;
using Location        = automata::ta::Location<std::string>;
using Node            = search::SearchTreeNode<std::string, std::string>;
using TARegionState   = search::TARegionState<std::string>;

using search::LabelReason;
using search::NodeLabel;

using Catch::Matchers::Contains;

TEST_CASE("Search tree visualization", "[search][visualization]")
{
	auto create_test_node =
	  [](const std::set<CanonicalABWord> &                                            words,
	     std::map<std::pair<search::RegionIndex, std::string>, std::shared_ptr<Node>> children = {}) {
		  auto node         = std::make_shared<Node>(words);
		  node->is_expanded = true;
		  node->children    = children;
		  for (const auto &[action, child] : node->children) {
			  child->parents = {node.get()};
		  }
		  return node;
	  };
	const logic::MTLFormula            a{logic::AtomicProposition<std::string>{"a"}};
	const logic::MTLFormula            b{logic::AtomicProposition<std::string>{"b"}};
	std::vector<std::shared_ptr<Node>> children;
	auto                               n1 = create_test_node(
    {{{TARegionState{Location{"l0"}, "x", 0}}, {TARegionState{Location{"l0"}, "y", 1}}}});
	auto n2 = create_test_node(
	  {{{TARegionState{Location{"l0"}, "x", 1}}, {TARegionState{Location{"l0"}, "y", 2}}}});
	auto n3 = create_test_node(
	  {{{TARegionState{Location{"l0"}, "x", 1}}, {TARegionState{Location{"l0"}, "y", 2}}},
	   {{ATARegionState{a.until(b), 1}, TARegionState{Location{"l0"}, "x", 1}},
	    {TARegionState{Location{"l0"}, "y", 2}}}});
	auto root          = create_test_node({{{TARegionState{Location{"l0"}, "x", 0},
                                  TARegionState{Location{"l0"}, "y", 0}}}},
                               {{{1, "a"}, n1}, {{2, "b"}, n2}, {{3, "c"}, n3}});
	root->label        = NodeLabel::TOP;
	root->label_reason = LabelReason::GOOD_CONTROLLER_ACTION_FIRST;
	n1->label          = NodeLabel::TOP;
	n1->label_reason   = LabelReason::DEAD_NODE;
	n2->label          = NodeLabel::BOTTOM;
	n2->label_reason   = LabelReason::NO_BAD_ENV_ACTION;
	n3->label          = NodeLabel::BOTTOM;
	n3->label_reason   = LabelReason::BAD_ENV_ACTION_FIRST;

	auto graph = visualization::search_tree_to_graphviz(*root);
	graph.render_to_file("test_tree_visualization.png");
	const auto dot = graph.to_dot();

	// All nodes should have shape 'record'.
	CHECK_THAT(dot, Contains("shape=record"));

	// Check that all nodes have the expected labels.
	// TODO Add actions to visualization
	CHECK_THAT(dot,
	           Contains(
	             R"dot(label="{good controller action first}|{ { (l0, x, 0), (l0, y, 0) } }")dot"));
	CHECK_THAT(dot, Contains(R"dot(label="{dead node}|{ { (l0, x, 0) }|{ (l0, y, 1) } }")dot"));
	CHECK_THAT(dot,
	           Contains(R"dot(label="{no bad env action}|{ { (l0, x, 1) }|{ (l0, y, 2) } }")dot"));
	CHECK_THAT(
	  dot,
	  Contains(
	    R"dot(label="{bad env action first}|{ { (l0, x, 1) }|{ (l0, y, 2) } }|{ { (l0, x, 1), ((a U b), 1) }|{ (l0, y, 2) } }")dot"));

	// Check that both colors occur, we assume they are the right nodes.
	CHECK_THAT(dot, Contains("color=green"));
	CHECK_THAT(dot, Contains("color=red"));
}

} // namespace
