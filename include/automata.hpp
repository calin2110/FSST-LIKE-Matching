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

#ifndef FINITE_AUTOMATA_H
#define FINITE_AUTOMATA_H
#include <assert.h>
#include <list>
#include <deque>

#include <unordered_map>
#include <memory>
#include <vector>
#include <functional>
#include "encoder.hpp"

namespace automata {

    struct State {
    public:
        std::unordered_map<uint8_t, State*> transitions;
        State* defaultTransition;
        uint64_t level;
        std::optional<uint8_t> endIdx;

        explicit State(State* defaultTransition);
        State();
        void addTransition(uint8_t symbol, State* destination);
        void addEndTransition(uint8_t symbol, uint8_t endIndex, const std::vector<State*>& precomputedEnds);
        bool canTransition(uint8_t symbol) const;
        State* transition(uint8_t symbol) const;
        void copyTransitions(const State* source);
    };

    bool isEndState(const State* state, const std::vector<State>& endStates);
    std::ptrdiff_t getEndIndex(const State* state, const std::vector<State>& endStates);

    class FiniteAutomaton {
    public:
        std::deque<State> states;
        State* errorState;
        State* defaultTransition;

        explicit FiniteAutomaton(State* errorState);
        FiniteAutomaton(std::deque<State>&& states, State* errorState, State* defaultTransition);
        State* createState();

    };

    struct SingleStartFiniteAutomaton: public FiniteAutomaton {
    public:
        enum class Direction {
            FORWARD = 1,
            BACKWARD = -1
        };

        SingleStartFiniteAutomaton(State* errorState, Direction direction);

        std::unique_ptr<State> startState;
        Direction direction;
        std::basic_string<uint8_t> deterministicPath;
        State* actualStartState;

        SingleStartFiniteAutomaton(const SingleStartFiniteAutomaton&) = delete;
        SingleStartFiniteAutomaton& operator=(const SingleStartFiniteAutomaton&) = delete;
        SingleStartFiniteAutomaton(SingleStartFiniteAutomaton&&) noexcept = default;
        SingleStartFiniteAutomaton(std::deque<State>&& states, State* errorState, std::unique_ptr<State>& startState, Direction direction);
        SingleStartFiniteAutomaton& operator=(SingleStartFiniteAutomaton&&) noexcept = default;

        void findDeterministicPath();
        friend class SingleStartFiniteAutomataSerializer;
        friend class LikePatternAutomatonSerializer;
        friend class LikePatternAutomaton;
    };

    struct MultipleStartsFiniteAutomaton: public FiniteAutomaton {
    public:
        std::vector<State*> starts;
        explicit MultipleStartsFiniteAutomaton(State* errorNode);
        MultipleStartsFiniteAutomaton(const MultipleStartsFiniteAutomaton&) = delete;
        MultipleStartsFiniteAutomaton& operator=(const MultipleStartsFiniteAutomaton&) = delete;
        MultipleStartsFiniteAutomaton(MultipleStartsFiniteAutomaton&&) noexcept = default;
        MultipleStartsFiniteAutomaton& operator=(MultipleStartsFiniteAutomaton&&) noexcept = default;
    };
}
#endif //FINITE_AUTOMATA_H
