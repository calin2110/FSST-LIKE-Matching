//
// Created by popca on 15-May-25.
//


#ifndef FSST_ENV_HPP
#define FSST_ENV_HPP

#include <memory>
#include "utils.hpp"
#include "automata.hpp"
#include "common.hpp"
#include "shared.hpp"

extern Encoder encoder;
extern std::vector<std::string> asciiPatterns;
extern std::vector<std::basic_string<uint8_t>> patterns;
extern std::vector<uint8_t> chars;
extern std::vector<automata::State> endStates;
extern std::vector<automata::State*> precomputedEnds;
extern std::unordered_set<automata::State*> endsSet;
extern automata::support::TemporaryStarts tempStarts;
extern automata::State errorState;

extern std::unique_ptr<file::FileData> in;
extern std::unique_ptr<file::FileData> out;

#endif //FSST_ENV_HPP
