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

#include "codegen/llvmcodegen.hpp"
#include "utils.hpp"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/JumpThreading.h"
#include "llvm/Support/Host.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsX86.h"
#ifdef __SSE4_2__
    #include <nmmintrin.h>
    #include <pmmintrin.h>
#endif

void automata::codegen::llvmir::LLVMCompiler::generateTransitionArrays(const std::unique_ptr<parsing::LikePatternAutomaton> &automaton) {
    llvm::Type* boolTy = llvm::Type::getInt8Ty(*threadSafeContext.getContext());
    llvm::ArrayType* arrayTy = llvm::ArrayType::get(boolTy, 256);

    for (const auto& middleAutomaton : automaton->middleMatches) {
        State* start = middleAutomaton.defaultTransition;

        std::vector<llvm::Constant*> vals;
        vals.reserve(256);

        for (int i = 0; i < 256; ++i) {
            uint8_t symbol = static_cast<uint8_t>(i);
            bool canTransition = false;

            if (symbol < 255) {
                canTransition = start->canTransition(symbol);
            } else {
                canTransition = !start->transition(255)->transitions.empty();
            }

            vals.push_back(llvm::ConstantInt::get(boolTy, canTransition));
        }

        llvm::Constant* initializer = llvm::ConstantArray::get(arrayTy, vals);

        new llvm::GlobalVariable(
            *module,
            arrayTy,
            true, // isConstant
            llvm::GlobalValue::InternalLinkage,
            initializer,
            getTransitionArrayName(start)
        );

        auto parsingMode = StateCodegen::doesParsingUseSIMD(start, enableSIMD);
        if (parsingMode == StateCodegen::ParsingMode::SIMD_CMPESTRM) {
            std::vector<uint8_t> symbols{};
            for (const auto& [symbol, dest]: start->transitions) {
                if (symbol != 255 || !dest->transitions.empty()) {
                    symbols.push_back(symbol);
                }
            }

            llvm::Type *i8Ty = llvm::Type::getInt8Ty(*threadSafeContext.getContext());
            auto dummy = llvm::ConstantInt::get(i8Ty, 0);
            std::vector<llvm::Constant*> arrayElems(16, dummy);
            for (uint8_t i = 0; i < symbols.size(); ++i) {
                arrayElems[i] = llvm::ConstantInt::get(i8Ty, symbols[i]);
            }
            llvm::Constant* simdVecConstant = llvm::ConstantVector::get(arrayElems);
            llvm::FixedVectorType* simdVecTy = llvm::FixedVectorType::get(i8Ty, 16);
            auto* globalVariable = new llvm::GlobalVariable(
                *module,
                simdVecTy,
                true, // isConstant
                llvm::GlobalValue::InternalLinkage,
                simdVecConstant,
                getSIMDCmpestrmVectorName(start)
            );
            globalVariable->setAlignment(llvm::Align(16));
        } else if (parsingMode == StateCodegen::ParsingMode::SIMD_CMPEQEPI8) {
            for (const auto& [symbol, dest]: start->transitions) {
                if (symbol == 255 && dest->transitions.empty()) {
                    continue;
                }

                llvm::Type *i8Ty = llvm::Type::getInt8Ty(*threadSafeContext.getContext());
                std::vector<llvm::Constant*> arrayElems(16, nullptr);
                for (uint8_t i = 0; i < 16; ++i) {
                    arrayElems[i] = llvm::ConstantInt::get(i8Ty, symbol);
                }

                llvm::Constant* simdVecConstant = llvm::ConstantVector::get(arrayElems);
                llvm::FixedVectorType* simdVecTy = llvm::FixedVectorType::get(i8Ty, 16);
                auto* globalVariable = new llvm::GlobalVariable(
                    *module,
                    simdVecTy,
                    true, // isConstant
                    llvm::GlobalValue::InternalLinkage,
                    simdVecConstant,
                    getSIMDCmpeqepi8VectorName(start, symbol)
                );
                globalVariable->setAlignment(llvm::Align(16));
            }


        }
    }
}

void automata::codegen::llvmir::LLVMCompiler::generatePrefixVariables(const std::optional<parsing::LikePatternAutomaton::AutomatonParams> &params) {
    const std::basic_string<uint8_t>& prefix = params->deterministicPath;
    size_t offset = 0;
    while (offset < prefix.size()) {
        if (prefix.size() - offset >= 16 && enableSIMD) {
            std::vector<llvm::Constant*> arrayElems(2, nullptr);
            llvm::Type* i64Ty = llvm::Type::getInt64Ty(*threadSafeContext.getContext());
            for (uint8_t i = 0; i < 2; ++i) {
                arrayElems[i] = llvm::ConstantInt::get(i64Ty, loadUnaligned<uint64_t>(prefix.data() + offset + 8 * i));
            }
            llvm::Constant* simdVecConstant = llvm::ConstantVector::get(arrayElems);
            llvm::FixedVectorType* simdVecTy = llvm::FixedVectorType::get(i64Ty, 2);
            auto* globalVariable = new llvm::GlobalVariable(
                *module,
                simdVecTy,
                true, // isConstant
                llvm::GlobalValue::InternalLinkage,
                simdVecConstant,
                getPrefixName(offset)
            );
            globalVariable->setAlignment(llvm::Align(16));
            offset += 16;
        } else {
            if (prefix.size() - offset >= 8) {
                llvm::Type* i64Ty = llvm::Type::getInt64Ty(*threadSafeContext.getContext());
                new llvm::GlobalVariable(*module, i64Ty, true, llvm::GlobalValue::InternalLinkage, llvm::ConstantInt::get(i64Ty, loadUnaligned<uint64_t>(prefix.data() + offset)), getPrefixName(offset));
                offset += 8;
            } else if (prefix.size() - offset >= 4) {
                llvm::Type* i32Ty = llvm::Type::getInt32Ty(*threadSafeContext.getContext());
                new llvm::GlobalVariable(*module, i32Ty, true, llvm::GlobalValue::InternalLinkage, llvm::ConstantInt::get(i32Ty, loadUnaligned<uint32_t>(prefix.data() + offset)), getPrefixName(offset));
                offset += 4;
            } else if (prefix.size() - offset >= 2) {
                llvm::Type* i16Ty = llvm::Type::getInt16Ty(*threadSafeContext.getContext());
                new llvm::GlobalVariable(*module, i16Ty, true, llvm::GlobalValue::InternalLinkage, llvm::ConstantInt::get(i16Ty, loadUnaligned<uint16_t>(prefix.data() + offset)), getPrefixName(offset));
                offset += 2;
            } else if (prefix.size() - offset >= 1) {
                llvm::Type* i8Ty = llvm::Type::getInt8Ty(*threadSafeContext.getContext());
                new llvm::GlobalVariable(*module, i8Ty, true, llvm::GlobalValue::InternalLinkage, llvm::ConstantInt::get(i8Ty, prefix[offset]), getPrefixName(offset));
                offset += 1;
            }
        }
    }
}

void automata::codegen::llvmir::LLVMCompiler::generateSuffixVariables(const std::optional<parsing::LikePatternAutomaton::AutomatonParams> &params) {
    const std::basic_string<uint8_t>& suffix = params->deterministicPath;
    size_t offset = suffix.size();
    while (offset > 0) {
        if (offset >= 16 && enableSIMD) {
            std::vector<llvm::Constant*> arrayElems(2, nullptr);
            llvm::Type* i64Ty = llvm::Type::getInt64Ty(*threadSafeContext.getContext());
            for (uint8_t i = 0; i < 2; ++i) {
                arrayElems[i] = llvm::ConstantInt::get(i64Ty, loadUnaligned<uint64_t>(suffix.data() + offset - 16 + 8 * i));
            }
            llvm::Constant* simdVecConstant = llvm::ConstantVector::get(arrayElems);
            llvm::FixedVectorType* simdVecTy = llvm::FixedVectorType::get(i64Ty, 2);
            auto* globalVariable = new llvm::GlobalVariable(
                *module,
                simdVecTy,
                true, // isConstant
                llvm::GlobalValue::InternalLinkage,
                simdVecConstant,
                getSuffixName(offset)
            );
            globalVariable->setAlignment(llvm::Align(16));
            offset -= 16;
        } else {
            if (offset >= 8) {
                llvm::Type* i64Ty = llvm::Type::getInt64Ty(*threadSafeContext.getContext());
                new llvm::GlobalVariable(*module, i64Ty, true, llvm::GlobalValue::InternalLinkage, llvm::ConstantInt::get(i64Ty,loadUnaligned<uint64_t>(suffix.data() + offset - sizeof(uint64_t))), getSuffixName(offset));
                offset -= 8;
            } else if (offset >= 4) {
                llvm::Type* i32Ty = llvm::Type::getInt32Ty(*threadSafeContext.getContext());
                new llvm::GlobalVariable(*module, i32Ty, true, llvm::GlobalValue::InternalLinkage, llvm::ConstantInt::get(i32Ty,  loadUnaligned<uint32_t>(suffix.data() + offset - sizeof(uint32_t))), getSuffixName(offset));
                offset -= 4;
            } else if (offset >= 2) {
                llvm::Type* i16Ty = llvm::Type::getInt16Ty(*threadSafeContext.getContext());
                new llvm::GlobalVariable(*module, i16Ty, true, llvm::GlobalValue::InternalLinkage, llvm::ConstantInt::get(i16Ty, loadUnaligned<uint16_t>(suffix.data() + offset - sizeof(uint16_t))), getSuffixName(offset));
                offset -= 2;
            } else if (offset >= 1) {
                llvm::Type* i8Ty = llvm::Type::getInt8Ty(*threadSafeContext.getContext());
                new llvm::GlobalVariable(*module, i8Ty, true, llvm::GlobalValue::InternalLinkage, llvm::ConstantInt::get(i8Ty, suffix[offset - 1]), getSuffixName(offset));
                offset -= 1;
            }
        }
    }
}

