//
// Created by pop on 4/27/26.
//

#ifndef FSST_UTILS_H
#define FSST_UTILS_H
#include <nlohmann/json.hpp>
#include <chrono>
#include <utility>
#include "utils.hpp"
#include "like_pattern_automaton.hpp"
#include "codegen/codegen.hpp"
#include "hybrid_string_search.hpp"
#include "vectorscan.hpp"
#include "codegen/cppcodegen.hpp"
#include "codegen/llvmcodegen.hpp"

template <typename F, typename... Args>
auto timeFunction(F&& func, Args&&... args) {
    using ReturnType = std::invoke_result_t<F, Args...>;
    auto start = std::chrono::steady_clock::now();
    ReturnType result = std::invoke(std::forward<F>(func), std::forward<Args>(args)...);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    return std::pair<ReturnType, double>(std::move(result), duration.count());
}

nlohmann::json readPatterns(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error(fmt::format("Error: Could not open file {}", filepath));
    }
    nlohmann::json obj;
    file >> obj;
    return obj;
}

struct BenchmarkResult {
public:
    double createTime;
    double compileTime;
    double matchTime;
    size_t count;

    BenchmarkResult() = default;
};

template <auto CreateFunc, auto CompileFunc, auto MatchFunc>
BenchmarkResult benchmarkAutomatonApproach(const std::unique_ptr<file::FileData> &fileData, const Encoder& encoder, const std::span<const uint8_t>& pattern) {
    BenchmarkResult returnResult{};
    auto [createResult, createTime] = timeFunction(CreateFunc, pattern, encoder);
    returnResult.createTime = createTime;

    auto [compileResult, compileTime] = timeFunction(CompileFunc, createResult);
    returnResult.compileTime = compileTime;

    auto [matchResult, matchTime] = timeFunction(MatchFunc, fileData, compileResult);
    returnResult.matchTime = matchTime;

    returnResult.count = matchResult;
    return returnResult;
}

template <auto CreateFunc, auto MatchFunc>
BenchmarkResult benchmarkDecodingApproach(const std::unique_ptr<file::FileData> &fileData, const Encoder& encoder, const std::span<const uint8_t>& pattern) {
    BenchmarkResult returnResult{};
    auto [createResult, createTime] = timeFunction(CreateFunc, pattern);
    returnResult.createTime = createTime;

    returnResult.compileTime = 0;

    auto [matchResult, matchTime] = timeFunction(MatchFunc, fileData, createResult, encoder);
    returnResult.matchTime = matchTime;

    returnResult.count = matchResult;
    return returnResult;
}

automata::parsing::LikePatternAutomatonParser createInterpreted(const std::span<const uint8_t>& pattern, const Encoder& encoder) {
    return automata::parsing::LikePatternAutomatonParser(pattern, encoder);
}

automata::parsing::LikePatternAutomatonParser compileInterpreted(automata::parsing::LikePatternAutomatonParser& parser) {
    return std::move(parser);
}

std::unique_ptr<automata::parsing::LikePatternAutomaton> createAutomaton(const std::span<const uint8_t>& pattern, const Encoder& encoder) {
    return automata::parsing::LikePatternAutomaton::build(std::span<const uint8_t>(pattern.data(), pattern.size()), encoder);
}

template <bool useSIMD>
std::unique_ptr<automata::codegen::Parser> compileCpp(const std::unique_ptr<automata::parsing::LikePatternAutomaton>& automaton) {
    std::string cppFile = "../automaton_bm.cpp";
    std::string destination = "libgenerated_bm.so";
    automata::codegen::cpp::CppCompiler compiler{cppFile, destination, useSIMD};
    return compiler.compile(automaton);
}

template <bool useSIMD>
std::unique_ptr<automata::codegen::Parser> compileLLVM(const std::unique_ptr<automata::parsing::LikePatternAutomaton>& automaton) {
    automata::codegen::llvmir::LLVMCompiler compiler{useSIMD};
    return compiler.compile(automaton);
}

template <bool useSIMD>
std::unique_ptr<HSSDecodedMatcher<useSIMD>> createHSS(const std::span<const uint8_t>& pattern) {
    return HSSDecodedMatcherFactory::buildMatcher<useSIMD>(pattern);
}

VectorScanMatcher createVS(const std::span<const uint8_t>& pattern) {
    return VectorScanMatcher{pattern};
}
#endif //FSST_UTILS_H
