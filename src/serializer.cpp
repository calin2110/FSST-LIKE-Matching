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

#include "serializer.hpp"
#include <fmt/format.h>


void automata::serializer::serializeSymbolTable(const std::shared_ptr<libfsst::SymbolTable>& symTable, std::basic_stringstream<uint8_t>& stream) {
    uint8_t nSymbols = symTable->nSymbols;
    writeToStream(stream, &nSymbols, sizeof(nSymbols));
    for (uint8_t i = 0; i < nSymbols; ++i) {
        uint8_t len = symTable->symbols[i].val.num == 0 ? 0 : symTable->symbols[i].length();
        writeToStream(stream, &len, sizeof(len));
        writeToStream(stream, symTable->symbols[i].val.str, len);
    }
}

void automata::serializer::serializeStateWithoutDefaultTransition(const State* state, std::basic_stringstream<uint8_t>& stream) {
    uint64_t stateptr = reinterpret_cast<uintptr_t>(state);
    serializeStateWithoutTransitions(state, stream);
    uint64_t numTransitions = state->transitions.size();
    writeToStream(stream, &numTransitions, sizeof(numTransitions));
    for (const auto& [symbol, destination]: state->transitions) {
        writeToStream(stream, &symbol, sizeof(symbol));
        stateptr = reinterpret_cast<uintptr_t>(destination);
        writeToStream(stream, &stateptr, sizeof(stateptr));
    }
}

void automata::serializer::serializeStateWithoutTransitions(const State* state, std::basic_stringstream<uint8_t>& stream) {
    uint64_t stateptr = reinterpret_cast<uintptr_t>(state);
    writeToStream(stream, &stateptr, sizeof(stateptr));
    writeToStream(stream, &state->level, sizeof(state->level));
    uint8_t endIdx = state->endIdx.has_value() ? state->endIdx.value() : 0xFF;
    writeToStream(stream, &endIdx, sizeof(endIdx));
}

void automata::serializer::serializeStateFull(const State* state, std::basic_stringstream<uint8_t>& stream) {
    serializeStateWithoutDefaultTransition(state, stream);
    uintptr_t stateptr = reinterpret_cast<uintptr_t>(state->defaultTransition);
    writeToStream(stream, &stateptr, sizeof(stateptr));
}

automata::serializer::SingleStartFiniteAutomataSerializer::SingleStartFiniteAutomataSerializer(
    SingleStartFiniteAutomaton &automaton, std::vector<State> &endStates, std::shared_ptr<libfsst::SymbolTable> &symTable
): automaton(std::move(automaton)), endStates(endStates), symTable(symTable) {}

std::basic_string<uint8_t> automata::serializer::SingleStartFiniteAutomataSerializer::serialize() const {
    std::basic_stringstream<uint8_t> stream{};
    serializeSymbolTable(symTable, stream);
    serializeStateFull(automaton.startState.get(), stream);
    serializeStateWithoutTransitions(automaton.errorState, stream);

    size_t numEnds = endStates.size();
    writeToStream(stream, &numEnds, sizeof(numEnds));
    for (State& end: endStates) {
        serializeStateWithoutTransitions(&end, stream);
    }

    size_t numStates = automaton.states.size();
    writeToStream(stream, &numStates, sizeof(numStates));
    for (const State& state: automaton.states) {
        serializeStateFull(&state, stream);
    }
    return stream.str();
}

automata::serializer::MultipleStartsFiniteAutomataSerializer::MultipleStartsFiniteAutomataSerializer(
    MultipleStartsFiniteAutomaton &automaton, std::vector<State> &endStates, std::shared_ptr<libfsst::SymbolTable> &symTable
): automaton(std::move(automaton)), endStates(endStates), symTable(symTable) {}

std::basic_string<uint8_t> automata::serializer::MultipleStartsFiniteAutomataSerializer::serialize() const {
    std::basic_stringstream<uint8_t> stream{};
    serializeSymbolTable(symTable, stream);
    size_t numStarts = 1;
    for (uint8_t idx = 1; idx < 8; ++idx) {
        numStarts += (automaton.starts[idx] != automaton.starts[idx + 1]);
    }
    writeToStream(stream, &numStarts, sizeof(numStarts));
    serializeStateFull(automaton.starts[0], stream);
    for (uint8_t idx = 1; idx < 8; ++idx) {
        if (automaton.starts[idx] != automaton.starts[idx + 1]) {
            serializeStateFull(automaton.starts[idx], stream);
        }
    }

    serializeStateWithoutTransitions(automaton.errorState, stream);

    size_t numEnds = endStates.size();
    writeToStream(stream, &numEnds, sizeof(numEnds));
    for (State& end: endStates) {
        serializeStateWithoutTransitions(&end, stream);
    }

    size_t numStates = automaton.states.size() - 1;
    writeToStream(stream, &numStates, sizeof(numStates));
    for (const State& state: automaton.states) {
        if (&state == automaton.starts[0]) {
            continue;
        }
        serializeStateFull(&state, stream);
    }
    return stream.str();
}

automata::serializer::LikePatternAutomatonSerializer::LikePatternAutomatonSerializer(
    std::unique_ptr<parsing::LikePatternAutomaton> &automaton, std::shared_ptr<libfsst::SymbolTable> &symTable
): automaton(std::move(automaton)), symTable(symTable) {}

std::basic_string<uint8_t> automata::serializer::LikePatternAutomatonSerializer::serialize() const {
    std::basic_stringstream<uint8_t> stream{};
    serializeSymbolTable(symTable, stream);

    const State* errorState = automaton->errorState.get();
    serializeStateFull(errorState, stream);

    std::vector<State>& acceptStates = automaton->acceptStates;
    size_t numAcceptStates = acceptStates.size();
    writeToStream(stream, &numAcceptStates, sizeof(numAcceptStates));
    for (const State& state: acceptStates) {
       serializeStateFull(&state, stream);
    }

    size_t numFwdAutomatons = static_cast<size_t>(automaton->startMatch.has_value()) + automaton->middleMatches.size();
    writeToStream(stream, &numFwdAutomatons, sizeof(numFwdAutomatons));

    if (automaton->startMatch.has_value()) {
        size_t numStates = automaton->startMatch->states.size() + 1;
        writeToStream(stream, &numStates, sizeof(numStates));
        serializeStateFull(automaton->startMatch->startState.get(), stream);
        for (const State& state:  automaton->startMatch->states) {
           serializeStateFull(&state, stream);
        }
    }

    for (auto& middleAutomaton: automaton->middleMatches) {
        size_t numStates = middleAutomaton.states.size();
        writeToStream(stream, &numStates, sizeof(numStates));
        for (const State& state: middleAutomaton.states) {
           serializeStateFull(&state, stream);
        }
    }

    size_t numBwdAutomatons = automaton->endMatch.has_value();
    writeToStream(stream, &numBwdAutomatons, sizeof(numBwdAutomatons));

    if (automaton->endMatch.has_value()) {
        size_t numStates = automaton->endMatch->states.size() + 1;
        writeToStream(stream, &numStates, sizeof(numStates));
        serializeStateFull(automaton->endMatch->startState.get(), stream);
        for (const State& state:  automaton->endMatch->states) {
           serializeStateFull(&state, stream);
        }
    }

    return stream.str();
}