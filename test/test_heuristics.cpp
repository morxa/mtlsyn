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

using Node = search::SearchTreeNode<std::string, std::string>;

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

	Node root{{}, nullptr, {}};
	CHECK(h.compute_cost(&root) == 0);
	Node c1{{}, &root, {{1, "a1"}}};
	CHECK(h.compute_cost(&c1) == 1);
	Node c2{{}, &root, {{3, "a1"}, {4, "b"}}};
	CHECK(h.compute_cost(&c2) == 3);
	Node cc1{{}, &c1, {{2, "a"}, {4, "a"}}};
	CHECK(h.compute_cost(&cc1) == 3);
	Node cc2{{}, &c2, {{2, "a"}, {4, "a"}}};
	CHECK(h.compute_cost(&cc2) == 5);
}

TEST_CASE("Test PreferEnvironmentActionHeuristic", "[search][heuristics]")
{
	search::PreferEnvironmentActionHeuristic<long, std::string, std::string> h{
	  std::set<std::string>{"environment_action"}};
	Node root{{}, nullptr, {}};
	Node n1{{}, &root, {{0, "environment_action"}}};
	CHECK(h.compute_cost(&n1) == 0);
	Node n2{{}, &root, {{0, "controller_action"}}};
	CHECK(h.compute_cost(&n2) == 1);
	Node n3{{}, &root, {{0, "environment_action"}, {1, "controller_action"}}};
	CHECK(h.compute_cost(&n3) == 0);
}

TEST_CASE("Test NumCanonicalWordsHeuristic", "[search][heuristics]")
{
	using CanonicalABWord = search::CanonicalABWord<std::string, std::string>;
	using TARegionState   = search::TARegionState<std::string>;
	using ATARegionState  = search::ATARegionState<std::string>;
	using Location        = automata::ta::Location<std::string>;
	search::NumCanonicalWordsHeuristic<long, std::string, std::string> h{};
	Node root{{}, nullptr, {}};
	Node n1{{CanonicalABWord{{TARegionState{Location{"l"}, "c", 0}}}}, &root, {{1, "a"}}};
	CHECK(h.compute_cost(&n1) == 1);
	Node n2{{CanonicalABWord{{TARegionState{Location{"l"}, "c1", 0}},
	                         {TARegionState{Location{"l"}, "c2", 1}}}},
	        &root,
	        {{1, "a"}}};
	CHECK(h.compute_cost(&n2) == 1);
	const logic::MTLFormula f{logic::AtomicProposition<std::string>{"a"}};
	Node                    n3{{CanonicalABWord{{TARegionState{Location{"l1"}, "c", 0}}},
           CanonicalABWord{{ATARegionState{f, 0}, TARegionState{Location{"l1"}, "c", 0}}}},
          &root,
          {{1, "a"}}};
	CHECK(h.compute_cost(&n3) == 2);
}

TEST_CASE("Test CompositeHeuristic", "[search][heuristics]")
{
	Node root{{}, nullptr, {}};
	Node n1{{}, &root, {{0, "environment_action"}}};
	Node n2{{}, &root, {{1, "controller_action"}}};
	Node n3{{}, &root, {{2, "environment_action"}, {3, "controller_action"}}};
	auto w_time = GENERATE(0, 1, 10);
	auto w_env  = GENERATE(0, 1, 10);
	SECTION(fmt::format("w_time={}, w_env={}", w_time, w_env))
	{
		std::vector<
		  std::pair<long,
		            std::unique_ptr<search::Heuristic<long, std::string, std::string>>>>
		  heuristics;
		heuristics.emplace_back(
		  w_time,
		  std::make_unique<search::TimeHeuristic<long, std::string, std::string>>());
		heuristics.emplace_back(
		  w_env,
		  std::make_unique<
		    search::PreferEnvironmentActionHeuristic<long, std::string, std::string>>(
		    std::set<std::string>{"environment_action"}));
		search::CompositeHeuristic<long, std::string, std::string> h{
		  std::move(heuristics)};
		CHECK(h.compute_cost(&n1) == 0);
		CHECK(h.compute_cost(&n2) == w_time * 1 + w_env * 1);
		CHECK(h.compute_cost(&n3) == w_time * 2);
	}
}

} // namespace
