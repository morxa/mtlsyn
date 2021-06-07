/***************************************************************************
 *  ta.h - Core functionality for timed automata
 *
 *  Created: Tue 26 May 2020 13:44:41 CEST 13:44
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

#ifndef SRC_AUTOMATA_INCLUDE_AUTOMATA_TA_H
#define SRC_AUTOMATA_INCLUDE_AUTOMATA_TA_H

#include "automata.h"

#include <NamedType/named_type.hpp>
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace automata::ta {

template <typename T>
using Location = fluent::NamedType<T,
                                   struct LocationTypeTag,
                                   fluent::Comparable,
                                   fluent::Callable,
                                   fluent::Printable,
                                   fluent::Hashable>;

/// Invalid location encountered
/*** This exception is thrown when some location (e.g., as part of a transition) is not  part of a
 * timed automaton.
 */
template <typename LocationT>
class InvalidLocationException : public std::invalid_argument
{
public:
	/** Constructor
	 */
	explicit InvalidLocationException(const automata::ta::Location<LocationT> &)
	: std::invalid_argument("Invalid location")
	{
	}
};

/** A TA Configuration, consisting of a location and a set of clock valuations.
 * @tparam LocationT The location type
 */
template <typename LocationT>
struct Configuration
{
	/** The current location of the TA */
	Location<LocationT> location;
	/** The current clock valuations of the TA */
	ClockSetValuation clock_valuations;

	/** Check if one configuration is lexicographically smaller than the other.
	 * @return true if the first configuration is smaler than the second
	 */
	[[nodiscard]] friend bool
	operator<(const Configuration<LocationT> &first, const Configuration<LocationT> &second)
	{
		return std::tie(first.location, first.clock_valuations)
		       < std::tie(second.location, second.clock_valuations);
	}
	/** Check if two configurations are identical.
	 * @return true if both configurations have the same location and clock valuations.
	 */
	[[nodiscard]] friend bool
	operator==(const Configuration<LocationT> &first, const Configuration<LocationT> &second)
	{
		return !(first < second) && !(second < first);
	}
};

template <typename LocationT, typename AP>
class TimedAutomaton;

template <typename LocationT, typename AP>
class Transition;

/** Print a transition
 * @param os The ostream to print to
 * @param transition The transition to print
 * @return A reference to the ostream
 */
template <typename LocationT, typename AP>
std::ostream &operator<<(std::ostream &                                 os,
                         const automata::ta::Transition<LocationT, AP> &transition);

template <typename LocationT, typename AP>
std::ostream &operator<<(std::ostream &os, const automata::ta::TimedAutomaton<LocationT, AP> &ta);

/// Compare two transitions.
/** Two transitions are equal if they use the same source, target, read the
 * same symbol, have the same clock constraints, and reset the same clocks.
 * @param lhs The left-hand side Transition
 * @param rhs The right-hand side Transition
 */
template <typename LocationT, typename AP>
bool operator==(const Transition<LocationT, AP> &lhs, const Transition<LocationT, AP> &rhs);

/// A transition in a timed automaton.
/** @see TimedAutomaton
 */
template <typename LocationT, typename AP>
class Transition
{
public:
	friend class TimedAutomaton<LocationT, AP>;
	friend bool operator==<>(const Transition &lhs, const Transition &rhs);

	/** Print a transition
	 * @param os The ostream to print to
	 * @param transition The transition to print
	 * @return A reference to the ostream
	 */
	// clang-format off
	friend std::ostream &operator<< <>(std::ostream &os, const Transition &transition);
	// clang-format on

	/** Constructor.
	 * @param source the source location
	 * @param symbol the symbol to read with this transition
	 * @param target the target location
	 * @param clock_constraints A map defining the constraints of the clock,
	 *        where the key specifies the name of the clock and the value is a
	 *        constraint on that clock
	 * @param clock_resets the set of clocks to reset on this transition
	 */
	Transition(const Location<LocationT> &                        source,
	           const AP &                                         symbol,
	           const Location<LocationT> &                        target,
	           const std::multimap<std::string, ClockConstraint> &clock_constraints = {},
	           const std::set<std::string> &                      clock_resets      = {})
	: source_(source),
	  target_(target),
	  symbol_(symbol),
	  clock_constraints_(clock_constraints),
	  clock_resets_(clock_resets)
	{
	}
	/** Check whether the transition is enabled on the given symbol and clock valuations.
	 * More specifically, check if the given symbol matches this transition's symbol, and that the
	 * clock valuations satisfy all clock constraints.
	 * @param symbol The symbol to check
	 * @param clock_vals The clock valuations, given as map with the clock names as keys and the
	 * clocks as value
	 * @return true if the transition can be taken.
	 */
	bool is_enabled(const AP &symbol, const ClockSetValuation &clock_vals) const;

	/**
	 * @brief Get the clock constraints defining the guard conditions
	 *
	 * @return const std::multimap<std::string, const ClockConstraint>&
	 */
	const std::multimap<std::string, ClockConstraint> &
	get_guards() const
	{
		return clock_constraints_;
	}

