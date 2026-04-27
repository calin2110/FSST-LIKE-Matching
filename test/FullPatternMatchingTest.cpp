//
// Created by pop on 6/13/25.
//

#include "shared.hpp"
#include "automata.hpp"
#include "env.hpp"

#include <gtest/gtest.h>

#include "like_pattern_automaton.hpp"

namespace test {
    TEST(FullPatternMatchTest, TestStartLongnamePatterns) {
        std::string path = "../../data/test_data/StartFullPatternMatchingTest";
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error(fmt::format("Could not open {}", path));
        }
        auto currentUncompressed = file::readBinaryFileData("../../data/longname/uncompressed");
        auto currentCompressed = file::readBinaryFileData("../../data/longname/compressed");
        auto currentEncoder = Encoder(SymbolTable::readFromFile("../../data/longname/symbolsBinary"));
        size_t n = currentCompressed->getNumElements();

        size_t numPatterns;
        read(fd, &numPatterns, sizeof(numPatterns));
        for (size_t _ = 0; _ < numPatterns; ++_) {
            size_t patternLen;
            read(fd, &patternLen, sizeof(patternLen));
            std::vector<uint8_t> pattern(patternLen + 1, 0);
            read(fd, pattern.data(), patternLen);
            pattern[patternLen] = '%';
            ++patternLen;
            std::span<const uint8_t> bytes(pattern.data(), patternLen);
            fmt::println("Checking pattern \"{}\"",  byteSpanToString(bytes));

            uint8_t uncompressedMatch[n];

            read(fd, uncompressedMatch, n * sizeof(uint8_t));
            auto parser = automata::parsing::LikePatternAutomatonParser(bytes, currentEncoder);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = parser.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(FullPatternMatchTest, TestEndLongnamePatterns) {
        std::string path = "../../data/test_data/EndFullPatternMatchingTest";
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error(fmt::format("Could not open {}", path));
        }
        auto currentUncompressed = file::readBinaryFileData("../../data/longname/uncompressed");
        auto currentCompressed = file::readBinaryFileData("../../data/longname/compressed");
        auto currentEncoder = Encoder(SymbolTable::readFromFile("../../data/longname/symbolsBinary"));
        size_t n = currentCompressed->getNumElements();

        size_t numPatterns;
        read(fd, &numPatterns, sizeof(numPatterns));
        for (size_t _ = 0; _ < numPatterns; ++_) {
            size_t patternLen;
            read(fd, &patternLen, sizeof(patternLen));
            std::vector<uint8_t> pattern(patternLen + 1, 0);
            read(fd, pattern.data() + 1, patternLen);
            pattern[0] = '%';
            ++patternLen;
            std::span<const uint8_t> bytes(pattern.data(), patternLen);
            fmt::println("Checking pattern \"{}\"",  byteSpanToString(bytes));

            uint8_t uncompressedMatch[n];

            read(fd, uncompressedMatch, n * sizeof(uint8_t));
            auto parser = automata::parsing::LikePatternAutomatonParser(bytes, currentEncoder);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = parser.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }


    TEST(FullPatternMatchTest, TestFirstnamePatterns) {
        auto testCaseName = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string path = fmt::format("../../data/test_data/{}{}", testCaseName, testName);
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error(fmt::format("Could not open {}", path));
        }
        auto currentUncompressed = file::readBinaryFileData("../../data/firstname/uncompressed");
        auto currentCompressed = file::readBinaryFileData("../../data/firstname/compressed");
        auto currentEncoder = Encoder(SymbolTable::readFromFile("../../data/firstname/symbolsBinary"));
        size_t n = currentCompressed->getNumElements();

        size_t patternLen;
        while (read(fd, &patternLen, sizeof(patternLen))) {
            std::vector<uint8_t> pattern(patternLen + 1, 0);
            read(fd, pattern.data(), patternLen);
            std::span<const uint8_t> bytes(pattern.data(), patternLen);
            fmt::println("Checking pattern \"{}\"",  byteSpanToString(bytes));

            uint8_t uncompressedMatch[n];

            read(fd, uncompressedMatch, n * sizeof(uint8_t));

            auto parser = automata::parsing::LikePatternAutomatonParser(bytes, currentEncoder);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = !parser.hasEmptyAutomaton() && parser.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(FullPatternMatchTest, TestHamletPatterns) {
        auto testCaseName = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string path = fmt::format("../../data/test_data/{}{}", testCaseName, testName);
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error(fmt::format("Could not open {}", path));
        }
        auto currentUncompressed = file::readBinaryFileData("../../data/hamlet/uncompressed");
        auto currentCompressed = file::readBinaryFileData("../../data/hamlet/compressed");
        auto currentEncoder = Encoder(SymbolTable::readFromFile("../../data/hamlet/symbolsBinary"));
        size_t n = currentCompressed->getNumElements();

        size_t patternLen;
        while (read(fd, &patternLen, sizeof(patternLen))) {
            std::vector<uint8_t> pattern(patternLen + 1, 0);
            read(fd, pattern.data(), patternLen);
            std::span<const uint8_t> bytes(pattern.data(), patternLen);
            fmt::println("Checking pattern \"{}\"", byteSpanToString(bytes));

            uint8_t uncompressedMatch[n];

            read(fd, uncompressedMatch, n * sizeof(uint8_t));

            auto parser = automata::parsing::LikePatternAutomatonParser(bytes, currentEncoder);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = !parser.hasEmptyAutomaton() && parser.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(FullPatternMatchTest, TestL_commentPatterns) {
        auto testCaseName = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string path = fmt::format("../../data/test_data/{}{}", testCaseName, testName);
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error(fmt::format("Could not open {}", path));
        }
        auto currentUncompressed = file::readBinaryFileData("../../data/l_comment/uncompressed");
        auto currentCompressed = file::readBinaryFileData("../../data/l_comment/compressed");
        auto currentEncoder = Encoder(SymbolTable::readFromFile("../../data/l_comment/symbolsBinary"));
        size_t n = currentCompressed->getNumElements();

        size_t patternLen;
        while (read(fd, &patternLen, sizeof(patternLen))) {
            std::vector<uint8_t> pattern(patternLen + 1, 0);
            read(fd, pattern.data(), patternLen);
            std::span<const uint8_t> bytes(pattern.data(), patternLen);
            fmt::println("Checking pattern \"{}\"", byteSpanToString(bytes));

            uint8_t uncompressedMatch[n];

            read(fd, uncompressedMatch, n * sizeof(uint8_t));

            auto parser = automata::parsing::LikePatternAutomatonParser(bytes, currentEncoder);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = !parser.hasEmptyAutomaton() && parser.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string \"{}\" (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(FullPatternMatchTest, TestPs_commentPatterns) {
        auto testCaseName = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string path = fmt::format("../../data/test_data/{}{}", testCaseName, testName);
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error(fmt::format("Could not open {}", path));
        }
        auto currentUncompressed = file::readBinaryFileData("../../data/ps_comment/uncompressed");
        auto currentCompressed = file::readBinaryFileData("../../data/ps_comment/compressed");
        auto currentEncoder = Encoder(SymbolTable::readFromFile("../../data/ps_comment/symbolsBinary"));
        size_t n = currentCompressed->getNumElements();

        size_t patternLen;
        while (read(fd, &patternLen, sizeof(patternLen))) {
            std::vector<uint8_t> pattern(patternLen + 1, 0);
            read(fd, pattern.data(), patternLen);
            std::span<const uint8_t> bytes(pattern.data(), patternLen);
            fmt::println("Checking pattern \"{}\"", byteSpanToString(bytes));

            uint8_t uncompressedMatch[n];

            read(fd, uncompressedMatch, n * sizeof(uint8_t));

            auto parser = automata::parsing::LikePatternAutomatonParser(bytes, currentEncoder);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = !parser.hasEmptyAutomaton() && parser.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string \"{}\" (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(FullPatternMatchTest, TestChinesePatterns) {
        auto testCaseName = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string path = fmt::format("../../data/test_data/{}{}", testCaseName, testName);
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error(fmt::format("Could not open {}", path));
        }
        auto currentUncompressed = file::readBinaryFileData("../../data/chinese/uncompressed");
        auto currentCompressed = file::readBinaryFileData("../../data/chinese/compressed");
        auto currentEncoder = Encoder(SymbolTable::readFromFile("../../data/chinese/symbolsBinary"));
        size_t n = currentCompressed->getNumElements();

        size_t patternLen;
        while (read(fd, &patternLen, sizeof(patternLen))) {
            std::vector<uint8_t> pattern(patternLen + 1, 0);
            read(fd, pattern.data(), patternLen);
            std::span<const uint8_t> bytes(pattern.data(), patternLen);
            fmt::println("Checking pattern \"{}\"", byteSpanToString(bytes));

            uint8_t uncompressedMatch[n];

            read(fd, uncompressedMatch, n * sizeof(uint8_t));

            auto parser = automata::parsing::LikePatternAutomatonParser(bytes, currentEncoder);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = !parser.hasEmptyAutomaton() && parser.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string \"{}\" (encoded as {}) with index {}: \n",
                    byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }

        }
        close(fd);
    }

}
