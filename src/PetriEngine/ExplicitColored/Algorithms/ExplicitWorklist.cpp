#ifndef EXPLICITWORKLIST_CPP
#define EXPLICITWORKLIST_CPP

#include "PetriEngine/ExplicitColored/Algorithms/ExplicitWorklist.h"
#include <PetriEngine/options.h>
#include "PetriEngine/ExplicitColored/SuccessorGenerator/ColoredSuccessorGenerator.h"
#include "PetriEngine/PQL/PQL.h"
#include "PetriEngine/ExplicitColored/ColoredPetriNetMarking.h"
#include "PetriEngine/PQL/Visitor.h"
#include "PetriEngine/ExplicitColored/Algorithms/ColoredSearchTypes.h"
#include "PetriEngine/ExplicitColored/ExplicitErrors.h"

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
        _encoder(ColoredEncoder{_net.getPlaces()}),
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

    bool ExplicitWorklist::check(const Strategy searchStrategy, const ColoredSuccessorGeneratorOption coloredSuccessorGeneratorOption, const bool encodeWaitingList) {
        if (coloredSuccessorGeneratorOption == ColoredSuccessorGeneratorOption::FIXED) {
            return _search<ColoredPetriNetStateFixed>(searchStrategy, encodeWaitingList);
        }
        if (coloredSuccessorGeneratorOption == ColoredSuccessorGeneratorOption::EVEN) {
            return _search<ColoredPetriNetStateEven>(searchStrategy, encodeWaitingList);
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

    template <template <typename, bool> typename WaitingList, typename T, bool encodeWaitingList>
    bool ExplicitWorklist::_genericSearch(WaitingList<T, encodeWaitingList> waiting) {
        ptrie::set<uint8_t> passed;
        const auto& initialState = _net.initial();
        const auto earlyTerminationCondition = _quantifier == Quantifier::EF;

        auto size = _encoder.encode(initialState);
        if (!_encoder.testEncodingDecoding(initialState)) {
            std::cout << "En/decoding is incorrect" << std::endl;
        }

        passed.insert(_encoder.data(), size);
        if constexpr (std::is_same_v<T, ColoredPetriNetStateEven>) {
            auto initial = ColoredPetriNetStateEven{initialState, _net.getTransitionCount()};
            initial.id = 0;
            initial.addEncoding(_encoder.data(), size);
            waiting.add(std::move(initial));
        } else {
            auto initial = ColoredPetriNetStateFixed{initialState};
            initial.id = 0;
            initial.addEncoding(_encoder.data(), size);
            waiting.add(std::move(initial));
        }

        _searchStatistics.exploredStates = 1;
        _searchStatistics.discoveredStates = 1;

        if (_check(initialState, 0) == earlyTerminationCondition) {
            _counterExampleId = 0;
            return _getResult(true, _encoder.isFullStatespace());
        }
        if (_net.getTransitionCount() == 0) {
            return _getResult(false, _encoder.isFullStatespace());
        }

        while (!waiting.empty()){
            auto& next = waiting.next();
            auto [successor, traceStep] = _successorGenerator.next(next);
            if (next.done()) {
                waiting.remove();
                _successorGenerator.shrinkState(next.id);
                continue;
            }

            if constexpr (std::is_same_v<T, ColoredPetriNetStateEven>) {
                if (next.shuffle){
                    next.shuffle = false;
                    waiting.shuffle();
                    continue;
                }
            }

            successor.shrink();
            const auto& marking = successor.marking;
            size = _encoder.encode(marking);
            _searchStatistics.discoveredStates++;
            if (!passed.exists(_encoder.data(), size).first) {
                if (_createTrace) {
                    _stateMap.transitions.emplace(successor.id, traceStep);
                }
                _searchStatistics.exploredStates += 1;
                if (_check(marking, successor.id) == earlyTerminationCondition) {
                    _searchStatistics.endWaitingStates = waiting.size();
                    _searchStatistics.biggestEncoding = _encoder.getBiggestEncoding();
                    _counterExampleId = successor.id;
                    return _getResult(true, _encoder.isFullStatespace());
                }
                successor.addEncoding(_encoder.data(), size);
                waiting.add(std::move(successor));
                passed.insert(_encoder.data(), size);
                _searchStatistics.peakWaitingStates = std::max(waiting.size(), _searchStatistics.peakWaitingStates);
            }
        }
        _searchStatistics.endWaitingStates = waiting.size();
        _searchStatistics.biggestEncoding = _encoder.getBiggestEncoding();
        return _getResult(false, _encoder.isFullStatespace());
    }

    template<typename SuccessorGeneratorState>
    bool ExplicitWorklist::_search(const Strategy searchStrategy, const bool encodeWaitingList) {
        switch (searchStrategy) {
            case Strategy::DEFAULT:
            case Strategy::DFS:
                return _dfs<SuccessorGeneratorState>(encodeWaitingList);
            case Strategy::BFS:
                return _bfs<SuccessorGeneratorState>(encodeWaitingList);
            case Strategy::RDFS:
                return _rdfs<SuccessorGeneratorState>(encodeWaitingList);
            case Strategy::HEUR:
                return _bestfs<SuccessorGeneratorState>(encodeWaitingList);
            default:
                throw explicit_error(ExplicitErrorType::UNSUPPORTED_STRATEGY);
        }
    }

    template <typename T>
    bool ExplicitWorklist::_dfs(const bool encodeWaitingList) {
        if (encodeWaitingList) {
            return _genericSearch<DFSStructure>(DFSStructure<T, true>{_encoder});
        }
        return _genericSearch<DFSStructure>(DFSStructure<T, false>{_encoder});
    }

    template <typename T>
    bool ExplicitWorklist::_bfs(const bool encodeWaitingList) {
        if (encodeWaitingList) {
            return _genericSearch<BFSStructure>(BFSStructure<T, true>{_encoder});
        }
        return _genericSearch<BFSStructure>(BFSStructure<T, false>{_encoder});
    }

    template <typename T>
    bool ExplicitWorklist::_rdfs(const bool encodeWaitingList) {
        if (encodeWaitingList) {
            return _genericSearch<RDFSStructure>(RDFSStructure<T, true>(_encoder, _seed));
        }
        return _genericSearch<RDFSStructure>(RDFSStructure<T, false>(_encoder, _seed));
    }

    template <typename T>
    bool ExplicitWorklist::_bestfs(const bool encodeWaitingList) {
        if (encodeWaitingList) {
            std::cout << "Heuristic search does not support encoding the waiting list" << std::endl;
            return _genericSearch<BestFSStructure>(
            BestFSStructure<T, true>(
                _encoder,
                _seed,
                _gammaQuery,
                _quantifier == Quantifier::AG
                )
            );
        }
        return _genericSearch<BestFSStructure>(
            BestFSStructure<T, false>(
                _encoder,
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