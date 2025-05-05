//
// Created by kira on 5/5/25.
//

#ifndef VERIFYPN_PRODUCTCOLORENCODER_H
#define VERIFYPN_PRODUCTCOLORENCODER_H

#include "ColoredPetriNetMarking.h"
#include "ColoredEncoder.h"
#include "ColoredPetriNetState.h"

namespace PetriEngine::ExplicitColored {

    class ProductColorEncoder{
    public:
        size_t encode(std::pair<ColoredPetriNetMarking, size_t> state){
            const auto& marking = state.first;
            size_t buchiState = state.second;
            _coloredEncoder._productColorOffset = 0;
            _coloredEncoder._isProductColor = true;
            auto buchiTypeSize = _coloredEncoder._convertToTypeSize(buchiState);
            _coloredEncoder._writeToPad(buchiTypeSize, EIGHT, _coloredEncoder._productColorOffset);
            _coloredEncoder._writeToPad(buchiState, buchiTypeSize, _coloredEncoder._productColorOffset);

            _coloredEncoder._productColorOffset += _coloredEncoder.encode(marking);

            if (_coloredEncoder._productColorOffset > UINT16_MAX){
                //If too big for representation partial statespace will be explored
                if (_coloredEncoder._fullStatespace) {
                    _coloredEncoder._biggestRepresentation = UINT16_MAX;
                    std::cout << "State with size: " << _coloredEncoder._productColorOffset <<
                              " cannot be represented correctly, so full statespace is not explored " << std::endl;
                }
                _coloredEncoder._fullStatespace = false;
                return UINT16_MAX;
            }
            _coloredEncoder._biggestRepresentation = std::max(_coloredEncoder._productColorOffset, _coloredEncoder._biggestRepresentation);
            _coloredEncoder._isProductColor = false;
            return _coloredEncoder._productColorOffset;

        }

        std::pair<ColoredPetriNetMarking, size_t> decode (const unsigned char* encoding){
            _coloredEncoder._productColorOffset = 0;
            _coloredEncoder._isProductColor = true;
            size_t buchiState;
            ColoredPetriNetMarking marking {};
            const auto buchiStateSize = static_cast<TYPE_SIZE>(_coloredEncoder._readFromEncoding(encoding, EIGHT, _coloredEncoder._productColorOffset));
            buchiState = _coloredEncoder._readFromEncoding(encoding, buchiStateSize, _coloredEncoder._productColorOffset);
            marking = _coloredEncoder.decode(encoding);
            _coloredEncoder._isProductColor = false;
            return {marking, buchiState};
        }

        [[nodiscard]] const uint8_t* data() const {
            return _coloredEncoder.data();
        }

        [[nodiscard]] bool isFullStateSpace() const {
            return _coloredEncoder.isFullStateSpace();
        }

        size_t getBiggestEncoding() const {
            return _coloredEncoder.getBiggestEncoding();
        }
    private:
        ColoredEncoder _coloredEncoder;


    };
}


#endif //VERIFYPN_PRODUCTCOLORENCODER_H
