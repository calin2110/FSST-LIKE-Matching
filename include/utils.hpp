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

#ifndef FSST_LIKE_MATCHING_UTILS_HPP
#define FSST_LIKE_MATCHING_UTILS_HPP

#include <span>
#include <memory>

template <typename T>
T loadUnaligned(const uint8_t* data) {
    T t;
    __builtin_memcpy(&t, data, sizeof(T));
    return t;
}

namespace file {
    class FileData {
    protected:
        size_t n;
        size_t totalLen;

    public:
        FileData(size_t totalLen, size_t n);
        [[nodiscard]] virtual const size_t* getIndexes() const = 0;
        [[nodiscard]] virtual const uint8_t* getBuffer() const = 0;
        [[nodiscard]] size_t getNumElements() const;
        [[nodiscard]] size_t getTotalLength() const;
        [[nodiscard]] virtual std::span<const uint8_t> get(size_t index) = 0;
        virtual ~FileData() = default;
    };

    class ASCIIFileData: public FileData {
    private:
        std::unique_ptr<uint8_t[]> buffer;
        std::unique_ptr<size_t[]> indexes;
    public:
        ASCIIFileData(size_t totalLength, size_t n, std::unique_ptr<unsigned char[]>& buffer, std::unique_ptr<size_t[]>& indexes);
        [[nodiscard]] const size_t* getIndexes() const override;
        [[nodiscard]] const uint8_t* getBuffer() const override;
        [[nodiscard]] std::span<const uint8_t> get(size_t index) override;
    };

    class BinaryFileData: public FileData {
    private:
        int handle;
        uint8_t* data;
        size_t size;

    public:
        BinaryFileData(int handle, uint8_t *data, size_t size);
        [[nodiscard]] const size_t* getIndexes() const override;
        [[nodiscard]] const uint8_t* getBuffer() const override;
        [[nodiscard]] std::span<const uint8_t> get(size_t index) override;
        ~BinaryFileData() override;
    };

    void writeChunks(int fd, const void* buffer, size_t totalLen);
    void readChunks(int fd, void* buffer, size_t totalLen);
    void dumpBinary(const char* path, size_t n, size_t totalLen, const size_t* lenOut, const uint8_t* buffer);
    std::unique_ptr<FileData> readASCIIFileData(const char* path);
    std::unique_ptr<FileData> readBinaryFileData(const char* path, int mmapFlags=0);
}
#endif //FSST_LIKE_MATCHING_UTILS_HPP