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

#ifndef SERIALIZER_H
#define SERIALIZER_H
#include "like_pattern_automaton.hpp"
#include "automata.hpp"
#include <sstream>

namespace automata::serializer {

    template <typename T>
    void writeToStream(std::basic_stringstream<uint8_t>& stream, const T* t, size_t len) {
        const uint8_t* bytePtr = reinterpret_cast<const uint8_t*>(t);
        stream.write(bytePtr, len);
    }

    void serializeSymbolTable(const std::shared_ptr<libfsst::SymbolTable>& symTable, std::basic_stringstream<uint8_t>& stream);
    void serializeStateWithoutDefaultTransition(const State* state, std::basic_stringstream<uint8_t>& stream);
    void serializeStateWithoutTransitions(const State* state, std::basic_stringstream<uint8_t>& stream);
    void serializeStateFull(const State* state, std::basic_stringstream<uint8_t>& stream);

    class SingleStartFiniteAutomataSerializer {
    private:
        SingleStartFiniteAutomaton automaton;
        std::vector<State>& endStates;
        std::shared_ptr<libfsst::SymbolTable> symTable;

    public:
        SingleStartFiniteAutomataSerializer(SingleStartFiniteAutomaton& automaton, std::vector<State>& endStates, std::shared_ptr<libfsst::SymbolTable>& symTable);
        [[nodiscard]] std::basic_string<uint8_t> serialize() const;
    };

    class MultipleStartsFiniteAutomataSerializer {
    private:
        MultipleStartsFiniteAutomaton automaton;
        std::vector<State>& endStates;
        std::shared_ptr<libfsst::SymbolTable> symTable;

    public:
        MultipleStartsFiniteAutomataSerializer(MultipleStartsFiniteAutomaton& fa, std::vector<State>& endStates, std::shared_ptr<libfsst::SymbolTable>& symTable);
        [[nodiscard]] std::basic_string<uint8_t> serialize() const;
    };

    class LikePatternAutomatonSerializer {
    private:
        std::unique_ptr<parsing::LikePatternAutomaton> automaton;
        std::shared_ptr<libfsst::SymbolTable> symTable;

    public:
        LikePatternAutomatonSerializer(std::unique_ptr<parsing::LikePatternAutomaton>& automaton, std::shared_ptr<libfsst::SymbolTable>& symTable);
        [[nodiscard]] std::basic_string<uint8_t> serialize() const;
    };
}
#endif //SERIALIZER_H
