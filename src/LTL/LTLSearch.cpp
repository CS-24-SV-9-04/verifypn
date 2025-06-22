/* Copyright (C) 2020  Nikolaj J. Ulrik <nikolaj@njulrik.dk>,
 *                     Simon M. Virenfeldt <simon@simwir.dk>,
 *                     Peter G. Jensen <root@petergjoel.dk>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "LTL/LTLSearch.h"
#include "LTL/LTLValidator.h"
#include "LTL/SuccessorGeneration/Spoolers.h"
#include "LTL/SuccessorGeneration/Heuristics.h"
#include "LTL/SuccessorGeneration/SpoolingSuccessorGenerator.h"
#include "LTL/Algorithm/NestedDepthFirstSearch.h"
#include "LTL/Algorithm/TarjanModelChecker.h"

#include "PetriEngine/PQL/PredicateCheckers.h"
#include "PetriEngine/PQL/PQL.h"
#include "PetriEngine/PQL/Expressions.h"
#include "PetriEngine/options.h"

#include <utility>

using namespace PetriEngine::PQL;
using namespace PetriEngine;


namespace LTL {

    /**
     * Converts a formula on the form A f, E f or f into just f, assuming f is an LTL formula.
     * In the case E f, not f is returned, and in this case the model checking result should be negated
     * (indicated by bool in return value)
     * @param formula - a formula on the form A f, E f or f
     * @return @code(ltl_formula, should_negate) - ltl_formula is the formula f if it is a valid LTL formula, nullptr otherwise.
     * should_negate indicates whether the returned formula is negated (in the case the parameter was E f)
     */
    std::tuple<Condition_ptr, bool> to_ltl(const Condition_ptr &formula, std::vector<std::string>& hyper_traces) {
        LTL::LTLValidator validator;
        bool should_negate = false;
        Condition_ptr converted;
        if (auto _formula = dynamic_cast<PathQuant *> (formula.get())) {
            bool exists = false;
            bool all = false;
            do {
                all |= _formula->type() == type_id<AllPaths>();
                exists |= _formula->type() == type_id<ExistPath>();
                if(all && exists)
                    return {nullptr, false};
                hyper_traces.emplace_back(_formula->name());
                converted = static_cast<PathQuant*>(_formula)->child();
                _formula = dynamic_cast<PathQuant*>(converted.get());
            } while(_formula);
            if(exists)
                converted = std::make_shared<NotCondition>(converted);
            should_negate = exists;
        } else if (auto _formula = dynamic_cast<ECondition *> (formula.get())) {
            converted = std::make_shared<NotCondition>((*_formula)[0]);
            should_negate = true;
        } else if (auto _formula = dynamic_cast<ACondition *> (formula.get())) {
            converted = (*_formula)[0];
        } else if (auto _formula = dynamic_cast<AGCondition *> (formula.get())) {
            auto f = std::make_shared<ACondition>(std::make_shared<GCondition>((*_formula)[0]));
            return to_ltl(f, hyper_traces);
        } else if (auto _formula = dynamic_cast<AFCondition *> (formula.get())) {
            auto f = std::make_shared<ACondition>(std::make_shared<FCondition>((*_formula)[0]));
            return to_ltl(f, hyper_traces);
        }
        else if (auto _formula = dynamic_cast<EFCondition *> (formula.get())) {
            auto f = std::make_shared<ECondition>(std::make_shared<FCondition>((*_formula)[0]));
            return to_ltl(f, hyper_traces);
        }
        else if (auto _formula = dynamic_cast<EGCondition *> (formula.get())) {
            auto f = std::make_shared<ECondition>(std::make_shared<GCondition>((*_formula)[0]));
            return to_ltl(f, hyper_traces);
        }
        else if (auto _formula = dynamic_cast<AUCondition *> (formula.get())) {
            auto f = std::make_shared<ACondition>(std::make_shared<UntilCondition>((*_formula)[0], (*_formula)[1]));
            return to_ltl(f, hyper_traces);
        }
        else if (auto _formula = dynamic_cast<EUCondition *> (formula.get())) {
            auto f = std::make_shared<ECondition>(std::make_shared<UntilCondition>((*_formula)[0], (*_formula)[1]));
            return to_ltl(f, hyper_traces);
        }
        else {
            converted = formula;
        }
        Visitor::visit(validator, converted);
        if (validator.bad()) {
            converted = nullptr;
        }
        return std::make_pair(converted, should_negate);
    }

    std::unique_ptr<Heuristic> make_heuristic(const PetriNet& net,
        const Condition_ptr &negated_formula,
        const Structures::BuchiAutomaton& automaton,
        const Strategy search_strategy,
        const LTLHeuristic heuristics,
        const uint64_t seed) {
        if (search_strategy == Strategy::RDFS || heuristics == LTLHeuristic::RDFS) {
            return std::make_unique<RandomHeuristic>(seed);
        }
        if (search_strategy != Strategy::HEUR && search_strategy != Strategy::DEFAULT) {
            return nullptr;
        }
        switch (heuristics) {
            case LTLHeuristic::Distance:
                return std::make_unique<DistanceHeuristic>(&net, negated_formula);
            case LTLHeuristic::Automaton:
                return std::make_unique<AutomatonHeuristic>(&net, automaton);
            case LTLHeuristic::FireCount:
                return std::make_unique<LogFireCountHeuristic>(net.numberOfTransitions(), 5000);
            case LTLHeuristic::DFS:
            case LTLHeuristic::RDFS:
                return nullptr;
            default:
                throw base_error("Unknown LTL heuristics: ", to_underlying(heuristics));
        }
    }
}