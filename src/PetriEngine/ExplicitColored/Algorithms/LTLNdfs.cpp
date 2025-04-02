#include "PetriEngine/ExplicitColored/Algorithms/LTLNdfs.h"

#include <LTL/LTLToBuchi.h>
#include "LTL/Structures/BuchiAutomaton.h"

namespace PetriEngine::ExplicitColored {
    LTLNdfs::LTLNdfs(
        const ColoredPetriNet& net,
        const PQL::Condition_ptr& condition,
        const std::unordered_map<std::string, uint32_t>& placeNameIndices,
        const std::unordered_map<std::string, Transition_t>& transitionNameIndices
        )
    : _net(net), _placeNameIndices(placeNameIndices), _transitionNameIndices(transitionNameIndices) {
        _buchiAutomaton = make_buchi_automaton(condition, LTL::BuchiOptimization::Low, LTL::APCompression::None);
    }

    bool LTLNdfs::check() {
    }
}
