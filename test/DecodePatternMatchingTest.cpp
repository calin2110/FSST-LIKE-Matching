//
// Created by pop on 7/25/25.
//
#include "shared.hpp"
#include "automata.hpp"
#include "env.hpp"

#include <gtest/gtest.h>

#include "hybrid_string_search.hpp"
#include "vectorscan.hpp"

namespace test {
    template <bool enableSSE>
    std::unique_ptr<HSSDecodedMatcher<enableSSE>> constructHybridStringSearchMatcher(const std::span<const uint8_t>& bytes) {
        return HSSDecodedMatcherFactory::buildMatcher<enableSSE>(bytes);
    }

    VectorScanMatcher constructVectorScanMatcher(const std::span<const uint8_t>& bytes) {
        return VectorScanMatcher(bytes);
    }

    template <bool enableSSE>
    bool matchHybridStringSearch(const std::unique_ptr<HSSDecodedMatcher<enableSSE>>& matcher, const std::basic_string<uint8_t>& decodedString) {
        return matcher->matchDecoded(decodedString.data(), decodedString.size());
    }

    bool matchVectorScan(const VectorScanMatcher& matcher, const std::basic_string<uint8_t>& decodedString) {
        return matcher.matches(std::span<const uint8_t>(decodedString.data(), decodedString.size()));
    }


