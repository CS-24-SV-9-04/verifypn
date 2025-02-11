#ifndef COLOREDSUCCESSORGENERATOR_H
#define COLOREDSUCCESSORGENERATOR_H

#include "ColoredPetriNet.h"
#include "ColoredPetriNetState.h"
#include <limits>

namespace PetriEngine{
    namespace ExplicitColored{
        enum class CheckingBool {
            FALSE,
            TRUE,
            NEVERTRUE
        };
        struct TransitionVariables{
            std::map<Variable_t, std::vector<uint32_t>> possibleValues = std::map<Variable_t, std::vector<uint32_t>>{};
            uint32_t totalBindings = 0;
        };

        class ColoredSuccessorGenerator {
        public:
            explicit ColoredSuccessorGenerator(const ColoredPetriNet& net);
            ~ColoredSuccessorGenerator();

            ColoredPetriNetState next(ColoredPetriNetState& state) const {
                return _next(state);
            }

            ColoredPetriNetStateOneTrans next(ColoredPetriNetStateOneTrans& state) const {
                return _nextOneTrans(state);
            }

            [[nodiscard]] const ColoredPetriNet& net() const {
                return _net;
            }
        protected:
            void getBinding(Transition_t tid, Binding_t bid, Binding& binding) const;
            bool check(const ColoredPetriNetMarking& state, Transition_t tid, const Binding& binding) const;
            bool checkInhibitor(const ColoredPetriNetMarking& state, Transition_t tid) const;
            bool checkPresetAndGuard(const ColoredPetriNetMarking& state, Transition_t tid, const Binding& binding) const;
            void consumePreset(ColoredPetriNetMarking& state, Transition_t tid, const Binding& binding) const;
            void producePostset(ColoredPetriNetMarking& state, Transition_t tid, const Binding& binding) const;

            void _fire(ColoredPetriNetMarking& state, Transition_t tid, const Binding& binding) const;

            ColoredPetriNetState _next(ColoredPetriNetState &state) const {
                const auto& tid = state.getCurrentTransition();
                const auto& bid = state.getCurrentBinding();
                Binding binding;
                while (tid < _net.getTransitionCount()) {
                    const auto totalBindings = _net._transitions[state.getCurrentTransition()].validVariables.second;
                    const auto nextBid = findNextValidBinding(state.marking, tid, bid, totalBindings, binding);
                    if (nextBid != std::numeric_limits<Binding_t>::max()) {
                        auto newState = ColoredPetriNetState(state.marking);
                        _fire(newState.marking, tid, binding);
                        state.nextBinding(nextBid);
                        return newState;
                    }
                    state.nextTransition();
                }
                state.setDone();
                return ColoredPetriNetState{{}};
            }

            // SuccessorGenerator but only considers current transition
            ColoredPetriNetStateOneTrans _nextOneTrans(ColoredPetriNetStateOneTrans &state) const {
                auto [tid, bid] = state.getNextPair();
                auto totalBindings = _net._transitions[tid].validVariables.second;
                Binding binding;
                //If bid is updated at the end optimizations seem to make the loop not work
                while (bid != std::numeric_limits<Binding_t>::max()){
                    {
                        const auto nextBid = findNextValidBinding(state.marking, tid, bid, totalBindings, binding);
                        if (nextBid != std::numeric_limits<Binding_t>::max()) {
                            auto newState = ColoredPetriNetStateOneTrans{state, _net.getTransitionCount()};
                            _fire(newState.marking, tid, binding);
                            state.updatePair(tid, nextBid);
                            return newState;
                        }
                    }
                    state.updatePair(tid, std::numeric_limits<Binding_t>::max());
                    auto [nextTid, nextBid] = state.getNextPair();
                    tid = nextTid;
                    bid = nextBid;
                    totalBindings = _net._transitions[tid].validVariables.second;
                }
                return {{},0};
            }
            [[nodiscard]] Binding_t findNextValidBinding(const ColoredPetriNetMarking& marking, Transition_t tid, Binding_t bid, uint64_t totalBindings, Binding& binding) const;
            friend FireabilityChecker;
        private:
            const ColoredPetriNet& _net;
            [[nodiscard]] bool _hasMinimalCardinality(const ColoredPetriNetMarking& marking, Transition_t tid) const;
            [[nodiscard]] bool _shouldEarlyTerminateTransition(const ColoredPetriNetMarking& marking, const Transition_t tid) const {
                if (!checkInhibitor(marking, tid))
                    return true;
                if (!_hasMinimalCardinality(marking, tid))
                    return true;
                return false;
            }
        };
    }
}
#endif /* COLOREDSUCCESSORGENERATOR_H */