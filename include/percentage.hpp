// this software is distributed under the MIT License (http://www.opensource.org/licenses/MIT):
//
// Copyright (c) 2026 Calin Pop George
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef FSST_LIKE_MATCHING_PERCENTAGE_HPP
#define FSST_LIKE_MATCHING_PERCENTAGE_HPP

#include "automata.hpp"

namespace automata::percentage {
    State* constructPrefixAutomaton(const std::basic_string<uint8_t> &pattern, size_t index, const Encoder &encoder, const std::vector<State*>& precomputedEnds, const std::function<State*()>& createState, State* start);
    std::vector<State*> initialisePseudoEnds(FiniteAutomaton* automaton, const std::vector<State*>& precomputedEnds);
    void constructSuffixAutomaton(const std::span<const uint8_t> &pattern, const Encoder &encoder, FiniteAutomaton* automaton, const std::vector<State*>& pseudoEnds, State* startState);
    State* constructCachedPrefixAutomaton(
        const basic_string<uint8_t>& pattern, size_t index, const Encoder &encoder, const std::vector<State*>& precomputedEnds,
        const std::function<State*()>& createState, bool enableCaching, std::vector<State*>& cache, State* errorState,
        const std::unordered_set<const State*>& currentStates, State* start
    );
    void integrateSymbolsContainingPattern(const basic_string<uint8_t>& pattern, const Encoder &encoder, const std::vector<State*>& precomputedEnds, std::vector<State*>& startStates);
    void connectStartsToSubautomaton(
        const basic_string<uint8_t>& pattern, const Encoder &encoder, const std::vector<State*>& precomputedEnds,
        const std::function<State*()>& createState, State* errorState, std::vector<State*>& startStates, const std::unordered_set<const State*>& currentStates,
        std::array<State*, 255>& transitionStates, std::vector<State*>& cache, uint8_t i, const std::vector<uint8_t>& symbols
    );
    void linkStarts(std::vector<State*>& startStates);
    void splitStarts(std::vector<State*>& startStates, const std::unordered_set<const State*>& currentStates);
    void constructFallbackTransitions(std::vector<State*>& startStates, const std::unordered_set<const State*>& currentStates);
}
#endif //FSST_LIKE_MATCHING_PERCENTAGE_HPP