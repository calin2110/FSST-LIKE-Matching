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

#include "codegen/codegen.hpp"
#include <sstream>
#include <fmt/format.h>

void automata::codegen::StateCodegen::generate(const State* state, int direction, const State* error, const State* acceptStart, const State* acceptEnd) {
    if (state == error) {
        generateError(state, direction);
        return;
    }
    if (state >= acceptStart && state < acceptEnd) {
        std::ptrdiff_t endIdx = state - acceptStart;
        generateEnd(state, direction, endIdx);
        return;
    }

    if (state->defaultTransition == state) {
        generateMiddleStart(state, error, direction);
        return;
    }
    generateOther(state, error, direction);
}

automata::codegen::StateCodegen::ParsingMode automata::codegen::StateCodegen::doesParsingUseSIMD(const State *state, bool &enableSIMD) {
    size_t numTransitions = getNumRealTransitions(state);
    if (!enableSIMD || numTransitions > 16) {
        return ParsingMode::NO_SIMD;
    }
    if (numTransitions >= 6) {
        return ParsingMode::SIMD_CMPESTRM;
    }
    return ParsingMode::SIMD_CMPEQEPI8;
}

size_t automata::codegen::StateCodegen::getNumRealTransitions(const State *state) {
    return state->transition(255)->transitions.empty() ? state->transitions.size() - 1 : state->transitions.size();
}


automata::codegen::CompiledAutomaton::CompiledAutomaton(std::unique_ptr<Parser>& parser): parser(std::move(parser)) {}

bool automata::codegen::CompiledAutomaton::parse(const std::span<const uint8_t> &match) const {
    return parser->parse(match.data(), match.size());
}
