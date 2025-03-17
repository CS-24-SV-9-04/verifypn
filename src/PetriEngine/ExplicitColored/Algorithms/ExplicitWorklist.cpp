#ifndef EXPLICITWORKLIST_CPP
#define EXPLICITWORKLIST_CPP

#include "PetriEngine/ExplicitColored/Algorithms/ExplicitWorklist.h"
#include <PetriEngine/options.h>
#include "PetriEngine/ExplicitColored/ColoredSuccessorGenerator.h"
#include "PetriEngine/PQL/PQL.h"
#include "PetriEngine/ExplicitColored/ColoredPetriNetMarking.h"
#include "PetriEngine/PQL/Visitor.h"
#include "PetriEngine/ExplicitColored/Algorithms/ColoredSearchTypes.h"
#include "PetriEngine/ExplicitColored/FireabilityChecker.h"
#include "PetriEngine/ExplicitColored/ExplicitErrors.h"

namespace PetriEngine::ExplicitColored {
    ExplicitWorklist::ExplicitWorklist(
        const ColoredPetriNet& net,
        const PQL::Condition_ptr &query,
        const std::unordered_map<std::string, uint32_t>& placeNameIndices,
        const std::unordered_map<std::string, Transition_t>& transitionNameIndices,
        const IColoredResultPrinter& coloredResultPrinter,
        const size_t seed
    ) : _net(std::move(net)),
        _successorGenerator(ColoredSuccessorGenerator{_net}),
        _seed(seed),
        _coloredResultPrinter(coloredResultPrinter)
    {
        const GammaQueryCompiler queryCompiler(placeNameIndices, transitionNameIndices, _successorGenerator);
        if (const auto efGammaQuery = dynamic_cast<PQL::EFCondition*>(query.get())) {
            _quantifier = Quantifier::EF;
            _gammaQuery = queryCompiler.compile(efGammaQuery->getCond());
        } else if (const auto agGammaQuery = dynamic_cast<PQL::AGCondition*>(query.get())) {
            _quantifier = Quantifier::AG;
            _gammaQuery = queryCompiler.compile(agGammaQuery->getCond());
        } else {
            throw explicit_error{unsupported_query};
        }
    }

    bool ExplicitWorklist::check(const SearchStrategy searchStrategy, const ColoredSuccessorGeneratorOption colored_successor_generator_option) {
        if (colored_successor_generator_option == ColoredSuccessorGeneratorOption::FIXED) {
            return _search<ColoredPetriNetStateFixed>(searchStrategy);
        }
        if (colored_successor_generator_option == ColoredSuccessorGeneratorOption::EVEN) {
            return _search<ColoredPetriNetStateEven>(searchStrategy);
        }
        throw explicit_error(unsupported_generator);
    }

    const SearchStatistics & ExplicitWorklist::GetSearchStatistics() const {
        return _searchStatistics;
    }

    bool ExplicitWorklist::_check(const ColoredPetriNetMarking& state, size_t id) const {
        return _gammaQuery->eval(_successorGenerator, state, id);
    }

    template <template <typename> typename WaitingList, typename T>
    bool ExplicitWorklist::_genericSearch(WaitingList<T> waiting) {
        ptrie::set<uint8_t> passed;
        ColoredEncoder encoder = ColoredEncoder{_net.getPlaces()};
        waiting.setEncoder(encoder);
        const auto& initialState = _net.initial();
        const auto earlyTerminationCondition = _quantifier == Quantifier::EF;

        auto size = encoder.encode(initialState);
        passed.insert(encoder.data(), size);
        if constexpr (std::is_same_v<T, ColoredPetriNetStateEven>) {
            auto initial = ColoredPetriNetStateEven{initialState, _net.getTransitionCount()};
            initial.id = 0;
            initial.addEncoding(encoder.data(), size);
            waiting.add(std::move(initial));
        } else {
            auto initial = ColoredPetriNetStateFixed{initialState};
            initial.id = 0;
            initial.addEncoding(encoder.data(), size);
            waiting.add(std::move(initial));
        }

        _searchStatistics.passedCount = 1;
        _searchStatistics.checkedStates = 1;

        if (_check(initialState, 0) == earlyTerminationCondition) {
            encoder.printBiggestEncoding();
            return _getResult(true, encoder.isFullStatespace());
        }
        if (_net.getTransitionCount() == 0) {
            encoder.printBiggestEncoding();
            return _getResult(false, encoder.isFullStatespace());
        }

        while (!waiting.empty()){
            auto& next = waiting.next();
            auto successor = _successorGenerator.next(next);
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
            size = encoder.encode(marking);
            _searchStatistics.exploredStates++;
            if (!passed.exists(encoder.data(), size).first) {
                _searchStatistics.checkedStates += 1;
                if (_check(marking, successor.id) == earlyTerminationCondition) {
                    _searchStatistics.endWaitingStates = waiting.size();
                    encoder.printBiggestEncoding();
                    return _getResult(true, encoder.isFullStatespace());
                }
                successor.addEncoding(encoder.data(), size);
                waiting.add(std::move(successor));
                passed.insert(encoder.data(), size);
                _searchStatistics.passedCount += 1;
                _searchStatistics.peakWaitingStates = std::max(waiting.size(), _searchStatistics.peakWaitingStates);
            }
        }
        encoder.printBiggestEncoding();
        _searchStatistics.endWaitingStates = waiting.size();
        return _getResult(false, encoder.isFullStatespace());
    }

    template<typename SuccessorGeneratorState>
    bool ExplicitWorklist::_search(const SearchStrategy searchStrategy) {
        switch (searchStrategy) {
            case SearchStrategy::DFS:
                return _dfs<SuccessorGeneratorState>();
            case SearchStrategy::BFS:
                return _bfs<SuccessorGeneratorState>();
            case SearchStrategy::RDFS:
                return _rdfs<SuccessorGeneratorState>();
            case SearchStrategy::HEUR:
                return _bestfs<SuccessorGeneratorState>();
            default:
                throw base_error("Unsupported exploration type");
        }
    }

    template <typename T>
    bool ExplicitWorklist::_dfs() {
        return _genericSearch<DFSStructure>(DFSStructure<T> {});
    }

    template <typename T>
    bool ExplicitWorklist::_bfs() {
        return _genericSearch<BFSStructure>(BFSStructure<T> {});
    }

    template <typename T>
    bool ExplicitWorklist::_rdfs() {
        return _genericSearch<RDFSStructure>(RDFSStructure<T>(_seed));
    }

    template <typename T>
    bool ExplicitWorklist::_bestfs() {
        return _genericSearch<BestFSStructure>(
            BestFSStructure<T>(
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
        _coloredResultPrinter.printResults(_searchStatistics, res);
        return res == Reachability::ResultPrinter::Result::Satisfied;
    }
}

#endif //NAIVEWORKLIST_CPP