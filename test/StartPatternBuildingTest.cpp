//
// Created by pop on 6/26/25.
//

#include "gtest/gtest.h"
#include "env.hpp"
#include "automata.hpp"
#include "pattern.hpp"

namespace test {

    TEST(StartPatternBuildTest, TestBuildFirstPattern) {
        StringPattern sp{patterns[0]};
        automata::SingleStartFiniteAutomaton automaton = sp.createStartAutomaton(encoder, precomputedEnds, &errorState);

        automata::State* defaultState = &errorState;
        ASSERT_EQ(automaton.startState->level, 3);
        ASSERT_EQ(automaton.startState->transition(73)->level, 2);
        ASSERT_EQ(automaton.deterministicPath, std::basic_string<uint8_t>({73, 211}));
        ASSERT_EQ(automaton.actualStartState, automaton.startState->transition(73)->transition(211));
        automata::State* currentState = automaton.actualStartState;

        ASSERT_EQ(currentState->level, 1);
        ASSERT_EQ(currentState->defaultTransition, defaultState);
        ASSERT_EQ(currentState->transitions.size(), 11);

        ASSERT_TRUE(currentState->canTransition(17));
        ASSERT_EQ(currentState->transition(17), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(41));
        ASSERT_EQ(currentState->transition(41), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(76));
        ASSERT_EQ(currentState->transition(76), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(83));
        ASSERT_EQ(currentState->transition(83), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(84));
        ASSERT_EQ(currentState->transition(84), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(135));
        ASSERT_EQ(currentState->transition(135), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(137));
        ASSERT_EQ(currentState->transition(137), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(141));
        ASSERT_EQ(currentState->transition(141), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(174));
        ASSERT_EQ(currentState->transition(174), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(220));
        ASSERT_EQ(currentState->transition(220), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(238));
        ASSERT_EQ(currentState->transition(238), precomputedEnds[1]);
    }

    TEST(StartPatternBuildTest, TestBuildSecondPattern) {
        StringPattern sp{patterns[1]};
        automata::SingleStartFiniteAutomaton automaton = sp.createStartAutomaton(encoder, precomputedEnds, &errorState);
        ASSERT_EQ(automaton.startState->level, 3);
        ASSERT_EQ(automaton.startState->transition(253)->level, 2);
        ASSERT_EQ(automaton.startState->transition(253)->transition(152)->level, 1);
        ASSERT_EQ(automaton.deterministicPath, std::basic_string<uint8_t>({253, 152, 170}));
        ASSERT_EQ(automaton.actualStartState, automaton.startState->transition(253)->transition(152)->transition(170));
        automata::State* currentState = automaton.actualStartState;

        ASSERT_EQ(currentState, precomputedEnds[3]);
        ASSERT_EQ(currentState->level, 0);
    }

    TEST(StartPatternBuildTest, TestBuildThirdPattern) {
        StringPattern sp{patterns[2]};
        automata::SingleStartFiniteAutomaton automaton = sp.createStartAutomaton(encoder, precomputedEnds, &errorState);

        automata::State* defaultState = &errorState;
        ASSERT_TRUE(automaton.deterministicPath.empty());
        ASSERT_EQ(automaton.actualStartState, automaton.startState.get());
        automata::State* currentState = automaton.startState.get();
        ASSERT_EQ(currentState->level, 1);
        ASSERT_EQ(currentState->defaultTransition, defaultState);
        ASSERT_EQ(currentState->transitions.size(), 4);

        ASSERT_TRUE(currentState->canTransition(129));
        ASSERT_EQ(currentState->transition(129), precomputedEnds[2]);

        ASSERT_TRUE(currentState->canTransition(138));
        ASSERT_EQ(currentState->transition(138), precomputedEnds[2]);

        ASSERT_TRUE(currentState->canTransition(152));
        ASSERT_EQ(currentState->transition(152), precomputedEnds[2]);

        ASSERT_TRUE(currentState->canTransition(168));
        ASSERT_EQ(currentState->transition(168), precomputedEnds[2]);
    }

    TEST(StartPatternBuildTest, TestBuildFourthPattern) {
        StringPattern sp{patterns[3]};
        automata::SingleStartFiniteAutomaton automaton = sp.createStartAutomaton(encoder, precomputedEnds, &errorState);
        ASSERT_EQ(automaton.startState->level, 3);
        ASSERT_EQ(automaton.startState->transition(107)->level, 2);
        ASSERT_EQ(automaton.startState->transition(107)->transition(205)->level, 1);
        ASSERT_EQ(automaton.deterministicPath, std::basic_string<uint8_t>({107, 205, 178}));
        ASSERT_EQ(automaton.actualStartState, automaton.startState->transition(107)->transition(205)->transition(178));
        automata::State* currentState = automaton.actualStartState;
        ASSERT_EQ(currentState, precomputedEnds[3]);
        ASSERT_EQ(currentState->level, 0);
    }