void automata::codegen::llvmir::LLVMCompiler::generateBackwards(const std::optional<parsing::LikePatternAutomaton::AutomatonParams> &params) {
    std::string type = "bwd";
    int8_t direction = -1;
    llvm::Type* i32Ty = llvm::Type::getInt32Ty(*threadSafeContext.getContext());
    llvm::Type* i64Ty = llvm::Type::getInt64Ty(*threadSafeContext.getContext());
    llvm::Type* i8Ty = llvm::Type::getInt8Ty(*threadSafeContext.getContext());
    size_t idx = 0;
    for (const State* state: params->states) {
        llvm::Constant* constant = llvm::ConstantInt::get(i32Ty, idx);
        new llvm::GlobalVariable(*module, i32Ty, true, llvm::GlobalValue::InternalLinkage, constant, getEnumStateName(state, direction));
        ++idx;
    }

    llvm::Function* func = llvm::Function::Create(funcType, llvm::Function::InternalLinkage, Compiler::getBackwardsParseFunctionName(), module.get());

    auto argIt = func->arg_begin();
    argIt->setName(fmt::format("{}.compressed", type));
    llvm::Value* dataArg = argIt++;
    argIt->setName(fmt::format("{}.len", type));
    llvm::Value* lenArg = argIt++;
    argIt->setName(fmt::format("{}.strIdx", type));
    llvm::Value* strIdxArg = argIt++;
    argIt->setName(fmt::format("{}.symIdx", type));
    llvm::Value* symIdxArg = argIt++;
    llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.entry", type), func);
    builder.SetInsertPoint(entryBlock);
    llvm::BasicBlock* wrongSuffixBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.wrongsuffix", type), func);
    llvm::BasicBlock* correctSuffixBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.correctsuffix", type), func);

    llvm::BasicBlock* startCheckBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.checksuffix", type), func);
    builder.CreateBr(startCheckBlock);
    generateSuffixCheck(params->deterministicPath.data() + params->deterministicPath.size(), params->deterministicPath.size(), 0, wrongSuffixBlock, correctSuffixBlock, func, dataArg, strIdxArg, lenArg, startCheckBlock);

    builder.SetInsertPoint(wrongSuffixBlock);
    builder.CreateRet(llvm::ConstantInt::getFalse(*threadSafeContext.getContext()));

    builder.SetInsertPoint(correctSuffixBlock);
    llvm::Value* lvlPtr = builder.CreateAlloca(i64Ty, nullptr, fmt::format("{}.level", type));
    llvm::Value* level = llvm::ConstantInt::get(i64Ty, params->start->level);
    builder.CreateStore(level, lvlPtr);

    llvm::GlobalValue* startStatePtr = module->getGlobalVariable(getEnumStateName(params->start, direction), true);
    llvm::Value* startStateVal = builder.CreateLoad(builder.getInt32Ty(), startStatePtr, fmt::format("{}.value", getEnumStateName(params->start, direction)));
    llvm::Value* qPtr = builder.CreateAlloca(i32Ty, nullptr, fmt::format("{}.q", type));
    builder.CreateStore(startStateVal, qPtr);

    llvm::BasicBlock* loopCondBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.loop.cond", type), func);
    llvm::BasicBlock* loopBodyBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.loop.body", type), func);
    llvm::BasicBlock* loopEndBB  = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.loop.end", type), func);
    builder.CreateBr(loopCondBB);
    builder.SetInsertPoint(loopCondBB);
    llvm::Value* strIdxVal = builder.CreateLoad(i64Ty, strIdxArg, fmt::format("{}.strIdxVal", type));

    llvm::Value* lvlVal = builder.CreateLoad(i64Ty, lvlPtr, fmt::format("{}.level", type));
    llvm::Value* constAdd = llvm::ConstantInt::get(i64Ty, 1);
    llvm::Value* currentSum = builder.CreateAdd(strIdxVal, constAdd, fmt::format("{}.summed", type));
    llvm::Value* inRow = builder.CreateICmpSGE(strIdxVal, llvm::ConstantInt::get(i64Ty, 0), fmt::format("{}.inRow", type));
    llvm::Value* levelOk = builder.CreateICmpSGE(currentSum, lvlVal, fmt::format("{}.levelOk", type));
    llvm::Value* cond = builder.CreateAnd(inRow, levelOk, fmt::format("{}.cmp", type));
    builder.CreateCondBr(cond, loopBodyBB, loopEndBB);

    builder.SetInsertPoint(loopBodyBB);
    llvm::Value *qVal = builder.CreateLoad(builder.getInt32Ty(), qPtr, fmt::format("{}.qVal", type));
    strIdxVal = builder.CreateLoad(builder.getInt64Ty(), strIdxArg, fmt::format("{}.strIdxVal", type));
    llvm::Value *elemPtr = builder.CreateInBoundsGEP(builder.getInt8Ty(), dataArg, strIdxVal, fmt::format("{}.elemPtr", type));
    llvm::Value *elemVal = builder.CreateLoad(builder.getInt8Ty(), elemPtr, fmt::format("{}.elemVal", type));

    std::unordered_map<const State*, llvm::BasicBlock*> stateToSwitchBlock{};
    for (const State* state: params->states) {
        stateToSwitchBlock[state] = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.switch", getEnumStateName(state, direction)), func);
    }
    llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.merge", type), func);
    llvm::SwitchInst *Switch = builder.CreateSwitch(qVal, mergeBlock, params->states.size());

    for (const State* state: params->states) {
        Switch->addCase(llvm::dyn_cast<llvm::ConstantInt>(module->getGlobalVariable(getEnumStateName(state, direction), true)->getInitializer()), stateToSwitchBlock[state]);
    }
    std::unordered_map<const State*, llvm::BasicBlock*> transitionBlocks{};
    std::vector<const State*> pseudoEnds{};

    llvm::Value* prevBytePtr = nullptr;
    llvm::BasicBlock* trapBlock = nullptr;
    stateCompiler = std::make_unique<LLVMStateCodegen>(
        stateToSwitchBlock, transitionBlocks, builder, threadSafeContext, module, elemPtr, strIdxArg, symIdxArg, dataArg,
        lenArg, lvlPtr, i64Ty, i32Ty, i8Ty, elemVal, strIdxVal, prevBytePtr, func, trapBlock, type, enableSIMD
    );
    for (const State* state: params->states) {
        if (state->endIdx.has_value()) {
            pseudoEnds.push_back(state);
        }
        stateCompiler->generate(state, direction, params->error, params->acceptStates->data(), params->acceptStates->data() + params->acceptStates->size());
    }

    for (auto& [state, block]: transitionBlocks) {
        builder.SetInsertPoint(block);
        llvm::Value* nextLvlVal = llvm::ConstantInt::get(i64Ty, state->level);
        builder.CreateStore(nextLvlVal, lvlPtr);
        llvm::GlobalValue* nextStatePtr = module->getGlobalVariable(getEnumStateName(state, direction), true);
        llvm::Value* nextStateVal = builder.CreateLoad(builder.getInt32Ty(), nextStatePtr, fmt::format("{}.value", getEnumStateName(state, direction)));
        builder.CreateStore(nextStateVal, qPtr);
        builder.CreateBr(mergeBlock);
    }

    builder.SetInsertPoint(mergeBlock);
    llvm::Value* oldVal = builder.CreateLoad(builder.getInt64Ty(), strIdxArg, fmt::format("{}.oldVal", type));
    llvm::Value* directionVal = llvm::ConstantInt::get(builder.getInt64Ty(), direction);
    llvm::Value* newVal = builder.CreateAdd(oldVal, directionVal, fmt::format("{}.newVal", type));
    builder.CreateStore(newVal, strIdxArg);
    builder.CreateBr(loopCondBB);

    builder.SetInsertPoint(loopEndBB);
    qVal = builder.CreateLoad(builder.getInt32Ty(), qPtr, fmt::format("{}.qVal", type));
    llvm::BasicBlock* finalDefaultBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.finaldefault", type), func);
    std::unordered_map<const State*, llvm::BasicBlock*> acceptStateToBlock{};
    llvm::SwitchInst* currentSwitch = builder.CreateSwitch(qVal, finalDefaultBlock, params->acceptStates->size());

    for (size_t i = 0; i < params->acceptStates->size(); ++i) {
        const State* state = params->acceptStates->data() + i;
        acceptStateToBlock[state] = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.finalswitch", getEnumStateName(state, direction)), func);
        currentSwitch->addCase(llvm::dyn_cast<llvm::ConstantInt>(module->getGlobalVariable(getEnumStateName(state, direction), true)->getInitializer()), acceptStateToBlock[state]);
    }

    for (const State* pseudoEnd: pseudoEnds) {
        acceptStateToBlock[pseudoEnd] = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.finalswitch", getEnumStateName(pseudoEnd, direction)), func);
        currentSwitch->addCase(llvm::dyn_cast<llvm::ConstantInt>(module->getGlobalVariable(getEnumStateName(pseudoEnd, direction), true)->getInitializer()), acceptStateToBlock[pseudoEnd]);
    }

    for (size_t i = 0; i < params->acceptStates->size(); ++i) {
        const State* state = params->acceptStates->data() + i;
        builder.SetInsertPoint(acceptStateToBlock[state]);
        llvm::Value* endIdxVal = llvm::ConstantInt::get(builder.getInt8Ty(), static_cast<uint8_t>(i));
        builder.CreateStore(endIdxVal, symIdxArg);

        oldVal = builder.CreateLoad(builder.getInt64Ty(), strIdxArg, fmt::format("{}.oldVal", type));
        directionVal = llvm::ConstantInt::get(builder.getInt64Ty(), direction - 1);
        newVal = builder.CreateSub(oldVal, directionVal, fmt::format("{}.newVal", type));
        builder.CreateStore(newVal, strIdxArg);
        builder.CreateRet(llvm::ConstantInt::getTrue(*threadSafeContext.getContext()));
    }

    for (const State* pseudoEnd: pseudoEnds) {
        builder.SetInsertPoint(acceptStateToBlock[pseudoEnd]);
        llvm::Value* endIdxVal = llvm::ConstantInt::get(builder.getInt8Ty(), pseudoEnd->endIdx.value());
        builder.CreateStore(endIdxVal, symIdxArg);

        oldVal = builder.CreateLoad(builder.getInt64Ty(), strIdxArg, fmt::format("{}.oldVal", type));
        directionVal = llvm::ConstantInt::get(builder.getInt64Ty(), direction);
        newVal = builder.CreateSub(oldVal, directionVal, fmt::format("{}.newVal", type));
        builder.CreateStore(newVal, strIdxArg);
        builder.CreateRet(llvm::ConstantInt::getTrue(*threadSafeContext.getContext()));
    }

    builder.SetInsertPoint(finalDefaultBlock);
    builder.CreateRet(llvm::ConstantInt::getFalse(*threadSafeContext.getContext()));
}

