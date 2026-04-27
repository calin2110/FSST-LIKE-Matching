//---------------------------------------------------------------------------
// Umbra
// (c) 2017 Thomas Neumann
//---------------------------------------------------------------------------


#ifndef HYBRID_STRING_SEARCH_HPP
#define HYBRID_STRING_SEARCH_HPP
#include <span>
#include <cstring>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>

#ifdef __SSE4_2__
#include <nmmintrin.h>
#include <pmmintrin.h>
#endif

inline int byteCmpObjects(const uint8_t* a, const uint8_t* b, std::size_t numElements) noexcept {
    return __builtin_memcmp(a, b, sizeof(uint8_t) * numElements);
}

inline void copyRawMem(void* target, const void* source, std::size_t numBytes) noexcept {
    if (numBytes) __builtin_memcpy(target, source, numBytes);
}

struct [[gnu::packed]] unaligned_uint64 {
    uint64_t value;
    /// Load the value
    constexpr uint64_t get() const noexcept { return value; }
    /// Implicit conversion to the value
    constexpr operator uint64_t() const noexcept { return value; }
    /// Implicit converting constructor
    constexpr unaligned_uint64(uint64_t value) noexcept : value(value) {}
    /// Get the potentially unaligned address
    void* getPtr() noexcept { return this; }
    /// Get the potentially unaligned address
    const void* getPtr() const noexcept { return this; }
};

inline uint64_t unalignedLoad(const void* ptr) noexcept { return reinterpret_cast<const unaligned_uint64*>(ptr)->value; }

inline uint64_t read8Unchecked(const uint8_t* str) noexcept {
    return unalignedLoad(str);
}

[[gnu::always_inline]] inline void unalignedStore(void* ptr, uint64_t value) noexcept { reinterpret_cast<unaligned_uint64*>(ptr)->value = value; }

