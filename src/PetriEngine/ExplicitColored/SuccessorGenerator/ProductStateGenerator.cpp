#include "PetriEngine/ExplicitColored/SuccessorGenerator/ProductStateGenerator.h"
#include "LTL/SuccessorGeneration/BuchiSuccessorGenerator.h"
namespace PetriEngine::ExplicitColored {
    ProductStateGenerator::ProductStateGenerator(
        const ColoredPetriNet &net,
        LTL::Structures::BuchiAutomaton buchiAutomaton,
        const std::unordered_map<std::string, uint32_t>& placeNameIndices,
        const std::unordered_map<std::string, uint32_t>& transitionNameIndices
    ) : _net(net), _buchiAutomaton(std::move(buchiAutomaton)), _successorGenerator(net, std::numeric_limits<Binding_t>::max()) {
        const ExplicitQueryPropositionCompiler compiler(placeNameIndices, transitionNameIndices, _successorGenerator);
        for (const auto& [index, atomicProposition] : _buchiAutomaton.ap_info()) {
            _compiledAtomicPropositions.emplace(index, compiler.compile(atomicProposition._expression));
        }
    }

    CPNProductState ProductStateGenerator::next(
        CPNProductStateFixed &currentState
    ) const {
        if (currentState._iterState == nullptr) {
            const auto state = _buchiAutomaton.buchi().state_from_number(currentState._buchiState);
            currentState._iterState = std::unique_ptr<spot::twa_succ_iterator, BuchiStateIterDeleter>(
                _buchiAutomaton.buchi().succ_iter(state),
                BuchiStateIterDeleter{ &_buchiAutomaton.buchi() }
            );
            state->destroy();
            currentState._iterState->first();
            currentState._currentSuccessor.marking = _successorGenerator.next(currentState._markingGeneratorState).first.marking;
        }
        if (currentState._markingGeneratorState.isDeadlock()) {
            for (; !currentState._iterState->done(); currentState._iterState->next()) {
                if (_check_condition(currentState._iterState->cond(), currentState._markingGeneratorState.marking, currentState._markingGeneratorState.id)) {
                    const auto dstState = currentState._iterState->dst();
                    auto buchiState = _buchiAutomaton.buchi().state_number(dstState);
                    dstState->destroy();
                    currentState._iterState->next();
                    return { currentState._markingGeneratorState.marking, buchiState };
                }
            }
        } else {
            while (!currentState._markingGeneratorState.done()) {
                for (; !currentState._iterState->done(); currentState._iterState->next()) {
                    if (_check_condition(currentState._iterState->cond(), currentState._markingGeneratorState.marking, currentState._markingGeneratorState.id)) {
                        const auto dstState = currentState._iterState->dst();
                        currentState._currentSuccessor.buchiState = _buchiAutomaton.buchi().state_number(dstState);
                        dstState->destroy();
                        currentState._iterState->next();
                        return currentState._currentSuccessor;
                    }
                }
                currentState._currentSuccessor.marking = _successorGenerator.next(currentState._markingGeneratorState).first.marking;
                currentState._iterState->first();
            }
        }
        currentState._done = true;
        return {{}, 0};
    }

    std::vector<CPNProductState> ProductStateGenerator::get_initial_states(
        const ColoredPetriNetMarking &initialMarking) const {
        const auto initBuchiState = _buchiAutomaton.buchi().get_init_state();
        std::vector<CPNProductState> initialStates;
        auto iter = _buchiAutomaton.buchi().succ_iter(initBuchiState);
        if (iter->first()) {
            do {
                if (_check_condition(iter->cond(), initialMarking, 0)) {
                    auto buchiState = iter->dst();
                    CPNProductState state(initialMarking, _buchiAutomaton.buchi().state_number(buchiState));
                    buchiState->destroy();
                    initialStates.push_back(std::move(state));
                }
            } while (iter->next());
        }
        initBuchiState->destroy();
        return initialStates;
    }

    bool ProductStateGenerator::has_invariant_self_loop(const CPNProductState &state) const {
        const auto buchiState = _buchiAutomaton.buchi().state_from_number(state.buchiState);
        const auto iter = std::unique_ptr<spot::twa_succ_iterator, BuchiStateIterDeleter>(
                _buchiAutomaton.buchi().succ_iter(buchiState),
                BuchiStateIterDeleter{ &_buchiAutomaton.buchi() }
            );
        buchiState->destroy();
        if (iter->first()) {
            do {
                const auto dst = iter->dst();
                const auto dst_number = _buchiAutomaton.buchi().state_number(dst);
                dst->destroy();
                if (iter->cond() == bddtrue && dst_number == state.buchiState) {
                    return true;
                }
            } while (iter->next());
        }
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