void automata::codegen::llvmir::LLVMCompiler::generateForwards(const std::optional<parsing::LikePatternAutomaton::AutomatonParams> &params) {
        std::string type = "fwd";
        int8_t direction = 1;
        llvm::Type* i32Ty = llvm::Type::getInt32Ty(*threadSafeContext.getContext());
        llvm::Type* i64Ty = llvm::Type::getInt64Ty(*threadSafeContext.getContext());
        llvm::Type* i8Ty = llvm::Type::getInt8Ty(*threadSafeContext.getContext());
        size_t idx = 0;
        for (const State* state: params->states) {
            llvm::Constant* constant = llvm::ConstantInt::get(i32Ty, idx);
            new llvm::GlobalVariable(*module, i32Ty, true, llvm::GlobalValue::InternalLinkage, constant, getEnumStateName(state, direction));
            ++idx;
        }

        llvm::Function* func = llvm::Function::Create(funcType, llvm::Function::InternalLinkage, Compiler::getForwardParseFunctionName(), module.get());

        auto argIt = func->arg_begin();
        argIt->setName(fmt::format("{}.compressed", type));
        llvm::Value* dataArg = argIt++;
        argIt->setName(fmt::format("{}.len", type));
        llvm::Value* lenArg = argIt++;
        argIt->setName(fmt::format("{}.strIdx", type));
        llvm::Value* strIdxArg = argIt++;
        argIt->setName(fmt::format("{}.symIdx", type));
        llvm::Value* symIdxArg = argIt++;
        llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.entry", type), func);
        builder.SetInsertPoint(entryBlock);
        llvm::BasicBlock* wrongPrefixBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.wrongprefix", type), func);
        llvm::BasicBlock* correctPrefixBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.correctprefix", type), func);

        llvm::BasicBlock* startCheckBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.checkprefix", type), func);
        builder.CreateBr(startCheckBlock);
        generatePrefixCheck(params->deterministicPath.data(), params->deterministicPath.size(), 0, wrongPrefixBlock, correctPrefixBlock, func, dataArg, strIdxArg, startCheckBlock);

        builder.SetInsertPoint(wrongPrefixBlock);
        builder.CreateRet(llvm::ConstantInt::getFalse(*threadSafeContext.getContext()));

        builder.SetInsertPoint(correctPrefixBlock);
        llvm::Value* lvlPtr = builder.CreateAlloca(i64Ty, nullptr, fmt::format("{}.level", type));
        llvm::Value* level = llvm::ConstantInt::get(i64Ty, params->start->level);
        builder.CreateStore(level, lvlPtr);

        llvm::GlobalValue* startStatePtr = module->getGlobalVariable(getEnumStateName(params->start, direction), true);
        llvm::Value* startStateVal = builder.CreateLoad(builder.getInt32Ty(), startStatePtr, fmt::format("{}.value", getEnumStateName(params->start, direction)));
        llvm::Value* qPtr = builder.CreateAlloca(i32Ty, nullptr, fmt::format("{}.q", type));
        builder.CreateStore(startStateVal, qPtr);

        llvm::Value* prevBytePtr = builder.CreateAlloca(builder.getInt8Ty(), nullptr, fmt::format("{}.prevBytePtr", type));

        llvm::BasicBlock* loopCondBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.loop.cond", type), func);
        llvm::BasicBlock* loopBodyBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.loop.body", type), func);
        llvm::BasicBlock* loopEndBB  = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.loop.end", type), func);
        builder.CreateBr(loopCondBB);
        builder.SetInsertPoint(loopCondBB);
        llvm::Value* strIdxVal = builder.CreateLoad(i64Ty, strIdxArg, fmt::format("{}.strIdxVal", type));

        llvm::Value* lvlVal = builder.CreateLoad(i64Ty, lvlPtr, fmt::format("{}.level", type));
        llvm::Value* currentSum = builder.CreateAdd(strIdxVal, lvlVal, fmt::format("{}.summed", type));
        llvm::Value* cond = builder.CreateICmpSLE(currentSum, lenArg, fmt::format("{}.cmp", type));
        builder.CreateCondBr(cond, loopBodyBB, loopEndBB);

        builder.SetInsertPoint(loopBodyBB);
        llvm::Value *qVal = builder.CreateLoad(builder.getInt32Ty(), qPtr, fmt::format("{}.qVal", type));
        strIdxVal = builder.CreateLoad(builder.getInt64Ty(), strIdxArg, fmt::format("{}.strIdxVal", type));
        llvm::Value *elemPtr = builder.CreateInBoundsGEP(builder.getInt8Ty(), dataArg, strIdxVal, fmt::format("{}.elemPtr", type));
        llvm::Value *elemVal = builder.CreateLoad(builder.getInt8Ty(), elemPtr, fmt::format("{}.elemVal", type));

        std::unordered_map<const State*, llvm::BasicBlock*> stateToSwitchBlock{};
        for (const State* state: params->states) {
            stateToSwitchBlock[state] = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.switch", getEnumStateName(state, direction)), func);
        }
        llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.merge", type), func);
        llvm::SwitchInst *Switch = builder.CreateSwitch(qVal, mergeBlock, params->states.size());

        for (const State* state: params->states) {
            Switch->addCase(llvm::dyn_cast<llvm::ConstantInt>(module->getGlobalVariable(getEnumStateName(state, direction), true)->getInitializer()), stateToSwitchBlock[state]);
        }
        std::unordered_map<const State*, llvm::BasicBlock*> transitionBlocks{};
        llvm::BasicBlock* trapBlock  = llvm::BasicBlock::Create(*threadSafeContext.getContext(), "trapBlock", func);
        stateCompiler = std::make_unique<LLVMStateCodegen>(
            stateToSwitchBlock, transitionBlocks, builder, threadSafeContext, module, elemPtr, strIdxArg, symIdxArg, dataArg,
            lenArg, lvlPtr, i64Ty, i32Ty, i8Ty, elemVal, strIdxVal, prevBytePtr, func, trapBlock, type, enableSIMD
        );

        for (const State* state: params->states) {
            stateCompiler->generate(state, direction, params->error, params->acceptStates->data(), params->acceptStates->data() + params->acceptStates->size());
        }

        for (auto& [state, block]: transitionBlocks) {
            builder.SetInsertPoint(block);
            llvm::GlobalValue* nextStatePtr = module->getGlobalVariable(getEnumStateName(state, direction), true);
            llvm::Value* nextLvlVal = llvm::ConstantInt::get(i64Ty, state->level);
            builder.CreateStore(nextLvlVal, lvlPtr);

            llvm::Value* nextStateVal = builder.CreateLoad(builder.getInt32Ty(), nextStatePtr, fmt::format("{}.value", getEnumStateName(state, direction)));
            builder.CreateStore(nextStateVal, qPtr);
            builder.CreateBr(mergeBlock);
        }

        builder.SetInsertPoint(mergeBlock);
        llvm::Value* oldVal = builder.CreateLoad(builder.getInt64Ty(), strIdxArg, fmt::format("{}.oldVal", type));
        llvm::Value* directionVal = llvm::ConstantInt::get(builder.getInt64Ty(), direction);
        llvm::Value* newVal = builder.CreateAdd(oldVal, directionVal, fmt::format("{}.newVal", type));
        builder.CreateStore(newVal, strIdxArg);
        builder.CreateBr(loopCondBB);

        builder.SetInsertPoint(loopEndBB);
        qVal = builder.CreateLoad(builder.getInt32Ty(), qPtr, fmt::format("{}.qVal", type));
        llvm::BasicBlock* finalDefaultBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.finaldefault", type), func);
        std::unordered_map<const State*, llvm::BasicBlock*> acceptStateToBlock{};
        llvm::SwitchInst* currentSwitch = builder.CreateSwitch(qVal, finalDefaultBlock, params->acceptStates->size());

        for (size_t i = 0; i < params->acceptStates->size(); ++i) {
            const State* state = params->acceptStates->data() + i;
            acceptStateToBlock[state] = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.finalswitch", getEnumStateName(state, direction)), func);
            currentSwitch->addCase(llvm::dyn_cast<llvm::ConstantInt>(module->getGlobalVariable(getEnumStateName(state, direction), true)->getInitializer()), acceptStateToBlock[state]);
        }

        for (size_t i = 0; i < params->acceptStates->size(); ++i) {
            const State* state = params->acceptStates->data() + i;
            builder.SetInsertPoint(acceptStateToBlock[state]);
            llvm::Value* endIdxVal = llvm::ConstantInt::get(builder.getInt8Ty(), static_cast<uint8_t>(i));
            builder.CreateStore(endIdxVal, symIdxArg);

            oldVal = builder.CreateLoad(builder.getInt64Ty(), strIdxArg, fmt::format("{}.oldVal", type));
            directionVal = llvm::ConstantInt::get(builder.getInt64Ty(), direction);
            newVal = builder.CreateSub(oldVal, directionVal, fmt::format("{}.newVal", type));
            builder.CreateStore(newVal, strIdxArg);
            builder.CreateRet(llvm::ConstantInt::getTrue(*threadSafeContext.getContext()));
        }

        builder.SetInsertPoint(finalDefaultBlock);
        builder.CreateRet(llvm::ConstantInt::getFalse(*threadSafeContext.getContext()));

        builder.SetInsertPoint(trapBlock);
        llvm::Function* trap = llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::trap);
        builder.CreateCall(trap);
        builder.CreateUnreachable();
}

