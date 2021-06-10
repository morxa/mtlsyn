/***************************************************************************
 *  operators.h - Operators for relations between words
 *
 *  Created:   Mon  8 Feb 14:01:53 CET 2021
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

#pragma once

#include "canonical_word.h"

#include <algorithm>

namespace search {

template <typename LocationT, typename ActionT>
class SearchTreeNode;

/**
 * @brief Checks if the word w1 is monotonically dominated by w2.
 *
 * @tparam LocationT
 * @tparam ActionT
 * @param w1 The word which may be dominated.
 * @param w2 The potentially dominating word.
 * @return true if w2 dominates w1.
 * @return false otherwise.
 */
template <typename LocationT, typename ActionT>
bool
is_monotonically_dominated(const CanonicalABWord<LocationT, ActionT> &w1,
                           const CanonicalABWord<LocationT, ActionT> &w2)
{
	// required to guarantee global monotonicity in w2
	std::size_t next_w2_idx = 0;
	// iterate over all sets in w1
	for (std::size_t w1_idx = 0; w1_idx < w1.size(); ++w1_idx) {
		bool found = false;
		// find matching set in w2, keep track of monotonicity via initialization of w2_idx
		for (std::size_t w2_idx = next_w2_idx; w2_idx < w2.size(); ++w2_idx) {
			if (std::includes(
			      w2[w2_idx].begin(), w2[w2_idx].end(), w1[w1_idx].begin(), w1[w1_idx].end())) {
				next_w2_idx = w2_idx + 1;
				found       = true;
				break;
			}
		}
		if (!found) {
			return false;
		}
	}
	return true;
}

/**
 * @brief Checks if each word of the first set is monotonically dominated by some word in the second
 * set.
 *
 * @tparam LocationT
 * @tparam ActionT
 * @param set1 First set of canonical words which is to be dominated.
 * @param set2 Second set of canonical words which should dominate the first set.
 * @return true if all words in the first set are dominated by some word in the second set.
 * @return false otherwise.
 */
template <typename LocationT, typename ActionT>
bool
is_monotonically_dominated(const std::set<CanonicalABWord<LocationT, ActionT>> &set1,
                           const std::set<CanonicalABWord<LocationT, ActionT>> &set2)
{
	return std::all_of(set2.begin(), set2.end(), [&set1](const auto &word2) {
		return std::any_of(set1.begin(), set1.end(), [&word2](const auto &word1) {
			return is_monotonically_dominated(word1, word2);
		});
	});
}

// TODO fix the broken monotonic domination check
/** Check if there is an ancestor that monotonally dominates the given node
 * @param node The node to check
 */
template <typename LocationT, typename ActionT>
bool
dominates_ancestor(__attribute__((unused)) SearchTreeNode<LocationT, ActionT> *node)
{
	/*
	const Node *ancestor = node->parent;
	for (const auto & parent : parents) {
	while (ancestor != nullptr) {
	  if (is_monotonically_dominated(ancestor->words, node->words)) {
	    return true;
	  }
	  ancestor = ancestor->parent;
	}
	*/
	return false;
}

} // namespace search
