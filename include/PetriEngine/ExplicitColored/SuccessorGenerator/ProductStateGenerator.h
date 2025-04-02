#include "PetriEngine/ExplicitColored/ColoredPetriNetState.h"

namespace PetriEngine {
    namespace ExplicitColored {
        class ProductStateGenerator {
        public:
            ProductStateGenerator(const ColoredPetriNet& net, LTL::Structures::BuchiAutomaton buchiAutomaton);
            ColeredPetriNetProductState next(ColeredPetriNetProductState& currentState);
        };
    }
}