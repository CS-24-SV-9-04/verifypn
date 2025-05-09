#include "PetriEngine/ExplicitColored/Algorithms/LTLNdfs.h"

#include <LTL/LTLToBuchi.h>
#include "LTL/Structures/BuchiAutomaton.h"

namespace PetriEngine::ExplicitColored {
    LTLNdfs::LTLNdfs(
        const ColoredPetriNet& net,
        const PQL::Condition_ptr& condition,
        const std::unordered_map<std::string, uint32_t>& placeNameIndices,
        const std::unordered_map<std::string, Transition_t>& transitionNameIndices
        )
    :
        _buchiAutomaton(make_buchi_automaton(condition, LTL::BuchiOptimization::Low, LTL::APCompression::None)),
        _net(net),
        _placeNameIndices(placeNameIndices),
        _transitionNameIndices(transitionNameIndices),
        _productColorEncoder(ColoredEncoder(net.getPlaces())),
        _globalPassed(_productColorEncoder) { }

    bool LTLNdfs::check() {
        const ProductStateGenerator generator{_net, _buchiAutomaton, _placeNameIndices, _transitionNameIndices};
        return dfs(generator, {_net.initial(), _buchiAutomaton.buchi().get_init_state_number()});
    }

    bool LTLNdfs::dfs(const ProductStateGenerator& productStateGenerator, ColoredPetriNetProductState initialState) {
        std::stack<ColoredPetriNetProductState> waiting;
        if (_buchiAutomaton.buchi().state_is_accepting(initialState.getBuchiState()) && ndfs(productStateGenerator, initialState.copy(_buchiAutomaton))){
            _searchStatistics.endWaitingStates = waiting.size();
            _searchStatistics.biggestEncoding = _globalPassed.getBiggestEncoding();
            return true;
        }
        _searchStatistics.exploredStates = 1;
        _searchStatistics.discoveredStates = 1;
        for (auto& initState : productStateGenerator.get_initial_states(initialState.marking)) {
            waiting.push(std::move(initState));
        }
        _searchStatistics.peakWaitingStates = waiting.size();
        while (!waiting.empty()){
            auto& state = waiting.top();
            auto nextState = productStateGenerator.next(state);
            if (state.done()) {
                waiting.pop();
                continue;
            }
            _searchStatistics.discoveredStates += 1;
            if (!_globalPassed.existsOrAdd({nextState.marking, nextState.getBuchiState()})) {
                _searchStatistics.exploredStates += 1;
                if (_buchiAutomaton.buchi().state_is_accepting(nextState.getBuchiState()) && ndfs(productStateGenerator, nextState.copy(_buchiAutomaton))) {
                    _searchStatistics.endWaitingStates = waiting.size();
                    _searchStatistics.biggestEncoding = _globalPassed.getBiggestEncoding();
                    return true;
                }
                waiting.push(std::move(nextState));
                _searchStatistics.peakWaitingStates  = std::max(static_cast<uint32_t>(waiting.size()), _searchStatistics.peakWaitingStates);

            }
        }
        _searchStatistics.endWaitingStates = waiting.size();
        _searchStatistics.biggestEncoding = _globalPassed.getBiggestEncoding();
        return false;
    }

    bool LTLNdfs::ndfs(const ProductStateGenerator& productStateGenerator, ColoredPetriNetProductState initialState) {
        PassedList<ProductColorEncoder, std::pair<ColoredPetriNetMarking, size_t>> localPassed(_productColorEncoder);
        std::stack<ColoredPetriNetProductState> waiting;
        localPassed.add({initialState.marking, initialState.getBuchiState()});
        auto targetState = std::make_pair(initialState.marking, initialState.getBuchiState());
        waiting.push(std::move(initialState));

        while (!waiting.empty()) {
            auto& state = waiting.top();
            auto nextState = productStateGenerator.next(state);
            _searchStatistics.discoveredStates += 1;
            if (state.done()) {
                waiting.pop();
                continue;
            }
            if (_buchiAutomaton.buchi().state_is_accepting(nextState.getBuchiState())) {
                if (nextState.getBuchiState() == targetState.second && nextState.marking == targetState.first) {
                    return true;
                }
                if (!localPassed.existsOrAdd({nextState.marking, nextState.getBuchiState()}) &&
                    !_globalPassed.exists({nextState.marking, nextState.getBuchiState()})) {
                    waiting.push(std::move(nextState));
                    _searchStatistics.exploredStates += 1;
                    _searchStatistics.peakWaitingStates  = std::max(static_cast<uint32_t>(waiting.size()), _searchStatistics.peakWaitingStates);
                }
            }
            waiting.pop();
        }
        return false;
    }

    const SearchStatistics & LTLNdfs::GetSearchStatistics() const {
        return _searchStatistics;
    }
}