void automata::codegen::llvmir::LLVMCompiler::generateFullParse(const std::optional<size_t>& backwardsLevel, const std::optional<size_t>& forwardLevel, const ParsingType& parseType) {
    auto *Int8PtrTy = llvm::Type::getInt8PtrTy(*threadSafeContext.getContext());
    auto *SizeTy = llvm::Type::getInt64Ty(*threadSafeContext.getContext());  // size_t -> i64
    auto *BoolTy = llvm::Type::getInt1Ty(*threadSafeContext.getContext());
    std::string type = "full";

    std::vector<llvm::Type*> ParamTypes = {Int8PtrTy, SizeTy};
    llvm::FunctionType *FuncTy = llvm::FunctionType::get(BoolTy, ParamTypes, false);

    llvm::Function* func = llvm::Function::Create(FuncTy, llvm::Function::ExternalLinkage, getParseFunctionName(), module.get());
    func->setCallingConv(llvm::CallingConv::C);

    llvm::BasicBlock *EntryBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.entry", type) , func);

    auto argIt = func->arg_begin();
    argIt->setName(fmt::format("{}.compressed", type));
    llvm::Value* dataArg = argIt++;
    argIt->setName(fmt::format("{}.len", type));
    llvm::Value* lenArg = argIt++;

    builder.SetInsertPoint(EntryBB);

    switch (parseType) {
        case ParsingType::NO_DIRECTION:
            builder.CreateRet(builder.getFalse());
            return;
        case ParsingType::ONLY_FORWARD:
        {
            llvm::BasicBlock *returnFalseBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.retFalse", type) , func);
            llvm::BasicBlock *continueCheckBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.continueChecking", type) , func);
            llvm::Value* levelConst = builder.getInt64(forwardLevel.value());
            llvm::Value* cannotMatch = builder.CreateICmpULT(lenArg, levelConst, fmt::format("{}.cannotMatch", type));
            builder.CreateCondBr(cannotMatch, returnFalseBB, continueCheckBB);
            builder.SetInsertPoint(continueCheckBB);
            llvm::Value* StrIdx = builder.CreateAlloca(builder.getInt64Ty(), nullptr, fmt::format("{}.strIdx2", type));
            builder.CreateStore(builder.getInt64(0), StrIdx);

            llvm::Value* SymIdx = builder.CreateAlloca(builder.getInt8Ty(), nullptr, fmt::format("{}.endSymIdx", type));
            llvm::Function* ParseFwdStateFunc = module->getFunction(getForwardParseFunctionName());

            llvm::Value* CanParse = builder.CreateCall(
                ParseFwdStateFunc,
                {dataArg, lenArg, StrIdx, SymIdx},
                fmt::format("{}.canParseFwd",  type)
            );
            builder.CreateRet(CanParse);

            builder.SetInsertPoint(returnFalseBB);
            builder.CreateRet(builder.getFalse());
            return;
        }
        case ParsingType::ONLY_BACKWARDS:
        {
            llvm::BasicBlock *returnFalseBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.retFalse", type) , func);
            llvm::BasicBlock *continueCheckBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.continueChecking", type) , func);
            llvm::Value* levelConst = builder.getInt64(backwardsLevel.value());
            llvm::Value* cannotMatch = builder.CreateICmpULT(lenArg, levelConst, fmt::format("{}.cannotMatch", type));
            builder.CreateCondBr(cannotMatch, returnFalseBB, continueCheckBB);
            builder.SetInsertPoint(continueCheckBB);
            llvm::Value* StrIdx = builder.CreateAlloca(builder.getInt64Ty(), nullptr, fmt::format("{}.strIdx", type));
            builder.CreateStore(builder.CreateSub(lenArg, builder.getInt64(1)), StrIdx);

            llvm::Value* SymIdx = builder.CreateAlloca(builder.getInt8Ty(), nullptr, fmt::format("{}.endSymIdx", type));
            llvm::Function* ParseBwdStateFunc = module->getFunction(getBackwardsParseFunctionName());

            llvm::Value* CanParse = builder.CreateCall(
                ParseBwdStateFunc,
                {dataArg, lenArg, StrIdx, SymIdx},
                fmt::format("{}.canParseBwd",  type)
            );
            builder.CreateRet(CanParse);

            builder.SetInsertPoint(returnFalseBB);
            builder.CreateRet(builder.getFalse());
            return;
        }
        case ParsingType::BOTH_DIRECTIONS:
        {
            llvm::BasicBlock *ReturnFalseBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.retFalse", type) , func);
            llvm::BasicBlock *BeforeBwdBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.beforeBwd", type), func);
            llvm::Value* LenCheck = builder.CreateICmpULT(lenArg, builder.getInt64(backwardsLevel.value() + forwardLevel.value() - 1));
            builder.CreateCondBr(LenCheck, ReturnFalseBB, BeforeBwdBB);
            builder.SetInsertPoint(BeforeBwdBB);
            llvm::Value* StrIdx = builder.CreateAlloca(builder.getInt64Ty(), nullptr, fmt::format("{}.strIdx", type));
            builder.CreateStore(builder.CreateSub(lenArg, builder.getInt64(1)), StrIdx);

            llvm::Value* EndSymIdx = builder.CreateAlloca(builder.getInt8Ty(), nullptr, fmt::format("{}.endSymIdx", type));

            llvm::Function* ParseBwdStateFunc = module->getFunction(getBackwardsParseFunctionName());
            // If not found, declare it similarly

            llvm::Value* CanParse = builder.CreateCall(
                ParseBwdStateFunc,
                {dataArg, lenArg, StrIdx, EndSymIdx},
                fmt::format("{}.canParseBwd",  type)
            );

            llvm::BasicBlock *secondCheckBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.secondCheck", type), func);

            builder.CreateCondBr(CanParse, secondCheckBB, ReturnFalseBB);


            builder.SetInsertPoint(secondCheckBB);
            llvm::Value* currentStrIdxVal = builder.CreateLoad(builder.getInt64Ty(), StrIdx, "{}.loadedStrIdx");
            llvm::Value* compareConst = builder.getInt64(forwardLevel.value() - 1);
            llvm::Value* cannotMatch = builder.CreateICmpULT(currentStrIdxVal, compareConst, fmt::format("{}.cannotMatch", type));
            llvm::BasicBlock *checkFwdBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.checkFwd", type), func);
            builder.CreateCondBr(cannotMatch, ReturnFalseBB, checkFwdBB);

            builder.SetInsertPoint(checkFwdBB);

            llvm::Value* StartSymIdx = builder.CreateAlloca(builder.getInt8Ty(), nullptr, fmt::format("{}.startSymIdx", type));
            llvm::Value* StrIdx2 = builder.CreateAlloca(builder.getInt64Ty(), nullptr, fmt::format("{}.strIdx2", type));
            builder.CreateStore(builder.getInt64(0), StrIdx2);

            llvm::Value* StrIdxVal = builder.CreateLoad(builder.getInt64Ty(), StrIdx, fmt::format("{}.strIdxVal", type));
            llvm::Value* StrIdxPlus1 = builder.CreateAdd(StrIdxVal, builder.getInt64(1));

            llvm::Function* ParseFwdStateFunc = module->getFunction(getForwardParseFunctionName());

            llvm::Value* CanParse2 = builder.CreateCall(
                ParseFwdStateFunc,
                {dataArg, StrIdxPlus1, StrIdx2, StartSymIdx},
                fmt::format("{}.canFwdParse", type)
            );

            llvm::Value* StrIdx2Val    = builder.CreateLoad(builder.getInt64Ty(), StrIdx2, fmt::format("{}.strIdx2Val", type) );
            llvm::Value* StartSymIdxVal = builder.CreateLoad(builder.getInt8Ty(), StartSymIdx, fmt::format("{}.startSymIdxVal", type) );
            llvm::Value* EndSymIdxVal   = builder.CreateLoad(builder.getInt8Ty(), EndSymIdx, fmt::format("{}.endSymIdxVal", type));

            llvm::Value* Cond1 = builder.CreateICmpULT(StrIdx2Val, StrIdxVal);
            llvm::Value* Cond2 = builder.CreateICmpULE(StartSymIdxVal, EndSymIdxVal);
            llvm::Value* FinalCond = builder.CreateAnd(CanParse2, builder.CreateOr(Cond1, Cond2));

            builder.CreateRet(FinalCond);

            builder.SetInsertPoint(ReturnFalseBB);
            builder.CreateRet(builder.getFalse());
        }
    }
}

