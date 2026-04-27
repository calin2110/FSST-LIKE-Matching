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

#ifndef ENCODER_HPP
#define ENCODER_HPP
#include <bit>
#include <array>
#include <optional>
#include <memory>
#include <span>
#include <fsst/libfsst.hpp>

class Encoder;

struct uint256_t {
    uint64_t bytes[4];

    uint256_t() = default;

    uint256_t(const uint256_t& other) = default;

    uint256_t& operator=(const uint256_t& other) = default;

    bool operator==(const uint256_t& other) const {
        return bytes[0] == other.bytes[0] && bytes[1] == other.bytes[1] && bytes[2] == other.bytes[2] && bytes[3] == other.bytes[3];
    }

    bool operator!=(const uint256_t& other) const {
        return !(*this == other);
    }

    uint16_t countl_zero() const {
        if (bytes[0] != 0) {
            return std::countl_zero(bytes[0]);
        }
        if (bytes[1] != 0) {
            return 64 + std::countl_zero(bytes[1]);
        }
        if (bytes[2] != 0) {
            return 128 + std::countl_zero(bytes[2]);
        }
        return 192 + std::countl_zero(bytes[3]);
    }

    void flipBit(uint8_t index) {
        uint8_t byte = index / 64;
        uint64_t mask = static_cast<uint64_t>(1) << (63 - index % 64);
        bytes[byte] ^= mask;
    }
};

class Bitmap {
private:
     uint256_t bitmap;

public:
    Bitmap() = default;

    [[nodiscard]] std::array<std::optional<uint8_t>, 3> getExactPrefixMatches(const std::span<const uint8_t> &string, size_t current, const Encoder& encoder) const;

    struct BitmapIterator {
    private:
        uint256_t bitmap;

        explicit BitmapIterator(const Bitmap& bitmap): bitmap(bitmap.bitmap) {}

        void advance();

    public:
        using iterator_category = std::forward_iterator_tag;

        ~BitmapIterator() = default;

        static BitmapIterator begin(const Bitmap& bitmap);

        static BitmapIterator end();

        uint8_t operator->() const {
            return bitmap.countl_zero();
        }

        uint8_t operator*() const {
            return bitmap.countl_zero();
        }

        BitmapIterator& operator++() {
            advance();
            return *this;
        }

        BitmapIterator operator++(int) {
            BitmapIterator prev(*this);
            ++(*this);
            return prev;
        }

        bool operator==(const BitmapIterator& other) const {
            return bitmap == other.bitmap;
        }

        bool operator!=(const BitmapIterator& other) const {
            return !(*this == other);
        }
    };

    [[nodiscard]] BitmapIterator begin() const {
        return BitmapIterator::begin(*this);
    }

    [[nodiscard]] BitmapIterator end() const {
        return BitmapIterator::end();
    }
    friend class Encoder;
};

class SymbolTable {
private:
    std::shared_ptr<libfsst::SymbolTable> symTable;

public:
    explicit SymbolTable(std::shared_ptr<libfsst::SymbolTable>& symTable);
    static SymbolTable readFromFile(const char* path);
    [[nodiscard]] std::shared_ptr<libfsst::SymbolTable> getSymTable() const;
};

class Encoder {
private:
    std::unique_ptr<libfsst::Encoder> encoder;

public:
    explicit Encoder(const SymbolTable& symbolTable) {
        encoder = std::make_unique<libfsst::Encoder>();
        encoder->symbolTable = symbolTable.getSymTable();
    }

    [[nodiscard]] Bitmap createPrefixBitmap(uint8_t byte) const;

    [[nodiscard]] Bitmap createAnywhereBitmap(uint8_t byte) const;

    bool isEncodingValid(const libfsst::u8* strOut, size_t lenOut) const;

    [[nodiscard]] std::vector<std::vector<uint8_t>> findAllSymbolsWithSuffix(const std::span<const uint8_t>& match, uint8_t max_n) const;

    [[nodiscard]] bool isEscapable(uint8_t byte) const;

    [[nodiscard]] const libfsst::Symbol* symbols() const {
        return encoder->symbolTable->symbols;
    }

    [[nodiscard]] uint8_t nSymbols() const {
        return encoder->symbolTable->nSymbols;
    }

    [[nodiscard]] shared_ptr<libfsst::SymbolTable> getSymbolTable() const {
        return encoder->symbolTable;
    }

    void encode(size_t lenIn, const libfsst::u8* strStart, size_t strOutSize, libfsst::u8* buffer, size_t* lenOut, libfsst::u8** strOut) const {
        fsst_compress(reinterpret_cast<fsst_encoder_t*>(encoder.get()), 1, &lenIn, &strStart, strOutSize, buffer, lenOut, strOut);
    }

    friend class Decoder;
};

class Decoder {
private:
    fsst_decoder_t decoder;

public:
    explicit Decoder(const Encoder& encoder): decoder(fsst_decoder(reinterpret_cast<fsst_encoder_t*>(encoder.encoder.get()))) {}

    std::basic_string<uint8_t> decode(size_t lenIn, const libfsst::u8* strIn) {
        size_t size = 8 * lenIn;
        std::basic_string<uint8_t> result(size, 0);
        size_t endSize = fsst_decompress(&decoder, lenIn, strIn, size, result.data());
        result.resize(endSize);
        return result;
    }
};

#endif //ENCODER_HPP
