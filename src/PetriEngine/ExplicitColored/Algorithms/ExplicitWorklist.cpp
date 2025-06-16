#ifndef EXPLICITWORKLIST_CPP
#define EXPLICITWORKLIST_CPP

#include "PetriEngine/ExplicitColored/Algorithms/ExplicitWorklist.h"
#include <PetriEngine/options.h>
#include "PetriEngine/ExplicitColored/SuccessorGenerator/ColoredSuccessorGenerator.h"
#include "PetriEngine/PQL/PQL.h"
#include "PetriEngine/ExplicitColored/ColoredPetriNetMarking.h"
#include "PetriEngine/PQL/Visitor.h"
#include "PetriEngine/ExplicitColored/Algorithms/ColoredSearchTypes.h"
#include "PetriEngine/ExplicitColored/FireabilityChecker.h"
#include "PetriEngine/ExplicitColored/ExplicitErrors.h"
#include "PetriEngine/ExplicitColored/PassedList.h"

namespace PetriEngine::ExplicitColored {
    ExplicitWorklist::ExplicitWorklist(
        const ColoredPetriNet& net,
        const PQL::Condition_ptr &query,
        const std::unordered_map<std::string, uint32_t>& placeNameIndices,
        const std::unordered_map<std::string, Transition_t>& transitionNameIndices,
        const size_t seed,
        bool createTrace
    ) : _net(std::move(net)),
        _successorGenerator(ColoredSuccessorGenerator{_net}),
        _seed(seed),
        _createTrace(createTrace)
    {
        const ExplicitQueryPropositionCompiler queryCompiler(placeNameIndices, transitionNameIndices, _successorGenerator);
        if (const auto efGammaQuery = dynamic_cast<PQL::EFCondition*>(query.get())) {
            _quantifier = Quantifier::EF;
            _gammaQuery = queryCompiler.compile(efGammaQuery->getCond());
        } else if (const auto agGammaQuery = dynamic_cast<PQL::AGCondition*>(query.get())) {
            _quantifier = Quantifier::AG;
            _gammaQuery = queryCompiler.compile(agGammaQuery->getCond());
        } else {
            throw explicit_error{ExplicitErrorType::UNSUPPORTED_QUERY};
        }
    }

    bool ExplicitWorklist::check(const Strategy searchStrategy, const ColoredSuccessorGeneratorOption coloredSuccessorGeneratorOption) {
        if (coloredSuccessorGeneratorOption == ColoredSuccessorGeneratorOption::FIXED) {
            return _search<FixedSuccessorInfo>(searchStrategy);
        }
        if (coloredSuccessorGeneratorOption == ColoredSuccessorGeneratorOption::EVEN) {
            return _search<EvenSuccessorInfo>(searchStrategy);
        }
        throw explicit_error(ExplicitErrorType::UNSUPPORTED_GENERATOR);
    }

    const SearchStatistics & ExplicitWorklist::GetSearchStatistics() const {
        return _searchStatistics;
    }

    std::optional<uint64_t> ExplicitWorklist::getCounterExampleId() const {
        return _counterExampleId;
    }

    std::optional<std::vector<InternalTraceStep>> ExplicitWorklist::getTraceTo(uint64_t counterExampleId) const {
        uint64_t currentId = counterExampleId;
        std::vector<InternalTraceStep> trace;
        while (currentId != 0) {
            auto it = _stateMap.transitions.find(currentId);
            if (it == _stateMap.transitions.end()) {
                return std::nullopt;
            }
            currentId = it->second.predecessorId;
            trace.push_back(InternalTraceStep {
                it->second.binding,
                it->second.transition
            });
        }
        return std::vector(trace.rbegin(), trace.rend());
    }

    bool ExplicitWorklist::_check(const ColoredPetriNetMarking& state, size_t id) const {
        return _gammaQuery->eval(_successorGenerator, state, id);
    }

