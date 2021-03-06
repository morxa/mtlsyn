/***************************************************************************
 *  ta_proto.h - Protobuf import/export for timed automata
 *
 *  Created:   Fri 19 Mar 10:31:34 CET 2021
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

#ifndef SRC_AUTOMATA_INCLUDE_AUTOMATA_TA_PROTO_H
#define SRC_AUTOMATA_INCLUDE_AUTOMATA_TA_PROTO_H

#include "automata/ta.h"
#include "automata/ta.pb.h"

namespace automata::ta {

TimedAutomaton<std::string, std::string> parse_proto(const proto::TimedAutomaton &ta_proto);
TimedAutomaton<std::vector<std::string>, std::string>
parse_product_proto(const proto::ProductAutomaton &ta_product_proto);

template <typename LocationT, typename ActionT>
proto::TimedAutomaton ta_to_proto(const TimedAutomaton<LocationT, ActionT> &ta);

namespace details {

proto::TimedAutomaton::Transition::ClockConstraint
clock_constraint_to_proto(const std::string &clock_name, const ClockConstraint &constraint);

}

} // namespace automata::ta

#include "ta_proto.hpp"

#endif /* ifndef SRC_AUTOMATA_INCLUDE_AUTOMATA_TA_PROTO_H */
