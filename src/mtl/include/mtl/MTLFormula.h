/***************************************************************************
 *  MTLFormula.h - MTL formula representation and satisfaction
 *
 *  Created:   Thu Jun 4 13:13:54 2020 +0200
 *  Copyright  2020  Stefan Schupp <stefan.schupp@cs.rwth-aachen.de>
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

#ifndef SRC_MTL_INCLUDE_MTL_MTLFORMULA_H
#define SRC_MTL_INCLUDE_MTL_MTLFORMULA_H

#include "utilities/Interval.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

/// MTLFormulas and types related to MTL.
namespace logic {

/// An interval endpoint used for constrained until and dual until operators.
using TimePoint = double;
/// An interval used for constrained until and dual until operators.
using TimeInterval = ::utilities::arithmetic::Interval<TimePoint>;

template <typename APType>
class MTLFormula;

/// Enum for logical operation-types
enum class LOP { LAND, LOR, LNEG, LUNTIL, LDUNTIL, AP, TRUE, FALSE };

/// Returns the dual of the passed operator
inline LOP
dual(LOP in)
{
	switch (in) {
	case LOP::LAND: return LOP::LOR;
	case LOP::LOR: return LOP::LAND;
	case LOP::LUNTIL: return LOP::LDUNTIL;
	case LOP::LDUNTIL: return LOP::LUNTIL;
	case LOP::TRUE: return LOP::FALSE;
	case LOP::FALSE: return LOP::TRUE;
	default: return in;
	}
}

/**
 * @brief Strong typing of atomic propositions, allows simplified construction from Boolean values
 * as well.
 *
 */
template <typename APType>
struct AtomicProposition
{
	AtomicProposition() = delete;
	/// Copy constructor
	AtomicProposition(const AtomicProposition &other) = default;
	/// Copy-assignment operator
	AtomicProposition &operator=(const AtomicProposition &other) = default;

	/**
	 * @brief Construct a new Atomic Proposition object from string
	 *
	 * @param name
	 */
	AtomicProposition(const APType &name) : ap_(name)
	{
	}

	/// less-operator
	bool
	operator<(const AtomicProposition &other) const
	{
		return ap_ < other.ap_;
	}

	APType ap_; ///< String representation of the ap
};

/// Comparison override
template <typename APType>
bool
operator==(const AtomicProposition<APType> &lhs, const AtomicProposition<APType> &rhs)
{
	return lhs.ap_ == rhs.ap_;
}

/// Not-equal operator
template <typename APType>
bool
operator!=(const AtomicProposition<APType> &lhs, const AtomicProposition<APType> &rhs)
{
	return !(rhs == lhs);
}

/**
 * @brief A (timed-) word which can be validated against a given MTL-formula.
 *
 */
template <typename APType>
class MTLWord
{
public:
	friend class MTLFormula<APType>;

	/**
	 * @brief Constructor
	 */
	MTLWord(std::initializer_list<std::pair<std::vector<AtomicProposition<APType>>, TimePoint>> a)
	: word_(a)
	{
	}

	/**
	 * @brief Checks satisfaction at a certain position
	 */
	bool satisfies_at(const MTLFormula<APType> &phi, std::size_t i) const;

	/**
	 * @brief Checks satisfaction at pos 0
	 */
	bool satisfies(const MTLFormula<APType> &phi) const;

private:
	std::vector<std::pair<std::vector<AtomicProposition<APType>>, TimePoint>> word_;
};

/**
 * @brief Class representing an MTL-formula with the usual operators.
 *
 */
template <typename APType>
class MTLFormula
{
public:
	friend class MTLWord<APType>;
	MTLFormula() = delete;

	/**
	 * @brief Constructor from atomic proposition
	 */
	MTLFormula(const AtomicProposition<APType> &ap);

	/** * @brief Copy-constructor */
	MTLFormula(const MTLFormula &) = default;
	/** Move constructor */
	MTLFormula(MTLFormula &&) = default;

	/** Copy assignment. */
	MTLFormula &operator=(const MTLFormula &) = default;
	/** Move assignment. */
	MTLFormula &operator=(MTLFormula &&) = default;

	/// copy-assignment operator
	MTLFormula &operator=(const MTLFormula &rhs) = default;

	/// Get a formula that is always true.
	static MTLFormula
	TRUE()
	{
		return MTLFormula{LOP::TRUE, {}};
	}

	/// Get a formula that is always false.
	static MTLFormula
	FALSE()
	{
		return MTLFormula{LOP::FALSE, {}};
	}

	/** Construct a conjuction of sub-formulas.
	 * @param conjuncts The sub-formulas of the conjunction
	 * @return A conjunction of all sub-formulas
	 */
	static MTLFormula
	create_conjunction(const std::vector<MTLFormula> &conjuncts)
	{
		return MTLFormula{LOP::LAND, std::begin(conjuncts), std::end(conjuncts)};
	}

	/** Construct a disjuction of sub-formulas.
	 * @param disjuncts The sub-formulas of the disjunction
	 * @return A disjunction of all sub-formulas
	 */
	static MTLFormula
	create_disjunction(const std::vector<MTLFormula> &disjuncts)
	{
		return MTLFormula{LOP::LOR, std::begin(disjuncts), std::end(disjuncts)};
	}

	/// Boolean-AND operator
	MTLFormula operator&&(const MTLFormula &rhs) const;

	/// Boolean-OR operator
	MTLFormula operator||(const MTLFormula &rhs) const;

