/***************************************************************************
 *  test_heuristics.cpp - Test search tree heuristic functions
 *
 *  Created:   Tue 23 Mar 09:08:56 CET 2021
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
#include "automata/ta_regions.h"
#include "search/canonical_word.h"
#include "search/heuristics.h"
#include "search/search_tree.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace {

using Node            = search::SearchTreeNode<std::string, std::string>;
using CanonicalABWord = search::CanonicalABWord<std::string, std::string>;
using Location        = automata::ta::Location<std::string>;
using TARegionState   = search::TARegionState<std::string>;

TEST_CASE("Test BFS heuristic", "[search][heuristics]")
{
	search::BfsHeuristic<long, std::string, std::string> bfs{};
	// The heuristic does not care about the actual node, we can just give it nullptrs.
	long h1 = bfs.compute_cost(nullptr);
	long h2 = bfs.compute_cost(nullptr);
	long h3 = bfs.compute_cost(nullptr);
	CHECK(h1 < h2);
	CHECK(h2 < h3);
}
TEST_CASE("Test DFS heuristic", "[search][heuristics]")
{
	search::DfsHeuristic<long, std::string, std::string> dfs{};
	// The heuristic does not care about the actual node, we can just give it nullptrs.
	long h1 = dfs.compute_cost(nullptr);
	long h2 = dfs.compute_cost(nullptr);
	long h3 = dfs.compute_cost(nullptr);
	CHECK(h1 > h2);
	CHECK(h2 > h3);
}

TEST_CASE("Test time heuristic", "[search][heuristics]")
{
	spdlog::set_level(spdlog::level::debug);
	search::TimeHeuristic<long, std::string, std::string> h;

	const auto dummy_words =
	  std::set<CanonicalABWord>{CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}}})};
	auto root = std::make_shared<Node>(std::set<CanonicalABWord>{});
	CHECK(h.compute_cost(root.get()) == 0);
	auto c1 = std::make_shared<Node>(dummy_words, root.get());
	root->add_child({1, "a1"}, c1);
	CHECK(h.compute_cost(c1.get()) == 1);
	auto c2 = std::make_shared<Node>(dummy_words, root.get());
	root->add_child({3, "a1"}, c2);
	root->add_child({4, "b"}, c2);
	CHECK(h.compute_cost(c2.get()) == 3);
	auto cc1 = std::make_shared<Node>(dummy_words, c1.get());
	c1->add_child({2, "a"}, cc1);
	c1->add_child({4, "a"}, cc1);
	CHECK(h.compute_cost(cc1.get()) == 3);
	auto cc2 = std::make_shared<Node>(dummy_words, c2.get());
	c2->add_child({2, "a"}, cc2);
	c2->add_child({4, "a"}, cc2);
	CHECK(h.compute_cost(cc2.get()) == 5);
}

TEST_CASE("Test PreferEnvironmentActionHeuristic", "[search][heuristics]")
{
	search::PreferEnvironmentActionHeuristic<long, std::string, std::string> h{
	  std::set<std::string>{"e1", "e2"}};
	const auto dummy_words =
	  std::set<CanonicalABWord>{CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}}})};
	auto root = std::make_shared<Node>(std::set<CanonicalABWord>{});
	auto n1   = std::make_shared<Node>(dummy_words, root.get());
	root->add_child({0, "e1"}, n1);
	CHECK(h.compute_cost(n1.get()) == 0);
	auto n2 = std::make_shared<Node>(dummy_words, root.get());
	root->add_child({0, "c1"}, n2);
	CHECK(h.compute_cost(n2.get()) == 1);
	auto n3 = std::make_shared<Node>(dummy_words, root.get());
	root->add_child({0, "e2"}, n3);
	root->add_child({0, "c2"}, n3);
	CHECK(h.compute_cost(n3.get()) == 0);
}

TEST_CASE("Test NumCanonicalWordsHeuristic", "[search][heuristics]")
{
	using CanonicalABWord = search::CanonicalABWord<std::string, std::string>;
	using TARegionState   = search::TARegionState<std::string>;
	using ATARegionState  = search::ATARegionState<std::string>;
	using Location        = automata::ta::Location<std::string>;
	search::NumCanonicalWordsHeuristic<long, std::string, std::string> h{};
	auto root = std::make_shared<Node>(std::set<CanonicalABWord>{});
	auto n1 =
	  std::make_shared<Node>(std::set<CanonicalABWord>{{{TARegionState{Location{"l"}, "c", 0}}}},
	                         root.get());
	root->add_child({1, "a"}, n1);
	CHECK(h.compute_cost(n1.get()) == 1);
	auto n2 = std::make_shared<Node>(
	  std::set<CanonicalABWord>{{{CanonicalABWord{{TARegionState{Location{"l"}, "c1", 0}},
	                                              {TARegionState{Location{"l"}, "c2", 1}}}}}},
	  root.get());
	root->add_child({1, "b"}, n2);
	CHECK(h.compute_cost(n2.get()) == 1);
	const logic::MTLFormula f{logic::AtomicProposition<std::string>{"a"}};
	auto                    n3 =
	  std::make_shared<Node>(std::set{CanonicalABWord{{TARegionState{Location{"l1"}, "c", 0}}},
	                                  CanonicalABWord{{ATARegionState{f, 0},
	                                                   TARegionState{Location{"l1"}, "c", 0}}}},
	                         root.get());
	root->add_child({1, "c"}, n3);
	CHECK(h.compute_cost(n3.get()) == 2);
}

TEST_CASE("Test CompositeHeuristic", "[search][heuristics]")
{
	auto       root = std::make_shared<Node>(std::set<CanonicalABWord>{});
	const auto dummy_words =
	  std::set<CanonicalABWord>{CanonicalABWord({{TARegionState{Location{"l0"}, "x", 0}}})};
	auto n1 = std::make_shared<Node>(dummy_words, root.get());
	root->add_child({0, "environment_action"}, n1);
	auto n2 = std::make_shared<Node>(dummy_words, root.get());
	root->add_child({1, "controller_action"}, n2);
	auto n3 = std::make_shared<Node>(dummy_words, root.get());
	root->add_child({2, "environment_action"}, n3);
	root->add_child({3, "controller_action"}, n3);
	auto w_time = GENERATE(0, 1, 10);
	auto w_env  = GENERATE(0, 1, 10);
	SECTION(fmt::format("w_time={}, w_env={}", w_time, w_env))
	{
		std::vector<std::pair<long, std::unique_ptr<search::Heuristic<long, std::string, std::string>>>>
		  heuristics;
		heuristics.emplace_back(
		  w_time, std::make_unique<search::TimeHeuristic<long, std::string, std::string>>());
		heuristics.emplace_back(
		  w_env,
		  std::make_unique<search::PreferEnvironmentActionHeuristic<long, std::string, std::string>>(
		    std::set<std::string>{"environment_action"}));
		search::CompositeHeuristic<long, std::string, std::string> h{std::move(heuristics)};
		CHECK(h.compute_cost(n1.get()) == 0);
		CHECK(h.compute_cost(n2.get()) == w_time * 1 + w_env * 1);
		CHECK(h.compute_cost(n3.get()) == w_time * 2);
	}
}

} // namespace
