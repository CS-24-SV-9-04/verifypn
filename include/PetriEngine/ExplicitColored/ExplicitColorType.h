#ifndef COLOREDPETRINETELEMENTS_H
#define COLOREDPETRINETELEMENTS_H

#include "SuccessorGenerator/IntegerPackCodec.h"

namespace PetriEngine::ExplicitColored {
    struct ColorType {
        ColorType() = delete;

        ColorType(const Color_t colorSize, std::vector<Color_t> basicColorSizes) :
            colorSize(colorSize), basicColorSizes(std::move(basicColorSizes)), colorCodec(this->basicColorSizes) {
        }

        explicit ColorType(const Color_t colorSize) :
            colorSize(colorSize), basicColorSizes(std::vector{colorSize}), colorCodec(basicColorSizes) {
        }

        void addBaseColorSize(const Color_t newBaseSize) {
            colorSize *= newBaseSize;
            basicColorSizes.push_back(newBaseSize);
            colorCodec = IntegerPackCodec{this->basicColorSizes};
        }

        Color_t colorSize{};
        std::vector<Color_t> basicColorSizes;
        IntegerPackCodec<uint64_t, Color_t> colorCodec;
    };
}
#endif //COLOREDPETRINETELEMENTS_H
