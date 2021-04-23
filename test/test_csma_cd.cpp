/***************************************************************************
 *  test_csma_cd.cpp - An example imeplementing a CSMA/CD protocol
 *
 *  Created:   Thu  22 Apr 20:33:27 CET 2021
 *  Copyright  2021  Stefan Schupp <stefan.schupp@cs.rwth-aachen.de>
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

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG

#include "automata/automata.h"
#include "automata/ta.h"
#include "automata/ta_product.h"
#include "csma_cd.h"
#include "mtl/MTLFormula.h"
#include "mtl_ata_translation/translator.h"
#include "synchronous_product/create_controller.h"
#include "synchronous_product/heuristics.h"
#include "synchronous_product/search.h"
#include "synchronous_product/search_tree.h"
#include "visualization/ta_to_graphviz.h"
#include "visualization/tree_to_graphviz.h"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {

using Location   = automata::ta::Location<std::string>;
using TA         = automata::ta::TimedAutomaton<std::string, std::string>;
using Transition = automata::ta::Transition<std::string, std::string>;
using automata::Time;
using logic::TimeInterval;
using F  = logic::MTLFormula<std::string>;
using AP = logic::AtomicProposition<std::string>;
using synchronous_product::NodeLabel;
using TreeSearch = synchronous_product::TreeSearch<std::vector<std::string>, std::string>;

TEST_CASE("One process accesses the carrier", "[csma_cd]")
{
	spdlog::set_level(spdlog::level::trace);
	spdlog::set_pattern("%t %v");
	// parameters
	const Time sigma  = 1;
	const Time lambda = 1;
	// system
	const auto &[product, controller_actions, environment_actions] =
	  create_csma_cd_instance(1, sigma, lambda);
	std::set<AP> actions;
	std::set_union(std::begin(controller_actions),
	               std::end(controller_actions),
	               std::begin(environment_actions),
	               std::end(environment_actions),
	               std::inserter(actions, end(actions)));
	// implement invariants as part of the specification
	const auto send_1 = F{AP{"send_1"}};
	const auto busy_1 = F{AP{"busy_1"}};
	const auto cd     = F{AP{"cd"}};
	const auto end    = F{AP{"end_1"}};
	const auto prob   = F{AP{"prob_1"}};
	const auto done_1 = F{AP{"done_1"}};
	// invariant in the cd-location asks to leave before sigma time passes.
	// idea: a second send signal before sigma time passes enforces a collision-detection (cd)
	const auto cd_caused_by_sending_twice_within_sigma =
	  send_1 && (send_1.until(cd, TimeInterval(0, sigma)));
	// invariant in the transmission-location forces leaving before lambda time passes
	// idea: send U (end v cd) within lambda time
	const auto transmission_time_limit =
	  send_1.until(F::create_disjunction({end, cd}), TimeInterval(0, lambda));
	// invariant in the collision detection location enforces direct leaving
	// idea: as before via until
	const auto cd_immediate = cd.until(prob, TimeInterval(0, 0));
	// invariant: after probing (prob) either resend or busy happen immediately
	const auto cd_acts_immediately =
	  prob.until(F::create_disjunction({busy_1, send_1}), TimeInterval(0, 0));

	auto good_behavior = !cd_caused_by_sending_twice_within_sigma;
	good_behavior      = (good_behavior && !(transmission_time_limit));
	good_behavior      = (good_behavior && !(cd_immediate));
	good_behavior      = (good_behavior && !(cd_acts_immediately));
	// check if we can end up sending at least once
	auto ata = mtl_ata_translation::translate(good_behavior && !(finally(done_1)));
	auto K   = std::max(product.get_largest_constant(),
                    (good_behavior && !(finally(done_1))).get_largest_constant());
	INFO("TA: " << product);
	INFO("ATA: " << ata);
	TreeSearch search{
	  &product,
	  &ata,
	  controller_actions,
	  environment_actions,
	  2,
	  true,
	  true,
	  std::make_unique<
	    synchronous_product::TimeHeuristic<long, std::vector<std::string>, std::string>>()};

	search.build_tree(true);
	INFO("Tree:\n" << synchronous_product::node_to_string(*search.get_root(), true));
#ifdef HAVE_VISUALIZATION
	visualization::search_tree_to_graphviz(*search.get_root(), true).render_to_file("csma_cd_1.svg");
	visualization::ta_to_graphviz(product).render_to_file("csma_cd_1_ta.svg");
	visualization::ta_to_graphviz(controller_synthesis::create_controller(search.get_root(), K))
	  .render_to_file("csma_cd_1_controller.svg");
#endif
	CHECK(search.get_root()->label == NodeLabel::TOP);
}

TEST_CASE("Two processes access the carrier", "[csma_cd]")
{
	spdlog::set_level(spdlog::level::trace);
	spdlog::set_pattern("%t %v");
	// parameters
	const Time        sigma     = 1;
	const Time        lambda    = 1;
	const std::size_t processes = 2;
	// system
	const auto &[product, controller_actions, environment_actions] =
	  create_csma_cd_instance(2, sigma, lambda);
	std::set<AP> actions;
	std::set_union(std::begin(controller_actions),
	               std::end(controller_actions),
	               std::begin(environment_actions),
	               std::end(environment_actions),
	               std::inserter(actions, end(actions)));
	// implement invariants as part of the specification
	const auto     cd = F{AP{"cd"}};
	std::vector<F> good_behavior;
	for (std::size_t i = 1; i <= processes; ++i) {
		const auto send = F{AP{"send_" + std::to_string(i)}};
		const auto busy = F{AP{"busy_" + std::to_string(i)}};
		const auto end  = F{AP{"end_" + std::to_string(i)}};
		const auto prob = F{AP{"prob_" + std::to_string(i)}};
		const auto done = F{AP{"done_" + std::to_string(i)}};
		// invariant in the cd-location asks to leave before sigma time passes.
		// idea: a second send signal before sigma time passes enforces a collision-detection (cd)
		const auto cd_caused_by_sending_twice_within_sigma =
		  send && (send.until(cd, TimeInterval(0, sigma)));
		good_behavior.push_back(cd_caused_by_sending_twice_within_sigma);
		// invariant in the transmission-location forces leaving before lambda time passes
		// idea: send U (end v cd) within lambda time
		const auto transmission_time_limit =
		  send.until(F::create_disjunction({end, cd}), TimeInterval(0, lambda));
		good_behavior.push_back(transmission_time_limit);
		// invariant in the collision detection location enforces direct leaving
		// idea: as before via until
		const auto cd_immediate = cd.until(prob, TimeInterval(0, 0));
		good_behavior.push_back(cd_immediate);
		// invariant: after probing (prob) either resend or busy happen immediately
		const auto cd_acts_immediately =
		  prob.until(F::create_disjunction({busy, send}), TimeInterval(0, 0));
		good_behavior.push_back(cd_acts_immediately);
		// goal: reach done
		const auto finally_done = finally(done);
		good_behavior.push_back(finally_done);
	}
	auto negated_good_behavior = !good_behavior.front();
	for (std::size_t i = 1; i < good_behavior.size(); ++i) {
		negated_good_behavior = negated_good_behavior && !good_behavior[i];
	}
	// create ata
	auto ata = mtl_ata_translation::translate(negated_good_behavior);
	auto K   = std::max(product.get_largest_constant(), negated_good_behavior.get_largest_constant());
	INFO("TA: " << product);
	INFO("ATA: " << ata);
	TreeSearch search{
	  &product,
	  &ata,
	  controller_actions,
	  environment_actions,
	  2,
	  true,
	  true,
	  std::make_unique<
	    synchronous_product::TimeHeuristic<long, std::vector<std::string>, std::string>>()};

	search.build_tree(true);
	INFO("Tree:\n" << synchronous_product::node_to_string(*search.get_root(), true));
#ifdef HAVE_VISUALIZATION
	visualization::search_tree_to_graphviz(*search.get_root(), true)
	  .render_to_file("csma_cd_" + std::to_string(processes) + ".svg");
	visualization::ta_to_graphviz(product).render_to_file("csma_cd_" + std::to_string(processes)
	                                                      + "_ta.svg");
	visualization::ta_to_graphviz(controller_synthesis::create_controller(search.get_root(), K))
	  .render_to_file("csma_cd_" + std::to_string(processes) + "_controller.svg");
#endif
	// CHECK(search.get_root()->label == NodeLabel::TOP);
}

} // namespace
