//
// Created by pop on 7/23/25.
//

#include <sys/mman.h>
#include "benchmark_utils.hpp"
#include "codegen/cppcodegen.hpp"
#include "codegen/llvmcodegen.hpp"

std::string getHeader(const std::string& dataset) {
    static const std::unordered_map<std::string, std::string> headers = {
        {"TPCH", "runNumber,scaleFactor,numLines,numBytes,queryNum,table,column,pattern,algorithm,preprocessTime,compileTime,runTime,numMatches\n"},
        {"StackOverflow", "runNumber,scaleFactor,numLines,numBytes,table,pattern,algorithm,preprocessTime,compileTime,runTime,numMatches\n"},
        {"IMDB", "runNumber,scaleFactor,numLines,numBytes,table,pattern,algorithm,preprocessTime,compileTime,runTime,numMatches\n"},
        {"PublicBI", "runNumber,numLines,numBytes,schema,table,column,pattern,algorithm,preprocessTime,compileTime,runTime,numMatches\n"}
    };
    if (!headers.contains(dataset)) {
        throw std::runtime_error(fmt::format("CSV Header Error: No header definition found for dataset {}. Please update getHeader() in your benchmark source.", dataset));
    }
    return headers.at(dataset);
}

std::string getBenchmarkPath(const std::string& csvFolder, const std::string& dataset) {
    static const std::unordered_map<std::string, std::string> paths = {
        {"TPCH", fmt::format("{}/tpch.csv", csvFolder)},
        {"StackOverflow", fmt::format("{}/stackoverflow.csv", csvFolder)},
        {"IMDB", fmt::format("{}/imdb.csv", csvFolder)},
        {"PublicBI", fmt::format("{}/publicbi.csv", csvFolder)}
    };
    if (!paths.contains(dataset)) {
        throw std::runtime_error(fmt::format("CSV Header Error: No header definition found for dataset {}. Please update getHeader() in your benchmark source.", dataset));
    }
    return paths.at(dataset);
}

BenchmarkResult benchmarkInterpreted(const std::unique_ptr<file::FileData> &fileData, const Encoder& encoder, const std::span<const uint8_t>& pattern) {
    constexpr auto Run = [](const std::unique_ptr<file::FileData>& data, const automata::parsing::LikePatternAutomatonParser& parser) {
        size_t count = 0;
        for (size_t i = 0; i < data->getNumElements(); ++i) {
            if (parser.parse(data->get(i)))
                ++count;
        }
        return count;
    };

    BenchmarkResult result = benchmarkAutomatonApproach<createInterpreted, compileInterpreted, Run>(fileData, encoder, pattern);
    return result;
}

template <bool useSIMD>
BenchmarkResult benchmarkCppCompiled(const std::unique_ptr<file::FileData> &fileData, const Encoder& encoder, const std::span<const uint8_t>& pattern) {
    constexpr auto Run = [](const std::unique_ptr<file::FileData>& data, const std::unique_ptr<automata::codegen::Parser>& parser) {
        automata::codegen::cpp::CppParser* cppParser = dynamic_cast<automata::codegen::cpp::CppParser*>(parser.get());

        size_t count = 0;
        for (size_t i = 0; i < data->getNumElements(); ++i) {
            std::span<const uint8_t> element = data->get(i);
            if (cppParser->parse(element.data(), element.size()))
                ++count;
        }
        return count;
    };

    BenchmarkResult result = benchmarkAutomatonApproach<createAutomaton, compileCpp<useSIMD>, Run>(fileData, encoder, pattern);
    return result;
}

template <bool useSIMD>
BenchmarkResult benchmarkLLVMCompiled(const std::unique_ptr<file::FileData> &fileData, const Encoder& encoder, const std::span<const uint8_t>& pattern) {
    constexpr auto Run = [](const std::unique_ptr<file::FileData>& data, const std::unique_ptr<automata::codegen::Parser>& parser) {
        automata::codegen::llvmir::LLVMParser* llvmParser = dynamic_cast<automata::codegen::llvmir::LLVMParser*>(parser.get());

        size_t count = 0;
        for (size_t i = 0; i < data->getNumElements(); ++i) {
            std::span<const uint8_t> element = data->get(i);
            if (llvmParser->parse(element.data(), element.size()))
                ++count;
        }
        return count;
    };

    BenchmarkResult result = benchmarkAutomatonApproach<createAutomaton, compileLLVM<useSIMD>, Run>(fileData, encoder, pattern);
    return result;
}

