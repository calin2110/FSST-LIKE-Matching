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

#ifndef VECTORSCAN_HPP
#define VECTORSCAN_HPP
#include <hs/hs.h>
#include <string>
#include <span>
#include <cstdint>

class VectorScanMatcher {
private:
    static int match_handler(unsigned int, unsigned long long, unsigned long long, unsigned int, void *context);
    hs_database_t* database;
    hs_compile_error_t* compile_err;
    hs_scratch_t* scratch;
    static std::string likePatternToRegex(const std::span<const uint8_t>& likePattern);

public:
    [[nodiscard]] bool matches(const std::span<const uint8_t>& data) const;
    explicit VectorScanMatcher(const std::span<const uint8_t>& likePattern);
    VectorScanMatcher(const VectorScanMatcher&) = delete;
    VectorScanMatcher& operator=(const VectorScanMatcher&) = delete;
    VectorScanMatcher(VectorScanMatcher&& ) noexcept;
    VectorScanMatcher& operator=(VectorScanMatcher&&) noexcept;
    ~VectorScanMatcher();
};
#endif //VECTORSCAN_HPP
