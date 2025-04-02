#ifndef LTLNDFS_H
#define LTLNDFS_H
#include <LTL/Structures/BuchiAutomaton.h>
#include <PetriEngine/ExplicitColored/AtomicTypes.h>
#include <PetriEngine/PQL/PQL.h>
#include <PetriEngine/ExplicitColored/ColoredPetriNet.h>
#include "PetriEngine/ExplicitColored/SuccessorGenerator/ProductStateGenerator.h"

namespace PetriEngine::ExplicitColored {
    class LTLNdfs {
    public:
        LTLNdfs(
            const ColoredPetriNet& net,
            const PQL::Condition_ptr& condition,
            const std::unordered_map<std::string, uint32_t>& placeNameIndices,
            const std::unordered_map<std::string, Transition_t>& transitionNameIndices
        );
        bool check();
    private:
        LTL::Structures::BuchiAutomaton _buchiAutomaton;
        const ColoredPetriNet& _net;
        const std::unordered_map<std::string, uint32_t>& _placeNameIndices;
        const std::unordered_map<std::string, Transition_t>& _transitionNameIndices;
    };
}

#endif //LTLNDFS_H