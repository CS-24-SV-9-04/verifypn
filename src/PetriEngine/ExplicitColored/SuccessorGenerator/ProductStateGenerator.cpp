#include "PetriEngine/ExplicitColored/SuccessorGenerator/ProductStateGenerator.h"
#include "LTL/SuccessorGeneration/BuchiSuccessorGenerator.h"
namespace PetriEngine::ExplicitColored {
    ProductStateGenerator::ProductStateGenerator(
        const ColoredPetriNet &net,
        LTL::Structures::BuchiAutomaton buchiAutomaton,
        const std::unordered_map<std::string, uint32_t>& placeNameIndices,
        const std::unordered_map<std::string, uint32_t>& transitionNameIndices
    ) : _net(net), _buchiAutomaton(std::move(buchiAutomaton)), _successorGenerator(net) {
        GammaQueryCompiler compiler(placeNameIndices, transitionNameIndices, _successorGenerator);
        for (const auto& [index, atomicProposition] : _buchiAutomaton.ap_info()) {
            _compiledAtomicPropositions.emplace(index, compiler.compile(atomicProposition._expression));
        }
    }

    ColoredPetriNetProductState ProductStateGenerator::next(
        ColoredPetriNetProductState &currentState
    ) {
        if (currentState.iterState == nullptr) {
            const auto state = _buchiAutomaton.buchi().state_from_number(currentState.getBuchiState());
            currentState.iterState = std::unique_ptr<spot::twa_succ_iterator, BuchiStateIterDeleter>(_buchiAutomaton.buchi().succ_iter(state), BuchiStateIterDeleter{ &_buchiAutomaton.buchi() });
            state->destroy();
            currentState.currentSuccessor = _successorGenerator.next(currentState);
            currentState.iterState->first();
        }
        while (!currentState.done()) {
            for (; !currentState.iterState->done(); currentState.iterState->next()) {
                if (_check_condition(currentState.iterState->cond(), currentState.currentSuccessor.marking, currentState.currentSuccessor.id)) {
                    auto dstState = currentState.iterState->dst();
                    ColoredPetriNetProductState newState(currentState.currentSuccessor, _buchiAutomaton.buchi().state_number(dstState));
                    dstState->destroy();
                    currentState.iterState->next();
                    return newState;
                }
            }
            currentState.currentSuccessor = _successorGenerator.next(currentState);
            currentState.iterState->first();
        }
        currentState.setDone();
        return {{}, 0};
    }

    bool ProductStateGenerator::_check_condition(bdd cond, const ColoredPetriNetMarking &marking, size_t markingId) {
        while (cond.id() > 1) {
            auto varIndex = bdd_var(cond);
            const auto& ap = _compiledAtomicPropositions.at(varIndex);
            if (ap->eval(_successorGenerator, marking, markingId)) {
                cond = bdd_high(cond);
            } else {
                cond = bdd_low(cond);
            }
        }
        return cond == bddtrue;
    }
}
