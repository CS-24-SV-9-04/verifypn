#ifndef FIREABILITYCHECKER_H
#define FIREABILITYCHECKER_H

#include "SuccessorGenerator/ColoredSuccessorGenerator.h"
#include "ColoredPetriNetMarking.h"
#include "ColoredPetriNet.h"
#include "Binding.h"

namespace PetriEngine::ExplicitColored {
    class FireabilityChecker {
    public:
        static bool canFire(const ColoredSuccessorGenerator& successorGenerator, const Transition_t tid, const ColoredPetriNetMarking& state, const size_t id) {
            Binding binding;
            const auto totalBindings = successorGenerator.net()._transitions[tid].totalBindings;
            return successorGenerator.findNextValidBinding(state, tid, 0, totalBindings, binding, id) != std::numeric_limits<Binding_t>::max();
        }

        //This is no good, but we do not really have deadlock queries
        static bool hasDeadlock (const ColoredSuccessorGenerator& successorGenerator, const ColoredPetriNetMarking& state, const size_t id) {
            const auto transitionCount = successorGenerator.net().getTransitionCount();
            for (Transition_t tid = 0; tid < transitionCount; tid++) {
                if (canFire(successorGenerator, tid, state, id)) {
                    return false;
                }
            }
            return true;
        }
    };
}

#endif //FIREABILITYCHECKER_H
