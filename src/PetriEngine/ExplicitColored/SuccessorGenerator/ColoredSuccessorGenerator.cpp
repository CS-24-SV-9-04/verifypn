#ifndef COLOREDSUCCESSORGENERATOR_CPP
#define COLOREDSUCCESSORGENERATOR_CPP

#include <memory>
#include "PetriEngine/ExplicitColored/SuccessorGenerator/ColoredSuccessorGenerator.h"

namespace PetriEngine::ExplicitColored{
    ColoredSuccessorGenerator::ColoredSuccessorGenerator(const ColoredPetriNet& net)
    : _net(net) {}

    void updateVariableMap(std::map<Variable_t, std::vector<uint32_t>>& map, const std::map<Variable_t, std::vector<uint32_t>>& newMap){
        for (auto&& pair : newMap){
            if (map.find(pair.first) != map.end()){
                auto& values = map[pair.first];
                auto newSet = std::vector<uint32_t>{};
                std::set_intersection(values.begin(), values.end(),
                    pair.second.begin(), pair.second.end(), std::back_inserter(newSet));
                map[pair.first] = std::move(newSet);
            }else{
                map.insert(pair);
            }
        }
    }

    void ColoredSuccessorGenerator::getBinding(const Transition_t tid, const Binding_t bid, Binding& binding) const {
        auto map = std::map<Variable_t, Color_t>{};
        auto interval = _net._transitions[tid].totalBindings;
        if (interval != 0) {
            for (const auto varIndex : _net._transitions[tid].variables){
                const auto size = _net._variables[varIndex].colorSize;
                interval /= size;
                map.emplace(varIndex, (bid / interval) % size);
            }
            binding = std::move(map);
        } else {
            binding = std::move(Binding{});
        }
    }

    bool ColoredSuccessorGenerator::check(const ColoredPetriNetMarking& state, const Transition_t tid, const Binding& binding) const{
        return checkInhibitor(state, tid) && checkPresetAndGuard(state, tid, binding);
    }

    bool ColoredSuccessorGenerator::checkPresetAndGuard(const ColoredPetriNetMarking& state, const Transition_t tid, const Binding& binding) const {
        if (_net._transitions[tid].guardExpression != nullptr && !_net._transitions[tid].guardExpression->eval(binding)){
            return false;
        }
        for (auto i = _net._transitionArcs[tid].first; i < _net._transitionArcs[tid].second; i++){
            auto& arc = _net._arcs[i];
            if (!arc.expression->isSubSet(state.markings[arc.from], binding)) {
                return false;
            }
        }
        return true;
    }

    bool ColoredSuccessorGenerator::checkInhibitor(const ColoredPetriNetMarking& state, const Transition_t tid) const {
        for (size_t i = _net._transitionInhibitors[tid]; i < _net._transitionInhibitors[tid + 1]; i++){
            auto& inhib = _net._inhibitorArcs[i];
            if (inhib.weight <= state.markings[inhib.from].totalCount()){
                return false;
            }
        }
        return true;
    }

    void ColoredSuccessorGenerator::consumePreset(ColoredPetriNetMarking& state, const Transition_t tid, const Binding& binding) const {
        for (auto i = _net._transitionArcs[tid].first; i < _net._transitionArcs[tid].second; i++){
            auto& arc = _net._arcs[i];
            arc.expression->consume(state.markings[arc.from], binding);
        }
    }

    void ColoredSuccessorGenerator::producePostset(ColoredPetriNetMarking& state, const Transition_t tid, const Binding& binding) const {
        for (auto i = _net._transitionArcs[tid].second; i < _net._transitionArcs[tid + 1].first; i++){
            auto& arc = _net._arcs[i];
            arc.expression->produce(state.markings[arc.to], binding);
        }
    }

    void ColoredSuccessorGenerator::fire(ColoredPetriNetMarking& state, const Transition_t tid, const Binding& binding) const{
        consumePreset(state, tid, binding);
        producePostset(state, tid, binding);
    }

    std::map<size_t, ConstraintData>::iterator ColoredSuccessorGenerator::_calculateConstraintData(
        const ColoredPetriNetMarking &marking, const size_t id, const Transition_t transition, bool &noPossibleBinding) const {
        ConstraintData constraintData;
        const auto& allVariables = _net.getAllTransitionVariables(transition);
        std::set<Variable_t> inputArcVariables;
        _net.extractInputVariables(transition, inputArcVariables);
        std::vector<Color_t> stateMaxes;
        for (Variable_t variable : allVariables) {
            PossibleValues values = PossibleValues::getAll();
            const auto variableColorSize = _net._variables[variable].colorSize;
            const auto& constraints = _net._transitions[transition].preplacesVariableConstraints.find(variable);
            if (constraints == _net._transitions[transition].preplacesVariableConstraints.end()) {
                stateMaxes.push_back(variableColorSize);
                constraintData.possibleVariableValues.push_back(PossibleValues::getAll());
                continue;
            }
            for (const auto& constraint : constraints->second) {
                const auto& place = marking.markings[constraint.place];

                if (constraint.isTop()) {
                    continue;
                }

                std::set<Color_t> possibleColors;

                if (values.allColors) {
                    values.colors.reserve(place.counts().size());
                }

                for (const auto& tokens : place.counts()) {
                    auto bindingValue = addColorOffset(
                        _net._places[constraint.place].colorType->colorCodec.decode(tokens.first, constraint.colorIndex),
                        -constraint.colorOffset,
                        variableColorSize
                    );
                    possibleColors.insert(bindingValue);
                }

                if (values.allColors) {
                    if (possibleColors.size() != variableColorSize) {
                        values.allColors = false;
                        for (const auto& color : possibleColors) {
                            values.colors.push_back(color);
                        }
                    }
                } else {
                    values.intersect(possibleColors);
                }

                if (values.colors.empty() && !values.allColors) {
                    noPossibleBinding = true;
                    return _constraintData.end();
                }
            }
            stateMaxes.push_back(values.allColors
                ? variableColorSize
                : values.colors.size());
            constraintData.possibleVariableValues.emplace_back(std::move(values));
        }

        constraintData.stateCodec = IntegerPackCodec(stateMaxes);

        constraintData.variableIndex.insert(
            constraintData.variableIndex.begin(),
            allVariables.begin(),
            allVariables.end()
        );
        _constraintData.emplace(_getKey(id, transition), std::move(constraintData));
        return _constraintData.find(_getKey(id, transition));
    }

