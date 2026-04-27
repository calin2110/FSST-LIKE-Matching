//
// Created by pop on 6/25/25.
//
#include "gtest/gtest.h"
#include "env.hpp"
#include "automata.hpp"
#include "pattern.hpp"

namespace test {
    std::vector<const automata::State*> findPseudoEnds(const automata::SingleStartFiniteAutomaton& automaton) {
        std::vector<const automata::State*> pseudoEnds(9, nullptr);
        for (const automata::State& node: automaton.states) {
            if (node.endIdx.has_value()) {
                pseudoEnds[node.endIdx.value()] = &node;
            }
        }
        return pseudoEnds;
    }

    TEST(EndPatternBuildTest, TestBuildFirstPattern) {
        StringPattern sp{patterns[0]};
        automata::SingleStartFiniteAutomaton automaton = sp.createEndAutomaton(encoder, precomputedEnds, &errorState);
        std::vector<const automata::State*> pseudoEnds = findPseudoEnds(automaton);

        automata::State* defaultState = &errorState;
        ASSERT_EQ(automaton.startState->level, 3);
        ASSERT_EQ(automaton.startState->transition(238)->level, 2);
        ASSERT_EQ(automaton.deterministicPath, std::basic_string<uint8_t>({211, 238}));
        ASSERT_EQ(automaton.actualStartState, automaton.startState->transition(238)->transition(211));
        automata::State* currentState = automaton.actualStartState;
        ASSERT_EQ(currentState->level, 1);
        ASSERT_EQ(currentState->defaultTransition, defaultState);
        ASSERT_EQ(currentState->transitions.size(), 2);

        ASSERT_TRUE(currentState->canTransition(73));
        ASSERT_EQ(currentState->transition(73), pseudoEnds[0]);

        ASSERT_TRUE(currentState->canTransition(236));

        currentState = currentState->transition(236);
        ASSERT_EQ(currentState->defaultTransition, defaultState);
        ASSERT_EQ(currentState->level, 1);
        ASSERT_EQ(currentState->transitions.size(), 3);

        ASSERT_TRUE(currentState->canTransition(114));
        ASSERT_EQ(currentState->transition(114), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(215));
        ASSERT_EQ(currentState->transition(215), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(219));
        ASSERT_EQ(currentState->transition(219), pseudoEnds[4]);
    }

    TEST(EndPatternBuildTest, TestBuildSecondPattern) {
        StringPattern sp{patterns[1]};
        automata::SingleStartFiniteAutomaton automaton = sp.createEndAutomaton(encoder, precomputedEnds, &errorState);
        std::vector<const automata::State*> pseudoEnds = findPseudoEnds(automaton);

        ASSERT_EQ(automaton.startState->level, 3);
        ASSERT_EQ(automaton.startState->transition(170)->level, 2);
        ASSERT_EQ(automaton.startState->transition(170)->transition(152)->level, 1);
        ASSERT_EQ(automaton.deterministicPath, std::basic_string<uint8_t>({253, 152, 170}));
        ASSERT_EQ(automaton.actualStartState, automaton.startState->transition(170)->transition(152)->transition(253));
        automata::State* currentState = automaton.actualStartState;
        ASSERT_EQ(currentState, pseudoEnds[0]);
        ASSERT_EQ(currentState->level, 0);
    }

