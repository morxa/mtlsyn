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

#include <memory>
#include <string>
#include <utility>
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include "mtl/MTLFormula.h"
#include "mtl_ata_translation/translator.h"
#include "search/search.h"
#include "search/search_tree.h"
#include "search/synchronous_product.h"

#include <spdlog/spdlog.h>

#include <catch2/catch_test_macros.hpp>

namespace {

using TreeSearch      = search::TreeSearch<std::string, std::string>;
using TATransition    = automata::ta::Transition<std::string, std::string>;
using TA              = automata::ta::TimedAutomaton<std::string, std::string>;
using TAConfiguration = automata::ta::Configuration<std::string>;
using TreeSearch      = search::TreeSearch<std::string, std::string>;
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
		CHECK(search.get_root()->parent == nullptr);
		CHECK(search.get_root()->incoming_actions.empty());
		CHECK(search.get_root()->children.empty());
	}

	SECTION("The first step computes the right children")
	{
		REQUIRE(search.step());
		const auto &children = search.get_root()->children;
		INFO("Tree:\n" << search::node_to_string(*search.get_root(), true));
		REQUIRE(children.size() == 3);
		CHECK(children[0]->words
		      == std::set{
		        CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}}, {ATARegionState{spec, 3}}}),
		        CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}, ATARegionState{spec, 4}}}),
		        CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}}, {ATARegionState{spec, 5}}})});
		CHECK(children[0]->incoming_actions
		      == std::set<std::pair<RegionIndex, std::string>>{{3, "a"}, {4, "a"}, {5, "a"}});
		CHECK(children[1]->words
		      == std::set{
		        CanonicalABWord({{TARegionState{Location{"l1"}, "x", 0}, ATARegionState{spec, 0}}})});
		CHECK(children[1]->incoming_actions == std::set<std::pair<RegionIndex, std::string>>{{0, "b"}});
		CHECK(children[2]->words
		      == std::set{
		        CanonicalABWord({{TARegionState{Location{"l1"}, "x", 1}, ATARegionState{spec, 1}}})});
		CHECK(children[2]->incoming_actions == std::set<std::pair<RegionIndex, std::string>>{{1, "b"}});
	}

	SECTION("The next steps compute the right children")
	{
		REQUIRE(search.step());
		INFO("Tree:\n" << search::node_to_string(*search.get_root(), true));
		REQUIRE(search.step());
		INFO("Tree:\n" << search::node_to_string(*search.get_root(), true));
		const auto &root_children = search.get_root()->children;
		REQUIRE(root_children.size() == std::size_t(3));

		{
			// Process first child of the root.
			// starts with [{(l0, x, 0), ((a U b), 3)}]
			const auto &children = root_children[0]->children;
			REQUIRE(children.size() == std::size_t(3));
			CHECK(children[0]->words
			      == std::set{CanonicalABWord(
			        {{TARegionState{Location{"l0"}, "x", 0}}, {ATARegionState{spec, 5}}})});
			CHECK(children[0]->incoming_actions
			      == std::set<std::pair<RegionIndex, std::string>>{{3, "a"}, {4, "a"}, {5, "a"}});
			CHECK(children[1]->words
			      == std::set{CanonicalABWord({{TARegionState{Location{"l1"}, "x", 0}}}),
			                  CanonicalABWord({{TARegionState{Location{"l1"}, "x", 0},
			                                    ATARegionState{logic::MTLFormula{AP{"sink"}}, 0}}})});
			CHECK(children[1]->incoming_actions
			      == std::set<std::pair<RegionIndex, std::string>>{{0, "b"}});
			CHECK(children[2]->words
			      == std::set{CanonicalABWord({{TARegionState{Location{"l1"}, "x", 1}}})});
			CHECK(children[2]->incoming_actions
			      == std::set<std::pair<RegionIndex, std::string>>{{1, "b"}});
			CHECK(root_children[0]->state == NodeState::UNKNOWN);
		}

		// Process second child of the root.
		REQUIRE(search.step());
		INFO("Tree:\n" << *search.get_root());
		CHECK(root_children[1]->children.empty()); // should be ({(l1, x, 0), ((a U b), 0)})
		// the node has no time-symbol successors (only time successors)
		CHECK(root_children[1]->state == NodeState::DEAD);

		// Process third child of the root.
		REQUIRE(search.step());
		INFO("Tree:\n" << *search.get_root());
		REQUIRE(root_children[2]->children.empty()); // should be ({(l1, x, 1), ((a U b), 1)})
		// the node has no time-symbol successors (only time successors)
		REQUIRE(root_children[2]->state == NodeState::DEAD);
	}

	SECTION("Compute the final tree")
	{
		// We do exactly 7 steps.
		for (size_t i = 0; i < 7; i++) {
			REQUIRE(search.step());
		}
		CHECK(!search.step());
		search.label();

		INFO("Tree:\n" << node_to_string(*search.get_root(), true));
		CHECK(search.get_root()->children.size() == 3);
		CHECK(search.get_root()->children[0]->children.size() == 3);
		CHECK(search.get_root()->children[1]->children.size() == 0);
		CHECK(search.get_root()->children[2]->children.size() == 0);
		CHECK(search.get_root()->children[0]->children[0]->children.size() == 0);
		CHECK(search.get_root()->children[0]->children[1]->children.size() == 0);
		CHECK(search.get_root()->children[0]->children[2]->children.size() == 0);

		CHECK(search.get_root()->state == NodeState::UNKNOWN);
		CHECK(search.get_root()->children[0]->state == NodeState::UNKNOWN);
		CHECK(search.get_root()->children[1]->state == NodeState::DEAD);
		CHECK(search.get_root()->children[2]->state == NodeState::DEAD);
		CHECK(search.get_root()->children[0]->children[0]->state == NodeState::GOOD);
		CHECK(search.get_root()->children[0]->children[1]->state == NodeState::BAD);
		CHECK(search.get_root()->children[0]->children[2]->state == NodeState::BAD);

		CHECK(search.get_root()->label == NodeLabel::TOP);
		CHECK(search.get_root()->children[0]->label == NodeLabel::BOTTOM);
		CHECK(search.get_root()->children[1]->label == NodeLabel::TOP);
		CHECK(search.get_root()->children[2]->label == NodeLabel::TOP);
		CHECK(search.get_root()->children[0]->children[0]->label == NodeLabel::TOP);
		CHECK(search.get_root()->children[0]->children[1]->label == NodeLabel::BOTTOM);
		CHECK(search.get_root()->children[0]->children[2]->label == NodeLabel::BOTTOM);
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
		auto searchTreeIt            = search.get_root()->begin();
		auto searchTreeIncrementalIt = search_incremental_labeling.get_root()->begin();
		while (searchTreeIt != search.get_root()->end()) {
			CHECK(*searchTreeIt == *searchTreeIncrementalIt);
			++searchTreeIt;
			++searchTreeIncrementalIt;
		}
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
	auto searchTreeIt            = search.get_root()->begin();
	auto searchTreeIncrementalIt = search_incremental.get_root()->begin();
	while (searchTreeIt != search.get_root()->end()) {
		CHECK(*searchTreeIt == *searchTreeIncrementalIt);
		++searchTreeIt;
		++searchTreeIncrementalIt;
	}
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
	auto searchTreeIt            = search.get_root()->begin();
	auto searchTreeIncrementalIt = search_incremental.get_root()->begin();
	CHECK(search_incremental.get_root()->label == NodeLabel::BOTTOM);
	while (searchTreeIt != search.get_root()->end()) {
		CHECK(*searchTreeIt == *searchTreeIncrementalIt);
		++searchTreeIt;
		++searchTreeIncrementalIt;
	}
}

