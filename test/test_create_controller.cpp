/***************************************************************************
 *  test_create_controller.cpp - Test the created controller
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

#define SPDLOG_ACTIVE_LEVEL SPDLOG_TRACE

#include "automata/automata.h"
#include "automata/ta.h"
#include "automata/ta_product.h"
#include "automata/ta_regions.h"
#include "mtl/MTLFormula.h"
#include "mtl_ata_translation/translator.h"
#include "railroad.h"
#include "search/canonical_word.h"
#include "search/create_controller.h"
#include "search/search.h"
#include "search/search_tree.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#ifdef HAVE_VISUALIZATION
#	include "visualization/ta_to_graphviz.h"
#	include "visualization/tree_to_graphviz.h"
#endif

#include <catch2/catch_test_macros.hpp>

namespace {

using F  = logic::MTLFormula<std::string>;
using AP = logic::AtomicProposition<std::string>;
using controller_synthesis::create_controller;
using search::NodeLabel;

using TA         = automata::ta::TimedAutomaton<std::string, std::string>;
using Transition = automata::ta::Transition<std::string, std::string>;
using Location   = automata::ta::Location<std::string>;

TEST_CASE("Create a simple controller", "[.][controller]")
{
	using L = automata::ta::Location<std::string>;
	spdlog::set_level(spdlog::level::trace);
	TA ta{{L{"l0"}},
	      {"c", "e"},
	      L{"l0"},
	      {L{"l0"}},
	      {"cc", "ce"},
	      {Transition{L{"l0"}, "c", L{"l0"}, {}, {"cc"}},
	       Transition{L{"l0"},
	                  "e",
	                  L{"l0"},
	                  {{"ce", automata::AtomicClockConstraintT<std::greater<automata::Time>>{1}}},
	                  {"ce"}}}};
	F  c{AP{"c"}};
	F  e{AP{"e"}};
	F  spec = finally(e);

	auto ata = mtl_ata_translation::translate(spec, {AP{"c"}, AP{"e"}});
	CAPTURE(ta);
	CAPTURE(spec);
	CAPTURE(ata);
	search::TreeSearch<std::string, std::string> search(&ta, &ata, {"c"}, {"e"}, 1, true, false);
	search.build_tree();
	// search.label();
#ifdef HAVE_VISUALIZATION
	visualization::search_tree_to_graphviz(*search.get_root()).render_to_file("simple_tree.svg");
	visualization::ta_to_graphviz(ta).render_to_file("simple_plant.svg");
#endif
	const auto controller = controller_synthesis::create_controller(search.get_root(), 2);
	CAPTURE(controller);
	visualization::ta_to_graphviz(controller, false).render_to_file("simple_controller.svg");
	CHECK(search.get_root()->label == search::NodeLabel::TOP);
}

TEST_CASE("Controller time bounds", "[.railroad][controller]")
{
	spdlog::set_level(spdlog::level::debug);
	TA         ta{{Location{"OPEN"}, Location{"OPENING"}, Location{"CLOSING"}, Location{"CLOSED"}},
        {"start_open", "finish_open", "start_close", "finish_close"},
        Location{"OPEN"},
        {Location{"CLOSED"}},
        {"c"},
        {Transition{Location{"OPEN"}, "start_close", Location{"CLOSING"}, {}, {"c"}},
         Transition{Location{"CLOSING"},
                    "finish_close",
                    Location{"CLOSED"},
                    {{"c", automata::AtomicClockConstraintT<std::equal_to<automata::Time>>{4}}},
                    {"c"}},
         Transition{Location{"CLOSED"},
                    "start_open",
                    Location{"OPENING"},
                    {{"c",
                      automata::AtomicClockConstraintT<std::greater_equal<automata::Time>>{1}}},
                    {"c"}},
         Transition{Location{"OPENING"},
                    "finish_open",
                    Location{"OPEN"},
                    {{"c", automata::AtomicClockConstraintT<std::equal_to<automata::Time>>{4}}},
                    {"c"}}}};
	const auto finish_close = F{AP{"finish_close"}};
	const auto start_open   = F{AP{"start_open"}};
	const auto enter        = F{AP{"enter"}};

	auto ata = mtl_ata_translation::translate(
	  ((!finish_close).until(enter) && finally(enter)),
	  {AP{"start_open"}, AP{"finish_open"}, AP{"start_close"}, AP{"finish_close"}});
	search::TreeSearch<std::string, std::string> search(
	  &ta, &ata, {"start_open", "start_close"}, {"finish_open", "finish_close"}, 4, true, true);

	search.build_tree();
	CHECK(search.get_root()->label == NodeLabel::TOP);
	auto controller = create_controller(search.get_root(), 4);

#ifdef HAVE_VISUALIZATION
	visualization::ta_to_graphviz(controller).render_to_file("railroad_bounds_controller.svg");
#endif

	// TODO Check more properties of the controller
	CHECK(controller.get_alphabet() == std::set<std::string>{"start_close", "finish_close"});
}

TEST_CASE("Controller can decide to do nothing", "[controller]")
{
	// The controller first needs to go to l1 with 'c', then the environment can do 'e'.
	TA ta{{Location{"l0"}, Location{"l1"}},
	      {"c", "e"},
	      Location{"l0"},
	      {Location{"l1"}},
	      {"c"},
	      {Transition{Location{"l0"}, "c", Location{"l1"}},
	       Transition{Location{"l1"}, "e", Location{"l1"}}}};

	const logic::AtomicProposition<std::string> ap_c{"c"};
	const logic::AtomicProposition<std::string> ap_e{"e"};
	// Never let the environment do an action.
	auto ata = mtl_ata_translation::translate(logic::finally(logic::MTLFormula{ap_e}), {ap_c, ap_e});
	CAPTURE(ta);
	CAPTURE(ata);
	search::TreeSearch<std::string, std::string> search(&ta, &ata, {"c"}, {"e"}, 0, true, false);
	search.build_tree(false);
	INFO("Tree:\n" << search::node_to_string(*search.get_root(), true));
	CHECK(search.get_root()->label == NodeLabel::TOP);
	auto controller = create_controller(search.get_root(), 1);
	CAPTURE(controller);
	CHECK(controller.get_transitions().empty());
}

TEST_CASE("Compute clock constraints from outgoing actions", "[controller]")
{
	using controller_synthesis::details::get_constraints_from_outgoing_action;
	using TARegionState = search::TARegionState<std::string>;
	CHECK(get_constraints_from_outgoing_action<std::string, std::string>(
	        {search::CanonicalABWord<std::string, std::string>(
	          {{TARegionState{Location{"s0"}, "c1", 0}}, {TARegionState{Location{"s0"}, "c2", 1}}})},
	        {search::RegionIndex{1}, "a"},
	        3)
	      == std::multimap<std::string, std::multimap<std::string, automata::ClockConstraint>>{
	        {"a",
	         std::multimap<std::string, automata::ClockConstraint>{
	           // TODO The first two constraints are correct actually unnecessary as they are implied
	           // by the third constraint. We should only produce the third constraint.
	           {"c1", automata::AtomicClockConstraintT<std::greater<automata::Time>>{0}},
	           {"c1", automata::AtomicClockConstraintT<std::less<automata::Time>>{1}},
	           {"c2", automata::AtomicClockConstraintT<std::equal_to<automata::Time>>{1}}}}});
	// TODO Fix this test, it tested constraint merging, which is broken
	// CHECK(get_constraints_from_outgoing_action<std::string, std::string>(
	//        {search::CanonicalABWord<std::string, std::string>(
	//          {{TARegionState{Location{"s0"}, "c1", 0}}, {TARegionState{Location{"s0"}, "c2",
	//          1}}})},
	//        {{search::RegionIndex{1}, "a"}, {search::RegionIndex{2}, "a"}},
	//        3)
	//      == std::multimap<std::string, std::multimap<std::string, automata::ClockConstraint>>{
	//        {"a",
	//         std::multimap<std::string, automata::ClockConstraint>{
	//           {"c1", automata::AtomicClockConstraintT<std::greater<automata::Time>>{0}},
	//           {"c1", automata::AtomicClockConstraintT<std::less_equal<automata::Time>>{1}},
	//           {"c2", automata::AtomicClockConstraintT<std::greater_equal<automata::Time>>{1}},
	//           {"c2", automata::AtomicClockConstraintT<std::less<automata::Time>>{2}}}}});
}

} // namespace
