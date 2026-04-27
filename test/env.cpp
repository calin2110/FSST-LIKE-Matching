//
// Created by popca on 15-May-25.
//

#include "env.hpp"

std::vector<std::string> asciiPatterns({"GOGANC", ")ANARAN", "AN", "DESTEMIR", "MEJCHE", "SSHECHRISH", "R"});
std::vector<std::basic_string<uint8_t>> patterns;
std::vector<uint8_t> chars{};
Encoder encoder{SymbolTable::readFromFile("../../data/firstname/symbolsBinary")};
std::vector<automata::State> endStates(9);
std::vector<automata::State*> precomputedEnds(9, nullptr);
automata::State errorState(nullptr);
std::unordered_set<automata::State*> endsSet{};
automata::support::TemporaryStarts tempStarts{};

std::unique_ptr<file::FileData> in;
std::unique_ptr<file::FileData> out;