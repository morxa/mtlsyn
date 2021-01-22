/***************************************************************************
 *  test_synchronous_product.cpp - Test synchronous products
 *
 *  Created:   Mon 21 Dec 16:56:08 CET 2020
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
 *  Read the full text in the LICENSE.md file.
 */

#include "automata/automata.h"
#include "automata/ta.h"
#include "automata/ta_regions.h"
#include "mtl/MTLFormula.h"
#include "synchronous_product/synchronous_product.h"
#include "utilities/numbers.h"

#include <catch2/catch.hpp>
#include <sstream>
#include <string>
#include <variant>

namespace {

using automata::ClockSetValuation;
using automata::ClockValuation;
using synchronous_product::ABRegionSymbol;
using synchronous_product::ATAConfiguration;
using ATARegionState = synchronous_product::ATARegionState<std::string>;
using synchronous_product::get_canonical_word;
using synchronous_product::get_time_successor;
using synchronous_product::InvalidCanonicalWordException;
using synchronous_product::is_valid_canonical_word;
using TARegionState = synchronous_product::TARegionState<std::string>;

TEST_CASE("Print a TARegionState", "[print]")
{
	const TARegionState state{"s", "c", 1};
	std::stringstream   stream;
	stream << state;
	REQUIRE(stream.str() == "(s, c, 1)");
}

TEST_CASE("Print an ATARegionState", "[print]")
{
	const ATARegionState state{logic::MTLFormula<std::string>{
	                             logic::AtomicProposition<std::string>{"s"}},
	                           2};
	std::stringstream    stream;
	stream << state;
	REQUIRE(stream.str() == "(s, 2)");
}

TEST_CASE("Print an ABRegionSymbol", "[print]")
{
	{
		const ABRegionSymbol<std::string, std::string> symbol = TARegionState("s", "c", 1);
		std::stringstream                              stream;
		stream << symbol;
		REQUIRE(stream.str() == "(s, c, 1)");
	}
	{
		const ABRegionSymbol<std::string, std::string> symbol =
		  ATARegionState(logic::MTLFormula<std::string>{logic::AtomicProposition<std::string>{"s"}}, 2);
		std::stringstream stream;
		stream << symbol;
		REQUIRE(stream.str() == "(s, 2)");
	}
}

TEST_CASE("Print a set of ABRegionSymbols (one Abs_i)", "[print]")
{
	{
		const std::set<ABRegionSymbol<std::string, std::string>> word;
		std::stringstream                                        stream;
		stream << word;
		CHECK(stream.str() == "{}");
	}
	{
		std::set<ABRegionSymbol<std::string, std::string>> word{
		  TARegionState("s", "c", 1),
		  ATARegionState(logic::MTLFormula<std::string>{logic::AtomicProposition<std::string>{"s"}},
		                 2)};
		std::stringstream stream;
		stream << word;
		CHECK(stream.str() == "{ (s, c, 1), (s, 2) }");
	}
}

TEST_CASE("Print the canonical word H(s)", "[print]")
{
	{
		const std::vector<std::set<ABRegionSymbol<std::string, std::string>>> word;
		std::stringstream                                                     stream;
		stream << word;
		CHECK(stream.str() == "[]");
	}
	{
		std::vector<std::set<ABRegionSymbol<std::string, std::string>>> word;
		word.push_back(std::set<ABRegionSymbol<std::string, std::string>>(
		  {TARegionState("s", "c", 1),
		   ATARegionState(logic::MTLFormula<std::string>{logic::AtomicProposition<std::string>{"s"}},
		                  2)}));
		{
			std::stringstream stream;
			stream << word;
			CHECK(stream.str() == "[ { (s, c, 1), (s, 2) } ]");
		}
		word.push_back(std::set<ABRegionSymbol<std::string, std::string>>(
		  {TARegionState("s", "c2", 5),
		   ATARegionState(logic::MTLFormula<std::string>{logic::AtomicProposition<std::string>{"a"}},
		                  3)}));
		{
			std::stringstream stream;
			stream << word;
			CHECK(stream.str() == "[ { (s, c, 1), (s, 2) }, { (s, c2, 5), (a, 3) } ]");
		}
		word.push_back(
		  std::set<ABRegionSymbol<std::string, std::string>>({TARegionState("s2", "c3", 10)}));
		{
			std::stringstream stream;
			stream << word;
			CHECK(stream.str() == "[ { (s, c, 1), (s, 2) }, { (s, c2, 5), (a, 3) }, { (s2, c3, 10) } ]");
		}
	}
}

TEST_CASE("Get a canonical word of a simple state", "[canonical_word]")
{
	const logic::MTLFormula                        f{logic::AtomicProposition<std::string>{"a"}};
	const ATAConfiguration<std::string>            ata_configuration = {{f, 0.0}};
	const ClockSetValuation                        v{{"c", 0}};
	const automata::ta::Configuration<std::string> ta_configuration{"s", v};
	const auto                                     w =
	  get_canonical_word<std::string, std::string>(ta_configuration, ata_configuration, 5);
	INFO("Canonical word: " << w);
	REQUIRE(w.size() == 1);
	const auto &abs1 = *w.begin();
	REQUIRE(abs1.size() == 2);
	const auto &symbol1 = *abs1.begin();
	REQUIRE(std::holds_alternative<TARegionState>(symbol1));
	CHECK(std::get<TARegionState>(symbol1) == TARegionState("s", "c", 0));
	const auto &symbol2 = *std::next(abs1.begin());
	REQUIRE(std::holds_alternative<ATARegionState>(symbol2));
	CHECK(std::get<ATARegionState>(symbol2) == ATARegionState(f, 0));
}

TEST_CASE("Get a canonical word of a more complex state", "[canonical_word]")
{
	const logic::MTLFormula                        a{logic::AtomicProposition<std::string>{"a"}};
	const logic::MTLFormula                        b{logic::AtomicProposition<std::string>{"b"}};
	const ATAConfiguration<std::string>            ata_configuration = {{a, 0.5}, {b, 1.5}};
	const ClockSetValuation                        v{{"c1", 0.1}, {"c2", 0.5}};
	const automata::ta::Configuration<std::string> ta_configuration{"s", v};
	const auto                                     w =
	  get_canonical_word<std::string, std::string>(ta_configuration, ata_configuration, 3);
	INFO("Canonical word: " << w);
	REQUIRE(w.size() == 2);
	{
		const auto &abs1 = *w.begin();
		REQUIRE(abs1.size() == 1);
		const auto &symbol1 = *abs1.begin();
		REQUIRE(std::holds_alternative<TARegionState>(symbol1));
		CHECK(std::get<TARegionState>(symbol1) == TARegionState("s", "c1", 1));
	}
	{
		const auto &abs2 = *std::next(w.begin());
		REQUIRE(abs2.size() == 3);
		const auto &symbol1 = *abs2.begin();
		REQUIRE(std::holds_alternative<TARegionState>(symbol1));
		CHECK(std::get<TARegionState>(symbol1) == TARegionState("s", "c2", 1));
		const auto &symbol2 = *std::next(abs2.begin());
		REQUIRE(std::holds_alternative<ATARegionState>(symbol2));
		CHECK(std::get<ATARegionState>(symbol2) == ATARegionState(a, 1));
		const auto &symbol3 = *std::next(abs2.begin(), 2);
		REQUIRE(std::holds_alternative<ATARegionState>(symbol3));
		CHECK(std::get<ATARegionState>(symbol3) == ATARegionState(b, 3));
	}
}

TEST_CASE("Validate a canonical word", "[canonical_word]")
{
	using CanonicalABWord = synchronous_product::CanonicalABWord<std::string, std::string>;
	CHECK_THROWS_AS(is_valid_canonical_word(CanonicalABWord{}), InvalidCanonicalWordException);
	CHECK(is_valid_canonical_word(
	  CanonicalABWord({{TARegionState{"s0", "c0", 0}}, {TARegionState{"s0", "c1", 1}}})));
	CHECK_THROWS_AS(is_valid_canonical_word(CanonicalABWord({{}})), InvalidCanonicalWordException);
	CHECK_THROWS_AS(is_valid_canonical_word(CanonicalABWord(
	                  {{TARegionState{"s0", "c0", 0}, TARegionState{"s0", "c1", 1}}})),
	                InvalidCanonicalWordException);
	CHECK_THROWS_AS(is_valid_canonical_word(CanonicalABWord(
	                  {{TARegionState{"s0", "c0", 0}}, {TARegionState{"s0", "c1", 0}}})),
	                InvalidCanonicalWordException);
	CHECK_THROWS_AS(is_valid_canonical_word(CanonicalABWord(
	                  {{TARegionState{"s0", "c0", 0}}, {TARegionState{"s0", "c1", 2}}})),
	                InvalidCanonicalWordException);
}

TEST_CASE("Get the time successor for a canonical AB word", "[canonical_word]")
{
	// TODO rewrite test cases to only contain valid words
	using CanonicalABWord = synchronous_product::CanonicalABWord<std::string, std::string>;
	CHECK(get_time_successor(
	        CanonicalABWord({{TARegionState{"s0", "c0", 0}}, {TARegionState{"s0", "c1", 1}}}), 3)
	      == CanonicalABWord({{TARegionState{"s0", "c1", 2}}, {TARegionState{"s0", "c0", 1}}}));
	CHECK(get_time_successor(CanonicalABWord({{TARegionState{"s0", "c0", 0}}}), 3)
	      == CanonicalABWord({{TARegionState{"s0", "c0", 1}}}));
	//	CHECK(get_time_successor(CanonicalABWord({{TARegionState{"s0", "c0", 0}},
	//	                                          {TARegionState{"s0", "c1", 1}},
	//	                                          {TARegionState{"s0", "c2", 2}}}),
	//	                         3)
	//	      == CanonicalABWord({{TARegionState{"s0", "c2", 3}},
	//	                          {TARegionState{"s0", "c0", 1}},
	//	                          {TARegionState{"s0", "c1", 1}}}));
	// CHECK(get_time_successor(
	//        CanonicalABWord({{TARegionState{"s0", "c0", 1}}, {TARegionState{"s0", "c1", 2}}}), 3)
	//      == CanonicalABWord({{TARegionState{"s0", "c1", 3}}, {TARegionState{"s0", "c0", 1}}}));
	CHECK(get_time_successor(
	        CanonicalABWord({{TARegionState{"s0", "c0", 1}}, {TARegionState{"s0", "c1", 1}}}), 3)
	      == CanonicalABWord({{TARegionState{"s0", "c1", 2}}, {TARegionState{"s0", "c0", 1}}}));
	const logic::AtomicProposition<std::string> a{"a"};
	const logic::AtomicProposition<std::string> b{"b"};
	// CHECK(get_time_successor(CanonicalABWord({{ATARegionState{a, 0}},
	//                                          {ATARegionState{b, 1}},
	//                                          {ATARegionState{a || b, 2}}}),
	//                         3)
	//      == CanonicalABWord(
	//        {{ATARegionState{a || b, 3}}, {ATARegionState{a, 1}}, {ATARegionState{b, 1}}}));
	CHECK(get_time_successor(CanonicalABWord({{ATARegionState{a, 7}}}), 3)
	      == CanonicalABWord({{ATARegionState{a, 7}}}));
	CHECK(get_time_successor(CanonicalABWord({{ATARegionState{b, 3}}, {ATARegionState{a, 7}}}), 3)
	      == CanonicalABWord({{ATARegionState{b, 4}}, {ATARegionState{a, 7}}}));

	// TODO fails because only the first region index is incremented, resulting in an invalid word
	// CHECK(
	//  get_time_successor(CanonicalABWord(
	//                       {{ATARegionState{b, 3}, ATARegionState{a,
	//                       7}}}),
	//                     3)
	//  == CanonicalABWord(
	//    {{ATARegionState{b, 4}, ATARegionState{a, 7}}}));

	// CHECK(
	//  get_time_successor(CanonicalABWord({{TARegionState{"s0", "c0", 3}},
	//                                      {TARegionState{"s1", "c0", 4}},
	//                                      {ATARegionState{a, 7}}}),
	//                     3)
	//  == CanonicalABWord(
	//    {{TARegionState{"s1", "c0", 5}}, {ATARegionState{a, 7}}, {TARegionState{"s0", "c0", 3}}}));
	CHECK(get_time_successor(CanonicalABWord({{ATARegionState{b, 1}, ATARegionState{a, 3}}}), 3)
	      == CanonicalABWord({{ATARegionState{b, 2}, ATARegionState{a, 4}}}));
}

TEST_CASE("Get a concrete candidate for a canonical word", "[canonical_word]")
{
	using automata::Time;
	using automata::ta::Integer;
	using CanonicalABWord = synchronous_product::CanonicalABWord<std::string, std::string>;
	using TAConf          = synchronous_product::TAConfiguration<std::string>;
	using ATAConf         = synchronous_product::ATAConfiguration<std::string>;
	using Candidate       = std::pair<TAConf, ATAConf>;
	using synchronous_product::get_candidate;
	using utilities::getFractionalPart;
	using utilities::getIntegerPart;
	const logic::AtomicProposition<std::string> a{"a"};
	const logic::AtomicProposition<std::string> b{"b"};

	// single state with fractional part 0, clock 0
	CHECK(get_candidate(CanonicalABWord({{TARegionState{"s0", "c0", 0}}}))
	      == Candidate(TAConf{std::pair<std::string, automata::ClockSetValuation>("s0", {{"c0", 0}})},
	                   ATAConf{}));
	// single state with fractional part 0, clock != 0
	CHECK(get_candidate(CanonicalABWord({{TARegionState{"s0", "c0", 2}}}))
	      == Candidate(TAConf{std::pair<std::string, automata::ClockSetValuation>("s0", {{"c0", 1}})},
	                   ATAConf{}));

	{
		// single state with non-zero fractional part in (0, 1)
		const Candidate cand = get_candidate(CanonicalABWord({{TARegionState{"s0", "c0", 1}}}));
		CHECK(cand.first.second.at("c0") > 0.0);
		CHECK(cand.first.second.at("c0") < 1.0);
		// The ATA configuration must be empty.
		CHECK(cand.second.empty());
	}

	{
		// single state with non-zero fractional part not in (0, 1)
		const Candidate cand = get_candidate(CanonicalABWord({{TARegionState{"s0", "c0", 5}}}));
		CHECK(cand.first.second.at("c0") > 2.0);
		CHECK(cand.first.second.at("c0") < 3.0);
		// The ATA configuration must be empty.
		CHECK(cand.second.empty());
	}

	// A single ATA state
	CHECK(get_candidate(CanonicalABWord({{ATARegionState{a, 0}}})) == Candidate({}, {{a, 0}}));
	CHECK(get_candidate(CanonicalABWord({{ATARegionState{a, 2}}})) == Candidate({}, {{a, 1}}));
	{
		// A single ATA state with fractional part in (0, 1)
		const Candidate cand = get_candidate(CanonicalABWord({{ATARegionState{a, 1}}}));
		REQUIRE(cand.second.size() == 1);
		const auto v = cand.second.begin()->second;
		CHECK(getFractionalPart<Integer>(v) > 0);
		CHECK(getIntegerPart<Integer>(v) == 0);
	}

	{
		// A single ATA state with fractional part not in (0, 1)
		const Candidate cand = get_candidate(CanonicalABWord({{ATARegionState{a, 3}}}));
		REQUIRE(cand.second.size() == 1);
		const auto v = cand.second.begin()->second;
		CHECK(getFractionalPart<Integer>(v) > 0);
		CHECK(getIntegerPart<Integer>(v) == 1);
	}

	{
		// two clocks, both non-fractional with same integer parts
		const Candidate cand = get_candidate(
		  CanonicalABWord({{TARegionState{"s0", "c0", 2}, TARegionState{"s0", "c1", 2}}}));
		CHECK(getFractionalPart<Integer>(cand.first.second.at("c0")) == 0);
		CHECK(getFractionalPart<Integer>(cand.first.second.at("c1")) == 0);
		CHECK(getIntegerPart<Integer>(cand.first.second.at("c0"))
		      == getIntegerPart<Integer>(cand.first.second.at("c1")));
		// The ATA configuration must be empty.
		CHECK(cand.second.empty());
	}

	{
		// two clocks, both non-fractional but with different integer parts
		const Candidate cand = get_candidate(
		  CanonicalABWord({{TARegionState{"s0", "c0", 0}, TARegionState{"s0", "c1", 2}}}));
		CHECK(getFractionalPart<Integer>(cand.first.second.at("c0")) == 0);
		CHECK(getFractionalPart<Integer>(cand.first.second.at("c1")) == 0);
		CHECK(getIntegerPart<Integer>(cand.first.second.at("c0"))
		      < getIntegerPart<Integer>(cand.first.second.at("c1")));
		// The ATA configuration must be empty.
		CHECK(cand.second.empty());
	}

	{
		// two states, one with a clock with fractional part, the other one without
		const Candidate cand = get_candidate(
		  CanonicalABWord({{TARegionState{"s0", "c0", 2}}, {TARegionState{"s0", "c1", 1}}}));
		CHECK(cand.first.second.at("c0") == 1.0);
		CHECK(cand.first.second.at("c1") > 0.0);
		CHECK(cand.first.second.at("c1") < 1.0);
		// The ATA configuration must be empty.
		CHECK(cand.second.empty());
	}

	{
		// two states, both clocks fractional with equal fractional parts and equal integer parts
		const Candidate cand = get_candidate(
		  CanonicalABWord({{TARegionState{"s0", "c0", 1}, TARegionState{"s0", "c1", 1}}}));
		CHECK(cand.first.second.at("c0") == cand.first.second.at("c1"));
	}

	{
		// two states, both clocks fractional with equal fractional parts but different integer parts
		const Candidate cand = get_candidate(
		  CanonicalABWord({{TARegionState{"s0", "c0", 1}, TARegionState{"s0", "c1", 3}}}));
		CHECK(getFractionalPart<Integer>(cand.first.second.at("c0"))
		      == getFractionalPart<Integer>(cand.first.second.at("c1")));
		CHECK(getIntegerPart<Integer>(cand.first.second.at("c0"))
		      < getIntegerPart<Integer>(cand.first.second.at("c1")));
	}

	{
		// two states, both clocks fractional with different fractional parts but same integer parts
		const Candidate cand = get_candidate(
		  CanonicalABWord({{TARegionState{"s0", "c0", 1}}, {TARegionState{"s0", "c1", 1}}}));
		CHECK(cand.first.second.at("c0") < cand.first.second.at("c1"));
		CHECK(getFractionalPart<Integer>(cand.first.second.at("c0"))
		      < getFractionalPart<Integer>(cand.first.second.at("c1")));
		CHECK(getIntegerPart<Integer>(cand.first.second.at("c0"))
		      == getIntegerPart<Integer>(cand.first.second.at("c1")));
	}

	{
		// two states, both clocks fractional with different fractional and integer parts
		const Candidate cand = get_candidate(
		  CanonicalABWord({{TARegionState{"s0", "c0", 1}}, {TARegionState{"s0", "c1", 3}}}));
		CHECK(cand.first.second.at("c0") < cand.first.second.at("c1"));
		CHECK(getFractionalPart<Integer>(cand.first.second.at("c0"))
		      < getFractionalPart<Integer>(cand.first.second.at("c1")));
		CHECK(getIntegerPart<Integer>(cand.first.second.at("c0"))
		      < getIntegerPart<Integer>(cand.first.second.at("c1")));
	}

	{
		// several clocks with different regions
		const Candidate cand =
		  get_candidate(CanonicalABWord({{TARegionState{"s0", "c0", 0}},
		                                 {TARegionState{"s0", "c1", 1}, TARegionState("s0", "c2", 3)},
		                                 {TARegionState{"s0", "c3", 1}}}));
		CHECK(cand.first.second.at("c0") == 0.0);
		CHECK(cand.first.second.at("c1") > 0.0);
		CHECK(cand.first.second.at("c2") > 1.0);
		CHECK(cand.first.second.at("c3") > 0.0);
		CHECK(cand.first.second.at("c1") < 1.0);
		CHECK(cand.first.second.at("c2") < 2.0);
		CHECK(cand.first.second.at("c3") < 1.0);
		CHECK(cand.first.second.at("c1") == cand.first.second.at("c2") - 1.0);
		CHECK(cand.first.second.at("c1") < cand.first.second.at("c3"));
	}
}

} // namespace