    TEST(EndPatternBuildTest, TestBuildThirdPattern) {
        StringPattern sp{patterns[2]};
        automata::SingleStartFiniteAutomaton automaton = sp.createEndAutomaton(encoder, precomputedEnds, &errorState);
        std::vector<const automata::State*> pseudoEnds = findPseudoEnds(automaton);

        automata::State* defaultState = &errorState;
        ASSERT_TRUE(automaton.deterministicPath.empty());
        ASSERT_EQ(automaton.actualStartState, automaton.startState.get());
        automata::State* currentState = automaton.startState.get();

        ASSERT_EQ(currentState->level, 1);
        ASSERT_EQ(currentState->transitions.size(), 13);

        ASSERT_TRUE(currentState->canTransition(129));
        ASSERT_EQ(currentState->transition(129), pseudoEnds[0]);

        ASSERT_TRUE(currentState->canTransition(136));
        ASSERT_EQ(currentState->transition(136), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(139));
        ASSERT_EQ(currentState->transition(139), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(146));
        ASSERT_EQ(currentState->transition(146), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(149));
        ASSERT_EQ(currentState->transition(149), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(158));
        ASSERT_EQ(currentState->transition(158), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(165));
        ASSERT_EQ(currentState->transition(165), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(169));
        ASSERT_EQ(currentState->transition(169), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(170));
        ASSERT_EQ(currentState->transition(170), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(183));
        ASSERT_EQ(currentState->transition(183), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(193));
        ASSERT_EQ(currentState->transition(193), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(211));
        ASSERT_EQ(currentState->transition(211), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(225));
        currentState = currentState->transition(225);
        ASSERT_EQ(currentState->defaultTransition, defaultState);

        ASSERT_EQ(currentState->level, 1);
        ASSERT_EQ(currentState->transitions.size(), 33);

        ASSERT_TRUE(currentState->canTransition(1));
        ASSERT_EQ(currentState->transition(1), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(5));
        ASSERT_EQ(currentState->transition(5), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(8));
        ASSERT_EQ(currentState->transition(8), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(17));
        ASSERT_EQ(currentState->transition(17), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(24));
        ASSERT_EQ(currentState->transition(24), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(34));
        ASSERT_EQ(currentState->transition(34), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(49));
        ASSERT_EQ(currentState->transition(49), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(86));
        ASSERT_EQ(currentState->transition(86), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(108));
        ASSERT_EQ(currentState->transition(108), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(113));
        ASSERT_EQ(currentState->transition(113), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(118));
        ASSERT_EQ(currentState->transition(118), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(122));
        ASSERT_EQ(currentState->transition(122), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(124));
        ASSERT_EQ(currentState->transition(124), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(1));
        ASSERT_EQ(currentState->transition(1), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(125));
        ASSERT_EQ(currentState->transition(125), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(126));
        ASSERT_EQ(currentState->transition(126), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(127));
        ASSERT_EQ(currentState->transition(127), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(128));
        ASSERT_EQ(currentState->transition(128), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(130));
        ASSERT_EQ(currentState->transition(130), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(131));
        ASSERT_EQ(currentState->transition(131), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(132));
        ASSERT_EQ(currentState->transition(132), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(134));
        ASSERT_EQ(currentState->transition(134), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(135));
        ASSERT_EQ(currentState->transition(135), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(140));
        ASSERT_EQ(currentState->transition(140), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(152));
        ASSERT_EQ(currentState->transition(152), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(156));
        ASSERT_EQ(currentState->transition(156), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(161));
        ASSERT_EQ(currentState->transition(161), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(167));
        ASSERT_EQ(currentState->transition(167), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(174));
        ASSERT_EQ(currentState->transition(174), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(175));
        ASSERT_EQ(currentState->transition(175), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(190));
        ASSERT_EQ(currentState->transition(190), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(197));
        ASSERT_EQ(currentState->transition(197), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(209));
        ASSERT_EQ(currentState->transition(209), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(216));
        ASSERT_EQ(currentState->transition(216), pseudoEnds[3]);
    }

