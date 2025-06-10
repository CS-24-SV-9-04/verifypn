#ifndef CORRECTLTLNDFS_H
#define CORRECTLTLNDFS_H
#include <LTL/Structures/BuchiAutomaton.h>
#include <PetriEngine/options.h>
#include <PetriEngine/ExplicitColored/AtomicTypes.h>
#include <PetriEngine/PQL/PQL.h>
#include <PetriEngine/ExplicitColored/ColoredPetriNet.h>
#include <PetriEngine/ExplicitColored/ColoredResultPrinter.h>
#include <PetriEngine/ExplicitColored/ExplicitColoredPetriNetBuilder.h>

#include "SearchStatistics.h"
#include "PetriEngine/ExplicitColored/SuccessorGenerator/ProductStateGenerator.h"
#include "PetriEngine/ExplicitColored/PassedList.h"
#include "PetriEngine/ExplicitColored/ProductColorEncoder.h"

namespace PetriEngine::ExplicitColored {
    template<typename SuccessorGeneratorState, typename StateFactory>
    class CorrectLTLNdfs {
    public:
        CorrectLTLNdfs(
            const ColoredPetriNet& net,
            const PQL::Condition_ptr& condition,
            const std::unordered_map<std::string, uint32_t>& placeNameIndices,
            const std::unordered_map<std::string, Transition_t>& transitionNameIndices,
            LTL::BuchiOptimization buchOptimization,
            LTL::APCompression apCompression,
            StateFactory stateGeneratorFunction
        ) :
            _buchiAutomaton(make_buchi_automaton(condition, buchOptimization, apCompression)),
            _net(net),
            _placeNameIndices(placeNameIndices),
            _transitionNameIndices(transitionNameIndices),
            _productColorEncoder(ColoredEncoder(net.getPlaces())),
            _globalPassed(_productColorEncoder),
            _productStateGenerator(net, _buchiAutomaton, placeNameIndices, _transitionNameIndices),
            _stateFactory(std::move(stateGeneratorFunction))
        {}

        Result check() {
            const ProductStateGenerator<SuccessorGeneratorState> generator{_net, _buchiAutomaton, _placeNameIndices, _transitionNameIndices};
            auto initialStates = generator.get_initial_states(_net.initial());
            bool found = false;
            for (auto& initialState : initialStates) {
                if (_dfs(initialState)) {
                    found = true;
                    break;
                }
            }
            if (found)
                return Result::SATISFIED;
            if (_isFullStateSpace)
                return Result::UNSATISFIED;
            return Result::UNKNOWN;
        }
        const SearchStatistics& GetSearchStatistics() const {
            return _searchStatistics;
        }

    private:
        bool _dfs(CPNProductState initialState) {
            light_deque<CPNProductStateSpecialized<SuccessorGeneratorState>> todo;
            light_deque<CPNProductStateSpecialized<SuccessorGeneratorState>> nested_todo;
            PassedList<ProductColorEncoder, std::pair<ColoredPetriNetMarking, size_t>> mark1(_productColorEncoder);
            PassedList<ProductColorEncoder, std::pair<ColoredPetriNetMarking, size_t>> mark2(_productColorEncoder);

            todo.push_back(_stateFactory(initialState));

            while (!todo.empty()) {
                auto& curState = todo.back();
                auto working = _productStateGenerator.next(curState);
                _searchStatistics.discoveredStates += 1;
                if (curState.done()) {
                    if (_buchiAutomaton.buchi().state_is_accepting(curState.getBuchiState())) {
                        _searchStatistics.exploredStates += 1;
                        auto productState = curState.makeProductState();
                        if (_productStateGenerator.has_invariant_self_loop(productState))
                            return true;
                        if (_ndfs(productState, nested_todo, mark2))
                            return true;
                    }
                    todo.pop_back();
                } else {
                    if (!mark1.existsOrAdd({working.marking, working.buchiState})) {
                        _searchStatistics.exploredStates += 1;
                        if (_buchiAutomaton.buchi().state_is_accepting(curState.getBuchiState())
                            && _productStateGenerator.has_invariant_self_loop(curState.makeProductState()))
                        {
                            return true;
                        }
                        todo.push_back(_stateFactory(working));
                    }
                }
            }
            return false;
        }

        bool _ndfs(CPNProductState state, light_deque<CPNProductStateSpecialized<SuccessorGeneratorState>>& nested_todo,  PassedList<ProductColorEncoder, std::pair<ColoredPetriNetMarking, size_t>>& mark2) {
            nested_todo.push_back(_stateFactory(state));
            while (!nested_todo.empty()) {
                auto &curState = nested_todo.back();
                auto working = _productStateGenerator.next(curState);
                _searchStatistics.discoveredStates += 1;
                if (curState.done()) {
                    nested_todo.pop_back();
                } else {
                    if (working == state) {
                        return true;
                    }
                    if (!mark2.existsOrAdd({working.marking, working.buchiState})) {
                        _searchStatistics.exploredStates += 1;
                        nested_todo.push_back(_stateFactory(working));
                    }
                }
            }
            return false;
        }

        LTL::Structures::BuchiAutomaton _buchiAutomaton;
        const ColoredPetriNet& _net;
        const std::unordered_map<std::string, uint32_t>& _placeNameIndices;
        const std::unordered_map<std::string, Transition_t>& _transitionNameIndices;
        ProductColorEncoder _productColorEncoder;
        PassedList<ProductColorEncoder, std::pair<ColoredPetriNetMarking, size_t>> _globalPassed;
        ProductStateGenerator<SuccessorGeneratorState> _productStateGenerator;
        SearchStatistics _searchStatistics;
        bool _isFullStateSpace = true;
        StateFactory _stateFactory;
    };

    template<typename SuccessorGeneratorState, typename StateFactory>
    CorrectLTLNdfs<SuccessorGeneratorState, StateFactory> makeCorrectLTLNdfs(
        const ColoredPetriNet& net,
        const Condition_ptr& condition,
        const ExplicitColoredPetriNetBuilder& builder,
        const options_t& options,
        StateFactory stateFactory)
    {
        return CorrectLTLNdfs<SuccessorGeneratorState, StateFactory>(
            net,
            condition,
            builder.getPlaceIndices(),
            builder.getTransitionIndices(),
            options.buchiOptimization,
            options.ltl_compress_aps,
            std::move(stateFactory)
        );
    }
}

#endif //CORRECTLTLNDFS_H