/***************************************************************************
 *  test_synchronous_product_print.cpp - Test output for sync product objects
 *
 *  Created:   Tue  9 Feb 13:21:10 CET 2021
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

#include "mtl/MTLFormula.h"
#include "synchronous_product/synchronous_product.h"

#include <catch2/catch.hpp>
#include <sstream>

namespace {

using synchronous_product::ABRegionSymbol;
using TARegionState  = synchronous_product::TARegionState<std::string>;
using ATARegionState = synchronous_product::ATARegionState<std::string>;
using AP             = logic::AtomicProposition<std::string>;
using MTLFormula     = logic::MTLFormula<std::string>;

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

} // namespace