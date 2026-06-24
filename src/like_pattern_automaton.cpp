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
#include <ranges>
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

automata::parsing::LikePatternAutomatonParser::LikePatternAutomatonParser(const std::span<const uint8_t> &match, const Encoder &encoder) {
    std::unique_ptr<LikePatternAutomaton> automaton = LikePatternAutomaton::build(match, encoder);
    if (!automaton) {
        return;
    }

    auto getUselessStates = [&](State* state, State* destination) {
        std::unordered_set<State*> uselessStates;
        std::basic_string<uint8_t> fix{};
        while (state != destination) {
            uselessStates.insert(state);
            fix.push_back(state->transitions.begin()->first);
            state = state->transitions.begin()->second;
        }
        return uselessStates;
    };

    if (automaton->startMatch.has_value() || !automaton->middleMatches.empty()) {
        std::unordered_map<State*, size_t> indexMap{};
        size_t numStates = 0;
        for (auto& middleAutomaton : automaton->middleMatches | std::views::reverse) {
            indexMap[middleAutomaton.defaultTransition] = numStates++;
        }

        auto& prefixAutomaton = automaton->startMatch;

        std::basic_string<uint8_t> prefix{};
        if (prefixAutomaton.has_value()) {
            prefix = prefixAutomaton->deterministicPath;
            auto uselessStates = getUselessStates(prefixAutomaton->startState.get(), prefixAutomaton->actualStartState);

            if (!uselessStates.contains(prefixAutomaton->startState.get())) {
                indexMap[prefixAutomaton->startState.get()] = numStates++;
            }
            for (auto& state: prefixAutomaton->states) {
                if (!uselessStates.contains(&state))
                    indexMap[&state] = numStates++;
            }
        }

        for (auto& middleAutomaton : automaton->middleMatches | std::views::reverse) {
            for (auto& state: middleAutomaton.states) {
                if (&state == middleAutomaton.defaultTransition) {
                    continue;
                }
                indexMap[&state] = numStates++;
            }
        }

        indexMap[automaton->errorState.get()] = numStates++;
        size_t error = indexMap[automaton->errorState.get()];
        for (auto& state: automaton->acceptStates) {
            indexMap[&state] = numStates++;
        }
        size_t firstAccept = indexMap[&automaton->acceptStates.front()];
        size_t start = indexMap[prefixAutomaton.has_value() ? prefixAutomaton->actualStartState : automaton->middleMatches.back().defaultTransition];

        forwardTable = std::make_optional<AutomatonTable>(indexMap, false, error, firstAccept, prefix, start, automaton->middleMatches.size());
    }

    if (automaton->endMatch.has_value()) {
        size_t numStates = 0;

        std::unordered_map<State*, size_t> indexMap{};
        auto& suffixAutomaton = automaton->endMatch;

        auto uselessStates = getUselessStates(suffixAutomaton->startState.get(), suffixAutomaton->actualStartState);
        std::basic_string<uint8_t> suffix = suffixAutomaton->deterministicPath;

        if (!uselessStates.contains(suffixAutomaton->startState.get())) {
            indexMap[suffixAutomaton->startState.get()] = numStates++;
        }
        for (auto& state: suffixAutomaton->states) {
            if (!uselessStates.contains(&state)) {
                indexMap[&state] = numStates++;
            }
        }

        indexMap[automaton->errorState.get()] = numStates++;
        size_t error = indexMap[automaton->errorState.get()];
        for (auto& state: automaton->acceptStates) {
            indexMap[&state] = numStates++;
        }
        size_t firstAccept = indexMap[&automaton->acceptStates.front()];
        size_t start = indexMap[suffixAutomaton->actualStartState];
        backwardsTable = std::make_optional<AutomatonTable>(indexMap, true, error, firstAccept, suffix, start, 0);
    }
}

