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

#include <ranges>
#include <fmt/format.h>
#include "automata.hpp"

automata::State::State(State *defaultTransition): defaultTransition(defaultTransition), level(UINT64_MAX) {}

automata::State::State() {
    defaultTransition = this;
    level = UINT64_MAX;
}

void automata::State::addTransition(uint8_t symbol, State *destination) {
    assert(!transitions.contains(symbol));
    transitions[symbol] = destination;
}

void automata::State::addEndTransition(uint8_t symbol, uint8_t endIndex, const std::vector<State *> &precomputedEnds) {
    assert(!transitions.contains(symbol));
    if (precomputedEnds[endIndex] != precomputedEnds[0] ) {
        addTransition(symbol, precomputedEnds[endIndex]->transition(symbol));
    } else {
        addTransition(symbol, precomputedEnds[0]);
    }
}

bool automata::State::canTransition(uint8_t symbol) const {
    return transitions.contains(symbol);
}

automata::State* automata::State::transition(uint8_t symbol) const {
    return transitions.contains(symbol) ? transitions.find(symbol)->second : defaultTransition;
}

void automata::State::copyTransitions(const State* source) {
    for (const auto& [symbol, node]: source->transitions) {
        if (!canTransition(symbol)) {
            addTransition(symbol, node);
        }
    }
}

bool automata::isEndState(const State *state, const std::vector<State>& endNodes) {
    return state >= endNodes.data() && state < endNodes.data() + endNodes.size();
}

std::ptrdiff_t automata::getEndIndex(const State* state, const std::vector<State>& endNodes) {
    return state - endNodes.data();
}

automata::FiniteAutomaton::FiniteAutomaton(State *errorNode): errorState(errorNode), defaultTransition(nullptr) {}

automata::FiniteAutomaton::FiniteAutomaton(std::deque<State> &&nodes, State *errorNode, State *defaultTransition): states(std::move(nodes)), errorState(errorNode), defaultTransition(defaultTransition) {}

automata::State * automata::FiniteAutomaton::createState() {
    states.emplace_back(defaultTransition);
    return &states.back();
}

automata::SingleStartFiniteAutomaton::SingleStartFiniteAutomaton(State *errorNode, Direction direction):
    FiniteAutomaton(errorNode), direction(direction), startState(std::make_unique<State>(errorNode)), deterministicPath(), actualStartState(nullptr) {
    defaultTransition = errorNode;
}

automata::SingleStartFiniteAutomaton::SingleStartFiniteAutomaton(std::deque<State> &&nodes, State *errorNode, std::unique_ptr<State> &startNode, Direction direction):
    FiniteAutomaton(std::move(nodes), errorNode, errorNode), direction(direction), startState(std::move(startNode)), deterministicPath(), actualStartState(nullptr) {
    findDeterministicPath();
}

void automata::SingleStartFiniteAutomaton::findDeterministicPath() {
    State* currentNode = startState.get();
    while (!currentNode->endIdx.has_value() && currentNode->defaultTransition == errorState && currentNode->transitions.size() == 1) {
        deterministicPath.push_back(currentNode->transitions.begin()->first);
        currentNode = currentNode->transitions.begin()->second;
    }
    actualStartState = currentNode;
    if (direction == Direction::BACKWARD) {
        std::reverse(deterministicPath.begin(), deterministicPath.end());
    }
}

automata::MultipleStartsFiniteAutomaton::MultipleStartsFiniteAutomaton(State *errorNode): FiniteAutomaton(errorNode), starts(9, nullptr) {}