//
// Created by pop on 4/27/26.
//
#include "benchmark_utils.hpp"
#include "sys/mman.h"
std::string getHeader() {
    return "runNumber,dataset,scaleFactor,schema,table,column,decodeTime,count\n";
}

std::string getBenchmarkPath(const std::string& csvFolder) {
    return fmt::format("{}/decoding_overhead.csv", csvFolder);
}

std::pair<double, size_t> measureDecodingTimes(const std::string& filepath) {
    auto fileData = file::readBinaryFileData(filepath.data(), MAP_POPULATE);
    std::string symTablePath = fmt::format("{}_symTable", filepath);
    Encoder encoder(SymbolTable::readFromFile(symTablePath.c_str()));
    Decoder decoder{encoder};

    size_t count = 0;
    auto start = std::chrono::steady_clock::now();
    size_t totalElements = fileData->getNumElements();
    for (size_t i = 0; i < totalElements; ++i) {
        std::span<const uint8_t> element = fileData->get(i);
        auto decoded = decoder.decode(element.size(), element.data());
        count += decoded.size();
    }
    auto end = std::chrono::steady_clock::now();

    std::chrono::duration<double, std::milli> duration = end - start;
    return {duration.count(), count};
}

void benchmarkTPCH(const std::string& jsonPath, const std::string& csvFolder, int runNumber) {
    std::vector<uint8_t> scaleFactors = {1, 10, 100};
    nlohmann::json patterns = readPatterns(jsonPath)["TPCH"]["0"];

    std::string benchmarkPath = getBenchmarkPath(csvFolder);
    bool writeHeader = !std::filesystem::exists(benchmarkPath);
    std::ofstream bmFile(benchmarkPath, std::ios::app);
    if (!bmFile.is_open()) {
        throw std::runtime_error(fmt::format("Could not open `{}`", benchmarkPath));
    }
    if (writeHeader) {
        bmFile << getHeader();
    }

    std::string dataset = "TPCH";
    std::string schemaName;
    for (uint8_t scaleFactor: scaleFactors) {
        std::string scale = fmt::format("{}", scaleFactor);
        for (const auto& [tableName, tableQueries]: patterns.items()) {
            for (const auto& [columnName, columnQueries]: tableQueries.items()) {
                std::string filepath = fmt::format("../../tpch/sf{}/{}_{}", scaleFactor, tableName, columnName);
                auto [duration, count] = measureDecodingTimes(filepath);
                bmFile << runNumber << "," << dataset << "," << scale << "," << schemaName << "," << tableName << ","
                       << columnName << "," << duration << "," << count << "\n";
            }
        }
    }

    bmFile.close();
}

void benchmarkStackOverflow(const std::string& jsonPath, const std::string& csvFolder, int runNumber) {
    std::vector<double> scaleFactors = {0.01, 0.05, 0.1, 0.5, 1, 5, 10, 50};
    nlohmann::json patterns = readPatterns(jsonPath)["StackOverflow"]["0"];

    std::string benchmarkPath = getBenchmarkPath(csvFolder);
    bool writeHeader = !std::filesystem::exists(benchmarkPath);
    std::ofstream bmFile(benchmarkPath, std::ios::app);
    if (!bmFile.is_open()) {
        throw std::runtime_error(fmt::format("Could not open `{}`", benchmarkPath));
    }

    if (writeHeader) {
        bmFile << getHeader();
    }

    std::string dataset = "StackOverflow";
    for (double scaleFactor: scaleFactors) {
        std::string scale = fmt::format("{}", scaleFactor);
        for (const auto& [tableName, tableQueries]: patterns.items()) {
            // Comments table does not have scale factor 50
            if (scaleFactor == 50 && tableName == "Comments") {
                continue;
            }
            std::string schemaName;
            std::string columnName;

            std::string filepath = fmt::format("../../stackoverflow/sf{}/{}", scaleFactor, tableName);
            auto [duration, count] = measureDecodingTimes(filepath);
            bmFile << runNumber << "," << dataset << "," << scale << "," << schemaName << "," << tableName << ","
                   << columnName << "," << duration << "," << count << "\n";
        }
    }
    bmFile.close();
}

