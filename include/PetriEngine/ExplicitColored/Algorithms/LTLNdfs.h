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
        bool isPassed(const ColoredPetriNetProductState& state){
            for (const auto& s:_localPassed) {
                if (state.getBuchiState() == s.getBuchiState() && state.marking == s.marking){
                    return true;
                }
            }
            return false;
        }
    private:
        LTL::Structures::BuchiAutomaton _buchiAutomaton;
        const ColoredPetriNet& _net;
        const std::unordered_map<std::string, uint32_t>& _placeNameIndices;
        const std::unordered_map<std::string, Transition_t>& _transitionNameIndices;

        std::vector<ColoredPetriNetProductState> _waiting;
        std::list<ColoredPetriNetProductState> _globalPassed;
        std::list<ColoredPetriNetProductState> _localPassed;
        bool dfs(ProductStateGenerator,  ColoredPetriNetProductState);
        bool ndfs(ProductStateGenerator, ColoredPetriNetProductState);
    };
}

#endif //LTLNDFS_H