void automata::codegen::llvmir::LLVMCompiler::generatePrefixCheck(const uint8_t* prefix, size_t size, size_t offset, llvm::BasicBlock *retFalse, llvm::BasicBlock* successBlock, llvm::Function* func, llvm::Value* dataArg, llvm::Value* strIdxArg, llvm::BasicBlock* currentBlock) {
    switch (size) {
        case 7:
        case 6:
        case 5:
        case 4:
        {
            // case II: prefix length in [4, 7]
            llvm::BasicBlock* nextBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("success.pfcheck.{}", offset), func);
            builder.SetInsertPoint(currentBlock);
            llvm::Value* llvmOffset = builder.getInt64(offset);
            llvm::Value* movedPtr = builder.CreateGEP(builder.getInt8Ty(), dataArg, llvmOffset, fmt::format("compressedptr.{}", offset));
            llvm::LoadInst* loadedValue = builder.CreateLoad(builder.getInt32Ty(), movedPtr, fmt::format("i32val.{}", offset));
            loadedValue->setAlignment(llvm::Align(1));
            llvm::GlobalValue* constantValuePtr = module->getGlobalVariable(getPrefixName(offset), true);
            llvm::Value* constantValue = builder.CreateLoad(builder.getInt32Ty(), constantValuePtr, fmt::format("i32prefix.{}", offset));
            llvm::Value* isEqual = builder.CreateICmpEQ(loadedValue, constantValue, fmt::format("eqcmp.{}", offset));
            builder.CreateCondBr(isEqual, nextBlock, retFalse);
            generatePrefixCheck(prefix + 4, size - 4, offset + 4, retFalse, successBlock, func, dataArg, strIdxArg, nextBlock);
            break;
        }
        case 3:
        case 2:
        {
            // case III: prefix length in [2, 3]
            llvm::BasicBlock* nextBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("success.pfcheck.{}", offset), func);
            builder.SetInsertPoint(currentBlock);
            llvm::Value* llvmOffset = builder.getInt64(offset);
            llvm::Value* movedPtr = builder.CreateGEP(builder.getInt8Ty(), dataArg, llvmOffset, fmt::format("compressedptr.{}", offset));
            llvm::LoadInst* loadedValue = builder.CreateLoad(builder.getInt16Ty(), movedPtr, fmt::format("i16val.{}", offset));
            loadedValue->setAlignment(llvm::Align(1));
            llvm::GlobalValue* constantValuePtr = module->getGlobalVariable(getPrefixName(offset), true);
            llvm::Value* constantValue = builder.CreateLoad(builder.getInt16Ty(), constantValuePtr, fmt::format("i16prefix.{}", offset));
            llvm::Value* isEqual = builder.CreateICmpEQ(loadedValue, constantValue, fmt::format("eqcmp.{}", offset));
            builder.CreateCondBr(isEqual, nextBlock, retFalse);
            generatePrefixCheck(prefix + 2, size - 2, offset + 2, retFalse, successBlock, func, dataArg, strIdxArg, nextBlock);
            break;
        }
        case 1:
        {
            // case IV: prefix length is 1
            llvm::BasicBlock* nextBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("success.pfcheck.{}", offset), func);
            builder.SetInsertPoint(currentBlock);
            llvm::Value* llvmOffset = builder.getInt64(offset);
            llvm::Value* movedPtr = builder.CreateGEP(builder.getInt8Ty(), dataArg, llvmOffset, fmt::format("compressedptr.{}", offset));
            llvm::LoadInst* loadedValue = builder.CreateLoad(builder.getInt8Ty(), movedPtr, fmt::format("i8val.{}", offset));
            loadedValue->setAlignment(llvm::Align(1));
            llvm::GlobalValue* constantValuePtr = module->getGlobalVariable(getPrefixName(offset), true);
            llvm::Value* constantValue = builder.CreateLoad(builder.getInt8Ty(), constantValuePtr, fmt::format("i8prefix.{}", offset));
            llvm::Value* isEqual = builder.CreateICmpEQ(loadedValue, constantValue, fmt::format("eqcmp.{}", offset));
            builder.CreateCondBr(isEqual, nextBlock, retFalse);
            generatePrefixCheck(prefix + 1, size - 1, offset + 1, retFalse, successBlock, func, dataArg, strIdxArg, nextBlock);
            break;
        }
        case 0:
        {
            builder.SetInsertPoint(currentBlock);
            llvm::Value* llvmOffset = builder.getInt64(offset);
            builder.CreateStore(llvmOffset, strIdxArg);
            builder.CreateBr(successBlock);
            break;
        }
        default:
        {
            // case I: prefix length >= 8
            if (size >= 16 && enableSIMD) {
                llvm::BasicBlock* nextBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("success.pfcheck.{}", offset), func);
                builder.SetInsertPoint(currentBlock);
                llvm::Value* llvmOffset = builder.getInt64(offset);
                llvm::Value* movedPtr = builder.CreateGEP(builder.getInt8Ty(), dataArg, llvmOffset, fmt::format("compressedptr.{}", offset));

                llvm::FixedVectorType* simdVecType = llvm::FixedVectorType::get(builder.getInt32Ty(), 4);
                llvm::GlobalValue* currentPrefixPtr = module->getGlobalVariable(getPrefixName(offset), true);
                llvm::Value* currentPrefixVal = builder.CreateAlignedLoad(simdVecType, currentPrefixPtr, llvm::Align(16), fmt::format("prefixValue.{}", offset));

                llvm::LoadInst* compressedVec = builder.CreateAlignedLoad(simdVecType, movedPtr, llvm::Align(1), fmt::format("i128val.{}", offset));

                llvm::Value* cmpVector = builder.CreateICmpEQ(currentPrefixVal, compressedVec, fmt::format("vecCmp.{}", offset));
                llvm::Value* isEqual = builder.CreateAndReduce(cmpVector);
                isEqual->setName(fmt::format("eqcmp.{}", offset));
                builder.CreateCondBr(isEqual, nextBlock, retFalse);
                generatePrefixCheck(prefix + 16, size - 16, offset + 16, retFalse, successBlock, func, dataArg, strIdxArg, nextBlock);
            } else {
                llvm::BasicBlock* nextBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("success.pfcheck.{}", offset), func);
                builder.SetInsertPoint(currentBlock);
                llvm::Value* llvmOffset = builder.getInt64(offset);
                llvm::Value* movedPtr = builder.CreateGEP(builder.getInt8Ty(), dataArg, llvmOffset, fmt::format("compressedptr.{}", offset));
                llvm::LoadInst* loadedValue = builder.CreateLoad(builder.getInt64Ty(), movedPtr, fmt::format("i64val.{}", offset));
                loadedValue->setAlignment(llvm::Align(1));
                llvm::GlobalValue* constantValuePtr = module->getGlobalVariable(getPrefixName(offset), true);
                llvm::Value* constantValue = builder.CreateLoad(builder.getInt64Ty(), constantValuePtr, fmt::format("i64prefix.{}", offset));
                llvm::Value* isEqual = builder.CreateICmpEQ(loadedValue, constantValue, fmt::format("eqcmp.{}", offset));
                builder.CreateCondBr(isEqual, nextBlock, retFalse);
                generatePrefixCheck(prefix + 8, size - 8, offset + 8, retFalse, successBlock, func, dataArg, strIdxArg, nextBlock);
            }
            break;
        }
    }
}

void automata::codegen::llvmir::LLVMCompiler::generateSuffixCheck(const uint8_t* suffix, size_t size, size_t offset, llvm::BasicBlock* retFalse, llvm::BasicBlock* successBlock, llvm::Function* func, llvm::Value* dataArg, llvm::Value* strIdxArg, llvm::Value* lenArg, llvm::BasicBlock* currentBlock) {
    switch (size) {
        case 7:
        case 6:
        case 5:
        case 4:
        {
            // case II: prefix length in [4, 7]
            llvm::BasicBlock* nextBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("success.sfcheck.{}", offset), func);
            builder.SetInsertPoint(currentBlock);
            llvm::Value* llvmOffset = builder.getInt64(offset);
            llvm::Value* lenMinusOffset = builder.CreateSub(lenArg, llvmOffset);
            llvm::Value* constFour = builder.getInt64(4);
            llvm::Value* llvmStartOffset = builder.CreateSub(lenMinusOffset, constFour, fmt::format("sfoffset.{}", offset));
            llvm::Value* movedPtr = builder.CreateGEP(builder.getInt8Ty(), dataArg, llvmStartOffset, fmt::format("compressedptr.{}", offset));
            llvm::LoadInst* loadedValue = builder.CreateLoad(builder.getInt32Ty(), movedPtr, fmt::format("i32val.{}", offset));
            loadedValue->setAlignment(llvm::Align(1));
            llvm::GlobalValue* constantValuePtr = module->getGlobalVariable(getSuffixName(size), true);
            llvm::Value* constantValue = builder.CreateLoad(builder.getInt32Ty(), constantValuePtr, fmt::format("i32suffix.{}", offset));
            llvm::Value* isEqual = builder.CreateICmpEQ(loadedValue, constantValue, fmt::format("eqcmp.{}", offset));
            builder.CreateCondBr(isEqual, nextBlock, retFalse);
            generateSuffixCheck(suffix - 4, size - 4, offset + 4, retFalse, successBlock, func, dataArg, strIdxArg, lenArg, nextBlock);
            break;
        }
        case 3:
        case 2:
        {
            // case III: prefix length in [2, 3]
            llvm::BasicBlock* nextBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("success.sfcheck.{}", offset), func);
            builder.SetInsertPoint(currentBlock);
            llvm::Value* llvmOffset = builder.getInt64(offset);
            llvm::Value* lenMinusOffset = builder.CreateSub(lenArg, llvmOffset);
            llvm::Value* constTwo = builder.getInt64(2);
            llvm::Value* llvmStartOffset = builder.CreateSub(lenMinusOffset, constTwo, fmt::format("sfoffset.{}", offset));
            llvm::Value* movedPtr = builder.CreateGEP(builder.getInt8Ty(), dataArg, llvmStartOffset, fmt::format("compressedptr.{}", offset));
            llvm::LoadInst* loadedValue = builder.CreateLoad(builder.getInt16Ty(), movedPtr, fmt::format("i16val.{}", offset));
            loadedValue->setAlignment(llvm::Align(1));
            llvm::GlobalValue* constantValuePtr = module->getGlobalVariable(getSuffixName(size), true);
            llvm::Value* constantValue = builder.CreateLoad(builder.getInt16Ty(), constantValuePtr, fmt::format("i16ptr.{}", offset));
            llvm::Value* isEqual = builder.CreateICmpEQ(loadedValue, constantValue, fmt::format("eqcmp.{}", offset));
            builder.CreateCondBr(isEqual, nextBlock, retFalse);
            generateSuffixCheck(suffix - 2, size - 2, offset + 2, retFalse, successBlock, func, dataArg, strIdxArg, lenArg, nextBlock);
            break;
        }
        case 1:
        {
            // case IV: prefix length is 1
            llvm::BasicBlock* nextBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("success.sfcheck.{}", offset), func);
            builder.SetInsertPoint(currentBlock);
            llvm::Value* llvmOffset = builder.getInt64(offset);
            llvm::Value* lenMinusOffset = builder.CreateSub(lenArg, llvmOffset);
            llvm::Value* constOne = builder.getInt64(1);
            llvm::Value* llvmStartOffset = builder.CreateSub(lenMinusOffset, constOne, fmt::format("sfoffset.{}", offset));
            llvm::Value* movedPtr = builder.CreateGEP(builder.getInt8Ty(), dataArg, llvmStartOffset, fmt::format("compressedptr.{}", offset));
            llvm::LoadInst* loadedValue = builder.CreateLoad(builder.getInt8Ty(), movedPtr, fmt::format("i8val.{}", offset));
            loadedValue->setAlignment(llvm::Align(1));
            llvm::GlobalValue* constantValuePtr = module->getGlobalVariable(getSuffixName(size), true);
            llvm::Value* constantValue = builder.CreateLoad(builder.getInt8Ty(), constantValuePtr, fmt::format("i8suffix.{}", offset));
            llvm::Value* isEqual = builder.CreateICmpEQ(loadedValue, constantValue, fmt::format("eqcmp.{}", offset));
            builder.CreateCondBr(isEqual, nextBlock, retFalse);
            generateSuffixCheck(suffix - 1, size - 1, offset + 1, retFalse, successBlock, func, dataArg, strIdxArg, lenArg, nextBlock);
            break;
        }
        case 0:
        {
            builder.SetInsertPoint(currentBlock);
            llvm::Value* llvmOffset = builder.getInt64(offset);
            llvm::Value* lenMinusOffset = builder.CreateSub(lenArg, llvmOffset);
            llvm::Value* constOne = builder.getInt64(1);
            llvm::Value* llvmStartOffset = builder.CreateSub(lenMinusOffset, constOne, fmt::format("sfoffset.{}", offset));
            builder.CreateStore(llvmStartOffset, strIdxArg);
            builder.CreateBr(successBlock);
            break;
        }
        default:
        {
            // case I: prefix length >= 8
            if (size >= 16 && enableSIMD) {
                llvm::BasicBlock* nextBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("success.sfcheck.{}", offset), func);
                builder.SetInsertPoint(currentBlock);
                llvm::Value* llvmOffset = builder.getInt64(offset);
                llvm::Value* lenMinusOffset = builder.CreateSub(lenArg, llvmOffset);
                llvm::Value* constSixteen = builder.getInt64(16);
                llvm::Value* llvmStartOffset = builder.CreateSub(lenMinusOffset, constSixteen, fmt::format("sfoffset.{}", offset));
                llvm::Value* movedPtr = builder.CreateGEP(builder.getInt8Ty(), dataArg, llvmStartOffset, fmt::format("compressedptr.{}", offset));

                llvm::FixedVectorType* simdVecType = llvm::FixedVectorType::get(builder.getInt32Ty(), 4);
                llvm::GlobalValue* currentSuffixPtr = module->getGlobalVariable(getSuffixName(size), true);
                llvm::Value* currentSuffixVal = builder.CreateAlignedLoad(simdVecType, currentSuffixPtr, llvm::Align(16), fmt::format("suffixValue.{}", offset));

                llvm::LoadInst* compressedVec = builder.CreateAlignedLoad(simdVecType, movedPtr, llvm::Align(1), fmt::format("i128val.{}", offset));
                llvm::Value* cmpVector = builder.CreateICmpEQ(currentSuffixVal, compressedVec, fmt::format("vecCmp.{}", offset));
                llvm::Value* isEqual = builder.CreateAndReduce(cmpVector);
                isEqual->setName(fmt::format("eqcmp.{}", offset));
                builder.CreateCondBr(isEqual, nextBlock, retFalse);
                generateSuffixCheck(suffix - 16, size - 16, offset + 16, retFalse, successBlock, func, dataArg, strIdxArg, lenArg, nextBlock);
            } else {
                llvm::BasicBlock* nextBlock = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("success.sfcheck.{}", offset), func);
                builder.SetInsertPoint(currentBlock);
                llvm::Value* llvmOffset = builder.getInt64(offset);
                llvm::Value* lenMinusOffset = builder.CreateSub(lenArg, llvmOffset);
                llvm::Value* constEight = builder.getInt64(8);
                llvm::Value* llvmStartOffset = builder.CreateSub(lenMinusOffset, constEight, fmt::format("sfoffset.{}", offset));
                llvm::Value* movedPtr = builder.CreateGEP(builder.getInt8Ty(), dataArg, llvmStartOffset, fmt::format("compressedptr.{}", offset));
                llvm::LoadInst* loadedValue = builder.CreateLoad(builder.getInt64Ty(), movedPtr, fmt::format("i64val.{}", offset));
                loadedValue->setAlignment(llvm::Align(1));
                llvm::GlobalValue* constantValuePtr = module->getGlobalVariable(getSuffixName(size), true);
                llvm::Value* constantValue = builder.CreateLoad(builder.getInt64Ty(), constantValuePtr, fmt::format("i64suffix.{}", offset));
                llvm::Value* isEqual = builder.CreateICmpEQ(loadedValue, constantValue, fmt::format("eqcmp.{}", offset));
                builder.CreateCondBr(isEqual, nextBlock, retFalse);
                generateSuffixCheck(suffix - 8, size - 8, offset + 8, retFalse, successBlock, func, dataArg, strIdxArg, lenArg, nextBlock);
            }
            break;
        }
    }
}

