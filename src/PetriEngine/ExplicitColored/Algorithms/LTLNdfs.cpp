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
    : _net(net), _placeNameIndices(placeNameIndices), _transitionNameIndices(transitionNameIndices) {
        _buchiAutomaton = make_buchi_automaton(condition, LTL::BuchiOptimization::Low, LTL::APCompression::None);
    }

    bool LTLNdfs::check() {
    }

    bool LTLNdfs::dfs(ProductStateGenerator productStateGenerator, ColoredPetriNetProductState initialState) {
        _waiting.emplace_back(std::move(initialState));
        while (!_waiting.empty()){
            for (auto& s : _waiting) {
                auto nextState = productStateGenerator.next(s);
                if(_buchiAutomaton.buchi().state_is_accepting(nextState.getBuchiState())){
                    if (ndfs(productStateGenerator, nextState)){
                        return true;
                    }
                }
                if(std::find(_globalPassed.begin(), _globalPassed.end(), nextState) == _globalPassed.end()){
                    _globalPassed.emplace_back(nextState);
                    _waiting.emplace_back(nextState);
                }
                _waiting.erase(std::remove(_waiting.begin(), _waiting.end(), initialState), _waiting.end());
            }
        }

        return false;
    }

    bool LTLNdfs::ndfs(ProductStateGenerator productStateGenerator, ColoredPetriNetProductState state) {
        _waiting.emplace_back(std::move(state));
        _localPassed.emplace_back(state);

        while (!_waiting.empty()){
            for (auto& s : _waiting){
                if(productStateGenerator.next(s).marking.markings.empty()){
                    return true;
                }
                auto ns = productStateGenerator.next(s);
                if (_buchiAutomaton.buchi().state_is_accepting(ns.getBuchiState())){
                    if(ns.marking == s.marking && ns.getBuchiState() == s.getBuchiState()){
                        return true;
                    }
                    if (!isPassed(ns)){
                        _localPassed.emplace_back(ns);

                        _waiting.emplace_back(ns);
                    }
                }
            }
        }
        return false;
    }
}
