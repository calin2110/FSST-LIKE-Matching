//
// Created by pop on 7/18/25.
//

#include "shared.hpp"
#include "automata.hpp"
#include "env.hpp"

#include <gtest/gtest.h>

#include "codegen/cppcodegen.hpp"

namespace test {
    automata::codegen::CompiledAutomaton createCppCompiledAutomaton(const std::unique_ptr<automata::parsing::LikePatternAutomaton>& automaton, bool enableSIMD) {
        int64_t identifier = getTimeNow();
        std::string cppFile = fmt::format("automaton_{}.cpp", identifier);
        std::string destination = fmt::format("libgenerated_{}.so", identifier);
        automata::codegen::cpp::CppCompiler compiler{cppFile, destination, enableSIMD, false};
        std::unique_ptr<automata::codegen::Parser> parser = compiler.compile(automaton);
        return automata::codegen::CompiledAutomaton{parser};
    }

    TEST(CppCompiledPatternMatchTest, TestStartLongnamePatternsNoSIMD) {
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), false);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(CppCompiledPatternMatchTest, TestStartLongnamePatternsSIMDEnabled) {
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), true);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(CppCompiledPatternMatchTest, TestEndLongnamePatternsNoSIMD) {
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), false);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(CppCompiledPatternMatchTest, TestEndLongnamePatternsSIMDEnabled) {
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), true);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(CppCompiledPatternMatchTest, TestFirstnamePatternsNoSIMD) {
        std::string path = "../../data/test_data/FullPatternMatchTestTestFirstnamePatterns";
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), false);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(CppCompiledPatternMatchTest, TestFirstnamePatternsSIMDEnabled) {
        std::string path = "../../data/test_data/FullPatternMatchTestTestFirstnamePatterns";
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), true);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(CppCompiledPatternMatchTest, TestHamletPatternsNoSIMD) {
        std::string path = "../../data/test_data/FullPatternMatchTestTestHamletPatterns";
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), false);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(CppCompiledPatternMatchTest, TestHamletPatternsSIMDEnabled) {
        std::string path = "../../data/test_data/FullPatternMatchTestTestHamletPatterns";
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), true);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(CppCompiledPatternMatchTest, TestL_commentPatternsNoSIMD) {
        std::string path = "../../data/test_data/FullPatternMatchTestTestL_commentPatterns";
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), false);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string \"{}\" (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(CppCompiledPatternMatchTest, TestL_commentPatternsSIMDEnabled) {
        std::string path = "../../data/test_data/FullPatternMatchTestTestL_commentPatterns";
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), true);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string \"{}\" (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(CppCompiledPatternMatchTest, TestPs_commentPatternsNoSIMD) {
        std::string path = "../../data/test_data/FullPatternMatchTestTestPs_commentPatterns";
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), false);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string \"{}\" (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(CppCompiledPatternMatchTest, TestPs_commentPatternsSIMDEnabled) {
        std::string path = "../../data/test_data/FullPatternMatchTestTestPs_commentPatterns";
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), true);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string \"{}\" (encoded as {}) with index {}: \n",
                  byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }
        }
        close(fd);
    }

    TEST(CppCompiledPatternMatchTest, TestChinesePatternsNoSIMD) {
        std::string path = "../../data/test_data/FullPatternMatchTestTestChinesePatterns";
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), false);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
                ASSERT_EQ(matches, uncompressedMatch[idx])
                    << fmt::format("pattern {} has different outputs for string \"{}\" (encoded as {}) with index {}: \n",
                    byteSpanToString(bytes), byteSpanToString(currentUncompressed->get(idx)), byteSpanToByteList(currentCompressed->get(idx)), idx)
                    << fmt::format("\t isMatchUncompressed: {}\n", uncompressedMatch[idx])
                    << fmt::format("\t isMatchCompressed: {}\n", matches);
            }

        }
        close(fd);
    }

    TEST(CppCompiledPatternMatchTest, TestChinesePatternsSIMDEnabled) {
        std::string path = "../../data/test_data/FullPatternMatchTestTestChinesePatterns";
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

            automata::codegen::CompiledAutomaton automaton = createCppCompiledAutomaton(automata::parsing::LikePatternAutomaton::build(bytes, currentEncoder), true);

            for (size_t idx = 0; idx < n; ++idx) {
                uint8_t matches = automaton.parse(currentCompressed->get(idx));
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
