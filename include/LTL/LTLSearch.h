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

/*
 * File:   LTLSearch.h
 * Author: Peter G. Jensen
 *
 * Created on 16 February 2022, 16.10
 */

#ifndef LTLSEARCH_H
#define LTLSEARCH_H

#include "PetriEngine/PetriNet.h"
#include "PetriEngine/PQL/PQL.h"
#include "PetriEngine/options.h"
#include "LTL/Structures/BuchiAutomaton.h"
#include "LTLOptions.h"
#include "LTLValidator.h"
#include "Algorithm/ModelChecker.h"
#include "Algorithm/NestedDepthFirstSearch.h"
#include "Algorithm/TarjanModelChecker.h"

namespace LTL {
    std::tuple<PetriEngine::PQL::Condition_ptr, bool> to_ltl(const PetriEngine::PQL::Condition_ptr &formula, std::vector<std::string>& hyper_traces);
    std::unique_ptr<Heuristic> make_heuristic(const PetriEngine::PetriNet& net,
            const PetriEngine::PQL::Condition_ptr &negated_formula,
            const Structures::BuchiAutomaton& automaton,
            const Strategy search_strategy = Strategy::HEUR,
            const LTLHeuristic heuristics = LTLHeuristic::Automaton,
            const uint64_t seed = 0);

    template<typename N>
    class LTLSearch {
    private:
        const PetriNetDataType<N>& _net;
        const PetriEngine::PQL::Condition_ptr _query;
        Structures::BuchiAutomaton _buchi;
        std::vector<std::string> _traces;
        PetriEngine::PQL::Condition_ptr _negated_formula;
        bool _negated_answer = false;
        APCompression _compression;
        std::unique_ptr<ModelChecker<N>> _checker;
        std::unique_ptr<Heuristic> _heuristic;
        bool _result;

    public:
        LTLSearch(const PetriNetDataType<N>& net,
        const PetriEngine::PQL::Condition_ptr &query, const BuchiOptimization optimization, const APCompression compression)
        : _net(net), _query(query), _compression(compression) {
            if(!LTLValidator().isLTL(query))
            {
                std::stringstream ss;
                query->toString(ss);
                throw base_error("Formula is not in supported LTL or HyperLTL fragment: ", ss.str());
            }
            _traces.clear();
            std::tie(_negated_formula, _negated_answer) = to_ltl(query, _traces);
            _buchi = make_buchi_automaton(_negated_formula, optimization, compression);
        }

        bool solve(
                const bool trace,
                const uint64_t k_bound = 0,
                const Algorithm algorithm = Algorithm::Tarjan,
                LTLPartialOrder por = LTLPartialOrder::Automaton,
                const Strategy search_strategy = Strategy::HEUR,
                const LTLHeuristic heuristics_flag = LTLHeuristic::Automaton,
                const bool utilize_weak = true,
                const uint64_t seed = 0) {
            if constexpr (std::is_same_v<N, PetriEngine::PetriNet>) {
                _heuristic = make_heuristic(_net, _negated_formula, _buchi, search_strategy, heuristics_flag, seed);
            } else {
                _heuristic = nullptr;
            }

            switch (algorithm) {
                case Algorithm::NDFS:
                {
                    _checker = std::make_unique<NestedDepthFirstSearch<N>>(_net, _negated_formula, _buchi, k_bound, _traces.size());
                    break;
                }
                case Algorithm::Tarjan:
                    if constexpr (std::is_same_v<N, PetriEngine::PetriNet>) {
                        _checker = std::make_unique<TarjanModelChecker>(_net, _negated_formula, _buchi, k_bound, _traces.size());
                        break;
                    }
                case Algorithm::None:
                    default:
                        assert(false);
                std::cerr << "Error: cannot LTL verify with algorithm None";
            }
            _checker->set_utilize_weak(utilize_weak);
            _checker->set_heuristic(_heuristic.get());
            _checker->set_partial_order(por);
            _checker->set_tracing(trace);
            _result = _checker->check();
            return _result xor _negated_answer;
        }

        void print_buchi(std::ostream& out, const BuchiOutType type = BuchiOutType::Dot)
        {
            if(_compression != APCompression::None)
                throw base_error("Printing of Büchi automata only supported with APCompression::None");
            _buchi.output_buchi(out, type);
        }

        void print_stats(std::ostream& out) {
            _checker->print_stats(out);
        }

        LTLPartialOrder used_partial_order() const {
            return _checker->used_partial_order();
        }

        bool is_weak() const {
            return _checker->is_weak();
        }

        size_t max_tokens() const {
            return _checker->max_tokens();
        }

        size_t configurations() const {
            return _checker->get_configurations();
        }

        size_t discovered() const {
            return _checker->get_discovered();
        }

        size_t markings() const {
            return _checker->get_markings();
        }

        size_t explored() const {
            return _checker->get_explored();
        }

        std::string heuristic_type() const {
            std::stringstream ss;
            if(_heuristic)
                _heuristic->output(ss);
            return ss.str();
        }

        bool print_trace(std::ostream& out, const PetriEngine::Reducer& reducer) const {
            if(!_result)
            {
                _print_trace(reducer, out);
                return true;
            }
            else
                return false;
        }

        const std::vector<std::vector<uint32_t>>& raw_trace() const { return _checker->trace(); }

    private:
        void _print_trace(const PetriEngine::Reducer& reducer, std::ostream& os) const {

            const auto& trace = _checker->trace();
            const size_t ntraces = _traces.empty() ? 1 : _traces.size();
            std::string tindent = ntraces <= 1 ? "" : "  ";
            std::string indent = tindent + "  ";
            std::string token_indent = indent + "  ";
            if(!_traces.empty())
                os << "<trace-list>\n";
            for(size_t j = 0; j < ntraces; ++j)
            {
                bool printed_deadlock = false;
                os << tindent << "<trace";
                if(!_traces.empty())
                    os << " name=\"" << _traces[j] << "\"";
                os << ">\n";
                reducer.initFire(os);
                for (size_t i = 0; i < trace.size(); ++i) {
                    if (i == _checker->loop_index())
                    {
                        if(trace[i][j] < std::numeric_limits<ptrie::uint>::max() - 1) // otherwise it is a deadlock.
                            os << indent << "<loop/>\n";
                    }
                    assert(trace[i].size() == ntraces);
                    print_transition(trace[i][j], reducer, os, indent, token_indent, printed_deadlock);
                }
                os << std::endl << tindent << "</trace>" << std::endl;
            }
            if(!_traces.empty())
                os << "</trace-list>\n";
        }
        std::ostream &
            print_transition(uint32_t transition, const PetriEngine::Reducer& reducer, std::ostream &os, const std::string& _indent, const std::string& _token_indent, bool& printed_deadlock) const {
            if (transition >= std::numeric_limits<ptrie::uint>::max() - 1) {
                if(!printed_deadlock)
                    os << _indent << "<deadlock/>";
                printed_deadlock = true;
                return os;
            }

            os << _indent << "<transition id="
                    // field width stuff obsolete without büchi state printing.
                    << std::quoted(*_net.transitionNames()[transition]);
            os << ">\n";
            reducer.tokenConsumption(os, *_net.transitionNames()[transition]);
            os << std::endl;
            os << _indent << "</transition>\n";
            reducer.postFire(os, *_net.transitionNames()[transition]);
            return os;
        }

    };

}


#endif /* LTLSEARCH_H */

