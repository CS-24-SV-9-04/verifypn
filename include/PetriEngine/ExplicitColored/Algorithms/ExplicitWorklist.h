#ifndef NAIVEWORKLIST_H
#define NAIVEWORKLIST_H

#include <PetriEngine/ExplicitColored/ExpressionCompilers/GammaQueryCompiler.h>
#include <PetriEngine/options.h>
#include "PetriEngine/ExplicitColored/ColoredPetriNet.h"
#include "PetriEngine/ExplicitColored/ColoredResultPrinter.h"
#include "PetriEngine/ExplicitColored/Algorithms/SearchStatistics.h"
#include "PetriEngine/ExplicitColored/SuccessorGenerator/ColoredSuccessorGenerator.h"
#include "PetriEngine/ExplicitColored/ColoredEncoder.h"

namespace PetriEngine::ExplicitColored {
    template <typename T>
    class RDFSStructure;
    class ColoredResultPrinter;

    enum class Quantifier {
        EF,
        AG
    };

    class ExplicitWorklist {
    public:
        ExplicitWorklist(
            const ColoredPetriNet& net,
            const PQL::Condition_ptr& query,
            const std::unordered_map<std::string, uint32_t>& placeNameIndices,
            const std::unordered_map<std::string, Transition_t>& transitionNameIndices,
            size_t seed
        );

        bool check(Strategy searchStrategy, ColoredSuccessorGeneratorOption coloredSuccessorGeneratorOption);
        [[nodiscard]] const SearchStatistics& GetSearchStatistics() const;
    private:
        std::shared_ptr<CompiledGammaQueryExpression> _gammaQuery;
        Quantifier _quantifier;
        const ColoredPetriNet& _net;
        const ColoredSuccessorGenerator _successorGenerator;
        const size_t _seed;
        bool _fullStatespace = true;
        SearchStatistics _searchStatistics;
        template <typename SuccessorGeneratorState>
        [[nodiscard]] bool _search(Strategy searchStrategy);
        [[nodiscard]] bool _check(const ColoredPetriNetMarking& state, size_t id) const;

        template <typename T>
        [[nodiscard]] bool _dfs();
        template <typename T>
        [[nodiscard]] bool _bfs();
        template <typename T>
        [[nodiscard]] bool _rdfs();
        template <typename T>
        [[nodiscard]] bool _bestfs();

        template <template <typename> typename WaitingList, typename T>
        [[nodiscard]] bool _genericSearch(WaitingList<T> waiting);
        [[nodiscard]] bool _getResult(bool found, bool fullStatespace) const;
    };
}

#endif //NAIVEWORKLIST_H
