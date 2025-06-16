#ifndef COLOREDSUCCESSORGENERATOR_H
#define COLOREDSUCCESSORGENERATOR_H

#include "IntegerPackCodec.h"
#include "PossibleValues.h"
#include "../ColoredPetriNet.h"
#include "../ColoredPetriNetState.h"
#include <limits>
#include <optional>
#include <utils/MathExt.h>

namespace PetriEngine::ExplicitColored {
    struct ConstraintData {
        IntegerPackCodec<size_t, Color_t> stateCodec;
        std::vector<Variable_t> variableIndex;
        std::vector<PossibleValues> possibleVariableValues;
    };

    struct TraceMapStep {
        uint64_t id;
        uint64_t predecessorId;
        Transition_t transition;
        Binding_t binding;
    };

    class ColoredSuccessorGenerator {
    public:
        explicit ColoredSuccessorGenerator(const ColoredPetriNet& net, size_t constrainedBindingsThreshold);
        ~ColoredSuccessorGenerator() = default;

        std::optional<TraceMapStep> next(ColoredPetriNetMarking& marking, FixedSuccessorInfo& fixedSuccessorInfo, size_t id) const;

        std::optional<TraceMapStep> next(ColoredPetriNetMarking& marking, EvenSuccessorInfo& evenSuccessorInfo, size_t id) const;

        [[nodiscard]] const ColoredPetriNet& net() const {
            return _net;
        }

        Binding_t findNextValidBinding(const ColoredPetriNetMarking& marking, Transition_t tid, Binding_t bid, uint64_t totalBindings, Binding& binding, size_t stateId) const;

        template<typename T> T getInitSuccInfo() const {
            if constexpr(std::is_same_v<T, FixedSuccessorInfo>) {
                auto rv = FixedSuccessorInfo{};
                return rv;
            } else if constexpr(std::is_same_v<T, EvenSuccessorInfo>) {
                auto rv = EvenSuccessorInfo{};
                rv._map.resize(_net._transitions.size());
                return rv;
            }
        }

        void shrinkState(const size_t stateId) const {
            const auto lower = _constraintData.lower_bound(_getKey(stateId, 0));
            const auto upper = _constraintData.upper_bound(_getKey(stateId, 0xFFFF));
            _constraintData.erase(lower, upper);
        }
        void getBinding(Transition_t tid, Binding_t bid, Binding& binding) const;
        void fire(ColoredPetriNetMarking& state, Transition_t tid, const Binding& binding) const;
    protected:
        [[nodiscard]] bool check(const ColoredPetriNetMarking& state, Transition_t tid, const Binding& binding) const;
        [[nodiscard]] bool checkInhibitor(const ColoredPetriNetMarking& state, Transition_t tid) const;
        [[nodiscard]] bool checkPresetAndGuard(const ColoredPetriNetMarking& state, Transition_t tid, const Binding& binding) const;
        void consumePreset(ColoredPetriNetMarking& state, Transition_t tid, const Binding& binding) const;
        void producePostset(ColoredPetriNetMarking& state, Transition_t tid, const Binding& binding) const;
    private:
        mutable std::map<size_t, ConstraintData> _constraintData;
        mutable size_t _nextId = 1;
        size_t _constrainedBindingsThreshold;
        const ColoredPetriNet& _net;
        std::map<size_t, ConstraintData>::iterator _calculateConstraintData(const ColoredPetriNetMarking& marking, size_t id, Transition_t transition, bool& noPossibleBinding) const;
        [[nodiscard]] bool _hasMinimalCardinality(const ColoredPetriNetMarking& marking, Transition_t tid) const;
        [[nodiscard]] bool _shouldEarlyTerminateTransition(const ColoredPetriNetMarking& marking, const Transition_t tid) const {
            if (!checkInhibitor(marking, tid)) {
                return true;
            }
            if (!_hasMinimalCardinality(marking, tid)) {
                return true;
            }
            return false;
        }

        [[nodiscard]] static uint64_t _getKey(const size_t stateId, const Transition_t transition) {
            return ((stateId & 0xFFFF'FFFF'FFFF) << 16) | ((static_cast<uint64_t>(transition) & 0xFFFF));
        }
    };
}

#endif /* COLOREDSUCCESSORGENERATOR_H */