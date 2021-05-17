/***************************************************************************
 *  csma_cd.h - Utility functions for csma_cd test scenario
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

#pragma once

#include "automata/automata.h"
#include "automata/ta.h"

#include <set>
#include <string>
#include <tuple>
#include <vector>

std::tuple<automata::ta::TimedAutomaton<std::vector<std::string>, std::string>,
           std::set<std::string>,
           std::set<std::string>>
create_csma_cd_instance(std::size_t    count,
                        automata::Time delay_self_assign,
                        automata::Time delay_enter_critical);
