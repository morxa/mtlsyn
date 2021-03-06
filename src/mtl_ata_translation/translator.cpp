/***************************************************************************
 *  translator.cpp - Translate an MTL formula into an ATA
 *
 *  Created: Thu 18 Jun 2020 11:21:13 CEST 11:21
 *  Copyright  2020  Till Hofmann <hofmann@kbsg.rwth-aachen.de>
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

#include "mtl_ata_translation/translator.h"

#include "automata/ata.h"
#include "automata/ata_formula.h"
#include "automata/automata.h"
#include "mtl/MTLFormula.h"

#include <fmt/format.h>

#include <memory>
#include <stdexcept>

namespace mtl_ata_translation {

using namespace automata;
using logic::AtomicProposition;
using logic::LOP;
using logic::MTLFormula;
using logic::TimeInterval;
using logic::TimePoint;

///@{
/// Formulas are always ATA formulas over MTLFormulas.
using Formula                = ata::Formula<MTLFormula<ActionType>>;
using TrueFormula            = ata::TrueFormula<MTLFormula<ActionType>>;
using FalseFormula           = ata::FalseFormula<MTLFormula<ActionType>>;
using ConjunctionFormula     = ata::ConjunctionFormula<MTLFormula<ActionType>>;
using ResetClockFormula      = ata::ResetClockFormula<MTLFormula<ActionType>>;
using DisjunctionFormula     = ata::DisjunctionFormula<MTLFormula<ActionType>>;
using LocationFormula        = ata::LocationFormula<MTLFormula<ActionType>>;
using ClockConstraintFormula = ata::ClockConstraintFormula<MTLFormula<ActionType>>;
///@}

///@{
/// The resulting type is an ATA over MTLFormulas and AtomicPropositions.
using AlternatingTimedAutomaton =
  ata::AlternatingTimedAutomaton<MTLFormula<ActionType>, AtomicProposition<ActionType>>;
using Transition = ata::Transition<MTLFormula<ActionType>, AtomicProposition<ActionType>>;
using utilities::arithmetic::BoundType;
///@}

namespace {

std::set<MTLFormula<ActionType>>
get_closure(const MTLFormula<ActionType> &formula)
{
	auto untils      = formula.get_subformulas_of_type(LOP::LUNTIL);
	auto dual_untils = formula.get_subformulas_of_type(LOP::LDUNTIL);
	untils.insert(dual_untils.begin(), dual_untils.end());
	return untils;
}

/// Creates constraint defining the passed interval
std::unique_ptr<Formula>
create_contains(TimeInterval duration)
{
	std::unique_ptr<Formula> lowerBound = std::make_unique<TrueFormula>();
	std::unique_ptr<Formula> upperBound = std::make_unique<TrueFormula>();
	if (duration.lowerBoundType() != BoundType::INFTY) {
		if (duration.lowerBoundType() == BoundType::WEAK) {
			lowerBound = std::make_unique<ClockConstraintFormula>(
			  AtomicClockConstraintT<std::greater_equal<TimePoint>>(duration.lower()));
		} else {
			lowerBound = std::make_unique<ClockConstraintFormula>(
			  AtomicClockConstraintT<std::greater<TimePoint>>(duration.lower()));
		}
	}
	if (duration.upperBoundType() != BoundType::INFTY) {
		if (duration.upperBoundType() == BoundType::WEAK) {
			upperBound = std::make_unique<ClockConstraintFormula>(
			  AtomicClockConstraintT<std::less_equal<TimePoint>>(duration.upper()));
		} else {
			upperBound = std::make_unique<ClockConstraintFormula>(
			  AtomicClockConstraintT<std::less<TimePoint>>(duration.upper()));
		}
	}
	return ata::create_conjunction(std::move(lowerBound), std::move(upperBound));
}

/// Creates constraint excluding the passed interval
std::unique_ptr<Formula>
create_negated_contains(TimeInterval duration)
{
	std::unique_ptr<Formula> lowerBound = std::make_unique<FalseFormula>();
	std::unique_ptr<Formula> upperBound = std::make_unique<FalseFormula>();
	if (duration.lowerBoundType() != BoundType::INFTY) {
		if (duration.lowerBoundType() == BoundType::WEAK) {
			lowerBound = std::make_unique<ClockConstraintFormula>(
			  AtomicClockConstraintT<std::less<TimePoint>>(duration.lower()));
		} else {
			lowerBound = std::make_unique<ClockConstraintFormula>(
			  AtomicClockConstraintT<std::less_equal<TimePoint>>(duration.lower()));
		}
	}
	if (duration.upperBoundType() != BoundType::INFTY) {
		if (duration.upperBoundType() == BoundType::WEAK) {
			upperBound = std::make_unique<ClockConstraintFormula>(
			  AtomicClockConstraintT<std::greater<TimePoint>>(duration.upper()));
		} else {
			upperBound = std::make_unique<ClockConstraintFormula>(
			  AtomicClockConstraintT<std::greater_equal<TimePoint>>(duration.upper()));
		}
	}
	return ata::create_disjunction(std::move(lowerBound), std::move(upperBound));
}

std::unique_ptr<Formula>
init(const MTLFormula<ActionType> &       formula,
     const AtomicProposition<ActionType> &ap,
     bool                                 first = false)
{
	switch (formula.get_operator()) {
	case LOP::TRUE: return std::make_unique<TrueFormula>();
	case LOP::FALSE: return std::make_unique<FalseFormula>();
	case LOP::LUNTIL:
	case LOP::LDUNTIL:
		// init(psi, a) = x.psi if psi \in cl(phi)
		if (first) {
			return std::make_unique<LocationFormula>(formula);
		} else {
			return std::make_unique<ResetClockFormula>(std::make_unique<LocationFormula>(formula));
		}
	case LOP::LAND:
		// init(psi_1 AND psi_2, a) = init(psi_1, a) AND init(psi_2, a)
		return ata::create_conjunction(init(formula.get_operands().front(), ap, first),
		                               init(formula.get_operands().back(), ap, first));
	case LOP::LOR:
		// init(psi_1 OR psi_2, a) = init(psi_1, a) OR init(psi_2, a)
		return ata::create_disjunction(init(formula.get_operands().front(), ap, first),
		                               init(formula.get_operands().back(), ap, first));
	case LOP::AP:
		if (formula == ap) {
			// init(b, a) = TRUE if b == a
			return std::make_unique<TrueFormula>();
		} else {
			// init(b, a) = FALSE if b != a
			return std::make_unique<FalseFormula>();
		}
	case LOP::LNEG:
		// init(NOT b, a) = NOT init(b, a)
		// ATA formulas do not have negations, directly compute the result.
		// We know that this is an atomic proposition because the input formula is in positive normal
		// form.
		switch (formula.get_operands().front().get_operator()) {
		case LOP::TRUE: return std::make_unique<FalseFormula>();
		case LOP::FALSE: return std::make_unique<TrueFormula>();
		case LOP::AP:
			if (formula.get_operands().front() == ap) {
				// init(not b, a) = FALSE if b == a
				return std::make_unique<FalseFormula>();
			} else {
				// init(not b, a) = TRUE if b != a
				return std::make_unique<TrueFormula>();
			}
		default:
			std::stringstream ss;
			ss << "The formula " << formula << " is not in positive normal form.";
			throw std::logic_error(ss.str());
		}
	}
	throw std::logic_error("Unexpected formula operator");
}

} // namespace

/** Translate an MTL formula into an ATA.
 * Create the ATA closely following the construction by Ouaknine and Worrell, 2005.
 * @param input_formula The formula to translate
 * @param alphabet The alphabet that the ATA should read, defaults to the symbols of the formula.
 * @return An ATA that accepts a word w iff the word is in the language of the formula.
 */
