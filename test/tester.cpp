//
// Created by popca on 15-May-25.
//
#include <gtest/gtest.h>
#include "env.hpp"
//---------------------------------------------------------------------------
#define FSST_MEMBUF (1ULL<<22)

class GlobalEnvironment : public ::testing::Environment {
private:
    static void setUpEncoder() {
        std::unordered_set<uint8_t> used_characters{};
        for (uint16_t sym_idx = 0; sym_idx < encoder.nSymbols(); ++sym_idx) {
            const libfsst::Symbol& symbol = encoder.symbols()[sym_idx];
            const uint8_t* symBytes = reinterpret_cast<const uint8_t*>(symbol.val.str);
            for (uint32_t idx = 0; idx < symbol.length(); ++idx) {
                used_characters.insert(symBytes[idx]);
            }
        }

        for (uint8_t c: used_characters) {
            chars.push_back(c);
        }
        for (uint8_t idx = 0; idx < 9; ++idx) {
            precomputedEnds[idx] = &endStates[idx];
            // why do we keep the default transitions?
            // so that we can actually *TEST* the symbols where we end
            endStates[idx].defaultTransition = &endStates[idx];
            endStates[idx].level = 0;
            endsSet.insert(&endStates[idx]);
        }
    }

    static void setUpUncompressedData() {
        const char uncompressed_path[] = "../../data/firstname/uncompressed";
        in = file::readBinaryFileData(uncompressed_path);
    }

    static void setUpCompressedData() {
        const char compressed_path[] = "../../data/firstname/compressed";
        out = file::readBinaryFileData(compressed_path);
    }

public:
    void SetUp() override {
        for (size_t idx = 0; idx < asciiPatterns.size(); ++idx) {
            patterns.emplace_back(reinterpret_cast<const uint8_t*>(asciiPatterns[idx].data()), asciiPatterns[idx].size());
        }

        GlobalEnvironment::setUpEncoder();
        GlobalEnvironment::setUpUncompressedData();
        GlobalEnvironment::setUpCompressedData();
    }

    void TearDown() override {}
};

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new GlobalEnvironment);
    return RUN_ALL_TESTS();
}