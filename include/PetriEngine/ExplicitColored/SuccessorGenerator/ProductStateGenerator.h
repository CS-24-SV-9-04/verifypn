#include <LTL/Structures/BuchiAutomaton.h>
#include <PetriEngine/ExplicitColored/ColoredPetriNet.h>
#include <PetriEngine/ExplicitColored/ExpressionCompilers/ExplicitQueryPropositionCompiler.h>

#include "ColoredSuccessorGenerator.h"
#include "PetriEngine/ExplicitColored/ColoredPetriNetState.h"

namespace PetriEngine {
    namespace ExplicitColored {
        template<typename SuccessorGeneratorState>
        class ProductStateGenerator {
        public:
            ProductStateGenerator(
                const ColoredPetriNet& net,
                LTL::Structures::BuchiAutomaton buchiAutomaton,
                const std::unordered_map<std::string, uint32_t>& placeNameIndices,
                const std::unordered_map<std::string, uint32_t>& transitionNameIndices
            ) : _net(net), _buchiAutomaton(std::move(buchiAutomaton)), _successorGenerator(net, -1) {
                const ExplicitQueryPropositionCompiler compiler(placeNameIndices, transitionNameIndices, _successorGenerator);
                for (const auto& [index, atomicProposition] : _buchiAutomaton.ap_info()) {
                    _compiledAtomicPropositions.emplace(index, compiler.compile(atomicProposition._expression));
                }
            }

            CPNProductState next(CPNProductStateSpecialized<SuccessorGeneratorState>& currentState) {
                if (currentState.getIterState() == nullptr) {
                    const auto state = _buchiAutomaton.buchi().state_from_number(currentState.getBuchiState());
                    currentState.setIterState(std::unique_ptr<spot::twa_succ_iterator, BuchiStateIterDeleter>(
                        _buchiAutomaton.buchi().succ_iter(state),
                        BuchiStateIterDeleter{ &_buchiAutomaton.buchi() }
                    ));
                    state->destroy();
                    currentState.getIterState()->first();
                    currentState.setSuccessorMarking(_successorGenerator.next(currentState.getSuccessorGeneratorState()).first.marking);
                }
                if (currentState.getSuccessorGeneratorState().isDeadlock()) {
                    for (; !currentState.getIterState()->done(); currentState.getIterState()->next()) {
                        if (_check_condition(currentState.getIterState()->cond(), currentState.getSuccessorGeneratorState().marking, currentState.getSuccessorGeneratorState().id)) {
                            const auto dstState = currentState.getIterState()->dst();
                            auto buchiState = _buchiAutomaton.buchi().state_number(dstState);
                            dstState->destroy();
                            currentState.getIterState()->next();
                            return { currentState.getSuccessorGeneratorState().marking, buchiState };
                        }
                    }
                } else {
                    while (!currentState.getSuccessorGeneratorState().done()) {
                        for (; !currentState.getIterState()->done(); currentState.getIterState()->next()) {
                            if (_check_condition(currentState.getIterState()->cond(), currentState.getSuccessor().marking, currentState.getSuccessorGeneratorState().id)) {
                                const auto dstState = currentState.getIterState()->dst();
                                currentState.setBuchiState(_buchiAutomaton.buchi().state_number(dstState));
                                dstState->destroy();
                                currentState.getIterState()->next();
                                return currentState.getSuccessor();
                            }
                        }
                        auto [nextSuccessorGeneratorMarking, traceStep] = _successorGenerator.next(currentState.getSuccessorGeneratorState());
                        if constexpr (std::is_same_v<SuccessorGeneratorState, ColoredPetriNetStateEven>) {
                            if (currentState.getSuccessorGeneratorState().shuffle){
                                nextSuccessorGeneratorMarking.shuffle = false;
                                continue;
                            }
                        }
                        currentState.setSuccessorMarking(std::move(nextSuccessorGeneratorMarking.marking));
                        currentState.getIterState()->first();
                    }
                }
                currentState.setDone();
                return {{}, 0};
            }

            std::vector<CPNProductState> get_initial_states(const ColoredPetriNetMarking& initialMarking) const {
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

            bool has_invariant_self_loop(const CPNProductState& state) const {
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

        private:
            bool _check_condition(bdd cond, const ColoredPetriNetMarking& marking, size_t markingId) const {
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

            const ColoredPetriNet& _net;
            LTL::Structures::BuchiAutomaton _buchiAutomaton;
            ColoredSuccessorGenerator _successorGenerator;
            std::unordered_map<int, std::unique_ptr<ExplicitQueryProposition>> _compiledAtomicPropositions;
        };
    }
}