TEST_CASE("Incremental labeling on constructed cases", "[search]")
{
	spdlog::set_level(spdlog::level::trace);
	using ActionType = std::string;
	using Node       = search::SearchTreeNode<std::string, ActionType>;

	logic::MTLFormula<std::string> a{AP("a")};
	logic::MTLFormula<std::string> b{AP("b")};
	auto                           dummyWords = std::set<CanonicalABWord>{
    CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}}, {ATARegionState{a.until(b), 0}}})};
	std::set<ActionType> controller_actions{"a", "b", "c"};
	std::set<ActionType> environment_actions{"x", "y", "z"};

	auto create_test_node = [](const std::set<CanonicalABWord> &words, Node *parent = nullptr) {
		auto node         = std::make_unique<Node>(words);
		node->parent      = parent;
		node->is_expanded = true;
		return node;
	};

	SECTION("Label tree: single-step propagate")
	{
		// root node
		auto root = create_test_node(dummyWords);
		// create children
		auto ch1 = create_test_node(dummyWords, root.get());
		auto ch2 = create_test_node(dummyWords, root.get());
		auto ch3 = create_test_node(dummyWords, root.get());
		// set incoming actions
		ch1->incoming_actions.emplace(std::make_pair(0, *controller_actions.begin()));
		ch2->incoming_actions.emplace(std::make_pair(1, *environment_actions.begin()));
		ch3->incoming_actions.emplace(std::make_pair(2, *environment_actions.begin()));
		// set child labels
		ch1->label = NodeLabel::TOP;
		ch2->label = NodeLabel::BOTTOM;
		ch3->label = NodeLabel::BOTTOM;
		// add children to tree
		root->children.emplace_back(std::move(ch1));
		root->children.emplace_back(std::move(ch2));
		root->children.emplace_back(std::move(ch3));
		// call to propagate on any child should assign a label TOP to root
		root->children[1]->label_propagate(controller_actions, environment_actions);
		CHECK(root->label == NodeLabel::TOP);
		// reset tree, this time label child 1 as Bottom
		auto resetTreeLabels = [&root] {
			auto it = root->begin();
			while (it != root->end()) {
				it->label = NodeLabel::UNLABELED;
				++it;
			}
		};
		auto resetIncomingActions = [&root] {
			auto it = root->begin();
			while (it != root->end()) {
				it->incoming_actions = std::set<std::pair<RegionIndex, std::string>>{};
				++it;
			}
		};
		resetTreeLabels();
		root->children[0]->label = NodeLabel::BOTTOM;
		root->children[1]->label = NodeLabel::TOP;
		root->children[2]->label = NodeLabel::TOP;
		// call to propagate on any child should assign a label TOP to root because all
		// environmental actions are good
		SPDLOG_TRACE("START TEST");
		root->children[1]->label_propagate(controller_actions, environment_actions);
		SPDLOG_TRACE("END TEST");
		CHECK(root->label == NodeLabel::TOP);
		// reset tree, next case: one of the environmental actions is bad
		resetTreeLabels();
		root->children[0]->label = NodeLabel::BOTTOM;
		root->children[1]->label = NodeLabel::TOP;
		root->children[2]->label = NodeLabel::BOTTOM;
		// call to propagate on any child should assign a label BOTTOM to root because not all
		// environmental actions are good
		root->children[1]->label_propagate(controller_actions, environment_actions);
		CHECK(root->label == NodeLabel::BOTTOM);
		// make the controller action the second one to be executable, reset tree
		resetTreeLabels();
		resetIncomingActions();
		root->children[0]->incoming_actions.emplace(std::make_pair(0, "x"));
		root->children[1]->incoming_actions.emplace(std::make_pair(1, "a"));
		root->children[2]->incoming_actions.emplace(std::make_pair(2, "z"));
		root->children[0]->label = NodeLabel::TOP;
		root->children[1]->label = NodeLabel::TOP;
		root->children[2]->label = NodeLabel::BOTTOM;
		root->children[1]->label_propagate(controller_actions, environment_actions);
		CHECK(root->label == NodeLabel::TOP);
		// next case: first environmental action is bad
		resetTreeLabels();
		root->children[0]->label = NodeLabel::BOTTOM;
		root->children[1]->label = NodeLabel::TOP;
		root->children[2]->label = NodeLabel::BOTTOM;
		root->children[1]->label_propagate(controller_actions, environment_actions);
		CHECK(root->label == NodeLabel::BOTTOM);
	}
	SECTION("Label tree: multi-step propagate")
	{
		// root node
		auto root = create_test_node(dummyWords);
		// utility functions
		auto resetTreeLabels = [&root] {
			auto it = root->begin();
			while (it != root->end()) {
				it->label = NodeLabel::UNLABELED;
				++it;
			}
		};
		// create children
		auto ch1 = create_test_node(dummyWords, root.get());
		auto ch2 = create_test_node(dummyWords, root.get());
		auto ch3 = create_test_node(dummyWords, root.get());
		// set incoming actions
		ch1->incoming_actions.emplace(std::make_pair(0, *controller_actions.begin()));
		ch2->incoming_actions.emplace(std::make_pair(1, *environment_actions.begin()));
		ch3->incoming_actions.emplace(std::make_pair(2, *environment_actions.begin()));
		// set child labels
		ch1->label = NodeLabel::UNLABELED;
		ch2->label = NodeLabel::BOTTOM;
		ch3->label = NodeLabel::BOTTOM;
		// add children to tree
		root->children.emplace_back(std::move(ch1));
		root->children.emplace_back(std::move(ch2));
		root->children.emplace_back(std::move(ch3));
		// add second layer of children to make the first child ch1 an intermediate node
		auto ch4   = create_test_node(dummyWords, root->children[0].get());
		auto ch5   = create_test_node(dummyWords, root->children[0].get());
		ch4->label = NodeLabel::BOTTOM;
		ch5->label = NodeLabel::TOP;
		ch4->incoming_actions.emplace(std::make_pair(0, *controller_actions.begin()));
		ch5->incoming_actions.emplace(std::make_pair(1, *environment_actions.begin()));
		root->children[0]->children.emplace_back(std::move(ch4));
		root->children[0]->children.emplace_back(std::move(ch5));
		// call to propagate on any child ch4, ch5 should assign a label TOP to ch1 and root should
		// be labelled TOP as well
		root->children[0]->children[0]->label_propagate(controller_actions, environment_actions);
		CHECK(root->children[0]->label == NodeLabel::TOP);
		CHECK(root->label == NodeLabel::TOP);
		// reset tree, this time label ch4 as good and ch5 as bad.
		resetTreeLabels();
		// set child labels
		root->label                           = NodeLabel::UNLABELED;
		root->children[0]->label              = NodeLabel::UNLABELED;
		root->children[1]->label              = NodeLabel::BOTTOM;
		root->children[2]->label              = NodeLabel::BOTTOM;
		root->children[0]->children[0]->label = NodeLabel::TOP;
		root->children[0]->children[1]->label = NodeLabel::BOTTOM;
		// call to propagate on any child ch4, ch4 should assign a label TOP to ch1 and root should
		// be labelled TOP as well
		root->children[0]->children[0]->label_propagate(controller_actions, environment_actions);
		CHECK(root->children[0]->label == NodeLabel::TOP);
		CHECK(root->label == NodeLabel::TOP);
		// reset tree, this time label ch4 and 5 as bad.
		resetTreeLabels();
		root->label                           = NodeLabel::UNLABELED;
		root->children[0]->label              = NodeLabel::UNLABELED;
		root->children[1]->label              = NodeLabel::TOP;
		root->children[2]->label              = NodeLabel::BOTTOM;
		root->children[0]->children[0]->label = NodeLabel::BOTTOM; // new
		root->children[0]->children[1]->label = NodeLabel::BOTTOM; // new
		// call propagate, root should be labelled as bad
		root->children[0]->children[0]->label_propagate(controller_actions, environment_actions);
		CHECK(root->children[0]->label == NodeLabel::BOTTOM);
		CHECK(root->label == NodeLabel::BOTTOM);
		// reset tree, this time we keep the labels as before but add child nodes to ch2. In this
		// case, propagation should not allow the root node to be labelled.
		resetTreeLabels();
		root->label                           = NodeLabel::UNLABELED;
		root->children[0]->label              = NodeLabel::UNLABELED;
		root->children[1]->label              = NodeLabel::UNLABELED; // new
		root->children[2]->label              = NodeLabel::TOP;
		root->children[0]->children[0]->label = NodeLabel::BOTTOM;
		root->children[0]->children[1]->label = NodeLabel::BOTTOM;
		auto ch6                              = create_test_node(dummyWords, root->children[1].get());
		ch6->label                            = NodeLabel::TOP;
		ch6->incoming_actions.emplace(std::make_pair(0, *environment_actions.begin()));
		root->children[1]->children.emplace_back(std::move(ch6));
		// call to propagate on ch4 or ch5 should render ch1 as bottom but root should be unlabeled.
		root->children[0]->children[0]->label_propagate(controller_actions, environment_actions);
		CHECK(root->children[0]->label == NodeLabel::BOTTOM);
		CHECK(root->label == NodeLabel::UNLABELED);
		// a call to label propagate on ch6 should resolve all uncertainties and ch2 should be
		// labelled with top and root with top (due to the existence of ch3, which is good).
		root->children[1]->children[0]->label_propagate(controller_actions, environment_actions);
		CHECK(root->children[1]->label == NodeLabel::TOP);
		CHECK(root->label == NodeLabel::TOP);
	}
}

TEST_CASE("Incremental labeling on tree without non-good/bad environment actions", "[search]")
{
	spdlog::set_level(spdlog::level::trace);
	TA ta{{"c", "e"}, Location{"l0"}, {Location{"l0"}, Location{"l1"}}};
	ta.add_clock("x");
	ta.add_transition(TATransition(Location{"l0"}, "c", Location{"l0"}));
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
	INFO("Tree:\n" << search::node_to_string(*search.get_root(), true));

	// The controller can directly choose to do 'c', which makes the specification unsatisfiable.
	CHECK(search.get_root()->label == NodeLabel::TOP);
}

TEST_CASE("Check a node for unsatisfiable ATA configurations", "[search]")
{
	using Node = search::SearchTreeNode<std::string, std::string>;
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

} // namespace
