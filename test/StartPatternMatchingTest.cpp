//
// Created by pop on 5/21/25.
//

#include <gtest/gtest.h>

#include "env.hpp"
#include "automata.hpp"
#include "pattern.hpp"


namespace test {
    std::optional<MatchIndex> checkStartMatch(const automata::SingleStartFiniteAutomaton &automaton, const std::vector<automata::State>& endStates, size_t idx) {
        std::span<const uint8_t> compressed = out->get(idx);
        std::basic_string_view<uint8_t> match_view(compressed.data(), compressed.size());
        std::basic_string_view<uint8_t> prefix_view(automaton.deterministicPath.data(), automaton.deterministicPath.size());

        if (!match_view.starts_with(prefix_view)) {
            return std::nullopt;
        }
        int64_t forwardStrIdx = prefix_view.size();
        int64_t maxIdx = static_cast<int64_t>(compressed.size()) - 1;
        automata::State* currentState = automaton.actualStartState;
        while (forwardStrIdx <= maxIdx && currentState != automaton.errorState && !automata::isEndState(currentState, endStates)) {
            currentState = currentState->transition(compressed[forwardStrIdx]);
            ++forwardStrIdx;
        }

        if (automata::isEndState(currentState, endStates)) {
            return std::make_optional<MatchIndex>(automata::getEndIndex(currentState, endStates), forwardStrIdx - 1);
        } else {
            return std::nullopt;
        }
    }

    TEST(StartPatternMatchTest, TestOneSymbol) {
        auto testCaseName = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string path = fmt::format("../../data/test_data/{}{}", testCaseName, testName);
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error(fmt::format("Could not open {}", path));
        }

