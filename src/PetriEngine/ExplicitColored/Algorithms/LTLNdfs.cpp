#include "PetriEngine/ExplicitColored/Algorithms/LTLNdfs.h"

#include <LTL/LTLToBuchi.h>
#include "LTL/Structures/BuchiAutomaton.h"

namespace PetriEngine::ExplicitColored {
    LTLNdfs::LTLNdfs(
        const ColoredPetriNet& net,
        const PQL::Condition_ptr& condition,
        const std::unordered_map<std::string, uint32_t>& placeNameIndices,
        const std::unordered_map<std::string, Transition_t>& transitionNameIndices,
        const ProductColorEncoder& encoder
        )
    : _net(net), _placeNameIndices(placeNameIndices), _transitionNameIndices(transitionNameIndices), _globalPassed(encoder),
      _localPassed(encoder) {
        _buchiAutomaton = make_buchi_automaton(condition, LTL::BuchiOptimization::Low, LTL::APCompression::None);
    }

    bool LTLNdfs::check() {
    }

    bool LTLNdfs::dfs(ProductStateGenerator productStateGenerator, ColoredPetriNetProductState initialState) {
        _waiting.push(std::move(initialState));
        while (!_waiting.empty()){
            auto& state = _waiting.top();
            auto nextState = productStateGenerator.next(state);
            if(_buchiAutomaton.buchi().state_is_accepting(nextState.getBuchiState())){
                if (ndfs(productStateGenerator, nextState)){
                    return true;
                }
            }
            if(_globalPassed.existsOrAdd({nextState.marking, nextState.getBuchiState()})){
                _waiting.push(std::move(nextState));
            }
            _waiting.pop();
        }
        return false;
    }

    bool LTLNdfs::ndfs(ProductStateGenerator productStateGenerator, ColoredPetriNetProductState& initialState) {
        _localPassed.add({initialState.marking, initialState.getBuchiState()});
        _waiting.push(std::move(initialState));

        while (!_waiting.empty()){
            auto& state = _waiting.top();
            if(productStateGenerator.next(state).marking.markings.empty()){
                return true;
            }
            auto nextState = productStateGenerator.next(state);
            if (_buchiAutomaton.buchi().state_is_accepting(nextState.getBuchiState())) {
                if (nextState.marking == state.marking && nextState.getBuchiState() == state.getBuchiState()) {
                    return true;
                }
                if (!_localPassed.existsOrAdd({nextState.marking, nextState.getBuchiState()}) &&
                    !_globalPassed.exists({nextState.marking, nextState.getBuchiState()})) {
                    _waiting.push(std::move(nextState));
                }
            }
            _waiting.pop();
        }
        return false;
    }
}