    template <template <typename> typename WaitingList, typename T>
    bool ExplicitWorklist::_genericSearch(WaitingList<WaitingState<T>> waiting) {
        ColoredEncoder encoder = ColoredEncoder{_net.getPlaces()};
        PassedList<ColoredEncoder, ColoredPetriNetMarking> passedList(encoder);

        const auto earlyTerminationCondition = _quantifier == Quantifier::EF;

        passedList.add(_net.initial());

        waiting.add(WaitingState<T> {
            _net.initial(),
            _successorGenerator.getInitSuccInfo<T>(),
            0
        });

        _searchStatistics.exploredStates = 1;
        _searchStatistics.discoveredStates = 1;

        if (_check(_net.initial(), 0) == earlyTerminationCondition) {
            _counterExampleId = 0;
            return _getResult(true, encoder.isFullStatespace());
        }
        if (_net.getTransitionCount() == 0) {
            return _getResult(false, encoder.isFullStatespace());
        }

        while (!waiting.empty()){
            WaitingState<T>& next = waiting.next();

            ColoredPetriNetMarking nextMarking = next.marking;

            std::optional<TraceMapStep> opt = _successorGenerator.next(nextMarking, next.succInfo, next.id);

            if constexpr (std::is_same_v<T, EvenSuccessorInfo>) {
                if (next.succInfo.shuffle) {
                    next.succInfo.shuffle = false;
                    waiting.shuffle();
                    continue;
                }
            }

            if (opt == std::nullopt) {
                waiting.remove();
                _successorGenerator.shrinkState(next.id);
                continue;
            }

            auto nextMarkingId = opt.value().id;
            nextMarking.shrink();
            if (!passedList.existsOrAdd(nextMarking)) {
                if (_createTrace) {
                    _stateMap.transitions.emplace(nextMarkingId, opt.value());
                }
                _searchStatistics.exploredStates += 1;
                if (_check(nextMarking, nextMarkingId) == earlyTerminationCondition) {
                    _searchStatistics.endWaitingStates = waiting.size();
                    _searchStatistics.biggestEncoding = encoder.getBiggestEncoding();
                    _counterExampleId = nextMarkingId;
                    return _getResult(true, encoder.isFullStatespace());
                }

                waiting.add(WaitingState<T> {
                    std::move(nextMarking),
                    _successorGenerator.getInitSuccInfo<T>(),
                    nextMarkingId
                });
                _searchStatistics.peakWaitingStates = std::max(waiting.size(), _searchStatistics.peakWaitingStates);
            }
        }
        _searchStatistics.endWaitingStates = waiting.size();
        _searchStatistics.biggestEncoding = encoder.getBiggestEncoding();
        return _getResult(false, encoder.isFullStatespace());
    }

    template<typename SuccessorGeneratorState>
    bool ExplicitWorklist::_search(const Strategy searchStrategy) {
        switch (searchStrategy) {
            case Strategy::DEFAULT:
            case Strategy::DFS:
                return _dfs<SuccessorGeneratorState>();
            case Strategy::BFS:
                return _bfs<SuccessorGeneratorState>();
            case Strategy::RDFS:
                return _rdfs<SuccessorGeneratorState>();
            case Strategy::HEUR:
                return _bestfs<SuccessorGeneratorState>();
            default:
                throw explicit_error(ExplicitErrorType::UNSUPPORTED_STRATEGY);
        }
    }

    template <typename T>
    bool ExplicitWorklist::_dfs() {
        return _genericSearch<DFSStructure, T>(DFSStructure<WaitingState<T>> {});
    }

    template <typename T>
    bool ExplicitWorklist::_bfs() {
        return _genericSearch<BFSStructure, T>(BFSStructure<WaitingState<T>> {});
    }

    template <typename T>
    bool ExplicitWorklist::_rdfs() {
        return _genericSearch<RDFSStructure, T>(RDFSStructure<WaitingState<T>>(_seed));
    }

    template <typename T>
    bool ExplicitWorklist::_bestfs() {
        return _genericSearch<BestFSStructure, T>(
            BestFSStructure<WaitingState<T>>(
                _seed,
                _gammaQuery,
                _quantifier == Quantifier::AG
                )
            );
    }

    bool ExplicitWorklist::_getResult(const bool found, const bool fullStatespace) const {
        Reachability::ResultPrinter::Result res;
        if (!found && !fullStatespace) {
            res = Reachability::ResultPrinter::Result::Unknown;
        }else {
            res = (
           (!found && _quantifier == Quantifier::AG) ||
           (found && _quantifier == Quantifier::EF))
               ? Reachability::ResultPrinter::Result::Satisfied
               : Reachability::ResultPrinter::Result::NotSatisfied;
        }
        return res == Reachability::ResultPrinter::Result::Satisfied;
    }
}

#endif //NAIVEWORKLIST_CPP