template <class T, class T2>
[[gnu::always_inline]] inline T* reinterpretAsVector(T2* ptr) {
    return reinterpret_cast<T*>(ptr);
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
inline uint64_t
#if defined(__clang__)
   __attribute__((no_sanitize("address", "undefined")))
#else
   __attribute__((no_sanitize_address, no_sanitize_undefined))
#endif
   read8(const uint8_t* str, uintptr_t len) noexcept {
    uint64_t block = 0;
    if (len >= 8) {
        memcpy(&block, str, 8);
        return block;
    }

    memcpy(&block, str, len);
    return block;
}

template <class Cmp>
    static int computeMaxSuffix(const uint8_t* pattern, int32_t length, int32_t& period, Cmp compare) {
    int32_t maxSuffix = -1;
    int32_t ptr = 0, candPeriod = 1;
    period = 1;

    while (ptr + candPeriod < length) {
        uint8_t c1 = pattern[ptr + candPeriod];
        uint8_t c2 = pattern[maxSuffix + candPeriod];
        if (!compare(c1, c2)) {
            if (c1 == c2) {
                if (candPeriod == period) {
                    candPeriod = 1;
                    ptr += period;
                } else {
                    candPeriod++;
                }
            } else {
                period = candPeriod = 1;
                maxSuffix = ptr;
                ptr++;
            }
        } else {
            ptr += candPeriod;
            period = ptr - maxSuffix;
            candPeriod = 1;
        }
    }
    return maxSuffix;
}

static const uint8_t* twoWaySearchImpl(const uint8_t* haystack, uint32_t haystacklen, const uint8_t* needle, int32_t needlelen, uint64_t preprocessing) noexcept {
    int32_t maxSuffix = static_cast<int32_t>(preprocessing >> 32), period = static_cast<int32_t>(preprocessing);
    bool equal = period & 1;
    period >>= 1;
    int32_t pos, lastPtr = -1;
    int32_t resetPtr = needlelen - period - 1;
    uint32_t offset = 0;

    haystacklen -= needlelen;

    if (!equal) {
        while (offset <= haystacklen) {
            pos = maxSuffix + 1;
            while (pos < needlelen && needle[pos] == haystack[pos + offset]) {
                pos++;
            }
            if (pos < needlelen) {
                offset += pos - maxSuffix;
            } else {
                // match.
                pos = maxSuffix;
                while (pos >= 0 && needle[pos] == haystack[pos + offset]) {
                    pos--;
                }
                if (pos >= 0) {
                    offset += period;
                } else {
                    return haystack + offset;
                }
            }
        }
    } else {
        while (offset <= haystacklen) {
            pos = std::max(maxSuffix, lastPtr) + 1;
            while (pos < needlelen && needle[pos] == haystack[pos + offset]) {
                pos++;
            }
            if (pos < needlelen) {
                lastPtr = -1;
                offset += pos - maxSuffix;
            } else {
                // match.
                pos = maxSuffix;
                while (pos > lastPtr && needle[pos] == haystack[pos + offset]) {
                    pos--;
                }
                if (pos > lastPtr) {
                    lastPtr = resetPtr;
                    offset += period;
                } else {
                    return haystack + offset;
                }
            }
        }
    }
    return nullptr;
}

static void preparePattern(const uint8_t* pattern, int32_t len, int32_t& maxSuffix, int32_t& period) {
    // Does not preprocess for patterns of length 1.
    if (len == 1) {
        maxSuffix = 0;
        period = 0;
    } else if (len == 2) {
        // The are only two cases for a pattern of length 2.
        if (pattern[0] == pattern[1]) {
            // maxSuffix = -1, period = 3, equal = true.
            maxSuffix = -1;
            period = (3 << 1) | 1;
        } else {
            // maxSuffix = 0, period = 1, equal = false.
            maxSuffix = 0;
            period = (1 << 1) | 0;
        }
    } else {
        // store in these variables the candidates for period.
        int32_t period1, period2;
        // maxSuffix for <=.
        int32_t suffix1 = computeMaxSuffix(pattern, len, period1, [](uint8_t a, uint8_t b) -> bool { return (a < b); });

        // maxSuffix for >=.
        int32_t suffix2 = computeMaxSuffix(pattern, len, period2, [](uint8_t a, uint8_t b) -> bool { return (a > b); });

        // Store the results of preprocessing.
        maxSuffix = (suffix1 > suffix2) ? suffix1 : suffix2;
        period = (suffix1 > suffix2) ? period1 : period2;

        // 'equal' stores the repetitive comparison in the initial Two-Way algorithm.
        bool equal = !byteCmpObjects(pattern, pattern + period, maxSuffix + 1);

        if (!equal)
            period = std::max(maxSuffix + 1, len - maxSuffix - 1) + 1;
        period = (period << 1) | equal;
    }
}

static const uint8_t* twoWaySearch(const uint8_t * haystack, uint32_t searchLimit, const uint8_t* needle, uint32_t needlelen, uint64_t preprocessing) noexcept {
    if (!needlelen) {
        return haystack;
    } else if (needlelen == 1) {
        return static_cast<const uint8_t*>(memchr(haystack, needle[0], searchLimit));
    } else {
        // No out of bounds here: searchLimit = textLen - sums + 1 and 'sums' includes 'needlelen'.
        uint32_t index = 0;
        while (index < searchLimit && (needle[0] != haystack[index] || needle[1] != haystack[index + 1])) {
            index++;
        }
        return (index == searchLimit) ? nullptr : twoWaySearchImpl(haystack + index, searchLimit - index + needlelen - 1, needle, needlelen, preprocessing);
    }
}

template <bool enableSSE>
static const uint8_t* hybridStringSearch(const uint8_t* begin, const uint8_t* end, const uint8_t* pattern, uint32_t patternLen, uint32_t offset, uint64_t preprocessing) noexcept {
    uint32_t textLen = end - begin;
    #ifdef __SSE4_2__
    if constexpr (enableSSE) {
        // Use SSE instructions if the pattern is short and the text long enough for an SSE register
        if ((patternLen <= 12) && (textLen >= 16)) {
            // Load the value into an SSE register
            uint64_t rawValues[2];
            if (patternLen <= 8) {
                rawValues[0] = read8(pattern, patternLen);
                rawValues[1] = 0;
            } else {
                rawValues[0] = read8Unchecked(pattern);
                rawValues[1] = read8(pattern + 8, patternLen - 8);
            }
            auto needle = _mm_loadu_si128(reinterpretAsVector<__m128i_u>(rawValues));

            // Main check
            auto iter = begin;
            int safeMatch = 17 - patternLen;
            while ((iter + 16) <= end) {
                auto haystack = _mm_loadu_si128(reinterpretAsVector<const __m128i_u>(iter));
                int match = _mm_cmpistri(needle, haystack, _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ORDERED);
                if (match < 16) {
                    iter += match;
                    if (match < safeMatch) return iter;
                    continue;
                }
                iter += 16;
            }

            // Check the last block. This is safe because we checked the length to be >= 16
            if (iter < end) {
                auto haystack = _mm_loadu_si128(reinterpretAsVector<const __m128i_u>(end - 16));
                int match = _mm_cmpistri(needle, haystack, _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ORDERED);
                if (match < safeMatch) return end - 16 + match;
            }
            // Not found
            return nullptr;
        }
    }
    return twoWaySearch(begin, textLen - offset + 1, pattern, patternLen, preprocessing);
    #else
    return twoWaySearch(begin, textLen - offset + 1, pattern, patternLen, preprocessing);
    #endif
}


template <bool enableSSE>
class HSSDecodedMatcher {
protected:
    std::vector<uint8_t> patternProgram;
    HSSDecodedMatcher() = default;
public:
    virtual bool matchDecoded(const uint8_t* str, size_t len) = 0;
    virtual ~HSSDecodedMatcher() = default;
};

template <bool enableSSE>
class SimpleHSSDecodedMatcher: public HSSDecodedMatcher<enableSSE> {
protected:

    void buildLikeProgram(const std::vector<std::span<const uint8_t>> &patterns) {
        auto addInt = [this](uint32_t value) {
            // when writing the following three lines of code as
            //    desc.insert(desc.end(), reinterpret_cast<char*>(&value), reinterpret_cast<char*>(&value) + sizeof(uint32_t))
            // gcc-12.1 emits a stringop-overflow compiler error which seems to be a false positive
            auto oldSize = this->patternProgram.size();
            this->patternProgram.resize(oldSize + sizeof(uint32_t));
            copyRawMem(this->patternProgram.data() + oldSize, &value, sizeof(uint32_t));
        };
        auto addString = [this](const std::span<const uint8_t>& v) { this->patternProgram.insert(this->patternProgram.end(), v.data(), v.data() + v.size()); };

        unsigned size = patterns.size();
        addInt(size - 2);
        addInt(patterns.front().size());
        addInt(patterns.back().size());

        // 'sums' is the sum of the lengths.
        uint32_t sums = patterns[0].size() + patterns.back().size();
        for (unsigned index = 1, limit = size - 1; index < limit; ++index) {
            sums += patterns[index].size();
            addInt(patterns[index].size());
        }
        addInt(sums);

        // Preprocess the maxSuffix and period for every pattern.
        for (unsigned index = 1, limit = size - 1; index < limit; index++) {
            int32_t maxSuffix, period;
            int32_t length = patterns[index].size();

            preparePattern(patterns[index].data(), length, maxSuffix, period);
            uint64_t preprocessing = (static_cast<uint64_t>(static_cast<uint32_t>(maxSuffix)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(period));
            this->patternProgram.resize(this->patternProgram.size() + sizeof(uint64_t));
            unalignedStore(this->patternProgram.data() + this->patternProgram.size() - sizeof(uint64_t), preprocessing);
        }

        // Add the patterns (prefix + suffix + others)
        addString(patterns.front());
        addString(patterns.back());
        for (unsigned index = 1, limit = patterns.size() - 1; index < limit; ++index)
            addString(patterns[index]);
    }

private:
    bool likeProgramImpl(const uint8_t* data, size_t size) const noexcept {

        // Analyze the header
        auto lens = reinterpret_cast<const int32_t*>(this->patternProgram.data());
        uint32_t patternCount = *(lens++);

        // The length of text.
        uint32_t sums = lens[patternCount + 2];

        // Does not include preprocessing for prefix and suffix.
        auto patternLookup = lens + patternCount + 3;
        const uint8_t* text = this->patternProgram.data() + (patternCount + (patternCount << 1) + 4) * sizeof(int32_t);

        // Check if the match is possible.
        auto begin = data, end = begin + size;
        if ((end - begin) < sums)
            return false;

        // Check the prefix
        if (auto prefix = lens[0]; prefix) {
            if (byteCmpObjects(begin, text, prefix) != 0) return false;
            text += prefix;
            begin += prefix;
            sums -= prefix;
        }

        // Check the suffix
        if (auto suffix = lens[1]; suffix) {
            if (byteCmpObjects(end - suffix, text, suffix) != 0) return false;
            text += suffix;
            end -= suffix;
            sums -= suffix;
        }

        // Search the rest
        for (unsigned index = 0; index < patternCount; ++index) {
            uint32_t len = lens[index + 2], textLen = end - begin;

            // Check if matching is possible.
            if (textLen < sums)
                return false;

            // maxSuffix is found in 'patternLookup' on index * 2, period on index * 2 + 1.
            uint64_t preprocessing = unalignedLoad(&patternLookup[index << 1]);
            const uint8_t* sep = hybridStringSearch<enableSSE>(begin, end, text, len, sums, preprocessing);
            if (!sep)
                return false;

            // at every step, decrease 'sums' by the length of the pattern.
            begin = sep + len;
            text += len;
            sums -= len;
        }
        return true;
    }

public:
    bool matchDecoded(const uint8_t *str, size_t len) override {
        return likeProgramImpl(str, len);
    }

    SimpleHSSDecodedMatcher(const std::vector<std::span<const uint8_t>>& patterns): HSSDecodedMatcher<enableSSE>() {
        buildLikeProgram(patterns);
    }
};

[[gnu::always_inline]] static constexpr unsigned clz(uint8_t a) noexcept {
    assert(a);
    return static_cast<unsigned>(static_cast<unsigned>(__builtin_clz(a)) - (8 * (sizeof(unsigned) - sizeof(uint8_t))));
}

static inline unsigned multiByteSequenceLength(uint8_t firstByte) noexcept
// Compute the length of a multi-byte utf8 sequence from the header byte
{
    // The header has the form 1...10<bits>, where the number of 1s is the number of bytes.
    unsigned len = clz(~firstByte);
    return len ? len : 1;
}

[[maybe_unused]] static const uint8_t* moveBackwardsToCodePointBegin(const uint8_t* reader)
// Find the begin of a code point
{
    while (((*reader) & 0xC0) == 0x80)
        --reader;
    return reader;
}

template <bool enableSSE>
class UnderscoreHSSDecodedMatcher: public HSSDecodedMatcher<enableSSE> {
protected:
    void buildLikeProgramWithUnderscore(const std::vector<std::span<const uint8_t>> &patterns) {
        auto addInt = [this](uint32_t value) {
          // when writing the following three lines of code as
          //    desc.insert(desc.end(), reinterpret_cast<char*>(&value), reinterpret_cast<char*>(&value) + sizeof(uint32_t))
          // gcc-12.1 emits a stringop-overflow compiler error which seems to be a false positive
          auto oldSize = this->patternProgram.size();
          this->patternProgram.resize(oldSize + sizeof(uint32_t));
          copyRawMem(this->patternProgram.data() + oldSize, &value, sizeof(uint32_t));
        };
        auto addString = [this](const std::span<const uint8_t>& v) { this->patternProgram.insert(this->patternProgram.end(), v.data(), v.data() + v.size()); };

        unsigned size = patterns.size();
        addInt(size - 2);
        addInt(patterns.front().size());
        addInt(patterns.back().size());

        uint32_t sums = patterns.front().size() + patterns.back().size();
        for (unsigned index = 1, limit = size - 1; index < limit; ++index) {
            sums += patterns[index].size();
            addInt(patterns[index].size());
        }
        addInt(sums);

        for (unsigned index = 1, limit = size - 1; index < limit; ++index) {
            uint32_t length = patterns[index].size();

            uint32_t offsetUnderscore = 0;
            while (offsetUnderscore < length && patterns[index][offsetUnderscore] == '_')
                offsetUnderscore++;

            // Underscore occupies the entire string.
            if (offsetUnderscore == length) {
                // start position = length, len = 0, maxSuffix = 0, period + flag = 0.
                addInt(length);
                addInt(0);
                addInt(0);
                addInt(0);
            } else {
                // Preprocess for the pattern found after the leading underscores.
                uint32_t ptr = offsetUnderscore;
                while (ptr < length && patterns[index][ptr] != '_')
                    ptr++;
                addInt(offsetUnderscore);
                addInt(ptr - offsetUnderscore);

                int32_t maxSuffix, period;
                preparePattern(patterns[index].data() + offsetUnderscore, ptr - offsetUnderscore, maxSuffix, period);
                addInt(maxSuffix);
                addInt(period);
          }
       }

       // Add the patterns (prefix + suffix + others)
       addString(patterns.front());
       addString(patterns.back());
       for (unsigned index = 1, limit = patterns.size() - 1; index < limit; ++index)
          addString(patterns[index]);
    }

private:
    bool likeProgramWithUnderscores(const uint8_t* data, size_t size) const noexcept {
       auto lens = reinterpret_cast<const int32_t*>(this->patternProgram.data());
       uint32_t patternCount = *(lens++);

       // The length of text.
       uint32_t sums = lens[patternCount + 2];

       // Does not include preprocessing for prefix and suffix.
       auto patternLookup = lens + patternCount + 3;
       const uint8_t* text = this->patternProgram.data() + (patternCount + (patternCount << 2) + 4) * sizeof(int32_t);
       auto s1 = data, s2 = s1 + size;

       // Check if the match is possible.
       if ((s2 - s1) < sums) {
          return false;
       }

       // Check the prefix.
       if (lens[0]) {
          uint32_t len = lens[0];
          unsigned index = 0;
          while (index < len) {
             char c = text[index];
             if (s1 == s2)
                return false;
             if (c == '_') {
                s1 += multiByteSequenceLength(*s1);
             } else if (c != *(s1++)) {
                return false;
             }
             index++;
          }
          if (index < len)
             return false;
          text += len;
          sums -= len;
       }

       // Check the suffix.
       if (lens[1]) {
          uint32_t len = lens[1];
          if ((s2 - s1) < sums)
             return false;

          unsigned index = len;
          auto iter = s2;
          while (index > 0) {
             uint8_t c = text[index - 1];
             if (iter <= s1)
                return false;
             if (c == '_') {
                iter = moveBackwardsToCodePointBegin(iter - 1);
             } else if (c != *(--iter)) {
                return false;
             }
             index--;
          }
          if (index > 0)
             return false;
          text += len;
          s2 = iter;
          sums -= len;
       }

       for (unsigned index = 0; index < patternCount; index++) {
          uint32_t len = lens[index + 2];

          // Check first if the match is possible.
          if ((s2 - s1) < sums)
             return false;

          const uint8_t* sep;
          uint32_t beginBuffer = patternLookup[index << 2], bufferLen = patternLookup[(index << 2) + 1];

          // Get rid of leading underscores.
          for (uint32_t offsetUnderscore = beginBuffer; offsetUnderscore > 0; offsetUnderscore--) {
             if (s1 == s2)
                return false;
             s1 += multiByteSequenceLength(*s1);
          }

          uint64_t preprocessing = (static_cast<uint64_t>(static_cast<uint32_t>(patternLookup[(index << 2) + 2])) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(patternLookup[(index << 2) + 3]));
          if (beginBuffer == 0 && bufferLen == len) {
             // There are no underscores.
             sep = hybridStringSearch<enableSSE>(s1, s2, text, len, sums, preprocessing);
             if (!sep)
                return false;
             s1 = sep + len;
          } else if (beginBuffer != len) {
             s1 = likeProgramWithUnderscoreStep(s1, s2, text + beginBuffer, text + len, bufferLen, sums - beginBuffer, preprocessing);
             if (!s1)
                return false;
          }
          text += len;
          sums -= len;
       }
       return true;
    }
    static const uint8_t* likeProgramWithUnderscoreStep(const uint8_t* haystack, const uint8_t* haystackEnd, const uint8_t* pattern, const uint8_t* patternEnd, uint32_t bufferLen, uint32_t sums, uint64_t preprocessing) {

        // Repeatedly search for the prefix
        while (true) {
            checkAgain:
                if ((haystackEnd - haystack) < sums)
                    return nullptr;
            haystack = hybridStringSearch<enableSSE>(haystack, haystackEnd, pattern, bufferLen, sums, preprocessing);

            if (!haystack || ((haystackEnd - haystack) < sums))
                return nullptr;

            // Check the suffix
            auto reader = haystack + bufferLen;
            unsigned steps = 0;
            for (auto iter = pattern + bufferLen; iter != patternEnd; ++iter) {
                if (reader == haystackEnd)
                    return nullptr;
                char c = *iter;
                if (c == '_') {
                    reader += multiByteSequenceLength(*reader);
                    steps += 6; // maximum utf8 length
                    continue;
                }
                if (*reader != c) {
                    auto next = static_cast<const uint8_t*>(memchr(reader, c, haystackEnd - reader));
                    if (!next)
                        return nullptr;
                    auto cand = next - steps - bufferLen;
                    haystack = max(haystack + 1, cand);
                    goto checkAgain;
                }
                ++reader;
                ++steps;
            }

            // Found a match
            return reader;
        }
    }
public:
    bool matchDecoded(const uint8_t *str, size_t len) override {
        return likeProgramWithUnderscores(str, len);
    }

    UnderscoreHSSDecodedMatcher(const std::vector<std::span<const uint8_t>>& patterns): HSSDecodedMatcher<enableSSE>() {
        buildLikeProgramWithUnderscore(patterns);
    }
};

class HSSDecodedMatcherFactory {
public:
    template <bool enableSSE>
    static std::unique_ptr<HSSDecodedMatcher<enableSSE>> buildMatcher(const std::span<const uint8_t>& pattern) {
        auto last = pattern.begin();
        std::vector<std::span<const uint8_t>> patterns{};
        bool hasUnderscores = false;
        for (auto iter = last, limit = pattern.end(); iter != limit; ++iter) {
            char c = *iter;
            if (c == '_') {
                hasUnderscores = true;
            } else if (c == '%') {
                if (patterns.empty() || (iter != last))
                    patterns.emplace_back(last, iter - last);
                last = iter + 1;
            }
        }
        patterns.emplace_back(last, pattern.end() - last);
        if (hasUnderscores) {
            return std::make_unique<UnderscoreHSSDecodedMatcher<enableSSE>>(patterns);
        } else {
            return std::make_unique<SimpleHSSDecodedMatcher<enableSSE>>(patterns);
        }
    }
};
#endif //HYBRID_STRING_SEARCH_HPP
