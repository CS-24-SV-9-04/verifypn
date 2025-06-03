#ifndef LTLNDFS_H
#define LTLNDFS_H
#include <LTL/Structures/BuchiAutomaton.h>
#include <PetriEngine/ExplicitColored/AtomicTypes.h>
#include <PetriEngine/PQL/PQL.h>
#include <PetriEngine/ExplicitColored/ColoredPetriNet.h>
#include <PetriEngine/ExplicitColored/ColoredResultPrinter.h>

#include "SearchStatistics.h"
#include "PetriEngine/ExplicitColored/SuccessorGenerator/ProductStateGenerator.h"
#include "PetriEngine/ExplicitColored/PassedList.h"
#include "PetriEngine/ExplicitColored/ProductColorEncoder.h"

namespace PetriEngine::ExplicitColored {
    class LTLNdfs {
    public:
        LTLNdfs(
            const ColoredPetriNet& net,
            const PQL::Condition_ptr& condition,
            const std::unordered_map<std::string, uint32_t>& placeNameIndices,
            const std::unordered_map<std::string, Transition_t>& transitionNameIndices,
            LTL::BuchiOptimization buchOptimization,
            LTL::APCompression apCompression
        );
        Result check();
        const SearchStatistics& GetSearchStatistics() const;
    private:
        bool _dfs(CPNProductState initialState);
        bool _ndfs(CPNProductState initialState);
        LTL::Structures::BuchiAutomaton _buchiAutomaton;
        const ColoredPetriNet& _net;
        const std::unordered_map<std::string, uint32_t>& _placeNameIndices;
        const std::unordered_map<std::string, Transition_t>& _transitionNameIndices;
        ProductColorEncoder _productColorEncoder;
        PassedList<ProductColorEncoder, std::pair<ColoredPetriNetMarking, size_t>> _globalPassed;
        ProductStateGenerator _productStateGenerator;
        SearchStatistics _searchStatistics;
        bool _isFullStateSpace = true;
    };
}

#endif //LTLNDFS_H