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
        _globalPassed(_productColorEncoder),
        _productStateGenerator(net, _buchiAutomaton, placeNameIndices, _transitionNameIndices)
    { }

    bool LTLNdfs::check() {
        const ProductStateGenerator generator{_net, _buchiAutomaton, _placeNameIndices, _transitionNameIndices};
        auto initialStates = generator.get_initial_states(_net.initial());
        for (auto& initialState : initialStates) {
            if (_dfs(initialState))
                return true;
        }
        return false;
    }

    bool LTLNdfs::_dfs(CPNProductState initialState) {
        std::stack<CPNProductStateFixed> waiting;
        if (
            _buchiAutomaton.buchi().state_is_accepting(initialState.buchiState)
            && _ndfs(initialState)
        ) {
            _searchStatistics.endWaitingStates = waiting.size();
            _searchStatistics.biggestEncoding = _globalPassed.getBiggestEncoding();
            return true;
        }
        waiting.emplace(std::move(initialState));
        _searchStatistics.exploredStates = 1;
        _searchStatistics.discoveredStates = 1;
        _searchStatistics.peakWaitingStates = waiting.size();
        while (!waiting.empty()){
            auto& state = waiting.top();
            auto nextState = _productStateGenerator.next(state);
            if (state.done()) {
                waiting.pop();
                continue;
            }
            _searchStatistics.discoveredStates += 1;
            if (!_globalPassed.existsOrAdd({
                nextState.marking, nextState.buchiState
            })) {
                _searchStatistics.exploredStates += 1;
                if (
                    _buchiAutomaton.buchi().state_is_accepting(nextState.buchiState)
                    && _ndfs(nextState)
                ) {
                    _searchStatistics.endWaitingStates = waiting.size();
                    _searchStatistics.biggestEncoding = _globalPassed.getBiggestEncoding();
                    return true;
                }
                waiting.emplace(std::move(nextState));
                _searchStatistics.peakWaitingStates  = std::max(static_cast<uint32_t>(waiting.size()), _searchStatistics.peakWaitingStates);
            }
        }
        _searchStatistics.endWaitingStates = waiting.size();
        _searchStatistics.biggestEncoding = _globalPassed.getBiggestEncoding();
        return false;
    }

    bool LTLNdfs::_ndfs(CPNProductState initialState) {
        if (_productStateGenerator.has_invariant_self_loop(initialState)) {
            return true;
        }
        PassedList<ProductColorEncoder, std::pair<ColoredPetriNetMarking, size_t>> localPassed(_productColorEncoder);
        std::stack<CPNProductStateFixed> waiting;
        localPassed.add({initialState.marking, initialState.buchiState});

        const auto targetState = initialState;

        waiting.emplace(std::move(initialState));

        while (!waiting.empty()) {
            auto& currentState = waiting.top();
            auto nextState = _productStateGenerator.next(currentState);
            _searchStatistics.discoveredStates += 1;
            if (currentState.done()) {
                waiting.pop();
                continue;
            }
            if (nextState == targetState) {
                return true;
            }
            if (!localPassed.existsOrAdd({nextState.marking, nextState.buchiState}) &&
                !_globalPassed.exists({nextState.marking, nextState.buchiState})) {
                waiting.emplace(std::move(nextState));
                _searchStatistics.exploredStates += 1;
                _searchStatistics.peakWaitingStates  = std::max(static_cast<uint32_t>(waiting.size()), _searchStatistics.peakWaitingStates);
            }
        }
        return false;
    }

    const SearchStatistics & LTLNdfs::GetSearchStatistics() const {
        return _searchStatistics;
    }
}
