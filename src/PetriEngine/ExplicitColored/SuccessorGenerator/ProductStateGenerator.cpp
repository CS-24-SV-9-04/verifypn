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

    ColeredPetriNetProductState ProductStateGenerator::next(
        ColeredPetriNetProductState &currentState
    ) {
        while (!currentState.done()) {
            if (currentState.iterState != nullptr) {
                currentState.iterState->next();

                for (currentState.iterState->first(); !currentState.iterState->done(); currentState.iterState->next())
                {
                    auto cond = currentState.iterState->cond();
                    while (cond.id() > 1) {
                        auto varIndex = bdd_var(cond);
                        const auto& ap = _compiledAtomicPropositions.at(varIndex);
                        auto res = ap->eval(_successorGenerator, currentState.marking, currentState.id);
                        if (res) {
                            cond = bdd_high(cond);
                        } else {
                            cond = bdd_low(cond);
                        }
                    }
                    //check condition and yield
                    if (cond == bddtrue) {
                        auto dstState = currentState.iterState->dst();
                        ColeredPetriNetProductState newState(currentState.currentSuccessor, _buchiAutomaton.buchi().state_number(dstState));
                        dstState->destroy();
                        return newState;
                    }
                }
            } else {
                auto state = _buchiAutomaton.buchi().state_from_number(currentState.getBuchiState());
                currentState.iterState = std::unique_ptr<spot::twa_succ_iterator, BuchiStateIterDeleter>(_buchiAutomaton.buchi().succ_iter(state), BuchiStateIterDeleter{ &_buchiAutomaton.buchi() });
                state->destroy();
            }

            auto nextState = _successorGenerator.next(currentState);
            currentState.currentSuccessor = nextState;
            if (!currentState.done()) {
                currentState.iterState->first();
            }
        }
    }
}