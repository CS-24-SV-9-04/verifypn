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
        waiting.push(std::move(initialState));
        _searchStatistics.exploredStates = 1;
        _searchStatistics.discoveredStates = 1;
        while (!waiting.empty()){
            auto& state = waiting.top();
            auto nextState = productStateGenerator.next(state);
            if (state.done()) {
                waiting.pop();
                continue;
            }
            if(!_globalPassed.existsOrAdd({nextState.marking, nextState.getBuchiState()})) {
                if(_buchiAutomaton.buchi().state_is_accepting(nextState.getBuchiState())) {
                    if (ndfs(productStateGenerator, nextState.copy(_buchiAutomaton))) {
                        return true;
                    }
                }
                waiting.push(std::move(nextState));
            }
        }
        return false;
    }

    bool LTLNdfs::ndfs(const ProductStateGenerator& productStateGenerator, ColoredPetriNetProductState initialState) {
        PassedList<ProductColorEncoder, std::pair<ColoredPetriNetMarking, size_t>> localPassed(_productColorEncoder);
        std::stack<ColoredPetriNetProductState> waiting;
        localPassed.add({initialState.marking, initialState.getBuchiState()});
        waiting.push(std::move(initialState));

        while (!waiting.empty()){
            auto& state = waiting.top();
            if(productStateGenerator.next(state).marking.markings.empty()){
                return true;
            }
            auto nextState = productStateGenerator.next(state);
            if (_buchiAutomaton.buchi().state_is_accepting(nextState.getBuchiState())) {
                if (nextState.marking == state.marking && nextState.getBuchiState() == state.getBuchiState()) {
                    return true;
                }
                if (!localPassed.existsOrAdd({nextState.marking, nextState.getBuchiState()}) &&
                    !_globalPassed.exists({nextState.marking, nextState.getBuchiState()})) {
                    waiting.push(std::move(nextState));
                }
            }
            waiting.pop();
        }
        return false;
    }
}