void benchmarkIMDB(const std::string& jsonPath, const std::string& csvFolder, int runNumber) {
    std::vector<double> scaleFactors = {0.001, 0.005, 0.01, 0.05, 0.1, 0.2};
    nlohmann::json patterns = readPatterns(jsonPath)["IMDB"]["0"];

    std::string benchmarkPath = getBenchmarkPath(csvFolder);
    bool writeHeader = !std::filesystem::exists(benchmarkPath);
    std::ofstream bmFile(benchmarkPath, std::ios::app);
    if (!bmFile.is_open()) {
        throw std::runtime_error(fmt::format("Could not open `{}`", benchmarkPath));
    }

    if (writeHeader) {
        bmFile << getHeader();
    }

    std::string dataset = "IMDB";
    for (double scaleFactor: scaleFactors) {
        std::string scale = fmt::format("{}", scaleFactor);
        for (const auto& [tableName, tableQueries]: patterns.items()) {
            std::string filepath = fmt::format("../../imdb/sf{}/{}", scaleFactor, tableName);
            std::string schemaName;
            std::string columnName;

            auto [duration, count] = measureDecodingTimes(filepath);
            bmFile << runNumber << "," << dataset << "," << scale << "," << schemaName << "," << tableName << ","
                   << columnName << "," << duration << "," << count << "\n";
        }
    }
    bmFile.close();
}

void benchmarkPublicBI(const std::string& jsonPath, const std::string& csvFolder, int runNumber) {
    nlohmann::json patterns = readPatterns(jsonPath)["PublicBI"]["0"];

    std::string benchmarkPath = getBenchmarkPath(csvFolder);
    bool writeHeader = !std::filesystem::exists(benchmarkPath);
    std::ofstream bmFile(benchmarkPath, std::ios::app);
    if (!bmFile.is_open()) {
        throw std::runtime_error(fmt::format("Could not open `{}`", benchmarkPath));
    }

    if (writeHeader) {
        bmFile << getHeader();
    }

    std::string dataset = "PublicBI";
    std::string scale;
    for (const auto& [schemaName, schemaQueries]: patterns.items()) {
        for (const auto& [tableName, tableQueries]: schemaQueries.items()) {
            for (const auto& [columnName, columnQueries]: tableQueries.items()) {
                std::string filepath = fmt::format("../../publicbi/{}_{}_{}", schemaName, tableName, columnName);
                auto [duration, count] = measureDecodingTimes(filepath);
                bmFile << runNumber << "," << dataset << "," << scale << "," << schemaName << "," << tableName << ","
                       << columnName << "," << duration << "," << count << "\n";
            }
        }
    }
    bmFile.close();
}

int main(int argc, char** argv) {
    if (argc < 3) {
        throw std::runtime_error("Usage: ./measure_decoding <dataset> (options: TPCH, StackOverflow, IMDB, PublicBI) <runNumber>");
    }
    std::string dataset(argv[1]);
    int runNumber = std::stoi(argv[2]);
    const std::string jsonPath = "../benchmark/patterns.json";
    const std::string csvPath = "../benchmark/data";
    if (dataset == "TPCH") {
        benchmarkTPCH(jsonPath, csvPath, runNumber);
    } else if (dataset == "StackOverflow") {
        benchmarkStackOverflow(jsonPath, csvPath, runNumber);
    } else if (dataset == "IMDB") {
        benchmarkIMDB(jsonPath, csvPath, runNumber);
    } else if (dataset == "PublicBI") {
        benchmarkPublicBI(jsonPath, csvPath, runNumber);
    } else {
        throw std::runtime_error(fmt::format("Unknown dataset {}", dataset));
    }
}