automata::codegen::llvmir::LLVMStateCodegen::LLVMStateCodegen(
    std::unordered_map<const State*, llvm::BasicBlock*> &stateToSwitchBlock, std::unordered_map<const State*, llvm::BasicBlock*> &transitionBlocks,
    llvm::IRBuilder<>& builder, llvm::orc::ThreadSafeContext& threadSafeContext, std::unique_ptr<llvm::Module>& module,
    llvm::Value*& elemPtr, llvm::Value*& strIdxArg, llvm::Value*& symIdxArg, llvm::Value*& dataArg, llvm::Value*& lenArg,
    llvm::Value*& lvlPtr, llvm::Type*& i64Ty, llvm::Type*& i32Ty, llvm::Type*& i8Ty, llvm::Value*& elemVal, llvm::Value*& strIdxVal,
    llvm::Value*& prevBytePtr, llvm::Function*& func, llvm::BasicBlock*& trapBlock, std::string& type, bool &enableSIMD):
        enableSIMD(enableSIMD), stateToSwitchBlock(stateToSwitchBlock), transitionBlocks(transitionBlocks),
        builder(builder), threadSafeContext(threadSafeContext), module(module), elemPtr(elemPtr), strIdxArg(strIdxArg),
        symIdxArg(symIdxArg), dataArg(dataArg), lvlPtr(lvlPtr), lenArg(lenArg), i64Ty(i64Ty), i32Ty(i32Ty), i8Ty(i8Ty),
        elemVal(elemVal), strIdxVal(strIdxVal), prevBytePtr(prevBytePtr), func(func), trapBlock(trapBlock), type(type)
{}

void automata::codegen::llvmir::LLVMStateCodegen::generateEnd(const State *state, int direction, std::ptrdiff_t endIdx) {
    builder.SetInsertPoint(stateToSwitchBlock[state]);
    llvm::Value* endIdxVal = llvm::ConstantInt::get(builder.getInt8Ty(), static_cast<uint8_t>(endIdx));
    builder.CreateStore(endIdxVal, symIdxArg);

    llvm::Value* directionVal;
    if (direction == 1) {
        directionVal = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
    } else {
        directionVal = llvm::ConstantInt::get(builder.getInt64Ty(), -2);
    }
    llvm::Value* newVal = builder.CreateSub(strIdxVal, directionVal, fmt::format("{}.newVal", type));
    builder.CreateStore(newVal, strIdxArg);
    builder.CreateRet(llvm::ConstantInt::getTrue(*threadSafeContext.getContext()));

}


namespace {
    // Whether byte `i` of `data` is the literal of an escape pair. Every non-255
    // byte ends a token, so only the parity of the run of 255 bytes in front of
    // `i` decides it: an even run leaves `i` a code (or a marker), an odd run
    // makes it the escaped literal. Emitted once per module, on first use.
    llvm::Function* getOrCreateIsEscapedLiteral(llvm::Module& module, llvm::IRBuilder<>& builder, llvm::Type* dataPtrTy) {
        if (llvm::Function* existing = module.getFunction("isEscapedLiteral")) {
            return existing;
        }
        llvm::LLVMContext& ctx = module.getContext();
        llvm::Type* i64Ty = llvm::Type::getInt64Ty(ctx);
        llvm::Type* i8Ty = llvm::Type::getInt8Ty(ctx);
        llvm::FunctionType* fnTy = llvm::FunctionType::get(llvm::Type::getInt1Ty(ctx), {dataPtrTy, i64Ty}, false);
        llvm::Function* fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "isEscapedLiteral", module);
        llvm::Value* data = fn->getArg(0);
        llvm::Value* idx = fn->getArg(1);
        llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(ctx, "entry", fn);
        llvm::BasicBlock* condBB = llvm::BasicBlock::Create(ctx, "run.cond", fn);
        llvm::BasicBlock* checkBB = llvm::BasicBlock::Create(ctx, "run.check", fn);
        llvm::BasicBlock* doneBB = llvm::BasicBlock::Create(ctx, "run.done", fn);

        llvm::IRBuilderBase::InsertPointGuard guard(builder);
        builder.SetInsertPoint(entryBB);
        builder.CreateBr(condBB);

        builder.SetInsertPoint(condBB);
        llvm::PHINode* runStart = builder.CreatePHI(i64Ty, 2, "runStart");
        runStart->addIncoming(idx, entryBB);
        llvm::Value* hasPrevious = builder.CreateICmpSGT(runStart, llvm::ConstantInt::get(i64Ty, 0), "hasPrevious");
        builder.CreateCondBr(hasPrevious, checkBB, doneBB);

        builder.SetInsertPoint(checkBB);
        llvm::Value* previousIndex = builder.CreateSub(runStart, llvm::ConstantInt::get(i64Ty, 1), "previousIndex");
        llvm::Value* previousBytePtr = builder.CreateInBoundsGEP(i8Ty, data, previousIndex, "previousBytePtr");
        llvm::Value* previousByteVal = builder.CreateLoad(i8Ty, previousBytePtr, "previousByteVal");
        llvm::Value* isEscapeByte = builder.CreateICmpEQ(previousByteVal, llvm::ConstantInt::get(i8Ty, 255), "isEscapeByte");
        runStart->addIncoming(previousIndex, checkBB);
        builder.CreateCondBr(isEscapeByte, condBB, doneBB);