	Location<LocationT>                         source_;            ///< source location
	Location<LocationT>                         target_;            ///< target location
	AP                                          symbol_;            ///< transition label
	std::multimap<std::string, ClockConstraint> clock_constraints_; ///< guards
	std::set<std::string>                       clock_resets_;      ///< resets
};

template <typename LocationT, typename AP>
bool operator<(const Transition<LocationT, AP> &lhs, const Transition<LocationT, AP> &rhs);

/// One specific (finite) path in the timed automaton.
template <typename LocationT, typename AP>
class Path
{
public:
	friend class TimedAutomaton<LocationT, AP>;
	/// Compare two paths of a TA
	friend bool
	operator<(const Path<LocationT, AP> &p1, const Path<LocationT, AP> &p2)
	{
		return p1.sequence_ < p2.sequence_;
	}
	/// Constructor
	/** Start a new path in the given initial location with the given clocks.
	 * @param initial_location the initial location of the path, should be the same as the TA's
	 * initial location
	 * @param clocks a set of clock names, should be names of the TA's clocks
	 */
	Path(LocationT initial_location, std::set<std::string> clocks);

	/** Get the current configuration of the path.
	 * The current configuration is the last configuration reached.
	 * @return The current configuration of the path
	 */
	Configuration<LocationT>
	get_current_configuration() const
	{
		return {current_location_, clock_valuations_};
	}

private:
	std::vector<std::tuple<AP, Time, Location<LocationT>>> sequence_;
	std::map<std::string, Clock>                           clock_valuations_;
	Location<LocationT>                                    current_location_;
	Time                                                   tick_;
};

/// A timed automaton.
/** A TimedAutomaton consists of a set of locations, an initial location, a final location, a set
 * of clocks, and a set of transitions. A simple timed automaton with two locations and a single
 * transition without constraints can be constructed with
 * @code
 * TimedAutomaton ta{"s0", {"s1"}};
 * ta.add_transition(Transition("s0", "a", "s1"));
 * @endcode
 * To construct a timed automaton with a clock constraint <tt>x < 1</tt>, use
 * @code
 * TimedAutomaton ta{"s0", {"s1"}};
 * ClockConstraint c = AtomicClockConstraintT<std::less<Time>>(1);
 * ta.add_transition(Transition("s0", "a", "s1", {{"x", c}}));
 * @endcode
 */
template <typename LocationT, typename AP>
class TimedAutomaton
{
public:
	/** Print a TimedAutomaton to an ostream. */
	// clang-format off
	friend std::ostream &operator<< <>(std::ostream &os, const TimedAutomaton<LocationT, AP> &ta);
	// clang-format on
	TimedAutomaton() = delete;
	/** Constructor.
	 * @param alphabet The valid symbols in the TimedAutomaton
	 * @param initial_location the initial location
	 * @param final_locations a set of final locations
	 */
	TimedAutomaton(const std::set<AP> &                 alphabet,
	               const Location<LocationT> &          initial_location,
	               const std::set<Location<LocationT>> &final_locations)
	: alphabet_(alphabet),
	  locations_{initial_location},
	  initial_location_(initial_location),
	  final_locations_(final_locations)
	{
		add_locations(final_locations_);
	}
	/** Constructor.
	 * @param locations All locations of the automaton
	 * @param alphabet The valid symbols in the TimedAutomaton
	 * @param initial_location The initial location of the automaton
	 * @param final_locations A set of final locations
	 * @param clocks The name of the automaton's clocks
	 * @param transitions The transitions of the timed automaton
	 */
	TimedAutomaton(const std::set<Location<LocationT>> &         locations,
	               const std::set<AP> &                          alphabet,
	               const Location<LocationT> &                   initial_location,
	               const std::set<Location<LocationT>>           final_locations,
	               std::set<std::string>                         clocks,
	               const std::vector<Transition<LocationT, AP>> &transitions)
	: alphabet_(alphabet),
	  locations_(locations),
	  initial_location_(initial_location),
	  final_locations_(final_locations),
	  clocks_(clocks)
	{
		if (!std::includes(
		      begin(locations), end(locations), begin(final_locations), end(final_locations))) {
			throw std::invalid_argument("Final locations must be a subset of all locations");
		}
		if (locations.find(initial_location) == end(locations)) {
			throw std::invalid_argument("Initial location is not a location of the TA");
		}
		for (const auto &transition : transitions) {
			for (const auto &[clock, constraint] : transition.clock_constraints_) {
				if (clocks.find(clock) == end(clocks)) {
					throw std::invalid_argument("Clock constraint uses unknown clock");
				}
				if (std::holds_alternative<AtomicClockConstraintT<std::not_equal_to<Time>>>(constraint)) {
					throw std::invalid_argument("Inequality is not allowed in a TA guard");
				}
			}
			add_transition(transition);
		}
	}

	/** Get the alphabet
	 * @return A reference to the set of symbols used by the TimedAutomaton
	 */
	const std::set<AP> &
	get_alphabet() const
	{
		return alphabet_;
	}