    template <auto ConstructMatcher, auto Match>
    void testTemplate(const char* testData, const char* uncompressedDataset, const char* compressedDataset, const char* symbolTable) {
        int fd = open(testData, O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error(fmt::format("Could not open {}", testData));
        }
        auto currentUncompressed = file::readBinaryFileData(uncompressedDataset);
        auto currentCompressed = file::readBinaryFileData(compressedDataset);
        Decoder decoder{Encoder(SymbolTable::readFromFile(symbolTable))};
        size_t n = currentCompressed->getNumElements();

        size_t patternLen;
        while (read(fd, &patternLen, sizeof(patternLen))) {
            std::vector<uint8_t> pattern(patternLen + 1, 0);
            read(fd, pattern.data(), patternLen);
            std::span<const uint8_t> bytes(pattern.data(), patternLen);
            fmt::println("Checking pattern \"{}\"", byteSpanToString(bytes));

            uint8_t uncompressedMatch[n];

            read(fd, uncompressedMatch, n * sizeof(uint8_t));
            auto matcher = ConstructMatcher(bytes);

            for (size_t idx = 0; idx < n; ++idx) {
                std::span<const uint8_t> currentEntry = currentCompressed->get(idx);
                std::basic_string<uint8_t> decodedString = decoder.decode( currentEntry.size(), currentEntry.data());
                uint8_t matches = Match(matcher, decodedString);
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }


    const char* firstnameTestData = "../../data/test_data/FullPatternMatchTestTestFirstnamePatterns";
    const char* firstnameUncompressedDataset = "../../data/firstname/uncompressed";
    const char* firstnameCompressedDataset = "../../data/firstname/compressed";
    const char* firstnameSymTable = "../../data/firstname/symbolsBinary";

    TEST(DecodePatternMatchTest, TestFirstnamePatternsHybridStringSearch) {
        testTemplate<constructHybridStringSearchMatcher<false>, matchHybridStringSearch<false>>(
            firstnameTestData, firstnameUncompressedDataset, firstnameCompressedDataset, firstnameSymTable
        );
    }

    TEST(DecodePatternMatchTest, TestFirstnamePatternsSIMDStringSearch) {
        testTemplate<constructHybridStringSearchMatcher<true>, matchHybridStringSearch<true>>(
            firstnameTestData, firstnameUncompressedDataset, firstnameCompressedDataset, firstnameSymTable
        );
    }

    TEST(DecodePatternMatchTest, TestFirstnamePatternsVectorScanSearch) {
        testTemplate<constructVectorScanMatcher, matchVectorScan>(
            firstnameTestData, firstnameUncompressedDataset, firstnameCompressedDataset, firstnameSymTable
        );
    }

    const char* hamletTestData = "../../data/test_data/FullPatternMatchTestTestHamletPatterns";
    const char* hamletUncompressedDataset = "../../data/hamlet/uncompressed";
    const char* hamletCompressedDataset = "../../data/hamlet/compressed";
    const char* hamletSymTable = "../../data/hamlet/symbolsBinary";

    TEST(DecodePatternMatchTest, TestHamletPatternsHybridStringSearch) {
        testTemplate<constructHybridStringSearchMatcher<false>, matchHybridStringSearch<false>>(
            hamletTestData, hamletUncompressedDataset, hamletCompressedDataset, hamletSymTable
        );
    }

    TEST(DecodePatternMatchTest, TestHamletPatternsSIMDStringSearch) {
        testTemplate<constructHybridStringSearchMatcher<true>, matchHybridStringSearch<true>>(
            hamletTestData, hamletUncompressedDataset, hamletCompressedDataset, hamletSymTable
        );
    }

    TEST(DecodePatternMatchTest, TestHamletPatternsVectorScanSearch) {
        testTemplate<constructVectorScanMatcher, matchVectorScan>(
            hamletTestData, hamletUncompressedDataset, hamletCompressedDataset, hamletSymTable
        );
    }

    const char* l_commentTestData = "../../data/test_data/FullPatternMatchTestTestL_commentPatterns";
    const char* l_commentUncompressedDataset = "../../data/l_comment/uncompressed";
    const char* l_commentCompressedDataset = "../../data/l_comment/compressed";
    const char* l_commentSymTable = "../../data/l_comment/symbolsBinary";

    TEST(DecodePatternMatchTest, TestL_commentPatternsHybridStringSearch) {
        testTemplate<constructHybridStringSearchMatcher<false>, matchHybridStringSearch<false>>(
            l_commentTestData, l_commentUncompressedDataset, l_commentCompressedDataset, l_commentSymTable
        );
    }

    TEST(DecodePatternMatchTest, TestL_commentPatternsSIMDStringSearch) {
        testTemplate<constructHybridStringSearchMatcher<true>, matchHybridStringSearch<true>>(
            l_commentTestData, l_commentUncompressedDataset, l_commentCompressedDataset, l_commentSymTable
        );
    }

    TEST(DecodePatternMatchTest, TestL_commentPatternsVectorScanSearch) {
        testTemplate<constructVectorScanMatcher, matchVectorScan>(
            l_commentTestData, l_commentUncompressedDataset, l_commentCompressedDataset, l_commentSymTable
        );
    }

    const char* ps_commentTestData = "../../data/test_data/FullPatternMatchTestTestPs_commentPatterns";
    const char* ps_commentUncompressedDataset = "../../data/ps_comment/uncompressed";
    const char* ps_commentCompressedDataset = "../../data/ps_comment/compressed";
    const char* ps_commentSymTable = "../../data/ps_comment/symbolsBinary";

    TEST(DecodePatternMatchTest, TestPs_commentPatternsHybridStringSearch) {
        testTemplate<constructHybridStringSearchMatcher<false>, matchHybridStringSearch<false>>(
            ps_commentTestData, ps_commentUncompressedDataset, ps_commentCompressedDataset, ps_commentSymTable
        );
    }

    TEST(DecodePatternMatchTest, TestPs_commentPatternsSIMDStringSearch) {
        testTemplate<constructHybridStringSearchMatcher<true>, matchHybridStringSearch<true>>(
            ps_commentTestData, ps_commentUncompressedDataset, ps_commentCompressedDataset, ps_commentSymTable
        );
    }

    TEST(DecodePatternMatchTest, TestPs_commentPatternsVectorScanSearch) {
        testTemplate<constructVectorScanMatcher, matchVectorScan>(
            ps_commentTestData, ps_commentUncompressedDataset, ps_commentCompressedDataset, ps_commentSymTable
        );
    }

    const char* chineseTestData = "../../data/test_data/FullPatternMatchTestTestChinesePatterns";
    const char* chineseUncompressedDataset = "../../data/chinese/uncompressed";
    const char* chineseCompressedDataset = "../../data/chinese/compressed";
    const char* chineseSymTable = "../../data/chinese/symbolsBinary";

    TEST(DecodePatternMatchTest, TestChinesePatternsHybridStringSearch) {
        testTemplate<constructHybridStringSearchMatcher<false>, matchHybridStringSearch<false>>(
            chineseTestData, chineseUncompressedDataset, chineseCompressedDataset, chineseSymTable
        );
    }

    TEST(DecodePatternMatchTest, TestChinesePatternsSIMDStringSearch) {
        testTemplate<constructHybridStringSearchMatcher<true>, matchHybridStringSearch<true>>(
            chineseTestData, chineseUncompressedDataset, chineseCompressedDataset, chineseSymTable
        );
    }

    TEST(DecodePatternMatchTest, TestChinesePatternsVectorScanSearch) {
        testTemplate<constructVectorScanMatcher, matchVectorScan>(
            chineseTestData, chineseUncompressedDataset, chineseCompressedDataset, chineseSymTable
        );
    }
}
