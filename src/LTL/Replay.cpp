/* Copyright (C) 2021  Nikolaj J. Ulrik <nikolaj@njulrik.dk>,
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

#include "PetriEngine/PQL/PQL.h"
#include "PetriEngine/PetriNet.h"
#include "LTL/Replay.h"
#include "PetriEngine/Structures/State.h"
#include "PetriEngine/SuccessorGenerator.h"
#include "LTL/LTLToBuchi.h"
#include "LTL/BuchiSuccessorGenerator.h"

#include <rapidxml.hpp>
#include <vector>
#include <iostream>
#include <cstring>

using namespace PetriEngine;
using namespace PetriEngine::PQL;

namespace LTL {
    bool guard_valid(const PQL::EvaluationContext ctx, const BuchiSuccessorGenerator &buchi, bdd bdd);

    void Replay::parse(std::ifstream &xml)
    {
        rapidxml::xml_document<> doc;
        std::vector<char> buffer((std::istreambuf_iterator<char>(xml)), std::istreambuf_iterator<char>());
        buffer.push_back('\0');
        doc.parse<0>(buffer.data());
        rapidxml::xml_node<> *root = doc.first_node();

        if (root) {
            parseRoot(root);
        } else {
            std::cerr << "Error getting root node." << std::endl;
            assert(false);
            exit(1);
        }
    }

    void Replay::parseRoot(const rapidxml::xml_node<> *pNode)
    {
        if (std::strcmp(pNode->name(), "trace") != 0) {
            std::cerr << "Error: Expected trace node. Got: " << pNode->name() << std::endl;
            assert(false);
            exit(1);
        }
        for (auto it = pNode->first_node(); it; it = it->next_sibling()) {
            if (std::strcmp(it->name(), "loop") == 0) continue;
            if (std::strcmp(it->name(), "deadlock") == 0) continue;
            if (std::strcmp(it->name(), "transition") != 0) {
                std::cerr << "Error: Expected transition or loop node. Got: " << it->name() << std::endl;
                assert(false);
                exit(1);
            }
            trace.push_back(parseTransition(it));
        }
    }

    Replay::Transition Replay::parseTransition(const rapidxml::xml_node<char> *pNode)
    {
        std::string id;
        int buchi = -1;
        for (auto it = pNode->first_attribute(); it; it = it->next_attribute()) {
            if (std::strcmp(it->name(), "id") == 0) {
                id = std::string(it->value());
            }
            if (std::strstr(it->name(), "buchi") != nullptr) {
                buchi = atoi(it->value());
            }
        }
        if (id.empty()) {
            std::cerr << "Error: Transition has no id attribute" << std::endl;
            assert(false);
            exit(1);
        }
        if (buchi == -1) {
            std::cerr << "Error: Missing Büchi state" << std::endl;
            assert(false);
            exit(1);
        }

        Transition transition(id, buchi);

        for (auto it = pNode->first_node(); it; it = it->next_sibling()) {
            assert(std::strcmp(it->name(), "token"));
            transition.tokens.push_back(parseToken(it));
        }

        return transition;
    }

    Replay::Token Replay::parseToken(const rapidxml::xml_node<char> *pNode)
    {
        std::string place;
        for (auto it = pNode->first_attribute(); it; it = it->next_attribute()) {
            if (std::strcmp(it->name(), "place") == 0) {
                place = std::string(it->value());
            }
        }
        assert(!place.empty());
        return Token{place};
    }

    bool Replay::replay(const PetriEngine::PetriNet *net, const PetriEngine::PQL::Condition_ptr &cond)
    {
        PetriEngine::Structures::State state;
        std::unordered_map<std::string, int> transitions;
        BuchiSuccessorGenerator buchiGenerator = makeBuchiAutomaton(cond);

        for (int i = 0; i < net->transitionNames().size(); ++i) {
            transitions[net->transitionNames()[i]] = i;
        }
        state.setMarking(net->makeInitialMarking());
        PetriEngine::SuccessorGenerator successorGenerator(*net);
        std::cout << "Playing back trace. Length: " << trace.size() << std::endl;
        //for (const Transition& transition : trace) {
        int size = trace.size();
        int prev_buchi = buchiGenerator.initial_state_number();
        for (int i = 0; i < size; ++i) {
            const Transition &transition = trace[i];
            successorGenerator.prepare(&state);
            buchiGenerator.prepare(prev_buchi);
            auto it = transitions.find(
                    transition.id); // std::find(std::begin(transitions), std::end(transitions), transition.id);
            if (it == std::end(transitions)) {
                std::cerr << "Unrecognized transition name " << transition.id << std::endl;
                assert(false);
                exit(1);
            }
            int tid = it->second;


            if (!successorGenerator.checkPreset(tid)) {
                std::cerr << "ERROR the provided trace cannot be replayed on the provided model. Offending transition: "
                          << transition.id << " at index: " << i << "\n";
                exit(1);
            }
            successorGenerator.consumePreset(state, tid);
            successorGenerator.producePostset(state, tid);

            size_t next_buchi = -1;
            bdd cond;
            while (buchiGenerator.next(next_buchi, cond)) {
                if (next_buchi == transition.buchi_state) break;
            }
            if (next_buchi != transition.buchi_state || !guard_valid(EvaluationContext{state.marking(), net}, buchiGenerator, cond)) {
                std::cerr << "ERROR the provided trace could not execute the Buchi automaton. Offending transition: "
                          << transition.id << " at index: " << i << '\n';
                exit(1);
            }
            else {
                prev_buchi = next_buchi;
            }

            //std::cerr << "Fired transition: " << transition.id << std::endl;
            if (i % 100000 == 0)
                std::cerr << i << "/" << size << std::endl;
        }
        std::cerr << "Replay complete. No errors" << std::endl;
        return true;
    }

    bool guard_valid(const PQL::EvaluationContext ctx, const BuchiSuccessorGenerator &buchi, bdd bdd)
    {
        using namespace PetriEngine::PQL;
        // IDs 0 and 1 are false and true atoms, respectively
        // More details in buddy manual for details ( http://buddy.sourceforge.net/manual/main.html )
        while (bdd.id() > 1/*!(bdd == bddtrue || bdd == bddfalse)*/) {
            // find variable to test, and test it
            size_t var = bdd_var(bdd);
            Condition::Result res = buchi.getExpression(var)->evaluate(ctx);
            switch (res) {
                case Condition::RUNKNOWN:
                    std::cerr << "Unexpected unknown answer from evaluating query!\n";
                    assert(false);
                    exit(1);
                    break;
                case Condition::RFALSE:
                    bdd = bdd_low(bdd);
                    break;
                case Condition::RTRUE:
                    bdd = bdd_high(bdd);
                    break;
            }
        }
        return bdd == bddtrue;
    }
}
