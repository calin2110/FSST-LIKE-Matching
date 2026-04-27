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

#ifndef CODEGEN_HPP
#define CODEGEN_HPP
#include "like_pattern_automaton.hpp"
#include <optional>
#include <fmt/format.h>
namespace automata::codegen {
    class Parser {
    public:
        Parser() = default;
        virtual bool parse(const uint8_t* pattern, size_t len) = 0;
        virtual ~Parser() = default;
        Parser(const Parser&) = delete;
        Parser& operator=(const Parser&) = delete;
        Parser(Parser&& ) noexcept = delete;
        Parser& operator=(Parser&&) noexcept = delete;
    };

    class StateCodegen {
    public:
        StateCodegen() = default;
        void generate(const State* state, int direction, const State* error, const State* acceptStart, const State* acceptEnd);
        virtual void generateEnd(const State* state, int direction, std::ptrdiff_t endIdx) = 0;
        virtual void generateMiddleStart(const State* state, const State* error, int direction) = 0;
        virtual void generateError(const State* state, int direction) = 0;
        virtual void generateOther(const State* state, const State* error, int direction) = 0;
        virtual ~StateCodegen() = default;

        enum class ParsingMode {
            NO_SIMD,
            SIMD_CMPESTRM,
            SIMD_CMPEQEPI8
        };
        static ParsingMode doesParsingUseSIMD(const State* state, bool& enableSIMD);
        static size_t getNumRealTransitions(const State* state);
    };

    class Compiler {
    protected:
        enum class ParsingType {
            ONLY_FORWARD,
            ONLY_BACKWARDS,
            BOTH_DIRECTIONS,
            NO_DIRECTION
        };
        std::unique_ptr<StateCodegen> stateCompiler;
        virtual void generateBackwards(const std::optional<parsing::LikePatternAutomaton::AutomatonParams>& params) = 0;
        virtual void generateForwards(const std::optional<parsing::LikePatternAutomaton::AutomatonParams>& params) = 0;
        virtual void generateFullParse(const std::optional<size_t>& levelsSum, const std::optional<size_t>& forwardLevel, const ParsingType& type) = 0;

    public:
        Compiler() = default;
        virtual std::unique_ptr<Parser> compile(const std::unique_ptr<parsing::LikePatternAutomaton>& automaton) = 0;
        virtual ~Compiler() = default;
        Compiler(const Compiler&) = delete;
        Compiler& operator=(const Compiler&) = delete;
        Compiler(Compiler&& ) noexcept = delete;
        Compiler& operator=(Compiler&&) noexcept = delete;

        static constexpr const char* getForwardParseFunctionName() {
            return "parseFwdState";
        }

        static constexpr const char* getBackwardsParseFunctionName() {
            return "parseBwdState";
        }

        static constexpr const char* getParseFunctionName() {
            return "parse";
        }
    };

    class CompiledAutomaton {
    private:
        std::unique_ptr<Parser> parser;
    public:
        explicit CompiledAutomaton(std::unique_ptr<Parser>& parser);
        CompiledAutomaton(const CompiledAutomaton&) = delete;
        CompiledAutomaton& operator=(const CompiledAutomaton&) = delete;
        CompiledAutomaton(CompiledAutomaton&& ) noexcept = delete;
        CompiledAutomaton& operator=(CompiledAutomaton&&) noexcept = delete;

        [[nodiscard]] bool parse(const std::span<const uint8_t>& match) const;

    };
}


#endif //CODEGEN_HPP
