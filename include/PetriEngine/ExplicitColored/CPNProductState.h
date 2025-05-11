#ifndef CPNPRODUCTSTATE
#define CPNPRODUCTSTATE
#include <memory>
#include "ColoredPetriNetMarking.h"

namespace PetriEngine::ExplicitColored {
    class CPNProductState {
    public:
        CPNProductState(ColoredPetriNetMarking marking, size_t buchiState)
            : marking(std::move(marking)), buchiState(buchiState) {}

        bool operator==(const CPNProductState &other) const {
            return other.buchiState == buchiState && other.marking == marking;
        }

        ColoredPetriNetMarking marking;
        size_t buchiState;
    };
};

#endif