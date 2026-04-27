// this software is distributed under the MIT License (http://www.opensource.org/licenses/MIT):
//
// Copyright (c) 2026 Calin Pop George
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "vectorscan.hpp"
#include <iostream>
#include <fmt/format.h>

int VectorScanMatcher::match_handler(unsigned int, unsigned long long, unsigned long long, unsigned int, void *context) {
    bool *found = static_cast<bool*>(context);
    *found = true;
    return 1;
}

std::string VectorScanMatcher::likePatternToRegex(const std::span<const uint8_t>& likePattern) {
    std::string regex;
    size_t startIdx = 0;
    if (likePattern.front() == '%') {
        while (startIdx < likePattern.size() && likePattern[startIdx] == '%')
            ++startIdx;
        if (startIdx == likePattern.size())
            throw std::runtime_error(fmt::format("Invalid pattern to transform into regex: {}", std::string_view(reinterpret_cast<const char*>(likePattern.data()), likePattern.size())));
    } else {
        regex += '^';
    }

    std::string endAnchor;
    int64_t endIdx = likePattern.size() - 1;
    if (likePattern.back() == '%') {
        while (endIdx >= 0 && likePattern[endIdx] == '%' && (endIdx == 0 || likePattern[endIdx - 1] != '\\'))
            --endIdx;
    } else {
        endAnchor = "$";
    }

    std::span<const uint8_t> pattern(likePattern.data() + startIdx, endIdx - startIdx + 1);
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '\\') {
            if (i + 1 == pattern.size())
                throw std::runtime_error(fmt::format("Invalid escaped character in the pattern {}", std::string_view(reinterpret_cast<const char*>(pattern.data()), pattern.size())));

            char next = reinterpret_cast<const char*>(pattern.data())[++i];
            regex += "\\";
            regex += next;
            continue;
        }

        char c = reinterpret_cast<const char*>(pattern.data())[i];
        switch (c) {
            case '%':
                regex += ".*";
                break;
            case '_':
                regex += ".";
                break;
            case '.': case '^': case '$': case '*': case '+':
            case '?': case '(': case ')': case '[': case ']':
            case '{': case '}': case '|':
                regex += "\\";
                regex += c;
                break;
            default:
                regex += c;
                break;
        }
    }
    regex += endAnchor;
    return regex;
}

bool VectorScanMatcher::matches(const std::span<const uint8_t> &data) const {
    bool found = false;
    hs_scan(database, reinterpret_cast<const char*>(data.data()), data.size(), 0, scratch, match_handler, &found);
    return found;
}

VectorScanMatcher::VectorScanMatcher(const std::span<const uint8_t>& likePattern): database(nullptr), compile_err(nullptr), scratch(nullptr) {
    std::string regexPattern = VectorScanMatcher::likePatternToRegex(likePattern);
    constexpr unsigned int flags = HS_FLAG_SINGLEMATCH | HS_FLAG_DOTALL;
    hs_error_t err = hs_compile(regexPattern.c_str(), flags, HS_MODE_BLOCK, nullptr, &database, &compile_err);

    if (err != HS_SUCCESS) {
        // Check if we have detailed error info
        if (compile_err != nullptr) {
            std::cerr << "ERROR: Compilation failed!" << std::endl;
            std::cerr << "Message: " << compile_err->message << std::endl;
            std::cerr << "At expression index: " << compile_err->expression << std::endl;

            // CRITICAL: You must free the error object manually
            hs_free_compile_error(compile_err);
        } else {
            std::cerr << "ERROR: Compilation failed with code " << err << " but no error message was provided." << std::endl;
        }
        // Handle the failure (e.g., return, throw exception, etc.)
        return;
    }

    // CRITICAL MISSING PIECE: Allocation of scratch space
    err = hs_alloc_scratch(database, &scratch);
    if (err != HS_SUCCESS) {
        std::cerr << "ERROR: Scratch allocation failed with code " << err << std::endl;
        hs_free_database(database);
        database = nullptr;
        scratch = nullptr;
    }
}

VectorScanMatcher::VectorScanMatcher(VectorScanMatcher&& other) noexcept: database(other.database), compile_err(other.compile_err), scratch(other.scratch) {
    other.database = nullptr;
    other.compile_err = nullptr;
    other.scratch = nullptr;
}

VectorScanMatcher& VectorScanMatcher::operator=(VectorScanMatcher&& other) noexcept {
    if (this != &other) {
        database = other.database;
        compile_err = other.compile_err;
        scratch = other.scratch;

        other.database = nullptr;
        other.compile_err = nullptr;
        other.scratch = nullptr;
    }
    return *this;
}

VectorScanMatcher::~VectorScanMatcher() {
    if (scratch != nullptr) {
        hs_free_scratch(scratch);
    }

    if (database != nullptr) {
        hs_free_database(database);
    }
}