    TEST(EndPatternBuildTest, TestBuildFourthPattern) {
        StringPattern sp{patterns[3]};
        automata::SingleStartFiniteAutomaton automaton = sp.createEndAutomaton(encoder, precomputedEnds, &errorState);
        std::vector<const automata::State*> pseudoEnds = findPseudoEnds(automaton);

        automata::State* defaultState = &errorState;
        ASSERT_EQ(automaton.startState->level, 3);
        ASSERT_EQ(automaton.deterministicPath, std::basic_string<uint8_t>({178}));
        ASSERT_EQ(automaton.actualStartState, automaton.startState->transition(178));
        automata::State* currentState = automaton.actualStartState;
        ASSERT_EQ(currentState->defaultTransition, defaultState);
        ASSERT_EQ(currentState->level, 2);

        ASSERT_EQ(currentState->transitions.size(), 2);
        {
            ASSERT_TRUE(currentState->canTransition(205));
            automata::State* workingState = currentState->transition(205);
            ASSERT_EQ(workingState->level, 1);
            ASSERT_EQ(workingState->defaultTransition, defaultState);

            ASSERT_EQ(workingState->transitions.size(), 2);

            ASSERT_TRUE(workingState->canTransition(107));
            ASSERT_EQ(workingState->transition(107), pseudoEnds[0]);

            ASSERT_TRUE(workingState->canTransition(191));
            ASSERT_EQ(workingState->transition(191), pseudoEnds[1]);
        }

        {
            ASSERT_TRUE(currentState->canTransition(14));
            automata::State* workingState = currentState->transition(14);
            ASSERT_EQ(workingState->defaultTransition, defaultState);
            ASSERT_EQ(workingState->level, 2);

            ASSERT_EQ(workingState->transitions.size(), 1);
            ASSERT_TRUE(workingState->canTransition(42));
            workingState = workingState->transition(42);
            ASSERT_EQ(workingState->defaultTransition, defaultState);
            ASSERT_EQ(workingState->level, 1);

            ASSERT_EQ(workingState->transitions.size(), 7);

            ASSERT_TRUE(workingState->canTransition(53));
            ASSERT_EQ(workingState->transition(53), pseudoEnds[1]);

            ASSERT_TRUE(workingState->canTransition(69));
            ASSERT_EQ(workingState->transition(69), pseudoEnds[1]);

            ASSERT_TRUE(workingState->canTransition(72));
            ASSERT_EQ(workingState->transition(72), pseudoEnds[1]);

            ASSERT_TRUE(workingState->canTransition(75));
            ASSERT_EQ(workingState->transition(75), pseudoEnds[1]);

            ASSERT_TRUE(workingState->canTransition(95));
            ASSERT_EQ(workingState->transition(95), pseudoEnds[1]);

            ASSERT_TRUE(workingState->canTransition(168));
            ASSERT_EQ(workingState->transition(168), pseudoEnds[2]);

            ASSERT_TRUE(workingState->canTransition(180));
            ASSERT_EQ(workingState->transition(180), pseudoEnds[2]);
        }
    }

    TEST(EndPatternBuildTest, TestBuildFifthPattern) {
        StringPattern sp{patterns[4]};
        automata::SingleStartFiniteAutomaton automaton = sp.createEndAutomaton(encoder, precomputedEnds, &errorState);
        std::vector<const automata::State*> pseudoEnds = findPseudoEnds(automaton);

        automata::State* defaultState = &errorState;

        ASSERT_EQ(automaton.deterministicPath, std::basic_string<uint8_t>({239, 137}));
        ASSERT_EQ(automaton.actualStartState, automaton.startState->transition(137)->transition(239));
        automata::State* currentState = automaton.actualStartState;
        ASSERT_EQ(currentState->level, 1);
        ASSERT_EQ(currentState->defaultTransition, defaultState);

        ASSERT_EQ(currentState->transitions.size(), 2);

        ASSERT_TRUE(currentState->canTransition(117));
        ASSERT_EQ(currentState->transition(117), pseudoEnds[0]);
        ASSERT_TRUE(currentState->canTransition(229));
        currentState = currentState->transition(229);
        ASSERT_EQ(currentState->level, 1);
        ASSERT_EQ(currentState->defaultTransition, defaultState);

        ASSERT_EQ(currentState->transitions.size(), 3);

        ASSERT_TRUE(currentState->canTransition(0));
        ASSERT_EQ(currentState->transition(0), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(94));
        ASSERT_EQ(currentState->transition(94), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(164));
        ASSERT_EQ(currentState->transition(164), pseudoEnds[2]);
    }

