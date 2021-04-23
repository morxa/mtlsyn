/***************************************************************************
 *  csma_cd.cpp - Utility functions for csma_cd test scenario
 *
 *  Created: Thu 22 Apr 2021 19:33:42 CEST 14:00
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
 *  Read the full text in the LICENSE.GPL file in the doc directory.
 */

#include "csma_cd.h"

#include "automata/automata.h"
#include "automata/ta.h"
#include "automata/ta_product.h"

using automata::Time;
using Location   = automata::ta::Location<std::string>;
using TA         = automata::ta::TimedAutomaton<std::string, std::string>;
using Transition = automata::ta::Transition<std::string, std::string>;
using automata::AtomicClockConstraintT;

std::tuple<automata::ta::TimedAutomaton<std::vector<std::string>, std::string>,
           std::set<std::string>,
           std::set<std::string>>
create_csma_cd_instance(std::size_t count, Time lambda, Time sigma)
{
	std::vector<TA>         automata;
	std::set<std::string>   controller_actions;
	std::set<std::string>   environment_actions;
	std::set<std::string>   medium_actions;
	std::set<Location>      medium_locations;
	std::vector<Transition> medium_transitions;
	std::vector<Transition> medium_interprocess_transitions;
	// common actions
	const std::string cd = "cd";
	medium_actions.insert(cd);
	controller_actions.insert(cd);
	// critical section setup
	const std::string timer = "y";
	Location          free{"FREE"};
	Location          blocked{"BLOCKED"};
	Location          collision{"COLLISION"};
	medium_locations.insert({free, blocked, collision});
	for (std::size_t i = 1; i <= count; ++i) {
		const std::string clock = "c_" + std::to_string(i);
		const std::string send  = "send_" + std::to_string(i);
		const std::string end   = "end_" + std::to_string(i);
		const std::string busy  = "busy_" + std::to_string(i);
		const std::string prob  = "prob_" + std::to_string(i);
		environment_actions.insert({end, busy, prob});
		controller_actions.insert(send);
		automata.push_back(
		  TA{{Location{"INIT"},
		      Location{"TRANSMIT"},
		      Location{"COLLIDE"},
		      Location{"RETRY"},
		      Location{"DONE"}},
		     {send, end, busy, cd, prob},
		     Location{"INIT"},
		     {Location{"DONE"}},
		     {clock},
		     {Transition(Location{"INIT"}, send, Location{"TRANSMIT"}),
		      Transition(Location{"TRANSMIT"},
		                 end,
		                 Location{"DONE"},
		                 {{clock, AtomicClockConstraintT<std::equal_to<Time>>(lambda)}},
		                 {}),
		      Transition(Location{"TRANSMIT"}, cd, Location{"COLLIDE"}, {}, {clock}),
		      Transition(Location{"COLLIDE"}, prob, Location{"RETRY"}),
		      Transition(Location{"RETRY"}, busy, Location{"COLLIDE"}),
		      Transition(Location("RETRY"), send, Location("TRANSMIT"))}});

		medium_actions.insert({send, busy, end});
		medium_transitions.push_back(Transition{free, send, blocked, {}, {timer}});
		medium_transitions.push_back(Transition{blocked, end, free, {}, {timer}});
		medium_transitions.push_back(
		  Transition{blocked,
		             busy,
		             blocked,
		             {{timer, AtomicClockConstraintT<std::greater_equal<Time>>(sigma)}},
		             {}});
		medium_transitions.push_back(Transition{
		  blocked, send, collision, {{timer, AtomicClockConstraintT<std::less_equal<Time>>(sigma)}}});
		medium_transitions.push_back(
		  Transition{collision,
		             cd,
		             free,
		             {{timer, AtomicClockConstraintT<std::less_equal<Time>>(sigma)}},
		             {timer}});
	}
	// add automaton for the carrier
	automata.push_back(TA{medium_locations, medium_actions, free, {}, {timer}, medium_transitions});
	return std::make_tuple(automata::ta::get_product(automata),
	                       controller_actions,
	                       environment_actions);
}