        builder.SetInsertPoint(doneBB);
        llvm::Value* runLength = builder.CreateSub(idx, runStart, "runLength");
        llvm::Value* runParity = builder.CreateAnd(runLength, llvm::ConstantInt::get(i64Ty, 1), "runParity");
        builder.CreateRet(builder.CreateICmpNE(runParity, llvm::ConstantInt::get(i64Ty, 0), "isEscapedLiteral"));
        return fn;
    }
}
void automata::codegen::llvmir::LLVMStateCodegen::generateMiddleStart(const State *state, const State *error, int direction) {
    builder.SetInsertPoint(stateToSwitchBlock[state]);
    std::string llvmName = LLVMCompiler::getEnumStateName(state, direction);
    llvm::Function* isEscapedLiteralFn = getOrCreateIsEscapedLiteral(*module, builder, dataArg->getType());
    for (const auto& [symbol, next]: state->transitions) {
        if (symbol == 255 && next->transitions.empty()) {
            continue;
        }
        if (!transitionBlocks.contains(next)) {
            transitionBlocks[next] = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("transition.{}", LLVMCompiler::getEnumStateName(next, direction)), func);
        }
    }

    llvm::Value* lvlVal = builder.getInt64(state->level);
    llvm::Value* maxIdxVal = builder.CreateSub(lenArg, lvlVal, fmt::format("{}.maxIdx", llvmName));
    llvm::BasicBlock* switchCasesBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.switchCases", llvmName), func);
    llvm::Type* i16Ty = builder.getInt16Ty();

    auto parsingMode = doesParsingUseSIMD(state, enableSIMD);
    if (parsingMode != ParsingMode::NO_SIMD) {
        llvm::Value* maskPtr = builder.CreateAlloca(i16Ty, nullptr, fmt::format("{}.maskPtr", llvmName));
        std::vector<llvm::Value*> simdValues{};
        llvm::FixedVectorType* simdVecType = llvm::FixedVectorType::get(i8Ty, 16);
        if (parsingMode == StateCodegen::ParsingMode::SIMD_CMPESTRM) {
            llvm::GlobalVariable* transitionVecPtr = module->getGlobalVariable(LLVMCompiler::getSIMDCmpestrmVectorName(state), true);
            llvm::Value* transitionVecVal = builder.CreateAlignedLoad(simdVecType, transitionVecPtr, llvm::Align(16), fmt::format("{}.simdTransitionsVector", llvmName));
            simdValues.push_back(transitionVecVal);
        } else {
            for (const auto& [symbol, next]: state->transitions) {
                if (symbol == 255 && next->transitions.empty()) {
                    continue;
                }
                llvm::GlobalVariable* transitionVecPtr = module->getGlobalVariable(LLVMCompiler::getSIMDCmpeqepi8VectorName(state, symbol), true);
                llvm::Value* transitionVecVal = builder.CreateAlignedLoad(simdVecType, transitionVecPtr, llvm::Align(16), fmt::format("{}.simd{}Vector", llvmName, symbol));
                simdValues.push_back(transitionVecVal);
            }
        }
        llvm::Value* simdReadSize = llvm::ConstantInt::get(i64Ty, 15);
        llvm::Value* maxSIMDIdx = builder.CreateSub(maxIdxVal, simdReadSize, fmt::format("{}.maxSIMDIdx", llvmName));
        llvm::BasicBlock* simdLoadCondBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.simdLoadCondition", llvmName), func);
        builder.CreateBr(simdLoadCondBB);
        builder.SetInsertPoint(simdLoadCondBB);

        llvm::Value* currentStrIdxVal = builder.CreateLoad(i64Ty, strIdxArg, fmt::format("{}.strIdxVal", llvmName));
        llvm::Value* canSIMDLoad = builder.CreateICmpSLE(currentStrIdxVal, maxSIMDIdx, fmt::format("{}.canSIMDLoad", llvmName));
        llvm::BasicBlock* simdCheckBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.simdCheck", llvmName), func);
        llvm::BasicBlock* afterSimdCheckBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.afterSimdCheck", llvmName), func);

        builder.CreateCondBr(canSIMDLoad, simdCheckBB, afterSimdCheckBB);
        builder.SetInsertPoint(simdCheckBB);
        llvm::Value* simdPtr = builder.CreateGEP(i8Ty, dataArg, currentStrIdxVal, fmt::format("{}.SIMDptr", llvmName));
        llvm::LoadInst* compressedVec = builder.CreateAlignedLoad(simdVecType, simdPtr, llvm::Align(1), fmt::format("{}.SIMDvec", llvmName));
        if (parsingMode == ParsingMode::SIMD_CMPESTRM) {
            int controlFlag;
            #ifdef __SSE4_2__
            controlFlag = _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_BIT_MASK;
            #else
            throw std::runtime_error("Cannot programatically obtain the mask value");
            #endif
            llvm::Function* pcmpestrmFn = llvm::Intrinsic::getDeclaration(
                module.get(),
                llvm::Intrinsic::x86_sse42_pcmpestrm128
            );
            llvm::Value* mask128i = builder.CreateCall(pcmpestrmFn, {
                simdValues[0],
                llvm::ConstantInt::get(i32Ty, getNumRealTransitions(state)),
                compressedVec,
                llvm::ConstantInt::get(i32Ty, 16),
                llvm::ConstantInt::get(i32Ty, controlFlag)
            },
                fmt::format("{}.mask128i", llvmName));

            llvm::FixedVectorType* v8i16Ty = llvm::FixedVectorType::get(i16Ty, 8);
            llvm::Value* castedMask = builder.CreateBitCast(mask128i, v8i16Ty, fmt::format("{}.castedMask", llvmName));
            llvm::Value* mask = builder.CreateExtractElement(castedMask, builder.getInt32(0), fmt::format("{}.mask", llvmName));
            builder.CreateStore(mask, maskPtr);
        } else {
            llvm::Value* mask = llvm::ConstantInt::get(i16Ty, 0);
            llvm::Function* pmovmskbFn = llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::x86_sse2_pmovmskb_128);

            for (size_t i = 0; i < simdValues.size(); ++i) {
                llvm::Value* currentMask = builder.CreateICmpEQ(compressedVec, simdValues[i], fmt::format("{}.currentMask{}", llvmName, i));
                llvm::Value* currentMaskExtended = builder.CreateSExt(currentMask, simdVecType, fmt::format("{}.extendedMask{}", llvmName, i));
                llvm::Value* mask32i = builder.CreateCall(pmovmskbFn, {currentMaskExtended}, fmt::format("{}.mask32i{}", llvmName, i));
                llvm::Value* mask16i = builder.CreateTrunc(mask32i, builder.getInt16Ty(), fmt::format("{}.mask16i{}", llvmName, i));
                mask = builder.CreateOr(mask, mask16i, fmt::format("{}.updatedMask{}", llvmName, i));
            }
            builder.CreateStore(mask, maskPtr);
        }
        llvm::BasicBlock* maskWhileCondBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.maskWhileCond", llvmName), func);
        builder.CreateBr(maskWhileCondBB);
        builder.SetInsertPoint(maskWhileCondBB);
        llvm::Value* currentMask = builder.CreateLoad(i16Ty, maskPtr, fmt::format("{}.currentMask", llvmName));
        llvm::Value* isMaskZero = builder.CreateICmpEQ(currentMask, llvm::ConstantInt::get(i16Ty, 0), fmt::format("{}.isMaskZero", llvmName));

        llvm::BasicBlock* incBySIMDSizeBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.incBySIMDSize", llvmName), func);
        llvm::BasicBlock* checkPrevBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.checkPrev", llvmName), func);

        builder.CreateCondBr(isMaskZero, incBySIMDSizeBB, checkPrevBB);
        builder.SetInsertPoint(checkPrevBB);
        llvm::Function* countrZeroFn = llvm::Intrinsic::getDeclaration(
            module.get(),
            llvm::Intrinsic::cttz,
            {builder.getInt16Ty()}
        );
        llvm::Value* i16MatchIndex = builder.CreateCall(countrZeroFn, {currentMask, builder.getTrue()}, fmt::format("{}.i16matchIndex", llvmName));
        llvm::Value* i64MatchIndex = builder.CreateZExt(i16MatchIndex, builder.getInt64Ty(), fmt::format("{}.i64matchIndex", llvmName));
        strIdxVal = builder.CreateLoad(i64Ty, strIdxArg, fmt::format("{}.strIdxVal", llvmName));
        llvm::Value* currentIndex = builder.CreateAdd(strIdxVal, i64MatchIndex, fmt::format("{}.currentIndex", llvmName));
        llvm::BasicBlock* updateMaskBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.updateMask", llvmName), func);
        llvm::BasicBlock* simdSuccessBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.simdSuccess", llvmName), func);

        llvm::Value* isEscaped = builder.CreateCall(isEscapedLiteralFn, {dataArg, currentIndex}, fmt::format("{}.isEscaped", llvmName));
        builder.CreateCondBr(isEscaped, updateMaskBB, simdSuccessBB);

        builder.SetInsertPoint(updateMaskBB);
        llvm::Value* currentMaskValue = builder.CreateLoad(i16Ty, maskPtr, fmt::format("{}.currentMaskValue", llvmName));
        llvm::Value* subOne = builder.CreateSub(currentMaskValue, builder.getInt16(1), fmt::format("{}.decrementedMask", llvmName));
        llvm::Value* newMaskValue = builder.CreateAnd(currentMaskValue, subOne, fmt::format("{}.updatedMask", llvmName));
        builder.CreateStore(newMaskValue, maskPtr);
        builder.CreateBr(maskWhileCondBB);

        builder.SetInsertPoint(incBySIMDSizeBB);
        strIdxVal = builder.CreateLoad(i64Ty, strIdxArg, fmt::format("{}.strIdxVal", llvmName));
        llvm::Value* newStrIdxVal = builder.CreateAdd(strIdxVal, llvm::ConstantInt::get(i64Ty, 16), fmt::format("{}.strIdxValPlus16", llvmName));
        builder.CreateStore(newStrIdxVal, strIdxArg);
        builder.CreateBr(simdLoadCondBB);

        builder.SetInsertPoint(simdSuccessBB);
        builder.CreateStore(currentIndex, strIdxArg);
        builder.CreateBr(switchCasesBB);

        builder.SetInsertPoint(afterSimdCheckBB);
    }

    // The SIMD loop above advances 16 bytes at a time without tracking escape
    // pairs, so recover from the byte stream whether we stand on a literal;
    // `prevBytePtr` then carries that flag (0 or 1) through the scalar loop.
    llvm::BasicBlock* loopCond1BB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.loop.cond1", llvmName), func);
    llvm::BasicBlock* loopCond2BB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.loop.cond2", llvmName), func);
    llvm::BasicBlock* loopCond3BB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.loop.cond3", llvmName), func);
    llvm::BasicBlock* loopBodyBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.loop.body", llvmName), func);
    llvm::BasicBlock* loopEndBB  = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.loop.end", llvmName), func);

    strIdxVal = builder.CreateLoad(i64Ty, strIdxArg, fmt::format("{}.strIdxVal", llvmName));
    llvm::Value* escapedAtEntry = builder.CreateCall(isEscapedLiteralFn, {dataArg, strIdxVal}, fmt::format("{}.escapedAtEntry", llvmName));
    builder.CreateStore(builder.CreateZExt(escapedAtEntry, i8Ty), prevBytePtr);
    builder.CreateBr(loopCond1BB);

    builder.SetInsertPoint(loopCond1BB);
    strIdxVal = builder.CreateLoad(i64Ty, strIdxArg, fmt::format("{}.strIdxVal", llvmName));
    llvm::Value* condValue1 = builder.CreateICmpSLE(strIdxVal, maxIdxVal, fmt::format("{}.loop.condValue1", llvmName));
    llvm::Value *currentBytePtr = builder.CreateInBoundsGEP(builder.getInt8Ty(), dataArg, strIdxVal, fmt::format("{}.currentBytePtr", llvmName));
    llvm::Value *currentByteVal = builder.CreateLoad(builder.getInt8Ty(), currentBytePtr, fmt::format("{}.currentByteVal", llvmName));
    builder.CreateCondBr(condValue1, loopCond2BB, loopEndBB);

    // standing on an escaped literal: skip it whatever its value
    builder.SetInsertPoint(loopCond2BB);
    llvm::Value* escapedVal = builder.CreateLoad(builder.getInt8Ty(), prevBytePtr, fmt::format("{}.escapedVal", llvmName));
    llvm::Value* isEscaped = builder.CreateICmpNE(escapedVal, builder.getInt8(0), fmt::format("{}.isEscaped", llvmName));
    builder.CreateCondBr(isEscaped, loopBodyBB, loopCond3BB);

    // a code: stop on the first one with a transition
    builder.SetInsertPoint(loopCond3BB);
    llvm::GlobalVariable* transitionArray = module->getGlobalVariable(LLVMCompiler::getTransitionArrayName(state), true);
    llvm::Value *currentByteValUnsigned = builder.CreateZExt(currentByteVal, builder.getInt16Ty(), fmt::format("{}.currentByteValUnsigned", llvmName));
    std::vector<llvm::Value*> indices = { builder.getInt32(0), currentByteValUnsigned };
    llvm::ArrayType* arrayTy = llvm::ArrayType::get(builder.getInt8Ty(), 256);
    llvm::Value* ptrToBool = builder.CreateInBoundsGEP(arrayTy, transitionArray, indices, fmt::format("{}.condValue2Ptr", llvmName));
    llvm::Value* rawBoolVal = builder.CreateLoad(builder.getInt8Ty(), ptrToBool);
    llvm::Value* condValue2 = builder.CreateICmpNE(rawBoolVal, builder.getInt8(0), fmt::format("{}.condValue2", llvmName));
    builder.CreateCondBr(condValue2, loopEndBB, loopBodyBB);

    // escaped = !escaped && byte == 255
    builder.SetInsertPoint(loopBodyBB);
    llvm::Value* nextStrIdxVal = builder.CreateAdd(strIdxVal, builder.getInt64(1), fmt::format("{}.nextStrIdxVal", llvmName));
    builder.CreateStore(nextStrIdxVal, strIdxArg);
    llvm::Value* notEscaped = builder.CreateICmpEQ(escapedVal, builder.getInt8(0), fmt::format("{}.notEscaped", llvmName));
    llvm::Value* isEscapeByte = builder.CreateICmpEQ(currentByteVal, builder.getInt8(255), fmt::format("{}.isEscapeByte", llvmName));
    llvm::Value* nextEscaped = builder.CreateAnd(notEscaped, isEscapeByte, fmt::format("{}.nextEscaped", llvmName));
    builder.CreateStore(builder.CreateZExt(nextEscaped, i8Ty), prevBytePtr);
    builder.CreateBr(loopCond1BB);

    builder.SetInsertPoint(loopEndBB);

    llvm::BasicBlock* retFalseBB = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("{}.retFalse", llvmName), func);
    strIdxVal = builder.CreateLoad(i64Ty, strIdxArg, fmt::format("{}.currentStrIdxVal", llvmName));
    llvm::Value* cmpResult = builder.CreateICmpSGT(strIdxVal, maxIdxVal, fmt::format("{}.finalCmpResult", llvmName));
    builder.CreateCondBr(cmpResult, retFalseBB, switchCasesBB);

    builder.SetInsertPoint(retFalseBB);
    builder.CreateRet(builder.getFalse());

    builder.SetInsertPoint(switchCasesBB);
    strIdxVal = builder.CreateLoad(i64Ty, strIdxArg, fmt::format("{}.currentStrIdxVal", llvmName));
    llvm::Value *switchBytePtr = builder.CreateInBoundsGEP(builder.getInt8Ty(), dataArg, strIdxVal, fmt::format("{}.switchBytePtr", llvmName));
    llvm::Value *switchByteVal = builder.CreateLoad(builder.getInt8Ty(), switchBytePtr, fmt::format("{}.switchByteVal", llvmName));
    size_t numBlocks = getNumRealTransitions(state);
    llvm::SwitchInst* currentSwitch = builder.CreateSwitch(switchByteVal, trapBlock, numBlocks);

    for (const auto& [symbol, next]: state->transitions) {
        if (symbol != 255 || !next->transitions.empty()) {
            currentSwitch->addCase(builder.getInt8(symbol), transitionBlocks[next]);
        }
    }
}

