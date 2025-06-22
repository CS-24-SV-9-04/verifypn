/* Copyright (C) 2020  Nikolaj J. Ulrik <nikolaj@njulrik.dk>,
 *                     Simon M. Virenfeldt <simon@simwir.dk>
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

#ifndef VERIFYPN_NESTEDDEPTHFIRSTSEARCH_H
#define VERIFYPN_NESTEDDEPTHFIRSTSEARCH_H

#include <LTL/Structures/CompoundStateSet.h>
#include <LTL/SuccessorGeneration/CompoundGenerator.h>
#include <PetriEngine/ExplicitColored/SuccessorGenerator/CPNProductSuccessorGenerator.h>

#include "ModelChecker.h"
#include "PetriEngine/Structures/StateSet.h"
#include "PetriEngine/Structures/State.h"
#include "PetriEngine/Structures/Queue.h"
#include "utils/structures/light_deque.h"
#include "LTL/Structures/ProductStateFactory.h"
#include <PetriEngine/ExplicitColored/CPNProductStateSet.h>
#include <ptrie/ptrie_map.h>

namespace LTL {

    /**
     * Implement the nested DFS algorithm for LTL model checking. Roughly based on versions given in
     * <p>
     *   Jaco Geldenhuys & Antti Valmari,<br>
     *   More efficient on-the-fly LTL verification with Tarjan's algorithm,<br>
     *   https://doi.org/10.1016/j.tcs.2005.07.004
     * </p>
     * and
     * <p>
     *   Gerard J. Holzmann, Doron Peled, and Mihalis Yannakakis<br>
     *   On Nested Depth First Search<br>
     *   https://spinroot.com/gerard/pdf/inprint/spin96.pdf
     * </p>
     */
    template<typename N>
    class NestedDepthFirstSearch : public ModelChecker<N> {
    public:
        NestedDepthFirstSearch(const PetriNetDataType<N>& net, const PetriEngine::PQL::Condition_ptr &query,
                               const Structures::BuchiAutomaton &buchi, uint32_t kbound, uint32_t hyper_traces)
                : ModelChecker<N>(net, query,  buchi), _kbound(kbound), _hyper_traces(hyper_traces == 0 ? 1 : hyper_traces) {}

        virtual bool check() {
            if constexpr (std::is_same_v<N, PetriEngine::PetriNet>) {
                if(ModelChecker<N>::_heuristic)
                {
                    if(_hyper_traces > 1)
                        throw base_error("Hyper-LTL with heuristics or partial order reduction not yet enabled.");
                    SpoolingSuccessorGenerator gen(ModelChecker<N>::_net, ModelChecker<N>::_formula);
                    EnabledSpooler spooler(ModelChecker<N>::_net, gen);
                    gen.set_spooler(spooler);
                    gen.set_heuristic(ModelChecker<N>::_heuristic);
                    return check_with_generator(gen);
                } else {
                    if(_hyper_traces <= 1)
                    {
                        ResumingSuccessorGenerator gen(ModelChecker<N>::_net);
                        return check_with_generator(gen);
                    }
                    else
                    {
                        CompoundGenerator gen(ModelChecker<N>::_net, _hyper_traces);
                        return check_with_generator(gen);
                    }
                }
            } else {
                return check_cpn_with_initial_state();
            }
        }

        void print_stats(std::ostream &os) const override {
            ModelChecker<N>::print_stats(os, _discovered, _max_tokens);
        }

        virtual size_t max_tokens() const override {
            return _max_tokens;
        }

        virtual size_t get_discovered() const override {
            return _discovered;
        }

        virtual size_t get_markings() const override {
            return _markings;
        }

        virtual size_t get_configurations() const override {
            return _configurations;
        }

    private:
        using State = LTL::Structures::ProductState;

        ptrie::map<size_t, uint8_t> _markers;
        static constexpr uint8_t MARKER1 = 1;
        static constexpr uint8_t MARKER2 = 2;
        size_t _mark_count[3] = {0,0,0};
        const uint32_t _kbound = 0;
        const uint32_t _hyper_traces = 0;
        size_t _discovered = 0;
        size_t _max_tokens = 0;
        size_t _markings = 0;
        size_t _configurations = 0;

        template<typename T>
        struct stack_entry_t {
            size_t _id;
            typename T::successor_info_t _sucinfo;
        };

        template<typename S>
        std::pair<bool,size_t> mark(S& states, State& state, uint8_t MARKER) {
            // technically we could decorate the states here instead of
            // maintaining the index twice in the _mark_count.
            // this would also spare us one ptrie lookup.
            auto[_, stateid, data_id] = states.add(state);
            if (stateid == std::numeric_limits<size_t>::max()) {
                return std::make_pair(false, stateid);
            }

            auto& r = states.get_data(data_id);
            const bool is_new = (r & MARKER) == 0;
            if(is_new)
            {
                r = (MARKER | r);
                ++_mark_count[MARKER];
            }
            return std::make_pair(is_new, stateid);
        }

        template<typename S>
        std::pair<bool,size_t> mark(S& states, PetriEngine::ExplicitColored::CPNProductState& state, uint8_t MARKER) {
            // technically we could decorate the states here instead of
            // maintaining the index twice in the _mark_count.
            // this would also spare us one ptrie lookup.
            auto[_, stateid, data_id] = states.add(state);
            if (stateid == std::numeric_limits<size_t>::max()) {
                return std::make_pair(false, stateid);
            }

            auto& r = states.get_data(data_id);
            const bool is_new = (r & MARKER) == 0;
            if(is_new)
            {
                r = (MARKER | r);
                ++_mark_count[MARKER];
            }
            return std::make_pair(is_new, stateid);
        }

        template<typename G>
        bool check_with_generator(G& gen) {
            ProductSuccessorGenerator prod_gen(ModelChecker<N>::_net, ModelChecker<N>::_buchi, gen);
            if constexpr (std::is_same<G,CompoundGenerator>::value) {
                Structures::CompoundStateSet<ptrie::map<Structures::stateid_t, uint8_t>> states(ModelChecker<N>::_net, _hyper_traces, _kbound);
                dfs(prod_gen, states);
                _discovered = states.discovered();
                _max_tokens = states.max_tokens();
                _configurations = states.configurations();
                _markings = states.markings();
            }
            else
            {
                LTL::Structures::BitProductStateSet<ptrie::map<Structures::stateid_t, uint8_t>> states(ModelChecker<N>::_net, _kbound);
                dfs(prod_gen, states);
                _discovered = states.discovered();
                _max_tokens = states.max_tokens();
                _configurations = states.configurations();
                _markings = states.markings();
            }
            return !ModelChecker<N>::_violation;
        }

        bool check_cpn_with_initial_state() {
            using namespace PetriEngine::ExplicitColored;
            CPNProductStateGenerator<FixedSuccessorInfo> prod_gen(ModelChecker<N>::_net, ModelChecker<N>::_buchi);
            CPNProductStateSet states(ModelChecker<N>::_net.cpn, _kbound);

            dfs(prod_gen, states);
            return !ModelChecker<N>::_violation;
        }

        template<typename SG, typename S>
        void dfs(SG& successor_generator, S& states, size_t init) {
            light_deque<stack_entry_t<SG>> todo;
            light_deque<stack_entry_t<SG>> nested_todo;

            auto working = this->_factory.new_state(_hyper_traces);
            auto curState = this->_factory.new_state(_hyper_traces);

            todo.push_back(stack_entry_t<SG>{init, successor_generator.initial_suc_info()});

            while (!todo.empty()) {
                auto &top = todo.back();
                states.decode(curState, top._id);
                successor_generator.prepare(&curState, top._sucinfo);
                if (top._sucinfo.has_prev_state()) {
                    states.decode(working, top._sucinfo._last_state);
                }
                if (!successor_generator.next(working, top._sucinfo)) {
                    // no successor
                    if (successor_generator.is_accepting(curState)) {
                        if(successor_generator.has_invariant_self_loop(curState))
                            ModelChecker<N>::_violation = true;
                        else
                            ndfs<SG, S, decltype(working)>(successor_generator, states, curState, nested_todo);
                        if (ModelChecker<N>::_violation) {
                            if(ModelChecker<N>::_build_trace)
                                build_trace(todo, nested_todo);
                            return;
                        }
                    }
                    todo.pop_back();
                } else {
                    auto [is_new, stateid] = mark(states, working, MARKER1);
                    if (stateid == std::numeric_limits<size_t>::max()) {
                        continue;
                    }
                    top._sucinfo._last_state = stateid;
                    if (is_new) {
                        if(ModelChecker<N>::_shortcircuitweak &&
                           successor_generator.is_accepting(curState) &&
                           successor_generator.has_invariant_self_loop(curState))
                        {
                            ModelChecker<N>::_violation = true;
                            if(ModelChecker<N>::_build_trace)
                                build_trace(todo, nested_todo);
                            return;
                        }
                        todo.push_back(stack_entry_t<SG>{stateid, successor_generator.initial_suc_info()});
                    }
                }
            }
        }

        template<typename SG, typename S>
        void dfs(SG& successor_generator, S& states) {
            auto initial_states = successor_generator.make_initial_state();
            for (auto &state : initial_states) {
                auto res = states.add(state);
                if (std::get<0>(res)) {
                    dfs<SG, S>(successor_generator, states, std::get<1>(res));
                    if(ModelChecker<N>::_violation)
                        break;
                }
            }
        }

        template<typename SG, typename S, typename State>
        void ndfs(SG& successor_generator, S& states, const State &state, light_deque<stack_entry_t<SG>>& nested_todo) {

            auto working = ModelChecker<N>::_factory.new_state(_hyper_traces);
            auto curState = ModelChecker<N>::_factory.new_state(_hyper_traces);

            nested_todo.push_back(stack_entry_t<SG>{std::get<1>(states.add(state)), successor_generator.initial_suc_info()});

            while (!nested_todo.empty()) {
                auto &top = nested_todo.back();
                states.decode(curState, top._id);
                successor_generator.prepare(&curState, top._sucinfo);
                if (top._sucinfo.has_prev_state()) {
                    states.decode(working, top._sucinfo._last_state);
                }
                if (!successor_generator.next(working, top._sucinfo)) {
                    nested_todo.pop_back();
                } else {
                    bool isEqual = false;
                    if(working.get_buchi_state() == state.get_buchi_state()) {
                        if constexpr (std::is_same_v<PetriEngine::PetriNet, N>) {
                            isEqual = std::equal(working.marking(), working.marking() + ModelChecker<N>::_net.numberOfPlaces()*_hyper_traces,
                                  state.marking());
                        } else {
                            isEqual = working.marking == state.marking;
                        }

                    }
                    if (isEqual) {
                        ModelChecker<N>::_violation = true;
                        return;
                    }
                    auto [is_new, stateid] = mark(states, working, MARKER2);
                    if (stateid == std::numeric_limits<size_t>::max())
                        continue;
                    top._sucinfo._last_state = stateid;
                    if (is_new) {
                        nested_todo.push_back(stack_entry_t<SG>{stateid, successor_generator.initial_suc_info()});
                    }
                }
            }
        }

        template<typename T>
        void build_trace(light_deque<stack_entry_t<T>>& todo, light_deque<stack_entry_t<T>>& nested_todo) {
            if constexpr (std::is_same_v<PetriEngine::PetriNet, N>) {
                size_t loop_id = std::numeric_limits<size_t>::max();
                // last element of todo-stack always has a "garbage" transition, it is the
                // current working element OR first element of nested.

                if(!todo.empty())
                    todo.pop_back();
                if(!nested_todo.empty()) {
                    // here the last state is significant
                    // of the successor is the check that demonstrates the violation.
                    loop_id = nested_todo.back()._id;
                    nested_todo.pop_back();
                }

                for(auto* stck : {&todo, &nested_todo})
                {
                    while(!(*stck).empty())
                    {
                        auto& top = (*stck).front();
                        if(top._id == loop_id)
                        {
                            ModelChecker<N>::_loop = ModelChecker<N>::_trace.size();
                            loop_id = std::numeric_limits<size_t>::max();
                        }
                        auto res = top._sucinfo.transition();
                        if constexpr (std::is_same<decltype(res),std::vector<uint32_t>>::value)
                        {
                            ModelChecker<N>::_trace.emplace_back(top._sucinfo.transition());
                        }
                        else
                        {
                            ModelChecker<N>::_trace.push_back({top._sucinfo.transition()});
                            assert(top._sucinfo.transition() < ModelChecker<N>::_net.numberOfTransitions());
                        }
                        (*stck).pop_front();
                    }
                }
            }
        }
    };
}

#endif //VERIFYPN_NESTEDDEPTHFIRSTSEARCH_H
