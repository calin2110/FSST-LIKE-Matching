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

#include "like_pattern_automaton.hpp"

#include <fmt/format.h>
#include <filesystem>

#include "pattern.hpp"
#include "common.hpp"

automata::parsing::LikePatternAutomaton::LikePatternAutomaton(): startMatch(), middleMatches(), endMatch(), acceptStates(9), errorState(std::make_unique<State>(nullptr)) {}

std::unique_ptr<automata::parsing::LikePatternAutomaton> automata::parsing::LikePatternAutomaton::build(const std::span<const uint8_t> &match, const Encoder& encoder) {
    assert(!match.empty());
    LikePatternAutomaton automaton{};

    std::vector<State*> precomputed(9);
    for (uint8_t i = 0; i < 9; ++i) {
        precomputed[i] = &automaton.acceptStates[i];
        automaton.acceptStates[i].level = 0;
    }

    std::vector<State*>& precomputedRef = precomputed;
    auto subpatterns = Pattern::splitIntoSubpatterns(match);

    if (subpatterns.back()) {
        automaton.endMatch = subpatterns.back()->createEndAutomaton(encoder, precomputedRef, automaton.errorState.get());
    }

    support::TemporaryStarts tempStarts{};
    for (size_t idx = subpatterns.size() - 2; idx >= 1; --idx) {
        automaton.middleMatches.emplace_back(subpatterns[idx]->createMiddleAutomaton(encoder, precomputedRef, automaton.errorState.get(), tempStarts.get()));
        precomputedRef = automaton.middleMatches.back().starts;
    }

    if (subpatterns.front()) {
        automaton.startMatch = subpatterns.front()->createStartAutomaton(encoder, precomputedRef, automaton.errorState.get());
    }
    for (size_t i = 0; i < 9; ++i) {
        automaton.acceptStates[i].defaultTransition = &automaton.acceptStates[0];
    }

    return std::make_unique<LikePatternAutomaton>(std::move(automaton));
}

std::optional<automata::parsing::LikePatternAutomaton::AutomatonParams> automata::parsing::LikePatternAutomaton::gatherForwardParams() const {
    if (!startMatch.has_value() && middleMatches.empty()) {
        return std::nullopt;
    }
    std::optional<AutomatonParams> ret = std::make_optional<AutomatonParams>();
    ret->start = startMatch.has_value() ? startMatch->actualStartState : middleMatches.back().starts[0];
    ret->deterministicPath = startMatch.has_value() ? startMatch->deterministicPath : std::basic_string<uint8_t>{};
    ret->minLength = startMatch.has_value() ? startMatch->startState->level : middleMatches.back().starts[0]->level;
    ret->acceptStates = &acceptStates;
    ret->error = errorState.get();
    for (const State& acceptState: acceptStates) {
        ret->states.push_back(&acceptState);
    }
    ret->states.push_back(errorState.get());
    if (startMatch.has_value()) {
        ret->states.push_back(startMatch->startState.get());
        for (const State& state: startMatch->states) {
            ret->states.push_back(&state);
        }
    }

    for (const auto& middleMatch: middleMatches) {
        for (const State& state: middleMatch.states) {
            ret->states.push_back(&state);
        }
    }
    return ret;
}

std::optional<automata::parsing::LikePatternAutomaton::AutomatonParams> automata::parsing::LikePatternAutomaton::gatherBackwardsParams() const {
    if (!endMatch.has_value()) {
        return std::nullopt;
    }
    std::optional<AutomatonParams> ret = std::make_optional<AutomatonParams>();
    ret->start = endMatch->actualStartState;
    ret->deterministicPath = endMatch->deterministicPath;
    ret->minLength = endMatch->startState->level;
    ret->acceptStates = &acceptStates;
    ret->error = errorState.get();
    for (const State& acceptState: acceptStates) {
        ret->states.push_back(&acceptState);
    }
    ret->states.push_back(errorState.get());
    ret->states.push_back(endMatch->startState.get());
    for (const State& state: endMatch->states) {
        ret->states.push_back(&state);
    }
    return ret;
}

automata::parsing::LikePatternAutomatonParser::LikePatternAutomatonParser(const std::span<const uint8_t> &match, const Encoder &encoder): automaton(LikePatternAutomaton::build(match, encoder)) {}

bool automata::parsing::LikePatternAutomatonParser::parse(const std::span<const uint8_t> &match) const {
    assert(!match.empty());

    int64_t backwardsStrIdx = static_cast<int64_t>(match.size()) - 1;
    uint8_t backwardsSymbolIdx = 8;
    if (automaton->endMatch.has_value()) {
        std::basic_string_view<uint8_t> match_view(match.data(), match.size());
        std::basic_string_view<uint8_t> suffix_view(automaton->endMatch->deterministicPath.data(), automaton->endMatch->deterministicPath.size());

        if (!match_view.ends_with(suffix_view)) {
            return false;
        }
        backwardsStrIdx = match.size() - suffix_view.size() - 1;
        State* currentState = automaton->endMatch->actualStartState;
        while (backwardsStrIdx + 1 >= currentState->level && currentState != automaton->errorState.get() && !isEndState(currentState, automaton->acceptStates)) {
            currentState = currentState->transition(match[backwardsStrIdx]);
            --backwardsStrIdx;
        }

        if (isEndState(currentState, automaton->acceptStates)) {
            backwardsStrIdx += 2;
            backwardsSymbolIdx = getEndIndex(currentState, automaton->acceptStates);
        } else {
            if (currentState->endIdx.has_value()) {
                ++backwardsStrIdx;
                backwardsSymbolIdx = currentState->endIdx.value();
            } else {
                return false;
            }
        }
    }

    if (!automaton->startMatch.has_value() && automaton->middleMatches.empty()) {
        return true;
    }

    State* currentState;
    size_t forwardStrIdx;
    if (automaton->startMatch.has_value()) {
        std::basic_string_view<uint8_t> match_view(match.data(), backwardsStrIdx + 1);
        std::basic_string_view<uint8_t> prefix_view(automaton->startMatch->deterministicPath.data(), automaton->startMatch->deterministicPath.size());

        if (!match_view.starts_with(prefix_view)) {
            return false;
        }
        currentState = automaton->startMatch->actualStartState;
        forwardStrIdx = prefix_view.size();
    } else {
        currentState = automaton->middleMatches.back().starts[0];
        forwardStrIdx = 0;
    }
    while (currentState != automaton->errorState.get() && forwardStrIdx + currentState->level <= backwardsStrIdx + 1 && !isEndState(currentState, automaton->acceptStates)) {
        currentState = currentState->transition(match[forwardStrIdx]);
        ++forwardStrIdx;
    }

    if (!isEndState(currentState, automaton->acceptStates)) {
        return false;
    }
    uint8_t forwardSymbolIdx = getEndIndex(currentState, automaton->acceptStates);
    return forwardStrIdx < backwardsStrIdx + 1 || forwardSymbolIdx <= backwardsSymbolIdx;
}

bool automata::parsing::LikePatternAutomatonParser::hasEmptyAutomaton() const {
    return automaton.get() == nullptr;
}