template <bool useSIMD>
BenchmarkResult benchmarkDecodingHybridStringSearch(const std::unique_ptr<file::FileData> &fileData, const Encoder& encoder, const std::span<const uint8_t>& pattern) {
    constexpr auto Run = [](const std::unique_ptr<file::FileData>& data, const std::unique_ptr<HSSDecodedMatcher<useSIMD>>& matcher, const Encoder& encoder) {
        Decoder decoder{encoder};
        std::basic_string<uint8_t> decodedString;
        HSSDecodedMatcher<useSIMD>* simpleMatcher = dynamic_cast<HSSDecodedMatcher<useSIMD>*>(matcher.get());
        size_t count = 0;
        for (size_t i = 0; i < data->getNumElements(); ++i) {
            std::span<const uint8_t> element = data->get(i);
            decodedString = std::move(decoder.decode(element.size(), element.data()));
            if (simpleMatcher->matchDecoded(decodedString.data(), decodedString.size()))
                ++count;
        }
        return count;
    };

    BenchmarkResult result = benchmarkDecodingApproach<createHSS<useSIMD>, Run>(fileData, encoder, pattern);
    return result;
}

BenchmarkResult benchmarkVectorScan(const std::unique_ptr<file::FileData> &fileData, const Encoder& encoder, const std::span<const uint8_t>& pattern) {
    constexpr auto Run = [](const std::unique_ptr<file::FileData>& data, const VectorScanMatcher& matcher, const Encoder& encoder) {
        Decoder decoder{encoder};
        size_t count = 0;
        std::basic_string<uint8_t> decodedString;
        for (size_t i = 0; i < data->getNumElements(); ++i) {
            std::span<const uint8_t> element = data->get(i);
            decodedString = std::move(decoder.decode(element.size(), element.data()));
            if (matcher.matches(std::span<const uint8_t>(decodedString.data(), decodedString.size())))
                ++count;
        }
        return count;
    };
    BenchmarkResult result = benchmarkDecodingApproach<createVS, Run>(fileData, encoder, pattern);
    return result;
}

using BenchmarkFuncType = std::function<BenchmarkResult(const std::unique_ptr<file::FileData>&, const Encoder&, const std::span<const uint8_t>&)>;

BenchmarkFuncType getBenchmarkFunction(const std::string& name) {
    static const std::unordered_map<std::string, BenchmarkFuncType> benchmarkFunctions = {
        {"InMemory", benchmarkInterpreted},
        {"SSECppCompiled", benchmarkCppCompiled<true>},
        {"NoSSECppCompiled", benchmarkCppCompiled<false>},
        {"SSELLVMCompiled", benchmarkLLVMCompiled<true>},
        {"NoSSELLVMCompiled", benchmarkLLVMCompiled<false>},
        {"HSSDecoded", benchmarkDecodingHybridStringSearch<false>},
        {"SSEDecoded", benchmarkDecodingHybridStringSearch<true>},
        {"VectorScan", benchmarkVectorScan}
    };

    if (!benchmarkFunctions.contains(name)) {
        throw std::invalid_argument(fmt::format("Unknown benchmark type: {}", name));
    }
    return benchmarkFunctions.at(name);
}

void benchmarkTPCH(const std::string& algorithm, const std::string& numUnderscores, const std::string& jsonPath, const std::string& csvFolder, int runNumber) {
    BenchmarkFuncType bmFunction = getBenchmarkFunction(algorithm);

    std::vector<uint8_t> scaleFactors = {1, 10, 100};
    nlohmann::json patterns = readPatterns(jsonPath)["TPCH"][numUnderscores];

    std::string benchmarkPath = getBenchmarkPath(csvFolder, "TPCH");
    bool writeHeader = !std::filesystem::exists(benchmarkPath);
    std::ofstream bmFile(benchmarkPath, std::ios::app);
    if (!bmFile.is_open()) {
        throw std::runtime_error(fmt::format("Could not open `{}`", benchmarkPath));
    }
    if (writeHeader) {
        bmFile << getHeader("TPCH");
    }

    for (uint8_t scaleFactor: scaleFactors) {
        for (const auto& [tableName, tableQueries]: patterns.items()) {
            for (const auto& [columnName, columnQueries]: tableQueries.items()) {
                std::string filepath = fmt::format("../../tpch/sf{}/{}_{}", scaleFactor, tableName, columnName);
                auto fileData = file::readBinaryFileData(filepath.data(), MAP_POPULATE);
                size_t numLines = fileData->getNumElements();
                size_t numBytes = fileData->getTotalLength();

                std::string symTablePath = fmt::format("{}_symTable", filepath);
                Encoder encoder(SymbolTable::readFromFile(symTablePath.c_str()));

                for (const auto& patternObject: columnQueries) {
                    std::string pattern = patternObject["pattern"].get<std::string>();
                    std::string query;
                    if (patternObject["query"].is_null()) {
                        query = "";
                    } else {
                        query = fmt::format("Q{}", patternObject["query"].get<int>());
                    }

                    BenchmarkResult bmResult = bmFunction(fileData, encoder, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pattern.data()), pattern.size()));
                    bmFile << runNumber << "," << (int) scaleFactor << "," << numLines << "," << numBytes << ","
                            << query << "," << tableName << "," << columnName << "," << fmt::format("\"{}\"", pattern)
                            << "," << algorithm << "," << bmResult.createTime << "," << bmResult.compileTime << "," << bmResult.matchTime  << "," << bmResult.count << "\n";
                }
            }
        }
    }

    bmFile.close();
}