bool automata::parsing::LikePatternAutomatonParser::parse(const std::span<const uint8_t> &match) const {
    assert(!match.empty());
    int64_t backwardsStrIdx = static_cast<int64_t>(match.size()) - 1;
    uint8_t backwardsSymbolIdx = 8;
    if (backwardsTable.has_value()) {
        if (!backwardsTable->parse<false>(&backwardsStrIdx, &backwardsSymbolIdx, match.data(), match.size()))
            return false;
    }

    if (forwardTable.has_value()) {
        int64_t forwardStrIdx = 0;
        uint8_t forwardSymbolIdx = 0;
        if (!forwardTable->parse<true>(&forwardStrIdx, &forwardSymbolIdx, match.data(), backwardsStrIdx + 1))
            return false;
        else
            return forwardStrIdx < backwardsStrIdx || forwardSymbolIdx <= backwardsSymbolIdx;
    }
    return true;
}


bool automata::parsing::LikePatternAutomatonParser::hasEmptyAutomaton() const {
    return !forwardTable.has_value() && !backwardsTable.has_value();
}

automata::parsing::AutomatonTable::AutomatonTable(std::unordered_map<State*, size_t>& indexMap, bool isBackwards, size_t error, size_t firstAccept, const std::basic_string<uint8_t>& fix, size_t start, size_t N): error(error), firstAccept(firstAccept), fix(fix), start(start), N(N) {
    size_t numStates = indexMap.size();
    if (numStates <= 256) {
        shift = 0;
    } else if (numStates <= 65536) {
        shift = 1;
    } else if (numStates <= 4294967296) {
        shift = 2;
    } else {
        shift = 3;
    }
    size_t numAcceptStates = 9;
    size_t numErrorStates = 1;
    size_t realStates = numStates - numAcceptStates - numErrorStates;

    size_t transitionsAllocation = realStates << (8 + shift);
    size_t transitionArrayAllocation = N * kTransitionArraySize;
    size_t tableAllocation = transitionsAllocation + transitionArrayAllocation;

    size_t levelAllocation = realStates * sizeof(size_t);
    size_t pseudoAcceptAllocation;
    if (isBackwards) {
        pseudoAcceptAllocation = realStates * sizeof(uint8_t);
    } else {
        pseudoAcceptAllocation = 0;
    }
    size_t totalMemoryAllocation = tableAllocation + levelAllocation + pseudoAcceptAllocation;
    if (totalMemoryAllocation % sizeof(AlignmentType) != 0) {
        totalMemoryAllocation = (totalMemoryAllocation / sizeof(AlignmentType) + 1) * sizeof(size_t);
    }
    data = std::make_unique<AlignmentType[]>(totalMemoryAllocation / sizeof(AlignmentType));

    levels = reinterpret_cast<size_t*>(data.get());
    table = reinterpret_cast<uint8_t*>(data.get()) + levelAllocation;
    if (pseudoAcceptAllocation == 0) {
        pseudoAccepts = nullptr;
    } else {
        pseudoAccepts = reinterpret_cast<uint8_t*>(data.get()) + tableAllocation + levelAllocation;
    }

    constexpr auto setBit = [](uint8_t* data, size_t i) {
        data[i / 8] |= (1u << (i % 8));
    };

    auto fillTransitions = [&]<typename T>(T) {
        for (auto& [state, index] : indexMap) {
            if (index >= realStates) {
                continue;
            }
            levels[index] = state->level;
            if (pseudoAccepts) {
                pseudoAccepts[index] = state->endIdx.has_value() ? state->endIdx.value() : kNonPseudoEnd;
            }

            T* row;
            if (index < N) {
                uint8_t* arrayPtr = table + (index << (8 + shift)) + index * kTransitionArraySize;
                for (uint8_t symbol : state->transitions | std::views::keys) {
                    setBit(arrayPtr, symbol);
                }
                row = reinterpret_cast<T*>(arrayPtr + kTransitionArraySize);
            } else {
                row = reinterpret_cast<T*>(table + (index << (8 + shift)) + N * kTransitionArraySize);
            }

            for (size_t c = 0; c < 256; ++c) {
                State* nextState = state->transition(c);
                assert(indexMap.contains(nextState));
                size_t nextIndex = indexMap[nextState];
                row[c] = static_cast<T>(nextIndex);
            }
        }
    };

    switch (shift) {
        case 0:
            fillTransitions(uint8_t{});
            break;
        case 1:
            fillTransitions(uint16_t{});
            break;
        case 2:
            fillTransitions(uint32_t{});
            break;
        case 3:
            fillTransitions(uint64_t{});
            break;
    }
}