AlternatingTimedAutomaton
translate(const MTLFormula<ActionType> &          input_formula,
          std::set<AtomicProposition<ActionType>> alphabet)
{
	const auto formula = input_formula.to_positive_normal_form();
	if (alphabet.empty()) {
		// The ATA alphabet is the same as the formula alphabet.
		alphabet = formula.get_alphabet();
	}
	if (alphabet.count({"l0"}) > 0) {
		throw std::invalid_argument("The formula alphabet must not contain the symbol 'l0'");
	}
	// S = cl(phi) U {l0}
	auto locations = get_closure(formula);
	locations.insert({AtomicProposition<ActionType>{"l0"}});
	const auto           untils              = formula.get_subformulas_of_type(LOP::LUNTIL);
	const auto           dual_untils         = formula.get_subformulas_of_type(LOP::LDUNTIL);
	const auto           accepting_locations = dual_untils;
	std::set<Transition> transitions;
	for (const auto &symbol : alphabet) {
		// Initial transition delta(l0, symbol) -> phi
		transitions.insert(
		  Transition(AtomicProposition<ActionType>{"l0"}, symbol.ap_, init(formula, symbol, true)));

		for (const auto &until : untils) {
			auto transition_formula = ata::create_disjunction(
			  // init(phi_2, symbol) AND x \in I
			  ata::create_conjunction(init(until.get_operands().back(), symbol),
			                          create_contains(until.get_interval())),
			  // init(phi_1, symbol) AND (psi_1 \until_I psi_2)
			  //                         \->   == until     <-/
			  ata::create_conjunction(init(until.get_operands().front(), symbol),
			                          std::unique_ptr<Formula>(
			                            std::make_unique<LocationFormula>(until))));
			transitions.insert(Transition(until, symbol, std::move(transition_formula)));
		}
		for (const auto &dual_until : dual_untils) {
			auto transition_formula = ata::create_conjunction(
			  ata::create_disjunction(init(dual_until.get_operands().back(), symbol),
			                          create_negated_contains(dual_until.get_interval())),
			  ata::create_disjunction(init(dual_until.get_operands().front(), symbol),
			                          std::unique_ptr<Formula>(
			                            std::make_unique<LocationFormula>(dual_until))));
			transitions.insert(Transition(dual_until, symbol, std::move(transition_formula)));
		}
	}
	return AlternatingTimedAutomaton(alphabet,
	                                 MTLFormula<ActionType>{{"l0"}},
	                                 accepting_locations,
	                                 std::move(transitions),
	                                 MTLFormula<ActionType>{{"sink"}});
}
} // namespace mtl_ata_translation