    bool ColoredSuccessorGenerator::_hasMinimalCardinality(const ColoredPetriNetMarking &marking, const Transition_t tid) const {
        for (auto i = _net._transitionArcs[tid].first; i < _net._transitionArcs[tid].second; i++) {
            auto& arc = _net._arcs[i];
            if (marking.markings[arc.from].totalCount() < arc.expression->getMinimalMarkingCount()) {
                return false;
            }
            if (!(arc.expression->getMinimalColorMarking().minimalMarkingMultiSet <= marking.markings[arc.from])) {
                return false;
            }
        }
        return true;
    }

    std::optional<TraceMapStep> ColoredSuccessorGenerator::next(ColoredPetriNetMarking &marking,
        FixedSuccessorInfo &fixedSuccessorInfo, size_t id) const {
        const auto tid = fixedSuccessorInfo.currentTransition;
        const auto bid = fixedSuccessorInfo.currentBinding;
        Binding binding;
        while (tid < _net.getTransitionCount()) {
            const auto totalBindings = _net._transitions[tid].totalBindings;
            const auto nextBid = findNextValidBinding(marking, tid, bid, totalBindings, binding, id);
            if (nextBid != std::numeric_limits<Binding_t>::max()) {
                auto step = TraceMapStep {
                    _nextId++,
                    id,
                    tid,
                    nextBid
                };

                fire(marking, tid, binding);
                fixedSuccessorInfo.currentBinding += 1;
                return step;
            }
            fixedSuccessorInfo.currentBinding = 0;
            fixedSuccessorInfo.currentTransition += 1;
        }
        fixedSuccessorInfo.done = true;
        return std::nullopt;
    }

    std::optional<TraceMapStep> ColoredSuccessorGenerator::next(ColoredPetriNetMarking &marking,
                                                            EvenSuccessorInfo &evenSuccessorInfo, size_t id) const {
        auto [tid, bid] = evenSuccessorInfo.getNextPair();
        auto totalBindings = _net._transitions[tid].totalBindings;
        Binding binding;
        while (bid != std::numeric_limits<Binding_t>::max()) {
            const auto nextBid = findNextValidBinding(marking, tid, bid, totalBindings, binding, id);
            evenSuccessorInfo.updatePair(tid, nextBid);
            if (nextBid != std::numeric_limits<Binding_t>::max()) {
                //TODO: maybe remove id
                auto step = TraceMapStep {
                    _nextId++,
                    id,
                    tid,
                    nextBid
                };

                fire(marking, tid, binding);

                return step;
            }
            std::tie(tid, bid) = evenSuccessorInfo.getNextPair();
            totalBindings = _net._transitions[tid].totalBindings;
        }
        return std::nullopt;
    }

    Binding_t ColoredSuccessorGenerator::findNextValidBinding(const ColoredPetriNetMarking& marking, const Transition_t tid, Binding_t bid, const uint64_t totalBindings, Binding& binding, size_t stateId) const {
        if (bid == 0 && _shouldEarlyTerminateTransition(marking, tid)) {
            return std::numeric_limits<Binding_t>::max();
        }

        if (totalBindings == 0) {
            if (bid == 0 && checkPresetAndGuard(marking, tid, Binding{})) {
                return bid;
            }
            return std::numeric_limits<Binding_t>::max();
        }

        auto constraintDataIt = _constraintData.find(_getKey(stateId, tid));
        if (totalBindings > 30 && constraintDataIt == _constraintData.end()) {
            bool noPossibleBinding = false;
            constraintDataIt = _calculateConstraintData(marking, stateId, tid, noPossibleBinding);
            if (noPossibleBinding) {
                return std::numeric_limits<Binding_t>::max();
            }
        }

        if (constraintDataIt == _constraintData.end()) {
            for (auto i = bid; i < totalBindings; i++) {
                getBinding(tid, i, binding);
                if (checkPresetAndGuard(marking, tid, binding)) {
                    return i;
                }
            }
            return std::numeric_limits<Binding_t>::max();
        }

        for (;bid < constraintDataIt->second.stateCodec.getMax(); bid++) {
            for (size_t variableIndex = 0; variableIndex < constraintDataIt->second.variableIndex.size(); variableIndex++) {
                const auto& possibleValues = constraintDataIt->second.possibleVariableValues[variableIndex];
                if (possibleValues.allColors) {
                    binding.setValue(
                        constraintDataIt->second.variableIndex[variableIndex],
                        constraintDataIt->second.stateCodec.decode(bid, variableIndex)
                    );
                } else {
                    binding.setValue(
                        constraintDataIt->second.variableIndex[variableIndex],
                        possibleValues.colors[constraintDataIt->second.stateCodec.decode(bid, variableIndex)]
                    );
                }
            }

            if (checkPresetAndGuard(marking, tid, binding)) {
                return bid;
            }
        }
        return std::numeric_limits<Binding_t>::max();
    }
}

#endif /* COLOREDSUCCESSORGENERATOR_CPP */