void benchmarkStackOverflow(const std::string& algorithm, const std::string& numUnderscores, const std::string& jsonPath, const std::string& csvFolder, int runNumber) {
    BenchmarkFuncType bmFunction = getBenchmarkFunction(algorithm);

    std::vector<double> scaleFactors = {0.01, 0.05, 0.1, 0.5, 1, 5, 10, 50};
    nlohmann::json patterns = readPatterns(jsonPath)["StackOverflow"][numUnderscores];

    std::string benchmarkPath = getBenchmarkPath(csvFolder, "StackOverflow");
    bool writeHeader = !std::filesystem::exists(benchmarkPath);
    std::ofstream bmFile(benchmarkPath, std::ios::app);
    if (!bmFile.is_open()) {
        throw std::runtime_error(fmt::format("Could not open `{}`", benchmarkPath));
    }

    if (writeHeader) {
        bmFile << getHeader("StackOverflow");
    }

    for (double scaleFactor: scaleFactors) {
        for (const auto& [tableName, tableQueries]: patterns.items()) {
            // Comments table does not have scale factor 50
            if (scaleFactor == 50 && tableName == "Comments") {
                continue;
            }

            std::string filepath = fmt::format("../../stackoverflow/sf{}/{}", scaleFactor, tableName);
            auto fileData = file::readBinaryFileData(filepath.data(), MAP_POPULATE);
            size_t numLines = fileData->getNumElements();
            size_t numBytes = fileData->getTotalLength();

            std::string symTablePath = fmt::format("{}_symTable", filepath);
            Encoder encoder(SymbolTable::readFromFile(symTablePath.c_str()));

            for (const auto& patternObject: tableQueries) {
                std::string pattern = patternObject.get<std::string>();
                BenchmarkResult bmResult = bmFunction(fileData, encoder, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pattern.data()), pattern.size()));
                bmFile << runNumber << "," << scaleFactor << "," << numLines << "," << numBytes << ","
                       << tableName << "," << fmt::format("\"{}\"", pattern)
                       << "," << algorithm << "," << bmResult.createTime << "," << bmResult.compileTime << "," << bmResult.matchTime << "," << bmResult.count << "\n";
            }
        }
    }
    bmFile.close();
}

void benchmarkIMDB(const std::string& algorithm, const std::string& numUnderscores, const std::string& jsonPath, const std::string& csvFolder, int runNumber) {
    BenchmarkFuncType bmFunction = getBenchmarkFunction(algorithm);

    std::vector<double> scaleFactors = {0.001, 0.005, 0.01, 0.05, 0.1, 0.2};
    nlohmann::json patterns = readPatterns(jsonPath)["IMDB"][numUnderscores];

    std::string benchmarkPath = getBenchmarkPath(csvFolder, "IMDB");
    bool writeHeader = !std::filesystem::exists(benchmarkPath);
    std::ofstream bmFile(benchmarkPath, std::ios::app);
    if (!bmFile.is_open()) {
        throw std::runtime_error(fmt::format("Could not open `{}`", benchmarkPath));
    }

    if (writeHeader) {
        bmFile << getHeader("IMDB");
    }

    for (double scaleFactor: scaleFactors) {
        for (const auto& [tableName, tableQueries]: patterns.items()) {
            std::string filepath = fmt::format("../../imdb/sf{}/{}", scaleFactor, tableName);
            auto fileData = file::readBinaryFileData(filepath.data(), MAP_POPULATE);
            size_t numLines = fileData->getNumElements();
            size_t numBytes = fileData->getTotalLength();

            std::string symTablePath = fmt::format("{}_symTable", filepath);
            Encoder encoder(SymbolTable::readFromFile(symTablePath.c_str()));

            for (const auto& patternObject: tableQueries) {
                std::string pattern = patternObject.get<std::string>();
                BenchmarkResult bmResult = bmFunction(fileData, encoder, std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(pattern.data()), pattern.size()));
                bmFile << runNumber << "," << scaleFactor << "," << numLines << "," << numBytes << ","
                       << tableName << "," << fmt::format("\"{}\"", pattern)
                       << "," << algorithm << "," << bmResult.createTime << "," << bmResult.compileTime << "," << bmResult.matchTime << "," << bmResult.count << "\n";
            }
        }
    }
    bmFile.close();
}