	/** Get the locations.
	 * @return A reference to the set of locations
	 */
	const std::set<Location<LocationT>> &
	get_locations() const
	{
		return locations_;
	}

	/** Get the initial location.
	 * @return The initial location
	 */
	const Location<LocationT> &
	get_initial_location() const
	{
		return initial_location_;
	}

	/** Get the final locations.
	 * @return The final locations
	 */
	const std::set<Location<LocationT>> &
	get_final_locations() const
	{
		return final_locations_;
	}

	/** Get the transitions of the TA.
	 * @return A multimap with entries (location, transition)
	 */
	const std::multimap<Location<LocationT>, Transition<LocationT, AP>> &
	get_transitions() const
	{
		return transitions_;
	}

	/** Get the clock names of the automaton.
	 * @return a set of clock names
	 */
	const std::set<std::string> &
	get_clocks() const
	{
		return clocks_;
	}

	/** Add a location to the TA.
	 * @param location the location to add
	 * @return true if a new location was added
	 */
	bool
	add_location(const Location<LocationT> &location)
	{
		return locations_.insert(location).second;
	}

	/** Add a final location to the TA.
	 * @param location the location to add
	 */
	void
	add_final_location(const Location<LocationT> &location)
	{
		locations_.insert(location);
		final_locations_.insert(location);
	}

	/** Add an action to the TA.
	 * @param action The action to add
	 */
	void
	add_action(const AP &action)
	{
		alphabet_.insert(action);
	}
	/** Add a clock to the TA.
	 * @param name the name of the clock
	 */
	void
	add_clock(const std::string &name)
	{
		clocks_.insert(name);
	}
	/** Add a set of locations to the TA
	 * @param locations the locations to add
	 */
	void
	add_locations(const std::set<Location<LocationT>> &locations)
	{
		for (const auto &location : locations) {
			add_location(location);
		}
	}
	/** Add a transition to the TA.
	 * @param transition The transition to add, must only mention clocks and locations that are
	 * already part of the TA.
	 */
	void add_transition(const Transition<LocationT, AP> &transition);

	/** Compute the resulting configuration after making a symbol step.
	 */
	std::set<Configuration<LocationT>> make_symbol_step(const Configuration<LocationT> &configuration,
	                                                    const AP &                      symbol) const;

	/// Let the TA make a transition on the given symbol at the given time.
	/** Check if there is a transition that can be enabled on the given symbol at the given time,
	 * starting with the given path. If so, modify the given path, i.e., apply the transition by
	 * switching to the new location, increasing all clocks by the time difference, and resetting
	 * all clocks specified in the transition. This always uses the first transition that is
	 * enabled, i.e., it does not work properly on non-deterministic TAs.
	 * @param path The path prefix to start at
	 * @param symbol The symbol to read
	 * @param time The (absolute) time associated with the symbol
	 * @return a (possibly empty) set of valid paths after applying the transition
	 */
	std::set<Path<LocationT, AP>>
	make_transition(Path<LocationT, AP> path, const AP &symbol, const Time &time) const;

	/// Check if the TA accepts the given timed word.
	/** Iteratively apply transitions for each (symbol,time) pair in the given timed word.
	 * @param word the word to read
	 * @return true if the word was accepted
	 */
	bool accepts_word(const TimedWord &word) const;

	/// Get the enabled transitions in a given configuration.
	std::vector<Transition<LocationT, AP>>
	get_enabled_transitions(const Configuration<LocationT> &configuration);

	/**
	 * @brief Get the largest constant any clock is compared to.
	 * @return Time
	 */
	Time get_largest_constant() const;

	/** Get the initial configuration of the automaton.
	 * @return The initial configuration
	 */
	Configuration<LocationT> get_initial_configuration() const;

	/** Check if the given configuration is an accepting configuration of this automaton
	 * @param configuration The configuration to check
	 * @return true if the given configuration is an accepting configuration
	 */
	[[nodiscard]] bool
	is_accepting_configuration(const Configuration<LocationT> &configuration) const;

private:
	std::set<AP>                                                  alphabet_;
	std::set<Location<LocationT>>                                 locations_;
	const Location<LocationT>                                     initial_location_;
	std::set<Location<LocationT>>                                 final_locations_;
	std::set<std::string>                                         clocks_;
	std::multimap<Location<LocationT>, Transition<LocationT, AP>> transitions_;
};

/** Print a multimap of transitions. */
template <typename LocationT, typename AP>
std::ostream &operator<<(
  std::ostream &                                                                     os,
  const std::multimap<Location<LocationT>, automata::ta::Transition<LocationT, AP>> &transitions);

/** Print a set of strings.
 * This is useful, e.g., to print the set of clock resets.
 */
template <typename T>
std::ostream &operator<<(std::ostream &os, const std::set<T> &strings);

template <typename Location>
std::ostream &operator<<(std::ostream &                               os,
                         const automata::ta::Configuration<Location> &configuration);

} // namespace automata::ta

#include "ta.hpp"

#endif /* ifndef SRC_AUTOMATA_INCLUDE_AUTOMATA_TA_H */
