#ifndef NULL_BUFFER_H
#define NULL_BUFFER_H
#include <streambuf>

class NullBuffer final : public std::streambuf
{
public:
    int overflow(const int c) override { return c; }
};

class NullStream : public std::ostream {
public:
    NullStream() : std::ostream(&_nullBuffer) {}
private:
    NullBuffer _nullBuffer;
};

#endif