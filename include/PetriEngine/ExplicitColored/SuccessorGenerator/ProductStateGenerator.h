#include <LTL/Structures/BuchiAutomaton.h>
#include <PetriEngine/ExplicitColored/ColoredPetriNet.h>
#include <PetriEngine/ExplicitColored/ExpressionCompilers/ExplicitQueryPropositionCompiler.h>

#include "ColoredSuccessorGenerator.h"
#include "PetriEngine/ExplicitColored/ColoredPetriNetState.h"

namespace PetriEngine {
    namespace ExplicitColored {
        class ProductStateGenerator {
        public:
            ProductStateGenerator(
                const ColoredPetriNet& net,
                LTL::Structures::BuchiAutomaton buchiAutomaton,
                const std::unordered_map<std::string, uint32_t>& placeNameIndices,
                const std::unordered_map<std::string, uint32_t>& transitionNameIndices
            );
            CPNProductState next(CPNProductStateFixed& currentState) const;
            std::vector<CPNProductState> get_initial_states(const ColoredPetriNetMarking& initialMarking) const;
            bool has_invariant_self_loop(const CPNProductState& state) const;
        private:
            bool _check_condition(bdd cond, const ColoredPetriNetMarking& marking, size_t markingId) const;
            const ColoredPetriNet& _net;
            LTL::Structures::BuchiAutomaton _buchiAutomaton;
            ColoredSuccessorGenerator _successorGenerator;
            std::unordered_map<int, std::unique_ptr<ExplicitQueryProposition>> _compiledAtomicPropositions;
        };
    }
}