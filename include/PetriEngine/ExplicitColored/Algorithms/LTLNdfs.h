#ifndef LTLNDFS_H
#define LTLNDFS_H
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
    class LTLNdfs {
    public:
        LTLNdfs(
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
            std::stack<CPNProductStateSpecialized<SuccessorGeneratorState>> waiting;
            if (
                _buchiAutomaton.buchi().state_is_accepting(initialState.buchiState)
                && _ndfs(initialState)
            ) {
                _searchStatistics.endWaitingStates = waiting.size();
                _searchStatistics.biggestEncoding = _globalPassed.getBiggestEncoding();
                return true;
            }
            waiting.emplace(_stateFactory(std::move(initialState)));
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
                        _isFullStateSpace = _globalPassed.isFullStateSpace() && _isFullStateSpace;
                        return true;
                    }
                    waiting.emplace(_stateFactory(std::move(nextState)));
                    _searchStatistics.peakWaitingStates  = std::max(static_cast<uint32_t>(waiting.size()), _searchStatistics.peakWaitingStates);
                }
            }
            _searchStatistics.endWaitingStates = waiting.size();
            _searchStatistics.biggestEncoding = _globalPassed.getBiggestEncoding();
            _isFullStateSpace = _globalPassed.isFullStateSpace() && _isFullStateSpace;
            return false;
        }

        bool _ndfs(CPNProductState initialState) {
            if (_productStateGenerator.has_invariant_self_loop(initialState)) {
                return true;
            }
            PassedList<ProductColorEncoder, std::pair<ColoredPetriNetMarking, size_t>> localPassed(_productColorEncoder);
            std::stack<CPNProductStateSpecialized<SuccessorGeneratorState>> waiting;
            localPassed.add({initialState.marking, initialState.buchiState});

            const auto targetState = initialState;

            waiting.emplace(_stateFactory(std::move(initialState)));

            while (!waiting.empty()) {
                auto& currentState = waiting.top();
                auto nextState = _productStateGenerator.next(currentState);
                _searchStatistics.discoveredStates += 1;
                if (currentState.done()) {
                    waiting.pop();
                    continue;
                }
                if (nextState == targetState) {
                    _isFullStateSpace = localPassed.isFullStateSpace() && _isFullStateSpace;
                    return true;
                }
                if (!localPassed.existsOrAdd({nextState.marking, nextState.buchiState}) &&
                    !_globalPassed.exists({nextState.marking, nextState.buchiState})) {
                    waiting.emplace(_stateFactory(std::move(nextState)));
                    _searchStatistics.exploredStates += 1;
                    _searchStatistics.peakWaitingStates  = std::max(static_cast<uint32_t>(waiting.size()), _searchStatistics.peakWaitingStates);
                    }
            }
            _isFullStateSpace = localPassed.isFullStateSpace() && _isFullStateSpace;
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
    LTLNdfs<SuccessorGeneratorState, StateFactory> makeLTLNdfs(
        const ColoredPetriNet& net,
        const PQL::Condition_ptr& condition,
        const ExplicitColoredPetriNetBuilder& builder,
        const options_t& options,
        StateFactory stateFactory)
    {
        return LTLNdfs<SuccessorGeneratorState, StateFactory>(
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

#endif //LTLNDFS_H