        for (size_t i = 0; i < encoder.nSymbols(); ++i) {
            const libfsst::Symbol &symbol = encoder.symbols()[i];
            if (symbol.val.num == 0) {
                continue;
            }
            std::string pattern(symbol.val.str, symbol.val.str + symbol.length());
         
            StringPattern sp{stringToBasicString(pattern)};
            automata::SingleStartFiniteAutomaton fa = sp.createStartAutomaton(encoder, precomputedEnds, &errorState);

            std::vector<uint16_t> matchIndexes{};
            std::vector<uint8_t> symbolIndexes{};
            std::vector<uint8_t> stringIndexes{};
            readExpectedResults(fd, matchIndexes, symbolIndexes, stringIndexes);

            size_t cnt = 0;

            for (size_t idx = 0; idx < in->getNumElements(); ++idx) {
                auto ret = checkStartMatch(fa, endStates, idx);
                bool isMatchCompressed = ret.has_value();
                uint8_t symIndexCompressed = ret.has_value() ? ret->symbolIndex : 0;
                size_t strIndexCompressed = ret.has_value() ? ret->strIndex : 0;

                if (isMatchCompressed && (cnt >= matchIndexes.size() || matchIndexes[cnt] != idx)) {
                    ASSERT_TRUE(false)
                        << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                            pattern, byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                        << "\t isMatchUncompressed: false\n\t isMatchCompressed: true\n";
                }
                if (!isMatchCompressed && cnt < matchIndexes.size() && matchIndexes[cnt] == idx) {
                    ASSERT_TRUE(false)
                        << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                                pattern, byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                        << "\t isMatchUncompressed: true\n\t isMatchCompressed: false\n";
                }

                if (isMatchCompressed) {
                    ASSERT_EQ(strIndexCompressed, stringIndexes[cnt])
                         << fmt::format("pattern {} has different string_indexes for string {} (encoded as {}) with index {}: \n",
                                pattern, byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                         << fmt::format("\t uncompressedStringIndex: {}\n", stringIndexes[cnt])
                         << fmt::format("\t compressedStringIndex: {}\n", strIndexCompressed);

                    ASSERT_EQ(symIndexCompressed, symbolIndexes[cnt])
                         << fmt::format("pattern {} has different symbol_indexes for string {} (encoded as {}) with index {}: \n",
                                pattern, byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                         << fmt::format("\t uncompressedSymbolIndex: {}\n", symbolIndexes[cnt])
                         << fmt::format("\t compressedSymbolIndex: {}\n", symIndexCompressed);

                    ++cnt;
                }
            }
        }
        close(fd);
    }

    TEST(StartPatternMatchTest, TestTwoSymbols) {
        auto testCaseName = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string path = fmt::format("../../data/test_data/{}{}", testCaseName, testName);
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error(fmt::format("Could not open {}", path));
        }

        size_t pruned_end = 15 * encoder.nSymbols() / 16;
        for (size_t i = 0; i < pruned_end; ++i) {
            const libfsst::Symbol &symbol_i = encoder.symbols()[i];
            if (symbol_i.val.num == 0) {
                continue;
            }
            std::string pattern_start(symbol_i.val.str, symbol_i.val.str + symbol_i.length());

            for (size_t j = pruned_end; j < encoder.nSymbols(); ++j) {
                const libfsst::Symbol &symbol_j = encoder.symbols()[j];
                if (symbol_j.val.num == 0) {
                    continue;
                }
                std::string pattern_end(symbol_j.val.str, symbol_j.val.str + symbol_j.length());
                std::string pattern = fmt::format("{}{}", pattern_start, pattern_end);

                StringPattern sp{stringToBasicString(pattern)};
                automata::SingleStartFiniteAutomaton fa = sp.createStartAutomaton(encoder, precomputedEnds, &errorState);

                std::vector<uint16_t> matchIndexes{};
                std::vector<uint8_t> symbolIndexes{};
                std::vector<uint8_t> stringIndexes{};
                readExpectedResults(fd, matchIndexes, symbolIndexes, stringIndexes);

                size_t cnt = 0;
                for (size_t idx = 0; idx < in->getNumElements(); ++idx) {
                    auto ret = checkStartMatch(fa, endStates, idx);
                    bool isMatchCompressed = ret.has_value();
                    uint8_t symIndexCompressed = ret.has_value() ? ret->symbolIndex : 0;
                    size_t strIndexCompressed = ret.has_value() ? ret->strIndex : 0;

                    if (isMatchCompressed && (cnt >= matchIndexes.size() || matchIndexes[cnt] != idx)) {
                        ASSERT_TRUE(false)
                            << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                                pattern, byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                            << "\t isMatchUncompressed: false\n\t isMatchCompressed: true\n";
                    }
                    if (!isMatchCompressed && cnt < matchIndexes.size() && matchIndexes[cnt] == idx) {
                        ASSERT_TRUE(false)
                            << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                                pattern, byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                            << "\t isMatchUncompressed: true\n\t isMatchCompressed: false\n";
                    }

                    if (isMatchCompressed) {
                        ASSERT_EQ(strIndexCompressed, stringIndexes[cnt])
                             << fmt::format("pattern {} has different string_indexes for string {} (encoded as {}) with index {}: \n",
                                 pattern, byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                             << fmt::format("\t uncompressedStringIndex: {}\n", stringIndexes[cnt])
                             << fmt::format("\t compressedStringIndex: {}\n", strIndexCompressed);

                        ASSERT_EQ(symIndexCompressed, symbolIndexes[cnt])
                             << fmt::format("pattern {} has different symbol_indexes for string {} (encoded as {}) with index {}: \n",
                                pattern, byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                             << fmt::format("\t uncompressedSymbolIndex: {}\n", symbolIndexes[cnt])
                             << fmt::format("\t compressedSymbolIndex: {}\n", symIndexCompressed);

                        ++cnt;
                    }
                }
            }
        }

        close(fd);
    }


    TEST(StartPatternMatchTest, TestSubsetTwoSymbols) {
        RNG rng{1337};
        auto testCaseName = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string path = fmt::format("../../data/test_data/{}{}", testCaseName, testName);
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error(fmt::format("Could not open {}", path));
        }

        size_t pruned_end = 15 * encoder.nSymbols() / 16;
        for (size_t i = 0; i < pruned_end; ++i) {
            const libfsst::Symbol &symbol_i = encoder.symbols()[i];
            if (symbol_i.val.num == 0) {
                continue;
            }
            std::string pattern_start(symbol_i.val.str, symbol_i.val.str + symbol_i.length());

            for (size_t j = pruned_end; j < encoder.nSymbols(); ++j) {
                const libfsst::Symbol &symbol_j = encoder.symbols()[j];
                if (symbol_j.val.num == 0) {
                    continue;
                }
                std::string pattern_end(symbol_j.val.str, symbol_j.val.str + symbol_j.length());
                std::string full_pattern = fmt::format("{}{}", pattern_start, pattern_end);
                size_t new_len = rng.generate_uniform_int(static_cast<size_t>(1), full_pattern.size());
                std::string pattern = full_pattern.substr(0, new_len);

                StringPattern sp{stringToBasicString(pattern)};
                automata::SingleStartFiniteAutomaton fa = sp.createStartAutomaton(encoder, precomputedEnds, &errorState);

                std::vector<uint16_t> matchIndexes{};
                std::vector<uint8_t> symbolIndexes{};
                std::vector<uint8_t> stringIndexes{};
                readExpectedResults(fd, matchIndexes, symbolIndexes, stringIndexes);

                size_t cnt = 0;
                for (size_t idx = 0; idx < in->getNumElements(); ++idx) {
                    auto ret = checkStartMatch(fa, endStates, idx);
                    bool isMatchCompressed = ret.has_value();
                    uint8_t symIndexCompressed = ret.has_value() ? ret->symbolIndex : 0;
                    size_t strIndexCompressed = ret.has_value() ? ret->strIndex : 0;

                    if (isMatchCompressed && (cnt >= matchIndexes.size() || matchIndexes[cnt] != idx)) {
                        ASSERT_TRUE(false)
                            << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                                pattern, byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                            << "\t isMatchUncompressed: false\n\t isMatchCompressed: true\n";
                    }
                    if (!isMatchCompressed && cnt < matchIndexes.size() && matchIndexes[cnt] == idx) {
                        ASSERT_TRUE(false)
                            << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                                pattern, byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                            << "\t isMatchUncompressed: true\n\t isMatchCompressed: false\n";
                    }

                    if (isMatchCompressed) {
                        ASSERT_EQ(strIndexCompressed, stringIndexes[cnt])
                             << fmt::format("pattern {} has different string_indexes for string {} (encoded as {}) with index {}: \n",
                                 pattern, byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                             << fmt::format("\t uncompressedStringIndex: {}\n", stringIndexes[cnt])
                             << fmt::format("\t compressedStringIndex: {}\n", strIndexCompressed);

                        ASSERT_EQ(symIndexCompressed, symbolIndexes[cnt])
                             << fmt::format("pattern {} has different symbol_indexes for string {} (encoded as {}) with index {}: \n",
                                pattern, byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                             << fmt::format("\t uncompressedSymbolIndex: {}\n", symbolIndexes[cnt])
                             << fmt::format("\t compressedSymbolIndex: {}\n", symIndexCompressed);

                        ++cnt;
                    }
                }
            }
        }
        close(fd);
    }

    TEST(StartPatternMatchTest, TestFuzzing) {
         RNG rng{420};
         auto testCaseName = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
         auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
         std::string path = fmt::format("../../data/test_data/{}{}", testCaseName, testName);
         int fd = open(path.c_str(), O_RDONLY);
         if (fd == -1) {
             throw std::runtime_error(fmt::format("Could not open {}", path));
         }

        size_t num_trials = 10000;
        for (size_t _ = 0; _ < num_trials; ++_) {
            std::vector<uint8_t> patternVector = rng.generate_pattern(1, 6, chars);
            std::basic_string<uint8_t> pattern = std::basic_string<uint8_t>(patternVector.data(), patternVector.size());

            StringPattern sp{pattern};
            automata::SingleStartFiniteAutomaton fa = sp.createStartAutomaton(encoder, precomputedEnds, &errorState);

            std::vector<uint16_t> matchIndexes{};
            std::vector<uint8_t> symbolIndexes{};
            std::vector<uint8_t> stringIndexes{};
            readExpectedResults(fd, matchIndexes, symbolIndexes, stringIndexes);

            size_t cnt = 0;
            for (size_t idx = 0; idx < in->getNumElements(); ++idx) {
                auto ret = checkStartMatch(fa, endStates, idx);
                bool isMatchCompressed = ret.has_value();
                uint8_t symIndexCompressed = ret.has_value() ? ret->symbolIndex : 0;
                size_t strIndexCompressed = ret.has_value() ? ret->strIndex : 0;

                if (isMatchCompressed && (cnt >= matchIndexes.size() || matchIndexes[cnt] != idx)) {
                    ASSERT_TRUE(false)
                        << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                                byteSpanToString(pattern), byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                        << "\t isMatchUncompressed: false\n\t isMatchCompressed: true\n";
                }
                if (!isMatchCompressed && cnt < matchIndexes.size() && matchIndexes[cnt] == idx) {
                    ASSERT_TRUE(false)
                        << fmt::format("pattern {} has different outputs for string {} (encoded as {}) with index {}: \n",
                                byteSpanToString(pattern), byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                        << "\t isMatchUncompressed: true\n\t isMatchCompressed: false\n";
                }

                if (isMatchCompressed) {
                    ASSERT_EQ(strIndexCompressed, stringIndexes[cnt])
                         << fmt::format("pattern {} has different string_indexes for string {} (encoded as {}) with index {}: \n",
                            byteSpanToString(pattern), byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                         << fmt::format("\t uncompressedStringIndex: {}\n", stringIndexes[cnt])
                         << fmt::format("\t compressedStringIndex: {}\n", strIndexCompressed);

                    ASSERT_EQ(symIndexCompressed, symbolIndexes[cnt])
                         << fmt::format("pattern {} has different symbol_indexes for string {} (encoded as {}) with index {}: \n",
                            byteSpanToString(pattern), byteSpanToString(in->get(idx)), byteSpanToByteList(out->get(idx)), idx)
                         << fmt::format("\t uncompressedSymbolIndex: {}\n", symbolIndexes[cnt])
                         << fmt::format("\t compressedSymbolIndex: {}\n", symIndexCompressed);

                    ++cnt;
                }
            }
        }

        close(fd);
    }
}