    TEST(StartPatternBuildTest, TestBuildFifthPattern) {
        StringPattern sp{patterns[4]};
        automata::SingleStartFiniteAutomaton automaton = sp.createStartAutomaton(encoder, precomputedEnds, &errorState);
        ASSERT_EQ(automaton.startState->level, 3);
        ASSERT_EQ(automaton.startState->transition(117)->level, 2);
        ASSERT_EQ(automaton.startState->transition(117)->transition(239)->level, 1);
        ASSERT_EQ(automaton.deterministicPath, std::basic_string<uint8_t>({117, 239, 137}));
        ASSERT_EQ(automaton.actualStartState, automaton.startState->transition(117)->transition(239)->transition(137));
        automata::State* currentState = automaton.actualStartState;
        ASSERT_EQ(currentState, precomputedEnds[3]);
        ASSERT_EQ(currentState->level, 0);
    }

    TEST(StartPatternBuildTest, TestBuildSixthPattern) {
        StringPattern sp{patterns[5]};
        automata::SingleStartFiniteAutomaton automaton = sp.createStartAutomaton(encoder, precomputedEnds, &errorState);

        automata::State* defaultState = &errorState;
        ASSERT_EQ(automaton.startState->level, 4);
        ASSERT_EQ(automaton.startState->transition(46)->level, 3);
        ASSERT_EQ(automaton.startState->transition(46)->transition(106)->level, 2);
        ASSERT_EQ(automaton.deterministicPath, std::basic_string<uint8_t>({46, 106, 220}));
        ASSERT_EQ(automaton.actualStartState, automaton.startState->transition(46)->transition(106)->transition(220));
        automata::State* currentState = automaton.actualStartState;
        ASSERT_EQ(currentState->level, 1);
        ASSERT_EQ(currentState->defaultTransition, defaultState);
        ASSERT_EQ(currentState->transitions.size(), 9);

        ASSERT_TRUE(currentState->canTransition(6));
        ASSERT_EQ(currentState->transition(6), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(36));
        ASSERT_EQ(currentState->transition(36), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(51));
        ASSERT_EQ(currentState->transition(51), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(106));
        ASSERT_EQ(currentState->transition(106), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(124));
        ASSERT_EQ(currentState->transition(124), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(139));
        ASSERT_EQ(currentState->transition(139), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(145));
        ASSERT_EQ(currentState->transition(145), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(185));
        ASSERT_EQ(currentState->transition(185), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(233));
        ASSERT_EQ(currentState->transition(233), precomputedEnds[1]);
    }

    TEST(StartPatternBuildTest, TestBuildSeventhPattern) {
        StringPattern sp{patterns[6]};
        automata::SingleStartFiniteAutomaton automaton = sp.createStartAutomaton(encoder, precomputedEnds, &errorState);

        automata::State* defaultState = &errorState;
        automata::State* currentState = automaton.startState.get();
        ASSERT_TRUE(automaton.deterministicPath.empty());
        ASSERT_EQ(automaton.actualStartState, automaton.startState.get());
        ASSERT_EQ(currentState->level, 1);
        ASSERT_EQ(currentState->defaultTransition, defaultState);
        ASSERT_EQ(currentState->transitions.size(), 19);

        ASSERT_TRUE(currentState->canTransition(33));
        ASSERT_EQ(currentState->transition(33), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(57));
        ASSERT_EQ(currentState->transition(57), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(75));
        ASSERT_EQ(currentState->transition(75), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(80));
        ASSERT_EQ(currentState->transition(80), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(104));
        ASSERT_EQ(currentState->transition(104), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(116));
        ASSERT_EQ(currentState->transition(116), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(121));
        ASSERT_EQ(currentState->transition(121), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(130));
        ASSERT_EQ(currentState->transition(130), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(143));
        ASSERT_EQ(currentState->transition(143), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(159));
        ASSERT_EQ(currentState->transition(159), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(161));
        ASSERT_EQ(currentState->transition(161), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(162));
        ASSERT_EQ(currentState->transition(162), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(164));
        ASSERT_EQ(currentState->transition(164), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(170));
        ASSERT_EQ(currentState->transition(170), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(179));
        ASSERT_EQ(currentState->transition(179), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(196));
        ASSERT_EQ(currentState->transition(196), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(202));
        ASSERT_EQ(currentState->transition(202), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(210));
        ASSERT_EQ(currentState->transition(210), precomputedEnds[1]);

        ASSERT_TRUE(currentState->canTransition(226));
        ASSERT_EQ(currentState->transition(226), precomputedEnds[1]);
    }
}