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

    ColoredPetriNetProductStateFixed ProductStateGenerator::next(
        ColoredPetriNetProductStateFixed &currentState
    ) const {
        if (currentState.iterState == nullptr) {
            const auto state = _buchiAutomaton.buchi().state_from_number(currentState.getBuchiState());
            currentState.iterState = std::unique_ptr<spot::twa_succ_iterator, BuchiStateIterDeleter>(_buchiAutomaton.buchi().succ_iter(state), BuchiStateIterDeleter{ &_buchiAutomaton.buchi() });
            state->destroy();
            currentState.currentSuccessor = _successorGenerator.next(currentState);
            currentState.iterState->first();
        }
        if (currentState.isDeadlock()) {
            for (; !currentState.iterState->done(); currentState.iterState->next()) {
                if (_check_condition(currentState.iterState->cond(), currentState.marking, currentState.id)) {
                    const auto dstState = currentState.iterState->dst();
                    ColoredPetriNetProductStateFixed newState(currentState.marking, _buchiAutomaton.buchi().state_number(dstState));
                    dstState->destroy();
                    currentState.iterState->next();
                    return newState;
                }
            }
        } else {
            while (!currentState.done() || currentState.isDeadlock()) {
                for (; !currentState.iterState->done(); currentState.iterState->next()) {
                    if (_check_condition(currentState.iterState->cond(), currentState.currentSuccessor.marking, currentState.currentSuccessor.id)) {
                        const auto dstState = currentState.iterState->dst();
                        ColoredPetriNetProductStateFixed newState(currentState.currentSuccessor, _buchiAutomaton.buchi().state_number(dstState));
                        dstState->destroy();
                        currentState.iterState->next();
                        return newState;
                    }
                }
                currentState.currentSuccessor = _successorGenerator.next(currentState);
                currentState.iterState->first();
            }
        }
        currentState.setDone();
        return {{}, 0};
    }

    std::vector<ColoredPetriNetProductStateFixed> ProductStateGenerator::get_initial_states(
        const ColoredPetriNetMarking &initialMarking) const {
        const auto initBuchiState = _buchiAutomaton.buchi().get_init_state();
        std::vector<ColoredPetriNetProductStateFixed> initialStates;
        auto iter = _buchiAutomaton.buchi().succ_iter(initBuchiState);
        if (iter->first()) {
            do {
                if (_check_condition(iter->cond(), initialMarking, 0)) {
                    auto buchiState = iter->dst();
                    ColoredPetriNetProductStateFixed state(initialMarking, _buchiAutomaton.buchi().state_number(buchiState));
                    buchiState->destroy();
                    initialStates.push_back(std::move(state));
                }
            } while (iter->next());
        }
        initBuchiState->destroy();
        return initialStates;
    }

    bool ProductStateGenerator::has_invariant_self_loop(const ColoredPetriNetProductStateFixed &state) const {
        auto buchi_state = _buchiAutomaton.buchi().state_from_number(state.getBuchiState());
        auto iter =_buchiAutomaton.buchi().succ_iter(buchi_state);
        if (iter->first()) {
            do {
                if (iter->cond() == bddtrue) {
                    buchi_state->destroy();
                    _buchiAutomaton.buchi().release_iter(iter);
                    return true;
                }
            } while (iter->next());
        }
        buchi_state->destroy();
        _buchiAutomaton.buchi().release_iter(iter);
        return false;
    }

    bool ProductStateGenerator::_check_condition(bdd cond, const ColoredPetriNetMarking &marking, size_t markingId) const {
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
