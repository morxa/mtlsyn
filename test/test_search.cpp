/***************************************************************************
 *  test_search.cpp - Test the main search algorithm
 *
 *  Created:   Mon  1 Feb 17:23:48 CET 2021
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

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include "mtl/MTLFormula.h"
#include "mtl_ata_translation/translator.h"
#include "search/search.h"
#include "search/search_tree.h"
#include "search/synchronous_product.h"
#include "visualization/tree_to_graphviz.h"

#include <spdlog/spdlog.h>

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace std {
std::ostream &
operator<<(std::ostream &os, const std::pair<search::RegionIndex, std::string> &timed_action)
{
	os << "(" << timed_action.first << ", " << timed_action.second << ")";
	return os;
}

std::ostream &
operator<<(std::ostream &os, const std::set<std::pair<search::RegionIndex, std::string>> &actions)
{
	os << "{ ";
	bool first = true;
	for (const auto &action : actions) {
		if (first) {
			first = false;
		} else {
			os << ", ";
		}
		os << action;
	}
	os << " }";
	return os;
}
} // namespace std

namespace {

using TreeSearch      = search::TreeSearch<std::string, std::string>;
using TATransition    = automata::ta::Transition<std::string, std::string>;
using TA              = automata::ta::TimedAutomaton<std::string, std::string>;
using TAConfiguration = automata::ta::Configuration<std::string>;
using CanonicalABWord = search::CanonicalABWord<std::string, std::string>;
using TARegionState   = search::TARegionState<std::string>;
using ATARegionState  = search::ATARegionState<std::string>;
using AP              = logic::AtomicProposition<std::string>;
using automata::AtomicClockConstraintT;
using search::NodeLabel;
using search::NodeState;
using search::RegionIndex;
using AP = logic::AtomicProposition<std::string>;
using utilities::arithmetic::BoundType;
using Location = automata::ta::Location<std::string>;
using Node     = search::SearchTreeNode<std::string, std::string>;

template <typename Key, typename Val>
std::set<Key>
get_map_keys(const std::map<Key, Val> &map)
{
	std::set<Key> keys;
	for (const auto &[key, val] : map) {
		keys.insert(key);
	}
	return keys;
}

auto
create_test_node(const std::set<CanonicalABWord> &words = {})
{
	auto node         = std::make_shared<Node>(words);
	node->is_expanded = true;
	return node;
}

auto
dummyWords(const RegionIndex &region = 0)
{
	logic::MTLFormula<std::string> a{AP("a")};
	logic::MTLFormula<std::string> b{AP("b")};
	return std::set<CanonicalABWord>{CanonicalABWord(
	  {{TARegionState{Location{"l0"}, "x", region}}, {ATARegionState{a.until(b), region}}})};
}

TEST_CASE("Search in an ABConfiguration tree", "[search]")
{
	spdlog::set_level(spdlog::level::trace);
	TA ta{{"a", "b"}, Location{"l0"}, {Location{"l0"}, Location{"l1"}, Location{"l2"}}};
	ta.add_clock("x");
	ta.add_transition(TATransition(Location{"l0"},
	                               "a",
	                               Location{"l0"},
	                               {{"x", AtomicClockConstraintT<std::greater<automata::Time>>(1)}},
	                               {"x"}));
	ta.add_transition(TATransition(Location{"l0"},
	                               "b",
	                               Location{"l1"},
	                               {{"x", AtomicClockConstraintT<std::less<automata::Time>>(1)}}));
	ta.add_transition(TATransition(Location{"l2"}, "b", Location{"l1"}));
	logic::MTLFormula<std::string> a{AP("a")};
	logic::MTLFormula<std::string> b{AP("b")};

	logic::MTLFormula spec = a.until(b, logic::TimeInterval{2, BoundType::WEAK, 2, BoundType::INFTY});
	auto              ata  = mtl_ata_translation::translate(spec, {AP{"a"}, AP{"b"}});
	TreeSearch        search(&ta, &ata, {"a"}, {"b"}, 2);
	TreeSearch        search_incremental_labeling(&ta, &ata, {"a"}, {"b"}, 2, true);

	SECTION("The search tree is initialized correctly")
	{
		CHECK(search.get_root()->words
		      == std::set{CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0},
		                                    ATARegionState{logic::MTLFormula{AP{"l0"}}, 0}}})});
		CHECK(search.get_root()->state == NodeState::UNKNOWN);
		CHECK(search.get_root()->parents.empty());
		CHECK(search.get_root()->get_children().empty());
		CHECK(search.get_size() == 1);
	}

	SECTION("The first step computes the right children")
	{
		REQUIRE(search.step());
		const auto &children = search.get_root()->get_children();
		visualization::search_tree_to_graphviz(*search.get_root(), false)
		  .render_to_file("search_step1.png");
		// Each action counts separately, even if it leads to the same child.
		REQUIRE(children.size() == 5);
		// Only unique nodes are counted, thus this should be the root and the 3 children.
		CHECK(search.get_size() == 4);
		CHECK(children.at({3, "a"})->words
		      == std::set{
		        CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}}, {ATARegionState{spec, 3}}}),
		        CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}, ATARegionState{spec, 4}}}),
		        CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}}, {ATARegionState{spec, 5}}})});
		CHECK(children.at({0, "b"})->words
		      == std::set{
		        CanonicalABWord({{TARegionState{Location{"l1"}, "x", 0}, ATARegionState{spec, 0}}})});
		CHECK(children.at({1, "b"})->words
		      == std::set{
		        CanonicalABWord({{TARegionState{Location{"l1"}, "x", 1}, ATARegionState{spec, 1}}})});
	}

	SECTION("The next steps compute the right children")
	{
		REQUIRE(search.step());
		visualization::search_tree_to_graphviz(*search.get_root(), false)
		  .render_to_file("search_step2.png");
		REQUIRE(search.step());
		visualization::search_tree_to_graphviz(*search.get_root(), false)
		  .render_to_file("search_step3.png");
		REQUIRE(search.step());
		visualization::search_tree_to_graphviz(*search.get_root(), false)
		  .render_to_file("search_step4.png");
		REQUIRE(search.step());
		visualization::search_tree_to_graphviz(*search.get_root(), false)
		  .render_to_file("search_step5.png");
		REQUIRE(search.step());
		visualization::search_tree_to_graphviz(*search.get_root(), false)
		  .render_to_file("search_step6.png");
		const auto &root_children = search.get_root()->get_children();
		REQUIRE(root_children.size() == 5);

		{
			// Process first child of the root.
			// starts with [{(l0, x, 0), ((a U b), 3)}]
			const auto &children = root_children.at({3, "a"})->get_children();
			CHECK(root_children.at({3, "a"})->state == NodeState::UNKNOWN);
			// (3, a), (4, a), (5, a), (0, b), (1, b)
			REQUIRE(children.size() == 5);
			CHECK(get_map_keys(children)
			      == std::set<std::pair<RegionIndex, std::string>>{
			        {3, "a"}, {4, "a"}, {5, "a"}, {0, "b"}, {1, "b"}});
			CHECK(children.at({3, "a"})->words
			      == std::set{CanonicalABWord(
			        {{TARegionState{Location{"l0"}, "x", 0}}, {ATARegionState{spec, 5}}})});
			// They point to the same node.
			CHECK(children.at({3, "a"}) == children.at({4, "a"}));
			CHECK(children.at({3, "a"}) == children.at({5, "a"}));
			CHECK(children.at({0, "b"})->words
			      == std::set{CanonicalABWord({{TARegionState{Location{"l1"}, "x", 0}}}),
			                  CanonicalABWord({{TARegionState{Location{"l1"}, "x", 0},
			                                    ATARegionState{logic::MTLFormula{AP{"sink"}}, 0}}})});
			CHECK(children.at({1, "b"})->words
			      == std::set{CanonicalABWord({{TARegionState{Location{"l1"}, "x", 1}}})});
		}

		// Process second child of the root.
		REQUIRE(search.step());
		INFO("Tree:\n" << *search.get_root());
		CHECK(
		  root_children.at({0, "b"})->get_children().empty()); // should be ({(l1, x, 0), ((a U b), 0)})
		// the node has no time-symbol successors (only time successors)
		CHECK(root_children.at({0, "b"})->state == NodeState::DEAD);

		// Process third child of the root.
		REQUIRE(search.step());
		INFO("Tree:\n" << *search.get_root());
		REQUIRE(
		  root_children.at({1, "b"})->get_children().empty()); // should be ({(l1, x, 1), ((a U b), 1)})
		// the node has no time-symbol successors (only time successors)
		REQUIRE(root_children.at({1, "b"})->state == NodeState::DEAD);
	}

	SECTION("Compute the final tree")
	{
		// TODO This should be less steps with a fixed monotonic domination check.
		// We do exactly 13 steps.
		for (size_t i = 0; i < 12; i++) {
			SPDLOG_INFO("Step {}", i + 1);
			REQUIRE(search.step());
			visualization::search_tree_to_graphviz(*search.get_root(), false)
			  .render_to_file(fmt::format("search_final_{}.png", i + 1));
		}
		CHECK(!search.step());
		search.label();

		visualization::search_tree_to_graphviz(*search.get_root(), false)
		  .render_to_file("search_final.png");
		CHECK(get_map_keys(search.get_root()->get_children())
		      == std::set<std::pair<RegionIndex, std::string>>{
		        {3, "a"}, {4, "a"}, {5, "a"}, {0, "b"}, {1, "b"}});
		CHECK(search.get_root()->get_children().size() == 5);
		CHECK(search.get_root()->get_children().at({3, "a"})->get_children().size() == 5);
		CHECK(search.get_root()->get_children().at({3, "a"})
		      == search.get_root()->get_children().at({4, "a"}));
		CHECK(search.get_root()->get_children().at({3, "a"})
		      == search.get_root()->get_children().at({5, "a"}));
		CHECK(search.get_root()->get_children().at({0, "b"})->get_children().size() == 0);
		CHECK(search.get_root()->get_children().at({1, "b"})->get_children().size() == 0);
		// TODO Fails because monotonic domination is broken
		// CHECK(search.get_root()->get_children()[0]->get_children()[0]->get_children().size() == 0);
		CHECK(search.get_root()
		        ->get_children()
		        .at({3, "a"})
		        ->get_children()
		        .at({0, "b"})
		        ->get_children()
		        .size()
		      == 0);
		CHECK(search.get_root()
		        ->get_children()
		        .at({3, "a"})
		        ->get_children()
		        .at({1, "b"})
		        ->get_children()
		        .size()
		      == 0);

		CHECK(search.get_root()->state == NodeState::UNKNOWN);
		CHECK(search.get_root()->get_children().at({3, "a"})->state == NodeState::UNKNOWN);
		CHECK(search.get_root()->get_children().at({0, "b"})->state == NodeState::DEAD);
		CHECK(search.get_root()->get_children().at({1, "b"})->state == NodeState::DEAD);
		// TODO Fails because monotonic domination is broken
		// CHECK(search.get_root()->get_children()[0]->get_children()[0]->state == NodeState::GOOD);
		CHECK(search.get_root()->get_children().at({3, "a"})->get_children().at({0, "b"})->state
		      == NodeState::BAD);
		CHECK(search.get_root()->get_children().at({3, "a"})->get_children().at({1, "b"})->state
		      == NodeState::BAD);

		CHECK(search.get_root()->label == NodeLabel::TOP);
		CHECK(search.get_root()->get_children().at({3, "a"})->label == NodeLabel::BOTTOM);
		CHECK(search.get_root()->get_children().at({0, "b"})->label == NodeLabel::TOP);
		CHECK(search.get_root()->get_children().at({1, "b"})->label == NodeLabel::TOP);
		// TODO Fails because monotonic domination is broken
		// CHECK(search.get_root()->get_children()[0]->get_children()[0]->label == NodeLabel::TOP);
		CHECK(search.get_root()->get_children().at({3, "a"})->get_children().at({0, "b"})->label
		      == NodeLabel::BOTTOM);
		CHECK(search.get_root()->get_children().at({3, "a"})->get_children().at({1, "b"})->label
		      == NodeLabel::BOTTOM);
	}
	SECTION("Compare to incremental labeling")
	{
		// build standard tree
		search.build_tree(false);
		search.label();
		// comparison to incremental labeling approach
		search_incremental_labeling.build_tree(false);
		INFO("Tree:\n" << *search.get_root());
		INFO("Tree (incremental):\n" << *search_incremental_labeling.get_root());
		// check trees for equivalence
		CHECK(search.get_root()->label == search_incremental_labeling.get_root()->label);
		// TODO Fix tests that used to use the tree iterator
		// auto searchTreeIt            = search.get_root()->begin();
		// auto searchTreeIncrementalIt = search_incremental_labeling.get_root()->begin();
		// while (searchTreeIt != search.get_root()->end()) {
		//	CHECK(*searchTreeIt == *searchTreeIncrementalIt);
		//	++searchTreeIt;
		//	++searchTreeIncrementalIt;
		//}
	}
}

TEST_CASE("Search in an ABConfiguration tree without solution", "[search]")
{
	spdlog::set_level(spdlog::level::trace);
	TA ta{{"e", "c"}, Location{"l0"}, {Location{"l0"}, Location{"l1"}}};
	ta.add_clock("x");
	ta.add_transition(TATransition(Location{"l0"}, "e", Location{"l0"}));
	ta.add_transition(TATransition(Location{"l1"}, "c", Location{"l1"}));
	ta.add_transition(TATransition(Location{"l0"},
	                               "c",
	                               Location{"l1"},
	                               {{"x", AtomicClockConstraintT<std::greater<automata::Time>>(1)}}));
	logic::MTLFormula<std::string> e{AP("e")};
	logic::MTLFormula<std::string> c{AP("c")};

	logic::MTLFormula f   = logic::MTLFormula<std::string>::TRUE().until(e);
	auto              ata = mtl_ata_translation::translate(f, {AP{"e"}, AP{"c"}});
	TreeSearch        search(&ta, &ata, {"c"}, {"e"}, 2);
	search.build_tree();
	search.label();
	INFO("TA:\n" << ta);
	INFO("ATA:\n" << ata);
	INFO("Tree:\n" << *search.get_root());
	CHECK(search.get_root()->label == NodeLabel::BOTTOM);
}

TEST_CASE("Search in an ABConfiguration tree with a bad sub-tree", "[.][search]")
{
	TA ta{{"a", "b"}, Location{"l0"}, {Location{"l1"}}};
	ta.add_location(Location{"l2"});
	ta.add_clock("x");
	ta.add_clock("y");
	ta.add_transition(
	  TATransition(Location{"l0"},
	               "a",
	               Location{"l0"},
	               {{"x", AtomicClockConstraintT<std::less_equal<automata::Time>>(1)}},
	               {"x"}));
	ta.add_transition(TATransition(Location{"l0"},
	                               "a",
	                               Location{"l1"},
	                               {{"y", AtomicClockConstraintT<std::greater<automata::Time>>(2)}}));
	ta.add_transition(TATransition(Location{"l0"},
	                               "b",
	                               Location{"l2"},
	                               {{"x", AtomicClockConstraintT<std::greater<automata::Time>>(1)}},
	                               {"x"}));
	ta.add_transition(TATransition(Location{"l1"}, "a", Location{"l1"}));
	ta.add_transition(TATransition(Location{"l2"}, "a", Location{"l2"}));
	ta.add_transition(TATransition(Location{"l1"}, "b", Location{"l1"}));
	ta.add_transition(TATransition(Location{"l2"}, "b", Location{"l2"}));
	logic::MTLFormula<std::string> a{AP("a")};
	logic::MTLFormula<std::string> b{AP("b")};

	logic::MTLFormula f   = a.until(b, logic::TimeInterval(2, BoundType::WEAK, 2, BoundType::INFTY));
	auto              ata = mtl_ata_translation::translate(f);
	TreeSearch        search(&ta, &ata, {"a"}, {"b"}, 2);
	search.build_tree();
	search.label();
	INFO("Tree:\n" << *search.get_root());
	INFO("Tree size: " << search.get_size());
	CHECK(false);
}

TEST_CASE("Invoke incremental labelling on a trivial example", "[search]")
{
	spdlog::set_level(spdlog::level::trace);
	TA ta{{"e0", "e1", "c"}, Location{"l0"}, {Location{"l1"}, Location{"l2"}}};
	ta.add_clock("x");
	ta.add_transition(TATransition(Location{"l1"}, "e0", Location{"l1"}));
	ta.add_transition(TATransition(Location{"l2"}, "e1", Location{"l2"}));
	ta.add_transition(
	  TATransition(Location{"l0"},
	               "c",
	               Location{"l1"},
	               {{"x", AtomicClockConstraintT<std::greater_equal<automata::Time>>(1)}}));
	ta.add_transition(TATransition(Location{"l0"},
	                               "e1",
	                               Location{"l2"},
	                               {{"x", AtomicClockConstraintT<std::greater<automata::Time>>(1)}}));
	logic::MTLFormula<std::string> e0{AP("e0")};
	logic::MTLFormula<std::string> e1{AP("e1")};
	logic::MTLFormula<std::string> c{AP("c")};

	logic::MTLFormula f   = c.until(e1, logic::TimeInterval(2, BoundType::WEAK, 2, BoundType::INFTY));
	auto              ata = mtl_ata_translation::translate(f);
	TreeSearch        search_incremental(&ta, &ata, {"c"}, {"e0", "e1"}, 2, true);
	TreeSearch        search(&ta, &ata, {"c"}, {"e0", "e1"}, 2, false);
	search.build_tree(false);
	search.label();
	search_incremental.build_tree(false);
	INFO("Tree:\n" << *search.get_root());
	// check trees for equivalence
	CHECK(search.get_root()->label == search_incremental.get_root()->label);
	// TODO Fix tests that used to use the tree iterator
	// auto searchTreeIt            = search.get_root()->begin();
	// auto searchTreeIncrementalIt = search_incremental.get_root()->begin();
	// while (searchTreeIt != search.get_root()->end()) {
	//	CHECK(*searchTreeIt == *searchTreeIncrementalIt);
	//	++searchTreeIt;
	//	++searchTreeIncrementalIt;
	//}
}

TEST_CASE("Incremental labeling: Simultaneous good and bad action", "[search]")
{
	spdlog::set_level(spdlog::level::trace);
	TA ta{{"e", "e_bad", "c"}, Location{"l0"}, {Location{"l1"}, Location{"l2"}}};
	ta.add_clock("x");
	ta.add_transition(TATransition(Location{"l0"}, "e", Location{"l1"}));
	ta.add_transition(TATransition(Location{"l1"}, "e_bad", Location{"l1"}));
	ta.add_transition(TATransition(Location{"l0"}, "c", Location{"l2"}));
	logic::MTLFormula spec =
	  logic::MTLFormula{logic::MTLFormula<std::string>::TRUE().until(AP{"e_bad"})};
	auto ata = mtl_ata_translation::translate(spec, {AP{"e"}, AP{"e_bad"}, AP{"c"}});
	INFO("TA:\n" << ta);
	INFO("ATA:\n" << ata);
	TreeSearch search{&ta, &ata, {"c"}, {"e", "e_bad"}, 1, false};
	TreeSearch search_incremental{&ta, &ata, {"c"}, {"e", "e_bad"}, 1, true};
	search.build_tree(false);
	search.label();
	search_incremental.build_tree(false);
	INFO("Full tree:\n" << *search.get_root());
	INFO("Inc  tree:\n" << *search_incremental.get_root());
	// TODO Fix tests that used to use the tree iterator
	// auto searchTreeIt            = search.get_root()->begin();
	// auto searchTreeIncrementalIt = search_incremental.get_root()->begin();
	// CHECK(search_incremental.get_root()->label == NodeLabel::BOTTOM);
	// while (searchTreeIt != search.get_root()->end()) {
	//	CHECK(*searchTreeIt == *searchTreeIncrementalIt);
	//	++searchTreeIt;
	//	++searchTreeIncrementalIt;
	//}
}

TEST_CASE("Single-step incremental labeling on constructed cases", "[search]")
{
	spdlog::set_level(spdlog::level::trace);
	using ActionType = std::string;

	logic::MTLFormula<std::string> a{AP("a")};
	logic::MTLFormula<std::string> b{AP("b")};

	std::set<ActionType> controller_actions{"a", "b", "c"};
	std::set<ActionType> environment_actions{"x", "y", "z"};

	// root node
	auto root = create_test_node();
	// create children
	auto ch1 = create_test_node(dummyWords());
	auto ch2 = create_test_node(dummyWords());
	auto ch3 = create_test_node(dummyWords());
	// set child labels
	ch1->label = NodeLabel::TOP;
	ch2->label = NodeLabel::BOTTOM;
	ch3->label = NodeLabel::BOTTOM;
	// add children to tree
	root->add_child({0, "a"}, ch1);
	root->add_child({1, "x"}, ch2);
	root->add_child({2, "x"}, ch3);

	SECTION("Label tree: single-step propagate")
	{
		// call to propagate on any child should assign a label TOP to root
		ch2->label_propagate(controller_actions, environment_actions);
		CHECK(root->label == NodeLabel::TOP);
	}

	SECTION("Label tree: single-step propagate with bad controller action")
	{
		ch1->label = NodeLabel::BOTTOM;
		ch2->label = NodeLabel::TOP;
		ch3->label = NodeLabel::TOP;
		// call to propagate on any child should assign a label TOP to root because all
		// environmental actions are good
		SPDLOG_TRACE("START TEST");
		ch2->label_propagate(controller_actions, environment_actions);
		SPDLOG_TRACE("END TEST");
		CHECK(root->label == NodeLabel::TOP);
	}

	SECTION("Label tree: single-step propagate with bad environment action")
	{
		ch1->label = NodeLabel::BOTTOM;
		ch2->label = NodeLabel::TOP;
		ch3->label = NodeLabel::BOTTOM;
		// call to propagate on any child should assign a label BOTTOM to root because not all
		// environmental actions are good
		ch2->label_propagate(controller_actions, environment_actions);
		CHECK(root->label == NodeLabel::BOTTOM);
	}

	// make the controller action the second one to be executable, reset tree
	root.reset();
	root = create_test_node();
	root->add_child({0, "x"}, ch1);
	root->add_child({1, "a"}, ch2);
	root->add_child({2, "z"}, ch3);

	SECTION("Label tree: single-step propagate with late controller action")
	{
		ch1->label = NodeLabel::TOP;
		ch2->label = NodeLabel::TOP;
		ch3->label = NodeLabel::BOTTOM;
		ch2->label_propagate(controller_actions, environment_actions);
		CHECK(root->label == NodeLabel::TOP);
	}

	SECTION("Label tree: single-step propagate with late controller action and bad env action")
	{
		// next case: first environmental action is bad
		ch1->label = NodeLabel::BOTTOM;
		ch2->label = NodeLabel::TOP;
		ch3->label = NodeLabel::BOTTOM;
		ch2->label_propagate(controller_actions, environment_actions);
		CHECK(root->label == NodeLabel::BOTTOM);
	}
}

TEST_CASE("Multi-step incremental labeling on constructed cases", "[search]")
{
	spdlog::set_level(spdlog::level::trace);
	using ActionType = std::string;
	std::set<ActionType> controller_actions{"a", "b", "c"};
	std::set<ActionType> environment_actions{"x", "y", "z"};

	// root node
	auto root = create_test_node();
	// create children
	auto ch1 = create_test_node(dummyWords(0));
	auto ch2 = create_test_node(dummyWords(1));
	auto ch3 = create_test_node(dummyWords(2));
	// add children to root node
	root->add_child({0, "a"}, ch1);
	root->add_child({1, "x"}, ch2);
	root->add_child({2, "x"}, ch3);

	// add second layer of children to make the first child ch1 an intermediate node
	auto ch11 = create_test_node(dummyWords(3));
	auto ch12 = create_test_node(dummyWords(4));
	// Add to ch1.
	ch1->add_child({0, "a"}, ch11);
	ch1->add_child({1, "x"}, ch12);

	SECTION("First good case")
	{
		// set child labels
		ch1->label  = NodeLabel::UNLABELED;
		ch2->label  = NodeLabel::BOTTOM;
		ch3->label  = NodeLabel::BOTTOM;
		ch11->label = NodeLabel::BOTTOM;
		ch12->label = NodeLabel::TOP;
		// call to propagate on any child ch11, ch12 should assign a label TOP to ch1 and root should
		// be labelled TOP as well
		ch11->label_propagate(controller_actions, environment_actions);
		CHECK(ch1->label == NodeLabel::TOP);
		CHECK(root->label == NodeLabel::TOP);
	}

	SECTION("Second good case")
	{
		// label ch11 as good and ch12 as bad.
		root->label = NodeLabel::UNLABELED;
		ch1->label  = NodeLabel::UNLABELED;
		ch2->label  = NodeLabel::BOTTOM;
		ch3->label  = NodeLabel::BOTTOM;
		ch11->label = NodeLabel::TOP;
		ch12->label = NodeLabel::BOTTOM;
		// call to propagate on any child ch11, ch11 should assign a label TOP to ch1 and root should
		// be labelled TOP as well
		ch11->label_propagate(controller_actions, environment_actions);
		CHECK(ch1->label == NodeLabel::TOP);
		CHECK(root->label == NodeLabel::TOP);
	}

	SECTION("First bad case")
	{
		// label ch11 and ch12 as bad.
		root->label = NodeLabel::UNLABELED;
		ch1->label  = NodeLabel::UNLABELED;
		ch2->label  = NodeLabel::TOP;
		ch3->label  = NodeLabel::BOTTOM;
		ch11->label = NodeLabel::BOTTOM; // new
		ch12->label = NodeLabel::BOTTOM; // new
		visualization::search_tree_to_graphviz(*root).render_to_file("search_propagate_bad_start.png");
		// call propagate, root should be labelled as bad
		ch11->label_propagate(controller_actions, environment_actions);
		visualization::search_tree_to_graphviz(*root).render_to_file("search_propagate_bad.png");
		CHECK(ch1->label == NodeLabel::BOTTOM);
		CHECK(root->label == NodeLabel::BOTTOM);
	}

	SECTION("No labeling")
	{
		// reset tree, this time we keep the labels as before but add child nodes to ch2. In this
		// case, propagation should not allow the root node to be labelled.
		root->label = NodeLabel::UNLABELED;
		ch1->label  = NodeLabel::UNLABELED;
		ch2->label  = NodeLabel::UNLABELED; // new
		ch3->label  = NodeLabel::TOP;
		ch11->label = NodeLabel::BOTTOM;
		ch12->label = NodeLabel::BOTTOM;
		auto ch13   = create_test_node(dummyWords(6));
		ch13->label = NodeLabel::TOP;
		ch2->add_child({0, "a"}, ch13);
		visualization::search_tree_to_graphviz(*root).render_to_file(
		  "search_propagate_no_label_start.png");
		// call to propagate on ch11 or ch12 should render ch1 as bottom but root should be unlabeled.
		ch11->label_propagate(controller_actions, environment_actions);
		visualization::search_tree_to_graphviz(*root).render_to_file(
		  "search_propagate_no_label_intermediate.png");
		CHECK(ch1->label == NodeLabel::BOTTOM);
		CHECK(root->label == NodeLabel::UNLABELED);
		// a call to label propagate on ch6 should resolve all uncertainties and ch2 should be
		// labelled with top and root with top (due to the existence of ch3, which is good).
		ch13->label_propagate(controller_actions, environment_actions);
		visualization::search_tree_to_graphviz(*root).render_to_file("search_propagate_no_label.png");
		CHECK(ch2->label == NodeLabel::TOP);
		CHECK(root->label == NodeLabel::TOP);
	}
}

TEST_CASE("Incremental labeling on tree without non-good/bad environment actions", "[search]")
{
	spdlog::set_level(spdlog::level::trace);
	TA ta{{"c", "e"}, Location{"l0"}, {Location{"l0"}, Location{"l1"}}};
	ta.add_clock("x");
	// ta.add_transition(TATransition(Location{"l0"}, "c", Location{"l0"}));
	ta.add_transition(TATransition(Location{"l0"}, "c", Location{"l1"}));
	ta.add_transition(TATransition(Location{"l1"}, "c", Location{"l1"}));
	auto ata = mtl_ata_translation::translate(logic::MTLFormula<std::string>::TRUE().until(AP{"c"}),
	                                          {AP{"c"}, AP{"e"}});
	INFO("TA:\n" << ta);
	INFO("ATA:\n" << ata);
	TreeSearch search(&ta, &ata, {"c"}, {"e"}, 0);
	TreeSearch search_incremental(&ta, &ata, {"c"}, {"e"}, 0, true);
	search.build_tree(false);
	search.label();
	search_incremental.build_tree(false);
	INFO("Full tree:\n" << *search.get_root());
	INFO("Inc  tree:\n" << *search_incremental.get_root());
	CHECK(search.get_root()->label == NodeLabel::TOP);
	CHECK(search_incremental.get_root()->label == NodeLabel::TOP);
}

TEST_CASE("Search on a specification that gets unsatisfiable", "[search]")
{
	TA ta{{Location{"l0"}, Location{"l1"}},
	      {"c", "e"},
	      Location{"l0"},
	      {Location{"l1"}},
	      {"c"},
	      {{TATransition(Location{"l0"}, "c", Location{"l1"})}}};
	using AP       = logic::AtomicProposition<std::string>;
	auto       ata = mtl_ata_translation::translate(logic::MTLFormula{AP{"e"}}, {AP{"c"}, AP{"e"}});
	TreeSearch search{&ta, &ata, {"c"}, {"e"}, 0, true};
	search.build_tree(false);
	visualization::search_tree_to_graphviz(*search.get_root(), false)
	  .render_to_file("search_incremental.png");

	// The controller can directly choose to do 'c', which makes the specification unsatisfiable.
	CHECK(search.get_root()->label == NodeLabel::TOP);
}

TEST_CASE("Check a node for unsatisfiable ATA configurations", "[search]")
{
	using search::has_satisfiable_ata_configuration;
	logic::MTLFormula<std::string> a{AP("a")};
	logic::MTLFormula<std::string> sink{AP("sink")};
	CHECK(search::has_satisfiable_ata_configuration(
	  Node{{CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}}, {ATARegionState{a, 0}}})}}));
	CHECK(!search::has_satisfiable_ata_configuration(
	  Node{{CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}}, {ATARegionState{sink, 0}}})}}));
	CHECK(!search::has_satisfiable_ata_configuration(Node{{CanonicalABWord(
	  {{TARegionState{Location{"l0"}, "x", 0}, ATARegionState{a, 0}}, {ATARegionState{sink, 0}}})}}));
	CHECK(search::has_satisfiable_ata_configuration(
	  Node{{CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}, ATARegionState{a, 0}}}),
	        CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}, ATARegionState{a, 0}}})}}));
}

TEST_CASE("Search graph", "[search]")
{
}

} // namespace
