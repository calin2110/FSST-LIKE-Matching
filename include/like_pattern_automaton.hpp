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

#ifndef LIKE_PATTERN_AUTOMATON_H
#define LIKE_PATTERN_AUTOMATON_H
#include <optional>

#include "automata.hpp"

namespace automata::parsing {
    class LikePatternAutomaton {
    public:
        std::optional<SingleStartFiniteAutomaton> startMatch;
        std::vector<MultipleStartsFiniteAutomaton> middleMatches;
        std::optional<SingleStartFiniteAutomaton> endMatch;
        std::vector<State> acceptStates;
        std::unique_ptr<State> errorState;

    public:
        struct AutomatonParams {
            std::deque<const State*> states;
            std::basic_string<uint8_t> deterministicPath;
            const std::vector<State>* acceptStates;
            const State* start;
            int64_t minLength;
            const State* error;
        };

        static std::unique_ptr<LikePatternAutomaton> build(const std::span<const uint8_t>& match, const Encoder& encoder);
        LikePatternAutomaton();
        LikePatternAutomaton(const LikePatternAutomaton&) = delete;
        LikePatternAutomaton& operator=(const LikePatternAutomaton&) = delete;
        LikePatternAutomaton(LikePatternAutomaton&& ) noexcept = default;
        LikePatternAutomaton& operator=(LikePatternAutomaton&&) noexcept = default;
        virtual ~LikePatternAutomaton() = default;
        std::optional<AutomatonParams> gatherForwardParams() const;
        std::optional<AutomatonParams> gatherBackwardsParams() const;

        friend class LikePatternAutomatonSerializer;
        friend class LikePatternAutomatonParser;
    };

    class AutomatonTable {
    using AlignmentType = size_t;

    private:
        static constexpr uint8_t kNonPseudoEnd = 0xFF;
        std::unique_ptr<AlignmentType[]> data;

    public:
        uint8_t* table;
        size_t* levels;
        uint8_t* pseudoAccepts;
        size_t error;
        size_t firstAccept;
        uint8_t shift;
        size_t start;
        std::basic_string<uint8_t> fix;

        AutomatonTable(std::unordered_map<State*, size_t>& indexMap, bool isBackwards, size_t error, size_t firstAccept, const std::basic_string<uint8_t>& fix, size_t start);

        template <bool Forwards>
        bool parse(int64_t* strIdx, uint8_t* symIdx, const uint8_t* str, size_t len) const {
            auto parseImplementation = [&]<typename T>(T) {
                T currentState = static_cast<T>(start);
                const T errorState = static_cast<T>(error);
                const T firstAcceptState = static_cast<T>(firstAccept);
                if constexpr (Forwards) {
                    if (!std::basic_string_view<uint8_t>{str, len}.starts_with(fix))
                        return false;
                    *strIdx = fix.size();
                } else {
                    if (!std::basic_string_view<uint8_t>{str, len}.ends_with(fix))
                        return false;
                    *strIdx = len - fix.size() - 1;
                }
                while (currentState < error && (Forwards ? *strIdx + levels[currentState] <= len :*strIdx + 1 >= levels[currentState])) {
                    size_t index = Forwards ? (*strIdx)++ : (*strIdx)--;
                    currentState = reinterpret_cast<T*>(table + (currentState << (8 + shift)))[str[index]];
                }
                if (currentState >= firstAcceptState) {
                    *symIdx = currentState - firstAcceptState;
                    *strIdx += (Forwards ? -1 : 2);
                    return true;
                }
                if (!Forwards && currentState != errorState && pseudoAccepts[currentState] != kNonPseudoEnd) {
                    *symIdx = pseudoAccepts[currentState];
                    *strIdx += 1;
                    return true;
                }
                return false;
            };

            switch (shift) {
                case 0: return parseImplementation(uint8_t{});
                case 1: return parseImplementation(uint16_t{});
                case 2: return parseImplementation(uint32_t{});
                case 3: return parseImplementation(uint64_t{});
            }
        }
    };

    class LikePatternAutomatonParser {
    private:
        std::optional<AutomatonTable> forwardTable;
        std::optional<AutomatonTable> backwardsTable;

    public:
        LikePatternAutomatonParser(const std::span<const uint8_t>& match, const Encoder& encoder);
        [[nodiscard]] bool parse(const std::span<const uint8_t>& match) const;
        [[nodiscard]] bool hasEmptyAutomaton() const;
    };
}
#endif //LIKE_PATTERN_AUTOMATON_H
