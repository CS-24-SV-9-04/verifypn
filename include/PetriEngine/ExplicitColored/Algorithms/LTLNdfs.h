#ifndef LTLNDFS_H
#define LTLNDFS_H
#include <LTL/Structures/BuchiAutomaton.h>
#include <PetriEngine/ExplicitColored/AtomicTypes.h>
#include <PetriEngine/PQL/PQL.h>
#include <PetriEngine/ExplicitColored/ColoredPetriNet.h>

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
            const std::unordered_map<std::string, Transition_t>& transitionNameIndices
        );
        bool check();
        const SearchStatistics & GetSearchStatistics() const;
    private:
        LTL::Structures::BuchiAutomaton _buchiAutomaton;
        const ColoredPetriNet& _net;
        const std::unordered_map<std::string, uint32_t>& _placeNameIndices;
        const std::unordered_map<std::string, Transition_t>& _transitionNameIndices;
        ProductColorEncoder _productColorEncoder;
        PassedList<ProductColorEncoder, std::pair<ColoredPetriNetMarking, size_t>> _globalPassed;
        bool dfs(const ProductStateGenerator&,  ColoredPetriNetProductStateFixed);
        bool ndfs(const ProductStateGenerator&, ColoredPetriNetProductStateFixed);
        SearchStatistics _searchStatistics;
    };
}

#endif //LTLNDFS_H