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

#ifndef FSST_LIKE_MATCHING_CPPCODEGEN_HPP
#define FSST_LIKE_MATCHING_CPPCODEGEN_HPP

#include <sstream>
#include "codegen.hpp"
namespace automata::codegen::cpp {
    class CppStateCodegen: public StateCodegen {
    private:
        std::stringstream& ss;
        bool& enableSIMD;

    public:
        CppStateCodegen(std::stringstream& ss, bool& enableSIMD);

        void generateEnd(const State* state, int direction, std::ptrdiff_t endIdx) override;
        void generateMiddleStart(const State* state, const State* error, int direction) override;
        void generateError(const State* state, int direction) override;
        void generateOther(const State* state, const State* error, int direction) override;
    };

    class CppCompiler: public Compiler {
    private:
        std::string cppFile;
        std::string destination;
        bool generateLLVM;
        bool enableSIMD;
        std::stringstream ss;

        void addIncludes();
        void generateTransitionArrays(const std::unique_ptr<parsing::LikePatternAutomaton>& automaton);
        void generatePrefixVariables(const std::optional<parsing::LikePatternAutomaton::AutomatonParams>& params);
        void generateSuffixVariables(const std::optional<parsing::LikePatternAutomaton::AutomatonParams>& params);
        void generateBackwards(const std::optional<parsing::LikePatternAutomaton::AutomatonParams>& params) override;
        void generateForwards(const std::optional<parsing::LikePatternAutomaton::AutomatonParams>& params) override;
        void generateFullParse(const std::optional<size_t>& levelsSum, const std::optional<size_t>& forwardLevel, const ParsingType& type) override;

        void generatePrefixCheck(const uint8_t* prefix, size_t size, size_t offset);
        void generateSuffixCheck(const uint8_t* suffix, size_t size, size_t offset);
        static std::string generateCppFunctionSignature(int8_t direction);

        static std::string getPrefixName(size_t offset) {
            return fmt::format("prefix{}", offset);
        }

        static std::string getSuffixName(size_t offset) {
            return fmt::format("suffix{}", offset);
        }

        static std::string getEnumStateName(const State* state, int direction) {
            return direction == 1 ? fmt::format("FwdState::q{}", reinterpret_cast<uintptr_t>(state)) : fmt::format("BwdState::q{}", reinterpret_cast<uintptr_t>(state));
        }

        static std::string getTransitionArrayName(const State* state) {
            return fmt::format("transitionArrayq{}", reinterpret_cast<uintptr_t>(state));
        }

        static std::string getSIMDCmpestrmSetName(const State* state) {
            return fmt::format("byteSetq{}", reinterpret_cast<uintptr_t>(state));
        }

        static std::string getSIMDCmpestrmVectorName(const State* state) {
            return fmt::format("byteVectorq{}", reinterpret_cast<uintptr_t>(state));
        }

        static std::string getSIMDCmpeqepi8SymbolName(const State* state, uint8_t symbol) {
            return fmt::format("byte{}Symbolq{}", symbol, reinterpret_cast<uintptr_t>(state));
        }

        static std::string getSIMDCmpeqepi8VectorName(const State* state, uint8_t symbol) {
            return fmt::format("byte{}Vectorq{}", symbol, reinterpret_cast<uintptr_t>(state));
        }

        static std::string getLabel(const State* state) {
            return fmt::format("q{}", reinterpret_cast<uintptr_t>(state));
        }

    public:
        CppCompiler(const std::string& cppFile, const std::string& destination, bool enableSIMD, bool generateLLVM = false);
        std::unique_ptr<Parser> compile(const std::unique_ptr<parsing::LikePatternAutomaton>& automaton) override;
        ~CppCompiler() override;

        friend class CppStateCodegen;
    };

    class CppParser: public Parser {
    private:
        typedef bool (*Function)(const uint8_t*, size_t);
        Function parseFunction;
        void* libraryHandle;
        Function getFunctionPointer(const char* functionName);

    public:
        explicit CppParser(void* libraryHandle);
        bool parse(const uint8_t *pattern, size_t len) override;
        ~CppParser() override;
    };
}
#endif //FSST_LIKE_MATCHING_CPPCODEGEN_HPP