#ifndef EXPLICIT_COLORED_PASSED_LIST
#define EXPLICIT_COLORED_PASSED_LIST
#include "PetriEngine/ExplicitColored/ColoredEncoder.h"
#include <ptrie/ptrie.h>

namespace PetriEngine::ExplicitColored {
    template<typename E, typename S>
    class PassedList {
    public:
        explicit PassedList(E& encoder) : _encoder(encoder) { }

        void add(const S& state) {
            size_t size = _encoder.encode(state);
            _passed.insert(_encoder.data(), size);
        }

        //Returns true if the element exists or false if it does not exist and inserts the element
        bool existsOrAdd(const S& state) {
            const size_t size = _encoder.encode(state);
            return _passed.insert(_encoder.data(), size).first;
        }

        bool exists(const S& state) {
            const size_t size = _encoder.encode(state);
            return _passed.exists(_encoder.data(), size).first;
        }

        bool isFullStateSpace() {
            return _encoder.isFullStateSpace();
        }

        size_t getBiggestEncoding() {
            return _encoder.getBiggestEncoding();
        }
    private:
        ptrie::set<uint8_t> _passed;
        E& _encoder;
    };
}

#endif