	/// Boolean negation operator
	MTLFormula operator!() const;

	/// timed-until operator (binary)
	MTLFormula until(const MTLFormula &rhs, const TimeInterval &duration = TimeInterval()) const;

	/// timed dual_until operator (binary)
	MTLFormula dual_until(const MTLFormula &rhs, const TimeInterval &duration = TimeInterval()) const;

	/// less operator
	bool operator<(const MTLFormula &rhs) const;

	/// larger operator
	bool
	operator>(const MTLFormula &rhs) const
	{
		return rhs < *this;
	}
	/// less-equal operator
	bool
	operator<=(const MTLFormula &rhs) const
	{
		return *this < rhs || *this == rhs;
	}

	/// larger-equal operator
	bool
	operator>=(const MTLFormula &rhs) const
	{
		return *this > rhs || *this == rhs;
	}

	/// equal operator
	bool
	operator==(const MTLFormula &rhs) const
	{
		return !(*this < rhs) && !(rhs < *this);
	}
	/// not-equal operator
	bool
	operator!=(const MTLFormula &rhs) const
	{
		return !(*this == rhs);
	}

	/**
	 * @brief Returns normalized formula (positive normal form)
	 * @details All negations are moved to the literals
	 * @return MTLFormula
	 */
	MTLFormula to_positive_normal_form() const;

	/// collects all used atomic propositions of the formula
	std::set<AtomicProposition<APType>> get_alphabet() const;

	/// collects all subformulas of a specific type
	std::set<MTLFormula<APType>> get_subformulas_of_type(LOP op) const;

	/// getter for operands
	const std::vector<MTLFormula> &
	get_operands() const
	{
		return operands_;
	}
	/// getter for the logical operator
	LOP
	get_operator() const
	{
		return operator_;
	}
	/**
	 * @brief getter for the duration
	 * @details throws std::bad_optional_value exception if not set
	 * @return TimeInterval
	 */
	TimeInterval
	get_interval() const
	{
		return duration_.value();
	}

	/**
	 * @brief getter for the atomic proposition
	 * @details throws std::bad_optional_value exception if no atomic proposition was set
	 * @return AtomicProposition
	 */
	AtomicProposition<APType>
	get_atomicProposition() const
	{
		return ap_.value();
	}

	/** Get the value of the largest constant occurring in the formula.  */
	TimePoint get_largest_constant() const;

	// TODO Refactor into utilities.
	/** Get the value of the largest constant occurring in the formula.  */
	std::size_t
	get_maximal_region_index() const
	{
		return 2 * get_largest_constant() + 1;
	}

private:
	bool
	is_consistent() const
	{
		return (ap_.has_value() == (operator_ == LOP::AP));
	}

	template <class It>
	MTLFormula(LOP op, It first, It last, const TimeInterval &duration = TimeInterval())
	: operator_(op), duration_(duration), operands_(first, last)
	{
		assert(is_consistent());
	}

	MTLFormula(LOP                               op,
	           std::initializer_list<MTLFormula> operands,
	           const TimeInterval &              duration = TimeInterval())
	: MTLFormula(op, std::begin(operands), std::end(operands), duration)
	{
	}

	std::optional<AtomicProposition<APType>> ap_;
	LOP                                      operator_;
	std::optional<TimeInterval>              duration_;
	std::vector<MTLFormula<APType>>          operands_;
};

/// Logical AND
template <typename APType>
MTLFormula<APType>
operator&&(const AtomicProposition<APType> &lhs, const AtomicProposition<APType> &rhs)
{
	return MTLFormula<APType>(lhs) && MTLFormula<APType>(rhs);
}

/// Logical OR
template <typename APType>
MTLFormula<APType>
operator||(const AtomicProposition<APType> &lhs, const AtomicProposition<APType> &rhs)
{
	return MTLFormula<APType>(lhs) || MTLFormula<APType>(rhs);
}

/// Logical NEGATION
template <typename APType>
MTLFormula<APType>
operator!(const AtomicProposition<APType> &ap)
{
	return !MTLFormula<APType>(ap);
}

/** Create a formula F_I phi, which is equivalent to True Until_I phi.
 * @param phi The sub-formula phi that must eventually be satisfied.
 * @param duration The time bound I for the finally operator (i.e., phi must
 * be satisfied within this interval)
 * @return An MTL formula equivalent to F_I phi
 */
template <typename APType>
MTLFormula<APType>
finally(const MTLFormula<APType> &phi, const TimeInterval &duration = TimeInterval())
{
	return MTLFormula<APType>::TRUE().until(phi, duration);
}

/** Create a formula G_I phi, which is equivalent to not(F_I not phi)
 * @param phi The sub-formula phi that must always be satisfied.
 * @param duration The time bound I for the globally operator (i.e., phi must always be satisfied
 * within this interval)
 * @return An MTL formula equivalent to G_I phi
 */
template <typename APType>
MTLFormula<APType>
globally(const MTLFormula<APType> &phi, const TimeInterval &duration = TimeInterval())
{
	return !finally(!phi, duration);
}

/// outstream operator
template <typename APType>
std::ostream &operator<<(std::ostream &out, const logic::AtomicProposition<APType> &a);

/// outstream operator
template <typename APType>
std::ostream &operator<<(std::ostream &out, const logic::MTLFormula<APType> &f);

} // namespace logic

#include "MTLFormula.hpp"

#endif /* ifndef SRC_MTL_INCLUDE_MTL_MTLFORMULA_H */