void automata::codegen::llvmir::LLVMStateCodegen::generateError(const State *state, int direction) {
    builder.SetInsertPoint(stateToSwitchBlock[state]);
    builder.CreateRet(llvm::ConstantInt::getFalse(*threadSafeContext.getContext()));
}

void automata::codegen::llvmir::LLVMStateCodegen::generateOther(const State *state, const State *error, int direction) {
    builder.SetInsertPoint(stateToSwitchBlock[state]);
    for (const auto& [symbol, next]: state->transitions) {
        if (!transitionBlocks.contains(next)) {
            transitionBlocks[next] = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("transition.{}", LLVMCompiler::getEnumStateName(next, direction)), func);
        }
    }

    if (!transitionBlocks.contains(state->defaultTransition)) {
        transitionBlocks[state->defaultTransition] = llvm::BasicBlock::Create(*threadSafeContext.getContext(), fmt::format("transition.{}", LLVMCompiler::getEnumStateName(state->defaultTransition, direction)), func);
    }
    llvm::BasicBlock* defaultBlock = transitionBlocks[state->defaultTransition];
    llvm::SwitchInst* currentSwitch = builder.CreateSwitch(elemVal, defaultBlock, state->transitions.size());

    for (const auto& [symbol, next]: state->transitions) {
        currentSwitch->addCase(builder.getInt8(symbol), transitionBlocks[next]);
    }
}

void automata::codegen::llvmir::LLVMCompiler::optimizeModule() {
    llvm::legacy::FunctionPassManager fpm(module.get());
    llvm::legacy::PassManager mpm;

    std::string triple = llvm::sys::getDefaultTargetTriple();
    module->setTargetTriple(triple);
    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (!target) {
        throw std::runtime_error(fmt::format("Error finding target {}", error));
    }

    llvm::TargetOptions opt;
    std::string cpu = llvm::sys::getHostCPUName().str();
    std::string features = "+sse2,sse4.2";

    std::unique_ptr<llvm::TargetMachine> targetMachine(target->createTargetMachine(
        triple, cpu, features, opt, llvm::Reloc::PIC_, llvm::CodeModel::Small, llvm::CodeGenOpt::Aggressive
    ));
    module->setDataLayout(targetMachine->createDataLayout());


    llvm::PassManagerBuilder builder;
    builder.OptLevel = 3;        // O3 optimization level
    builder.SizeLevel = 0;       // prioritize speed, not size
    builder.Inliner = llvm::createFunctionInliningPass(275); // aggressive inlining

    builder.populateFunctionPassManager(fpm);
    builder.populateModulePassManager(mpm);

    fpm.doInitialization();
    for (llvm::Function &F: *module) {
            fpm.run(F);
    }
    fpm.doFinalization();

    mpm.run(*module);
}

automata::codegen::llvmir::LLVMCompiler::LLVMCompiler(bool enableSIMD, bool generateLLVM): threadSafeContext(std::make_unique<llvm::LLVMContext>()), builder(*threadSafeContext.getContext()), generateLLVM(generateLLVM), enableSIMD(enableSIMD) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    JIT = llvm::cantFail(llvm::orc::LLJITBuilder().create());
    module = std::make_unique<llvm::Module>("automatonModule", *threadSafeContext.getContext());

    llvm::Type* i1Ty  = llvm::Type::getInt1Ty(*threadSafeContext.getContext());
    llvm::Type* i8Ty  = llvm::Type::getInt8Ty(*threadSafeContext.getContext());
    llvm::Type* i32Ty = llvm::Type::getInt32Ty(*threadSafeContext.getContext());
    llvm::Type* i64Ty = llvm::Type::getInt64Ty(*threadSafeContext.getContext());

    llvm::PointerType* i8PtrTy  = llvm::PointerType::getUnqual(i8Ty);
    llvm::PointerType* i64PtrTy = llvm::PointerType::getUnqual(i64Ty);

    std::vector<llvm::Type*> paramTypes = {i8PtrTy, i64Ty, i64PtrTy, i8PtrTy};
    funcType = llvm::FunctionType::get(i1Ty, paramTypes, false);
}

std::unique_ptr<automata::codegen::Parser> automata::codegen::llvmir::LLVMCompiler::compile(const std::unique_ptr<parsing::LikePatternAutomaton> &automaton) {
    ParsingType type;
    generateTransitionArrays(automaton);
    std::optional<size_t> backwardsLevel;
    std::optional<size_t> forwardLevel;
    if (!automaton) {
        type = ParsingType::NO_DIRECTION;
    } else {
        auto backwardParams = automaton->gatherBackwardsParams();
        if (backwardParams.has_value()) {
            backwardsLevel = backwardParams->minLength;
            generateSuffixVariables(backwardParams);
            generateBackwards(backwardParams);
        }
        auto forwardParams = automaton->gatherForwardParams();
        if (forwardParams.has_value()) {
            forwardLevel = forwardParams->minLength;
            generatePrefixVariables(forwardParams);
            generateForwards(forwardParams);
        }
        if (forwardParams.has_value() && backwardParams.has_value()) {
            type = ParsingType::BOTH_DIRECTIONS;
        } else {
            if (forwardParams.has_value()) {
                type = ParsingType::ONLY_FORWARD;
            } else {
                type = ParsingType::ONLY_BACKWARDS;
            }
        }
    }
    generateFullParse(backwardsLevel, forwardLevel, type);
    optimizeModule();

    if (generateLLVM) {
        std::error_code EC;
        llvm::raw_fd_ostream outFile("optimized_llvm.ll", EC, llvm::sys::fs::OF_Text);
        if (EC) {
            throw std::runtime_error(fmt::format("Could not open file: {}\n", EC.message()));
        }
        module->print(outFile, nullptr);
    }
    llvm::cantFail(JIT->addIRModule(
            llvm::orc::ThreadSafeModule(std::move(module), std::move(threadSafeContext))
    ));
    return std::make_unique<LLVMParser>(JIT);
}

bool automata::codegen::llvmir::LLVMParser::parse(const uint8_t *pattern, size_t len) {
    return parseFunction(pattern, len);
}

automata::codegen::llvmir::LLVMParser::LLVMParser(std::unique_ptr<llvm::orc::LLJIT> &JIT): Parser(), JIT(std::move(JIT)) {
    parseFunction = cantFail(this->JIT->lookup(Compiler::getParseFunctionName())).toPtr<bool (*) (const uint8_t*, size_t)>();
}