    TEST(EndPatternBuildTest, TestBuildSixthPattern) {
        StringPattern sp{patterns[5]};
        automata::SingleStartFiniteAutomaton automaton = sp.createEndAutomaton(encoder, precomputedEnds, &errorState);
        std::vector<const automata::State*> pseudoEnds = findPseudoEnds(automaton);

        automata::State* defaultState = &errorState;
        ASSERT_EQ(automaton.deterministicPath, std::basic_string<uint8_t>({220, 233}));
        ASSERT_EQ(automaton.actualStartState, automaton.startState->transition(233)->transition(220));
        ASSERT_EQ(automaton.startState->level, 4);
        ASSERT_EQ(automaton.startState->transition(233)->level, 3);
        automata::State* currentState = automaton.actualStartState;
        ASSERT_EQ(currentState->defaultTransition, defaultState);
        ASSERT_EQ(currentState->level, 2);

        ASSERT_EQ(currentState->transitions.size(), 2);

        {
            ASSERT_TRUE(currentState->canTransition(106));
            automata::State* workingState = currentState->transition(106);
            ASSERT_EQ(workingState->defaultTransition, defaultState);

            ASSERT_EQ(workingState->level, 1);
            ASSERT_TRUE(workingState->canTransition(46));
            ASSERT_EQ(workingState->transition(46), pseudoEnds[0]);
        }

        {
            ASSERT_TRUE(currentState->canTransition(151));
            automata::State* workingState = currentState->transition(151);
            ASSERT_EQ(workingState->level, 1);
            ASSERT_EQ(workingState->defaultTransition, defaultState);

            ASSERT_EQ(workingState->transitions.size(), 11);

            ASSERT_TRUE(workingState->canTransition(12));
            ASSERT_EQ(workingState->transition(12), pseudoEnds[1]);

            ASSERT_TRUE(workingState->canTransition(31));
            ASSERT_EQ(workingState->transition(31), pseudoEnds[1]);

            ASSERT_TRUE(workingState->canTransition(42));
            ASSERT_EQ(workingState->transition(42), pseudoEnds[1]);

            ASSERT_TRUE(workingState->canTransition(45));
            ASSERT_EQ(workingState->transition(45), pseudoEnds[1]);

            ASSERT_TRUE(workingState->canTransition(46));
            ASSERT_EQ(workingState->transition(46), pseudoEnds[1]);

            ASSERT_TRUE(workingState->canTransition(66));
            ASSERT_EQ(workingState->transition(66), pseudoEnds[1]);

            ASSERT_TRUE(workingState->canTransition(143));
            ASSERT_EQ(workingState->transition(143), pseudoEnds[2]);

            ASSERT_TRUE(workingState->canTransition(172));
            ASSERT_EQ(workingState->transition(172), pseudoEnds[2]);

            ASSERT_TRUE(workingState->canTransition(179));
            ASSERT_EQ(workingState->transition(179), pseudoEnds[2]);

            ASSERT_TRUE(workingState->canTransition(196));
            ASSERT_EQ(workingState->transition(196), pseudoEnds[2]);

            ASSERT_TRUE(workingState->canTransition(220));
            ASSERT_EQ(workingState->transition(220), pseudoEnds[4]);
        }
    }

    TEST(EndPatternBuildTest, TestBuildSeventhPattern) {
        StringPattern sp{patterns[6]};
        automata::SingleStartFiniteAutomaton automaton = sp.createEndAutomaton(encoder, precomputedEnds, &errorState);
        std::vector<const automata::State*> pseudoEnds = findPseudoEnds(automaton);

        automata::State* defaultState = &errorState;
        automata::State* currentState = automaton.startState.get();
        ASSERT_EQ(currentState->level, 1);
        ASSERT_EQ(currentState->defaultTransition, defaultState);
        ASSERT_EQ(currentState->transitions.size(), 21);

        ASSERT_TRUE(currentState->canTransition(9));
        ASSERT_EQ(currentState->transition(9), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(16));
        ASSERT_EQ(currentState->transition(16), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(26));
        ASSERT_EQ(currentState->transition(26), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(28));
        ASSERT_EQ(currentState->transition(28), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(29));
        ASSERT_EQ(currentState->transition(29), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(52));
        ASSERT_EQ(currentState->transition(52), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(57));
        ASSERT_EQ(currentState->transition(57), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(74));
        ASSERT_EQ(currentState->transition(74), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(88));
        ASSERT_EQ(currentState->transition(88), pseudoEnds[1]);

        ASSERT_TRUE(currentState->canTransition(133));
        ASSERT_EQ(currentState->transition(133), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(145));
        ASSERT_EQ(currentState->transition(145), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(150));
        ASSERT_EQ(currentState->transition(150), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(153));
        ASSERT_EQ(currentState->transition(153), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(155));
        ASSERT_EQ(currentState->transition(155), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(166));
        ASSERT_EQ(currentState->transition(166), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(171));
        ASSERT_EQ(currentState->transition(171), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(178));
        ASSERT_EQ(currentState->transition(178), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(188));
        ASSERT_EQ(currentState->transition(188), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(213));
        ASSERT_EQ(currentState->transition(213), pseudoEnds[2]);

        ASSERT_TRUE(currentState->canTransition(218));
        ASSERT_EQ(currentState->transition(218), pseudoEnds[4]);

        ASSERT_TRUE(currentState->canTransition(226));
        ASSERT_EQ(currentState->transition(226), pseudoEnds[0]);
    }
}