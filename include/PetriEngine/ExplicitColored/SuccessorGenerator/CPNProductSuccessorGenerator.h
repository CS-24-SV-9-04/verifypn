#ifndef CPNPRODUCTSUCCESSORGENERATOR_H
#define CPNPRODUCTSUCCESSORGENERATOR_H
#include <LTL/Structures/BuchiAutomaton.h>
#include <PetriEngine/ExplicitColored/ColoredPetriNet.h>
#include <PetriEngine/ExplicitColored/ExpressionCompilers/ExplicitQueryPropositionCompiler.h>

#include "ColoredSuccessorGenerator.h"
#include "PetriEngine/ExplicitColored/ColoredPetriNetState.h"

namespace PetriEngine {
    namespace ExplicitColored {
        template<typename SuccInfo>
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

            bool next(CPNProductState& state, CPNProductSuccessorInfo<SuccInfo>& succInfo) {
                bool deadlock = false;
                if (succInfo.iterState == nullptr) {
                    const auto buchiState = _buchiAutomaton.buchi().state_from_number(state.buchiState);
                    succInfo.iterState = std::unique_ptr<spot::twa_succ_iterator, BuchiStateIterDeleter>(
                        _buchiAutomaton.buchi().succ_iter(buchiState),
                        BuchiStateIterDeleter{ &_buchiAutomaton.buchi() }
                    );
                    buchiState->destroy();
                    succInfo.iterState->first();
                    succInfo.currentSuccessorMarking = std::move(state.marking);
                    std::optional<TraceMapStep> step = _successorGenerator.next(succInfo.currentSuccessorMarking, succInfo.markingSuccInfo, 0);
                    deadlock = !step.has_value();
                }
                if (deadlock) {
                    for (; !succInfo.iterState->done(); succInfo.iterState->next()) {
                        if (_check_condition(succInfo.iterState->cond(), state.marking, 0)) {
                            const auto dstState = succInfo.iterState->dst();
                            state.buchiState = _buchiAutomaton.buchi().state_number(dstState);
                            state.accepting = _buchiAutomaton.buchi().state_is_accepting(dstState);
                            dstState->destroy();
                            succInfo.iterState->next();
                            return true;
                        }
                    }
                } else {
                    while (true) {
                        for (;!succInfo.iterState->done(); succInfo.iterState->next()) {
                            if (_check_condition(succInfo.iterState->cond(), succInfo.currentSuccessorMarking, 0)) {
                                const auto dstState = succInfo.iterState->dst();
                                state.buchiState = _buchiAutomaton.buchi().state_number(dstState);
                                state.marking = succInfo.currentSuccessorMarking;
                                state.accepting = _buchiAutomaton.buchi().state_is_accepting(dstState);
                                dstState->destroy();
                                succInfo.iterState->next();
                                return true;
                            }
                        }
                        bool has_successor = false;
                        while (true) {
                            std::optional<TraceMapStep> opt = _successorGenerator.next(state.marking, succInfo.markingSuccInfo, 0);
                            if constexpr (std::is_same_v<SuccInfo, EvenSuccessorInfo>()) {
                                if (succInfo.markingSuccInfo.shuffle){
                                    succInfo.markingSuccInfo.shuffle = false;
                                    continue;
                                }
                            }
                            has_successor = opt.has_value();
                            break;
                        }
                        if (!has_successor) {
                            break;
                        }
                        succInfo.currentSuccessorMarking = std::move(state.marking);
                        succInfo.iterState->first();
                    }
                }
                return false;
            }

            std::vector<CPNProductState> get_initial_states() const {
                const auto initBuchiState = _buchiAutomaton.buchi().get_init_state();
                std::vector<CPNProductState> initialStates;
                auto iter = _buchiAutomaton.buchi().succ_iter(initBuchiState);
                const auto& initialMarking = _successorGenerator.net().initial();
                if (iter->first()) {
                    do {
                        if (_check_condition(iter->cond(), initialMarking, 0)) {
                            auto buchiState = iter->dst();
                            CPNProductState state{initialMarking, _buchiAutomaton.buchi().state_number(buchiState)};
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

            CPNProductSuccessorInfo<SuccInfo> initial_suc_info() {
                return CPNProductSuccessorInfo<SuccInfo>{
                    _successorGenerator.getInitSuccInfo<SuccInfo>(),
                    nullptr,
                    ColoredPetriNetMarking{},
                };
            }

            void prepare(CPNProductState* productState, CPNProductSuccessorInfo<SuccInfo>& succInfo) {}

            bool is_accepting(const CPNProductState& state) {
                return _buchiAutomaton.buchi().state_is_accepting(state.buchiState);
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
#endif //CPNPRODUCTSUCCESSORGENERATOR_H
