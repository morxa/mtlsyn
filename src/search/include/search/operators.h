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
 * The word w1 is monotonically dominated by w2 if each partition of w1 is a subset of a partition
 * in w2, and those partitions in w2 are strictly monotonically increasing.
 *
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
	auto current_w2_it = w2.begin();
	for (const auto &w1_partition : w1) {
		// Find the w2 partition that includes the current w1 partition.
		const auto next_w2_it =
		  std::find_if(current_w2_it, w2.end(), [&w1_partition](const auto &w2_partition) {
			  return std::includes(w2_partition.begin(),
			                       w2_partition.end(),
			                       w1_partition.begin(),
			                       w1_partition.end());
		  });
		if (next_w2_it == w2.end()) {
			return false;
		}
		current_w2_it = std::next(next_w2_it);
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