void benchmarkPublicBI(const std::string& algorithm, const std::string& numUnderscores, const std::string& jsonPath, const std::string& csvFolder, int runNumber) {
    BenchmarkFuncType bmFunction = getBenchmarkFunction(algorithm);

    nlohmann::json patterns = readPatterns(jsonPath)["PublicBI"][numUnderscores];

    std::string benchmarkPath = getBenchmarkPath(csvFolder, "PublicBI");
    bool writeHeader = !std::filesystem::exists(benchmarkPath);
    std::ofstream bmFile(benchmarkPath, std::ios::app);
    if (!bmFile.is_open()) {
        throw std::runtime_error(fmt::format("Could not open `{}`", benchmarkPath));
    }

    if (writeHeader) {
        bmFile << getHeader("PublicBI");
    }

    for (const auto& [schemaName, schemaQueries]: patterns.items()) {
        for (const auto& [tableName, tableQueries]: schemaQueries.items()) {
            for (const auto& [columnName, columnQueries]: tableQueries.items()) {
                std::string filepath = fmt::format("../../publicbi/{}_{}_{}", schemaName, tableName, columnName);
                auto fileData = file::readBinaryFileData(filepath.data(), MAP_POPULATE);
                size_t numLines = fileData->getNumElements();
                size_t numBytes = fileData->getTotalLength();

                std::string symTablePath = fmt::format("{}_symTable", filepath);
                Encoder encoder(SymbolTable::readFromFile(symTablePath.c_str()));

                for (const auto& patternObject: columnQueries) {
                    std::string pattern = patternObject.get<std::string>();
                    BenchmarkResult bmResult = bmFunction(fileData, encoder, std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(pattern.data()), pattern.size()));
                    bmFile << runNumber << "," << numLines << "," << numBytes << "," << schemaName << ","
                           << tableName << "," << columnName << "," << fmt::format("\"{}\"", pattern)
                           << "," << algorithm << "," << bmResult.createTime << "," << bmResult.compileTime << "," << bmResult.matchTime << "," << bmResult.count << "\n";
                }
            }
        }
    }
    bmFile.close();
}

int main(int argc, char** argv) {
    if (argc < 5) {
        throw std::runtime_error("Usage: ./measure_singlethreaded <dataset> (options: TPCH, StackOverflow, IMDB, PublicBI) <algorithm> (options: InMemory, SSECppCompiled, NoSSECppCompiled, SSELLVMCompiled, NoSSELLVMCompiled, HSSDecoded, SSEDecoded, VectorScan) <numUnderscores> (0, 1, >=2) <runNumber>");
    }
    const std::string jsonPath = "../benchmark/patterns.json";
    const std::string csvPath = "../benchmark/data";
    std::string dataset(argv[1]);
    std::string algorithm(argv[2]);
    std::string numUnderscores(argv[3]);
    int runNumber = std::stoi(argv[4]);
    if (dataset == "TPCH") {
        benchmarkTPCH(algorithm, numUnderscores, jsonPath, csvPath, runNumber);
    } else if (dataset == "StackOverflow") {
        benchmarkStackOverflow(algorithm, numUnderscores, jsonPath, csvPath, runNumber);
    } else if (dataset == "IMDB") {
        benchmarkIMDB(algorithm, numUnderscores, jsonPath, csvPath, runNumber);
    } else if (dataset == "PublicBI") {
        benchmarkPublicBI(algorithm, numUnderscores, jsonPath, csvPath, runNumber);
    } else {
        throw std::runtime_error(fmt::format("Unknown dataset {}", dataset));
    }
    return 0;
}
