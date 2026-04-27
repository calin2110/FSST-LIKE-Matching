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

#ifndef FSST_LIKE_MATCHING_LLVMCODEGEN_HPP
#define FSST_LIKE_MATCHING_LLVMCODEGEN_HPP

#include "codegen.hpp"
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/IRBuilder.h>
#include "llvm/Support/TargetSelect.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>  // <-- THIS is required
#include <llvm/Transforms/IPO.h>

namespace automata::codegen::llvmir {
    class LLVMStateCodegen: public StateCodegen {
    private:
        std::unordered_map<const State*, llvm::BasicBlock*>& stateToSwitchBlock;
        std::unordered_map<const State*, llvm::BasicBlock*>& transitionBlocks;
        llvm::orc::ThreadSafeContext& threadSafeContext;
        llvm::IRBuilder<>& builder;
        std::unique_ptr<llvm::Module>& module;
        llvm::Value*& elemPtr;
        llvm::Value*& strIdxArg;
        llvm::Value*& symIdxArg;
        llvm::Value*& dataArg;
        llvm::Value*& lenArg;
        llvm::Value*& lvlPtr;
        llvm::Type*& i64Ty;
        llvm::Type*& i32Ty;
        llvm::Type*& i8Ty;
        llvm::Value*& elemVal;
        llvm::Value*& strIdxVal;
        llvm::Value*& prevBytePtr;
        llvm::Function*& func;
        llvm::BasicBlock*& trapBlock;
        std::string& type;
        bool& enableSIMD;

    public:
        LLVMStateCodegen(
            std::unordered_map<const State*, llvm::BasicBlock*>& stateToSwitchBlock,
            std::unordered_map<const State*, llvm::BasicBlock*>& transitionBlocks,
            llvm::IRBuilder<>& builder,
            llvm::orc::ThreadSafeContext& threadSafeContext,
            std::unique_ptr<llvm::Module>& module,
            llvm::Value*& elemPtr,
            llvm::Value*& strIdxArg,
            llvm::Value*& symIdxArg,
            llvm::Value*& dataArg,
            llvm::Value*& lenArg,
            llvm::Value*& lvlPtr,
            llvm::Type*& i64Ty,
            llvm::Type*& i32Ty,
            llvm::Type*& i8Ty,
            llvm::Value*& elemVal,
            llvm::Value*& strIdxVal,
            llvm::Value*& prevBytePtr,
            llvm::Function*& func,
            llvm::BasicBlock*& trapBlock,
            std::string& type,
            bool& enableSIMD
        );

        void generateEnd(const State* state, int direction, std::ptrdiff_t endIdx) override;
        void generateMiddleStart(const State* state, const State* error, int direction) override;
        void generateError(const State* state, int direction) override;
        void generateOther(const State* state, const State* error, int direction) override;
    };

    class LLVMCompiler: public Compiler {
    private:
        llvm::orc::ThreadSafeContext threadSafeContext;
        std::unique_ptr<llvm::Module> module;
        std::unique_ptr<llvm::orc::LLJIT> JIT;
        llvm::IRBuilder<> builder;
        llvm::FunctionType* funcType;
        bool generateLLVM;
        bool enableSIMD;

        void generateTransitionArrays(const std::unique_ptr<parsing::LikePatternAutomaton>& automaton);
        void generatePrefixVariables(const std::optional<parsing::LikePatternAutomaton::AutomatonParams>& params);
        void generateSuffixVariables(const std::optional<parsing::LikePatternAutomaton::AutomatonParams>& params);
        void generateBackwards(const std::optional<parsing::LikePatternAutomaton::AutomatonParams>& params) override;
        void generateForwards(const std::optional<parsing::LikePatternAutomaton::AutomatonParams>& params) override;
        void generateFullParse(const std::optional<size_t>& backwardsLevel, const std::optional<size_t>& forwardLevel, const ParsingType& type) override;

        void generatePrefixCheck(const uint8_t* prefix, size_t size, size_t offset, llvm::BasicBlock* retFalse, llvm::BasicBlock* successBlock, llvm::Function* func, llvm::Value* dataArg, llvm::Value* strIdxArg, llvm::BasicBlock* currentBlock);
        void generateSuffixCheck(const uint8_t* prefix, size_t size, size_t offset, llvm::BasicBlock* retFalse, llvm::BasicBlock* successBlock, llvm::Function* func, llvm::Value* dataArg, llvm::Value* strIdxArg, llvm::Value* lenArg, llvm::BasicBlock* currentBlock);
        void optimizeModule();

        static std::string getPrefixName(size_t offset) {
            return fmt::format("prefix{}", offset);
        }

        static std::string getSuffixName(size_t offset) {
            return fmt::format("suffix{}", offset);
        }

        static std::string getEnumStateName(const State* state, int direction) {
            return direction == 1 ? fmt::format("fwd.q{}", reinterpret_cast<uintptr_t>(state)) : fmt::format("bwd.q{}", reinterpret_cast<uintptr_t>(state));
        }

        static std::string getTransitionArrayName(const State* state) {
            return fmt::format("arrq{}", reinterpret_cast<uintptr_t>(state));
        }

        static std::string getSIMDCmpestrmVectorName(const State* state) {
            return fmt::format("simdVector{}", reinterpret_cast<uintptr_t>(state));
        }

        static std::string getSIMDCmpeqepi8VectorName(const State* state, uint8_t symbol) {
            return fmt::format("byte{}Vectorq{}", symbol, reinterpret_cast<uintptr_t>(state));
        }
    public:
        explicit LLVMCompiler(bool enableSIMD, bool generateLLVM = false);

        std::unique_ptr<Parser> compile(const std::unique_ptr<parsing::LikePatternAutomaton> &automaton) override;
        friend class LLVMStateCodegen;
    };

    class LLVMParser: public Parser {
    private:
        std::unique_ptr<llvm::orc::LLJIT> JIT;
        bool (*parseFunction)(const uint8_t*, size_t);

    public:
        bool parse(const uint8_t *pattern, size_t len) override;
        explicit LLVMParser(std::unique_ptr<llvm::orc::LLJIT>& JIT);
        ~LLVMParser() override = default;
    };


}
#endif //FSST_LIKE_MATCHING_LLVMCODEGEN_HPP