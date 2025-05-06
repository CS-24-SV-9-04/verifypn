#ifndef NAIVEWORKLIST_H
#define NAIVEWORKLIST_H

#include <PetriEngine/ExplicitColored/ExpressionCompilers/ExplicitQueryPropositionCompiler.h>
#include <PetriEngine/options.h>
#include "PetriEngine/ExplicitColored/ColoredPetriNet.h"
#include "PetriEngine/ExplicitColored/ColoredResultPrinter.h"
#include "PetriEngine/ExplicitColored/Algorithms/SearchStatistics.h"
#include "PetriEngine/ExplicitColored/SuccessorGenerator/ColoredSuccessorGenerator.h"
#include "PetriEngine/ExplicitColored/ColoredEncoder.h"

namespace PetriEngine::ExplicitColored {
    enum class Quantifier {
        EF,
        AG
    };

    struct StateMap {
        std::unordered_map<uint32_t, TraceMapStep> transitions;
    };

    struct InternalTraceStep {
        Binding_t binding;
        Transition_t transition;
    };

    class ExplicitWorklist {
    public:
        ExplicitWorklist(
            const ColoredPetriNet& net,
            const PQL::Condition_ptr& query,
            const std::unordered_map<std::string, uint32_t>& placeNameIndices,
            const std::unordered_map<std::string, Transition_t>& transitionNameIndices,
            size_t seed,
            bool createTrace
        );

        bool check(Strategy searchStrategy, ColoredSuccessorGeneratorOption coloredSuccessorGeneratorOption, bool encodeWaitingList);
        [[nodiscard]] const SearchStatistics& GetSearchStatistics() const;
        std::optional<uint64_t> getCounterExampleId() const;
        std::optional<std::vector<InternalTraceStep>> getTraceTo(uint64_t counterExampleId) const;
    private:
        std::shared_ptr<ExplicitQueryProposition> _gammaQuery;
        std::optional<uint64_t> _counterExampleId;
        Quantifier _quantifier;
        const ColoredPetriNet& _net;
        const ColoredSuccessorGenerator _successorGenerator;
        const size_t _seed;
        bool _fullStatespace = true;
        bool _createTrace;
        StateMap _stateMap;
        SearchStatistics _searchStatistics;
        ColoredEncoder _encoder;
        template<typename SuccessorGeneratorState>
        [[nodiscard]] bool _search(Strategy searchStrategy, bool encodeWaitingList);
        [[nodiscard]] bool _check(const ColoredPetriNetMarking& state, size_t id) const;

        template <typename T>
        [[nodiscard]] bool _dfs(bool encodeWaitingList);
        template <typename T>
        [[nodiscard]] bool _bfs(bool encodeWaitingList);
        template <typename T>
        [[nodiscard]] bool _rdfs(bool encodeWaitingList);
        template <typename T>
        [[nodiscard]] bool _bestfs(bool encodeWaitingList);

        template <template <typename, bool> typename WaitingList, typename T, bool encodeWaitingList>
        [[nodiscard]] bool _genericSearch(WaitingList<T, encodeWaitingList> waiting);
        [[nodiscard]] bool _getResult(bool found, bool fullStatespace) const;
    };
}

#endif //NAIVEWORKLIST_H
