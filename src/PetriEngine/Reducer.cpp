/*
 * File:   Reducer.cpp
 * Author: srba
 *
 * Created on 15 February 2014, 10:50
 */

#include "PetriEngine/Reducer.h"
#include "PetriEngine/PetriNet.h"
#include "PetriEngine/PetriNetBuilder.h"
#include "PetriParse/PNMLParser.h"
#include <queue>
#include <set>
#include <algorithm>

namespace PetriEngine {

    Reducer::Reducer(PetriNetBuilder* p)
    : parent(p) {
    }

    Reducer::~Reducer() {

    }

    void Reducer::Print(QueryPlaceAnalysisContext& context) {
        std::cout   << "\nNET INFO:\n"
                    << "Number of places: " << parent->numberOfPlaces() << std::endl
                    << "Number of transitions: " << parent->numberOfTransitions()
                    << std::endl << std::endl;
        for (uint32_t t = 0; t < parent->numberOfTransitions(); t++) {
            std::cout << "Transition " << t << " :\n";
            if(parent->_transitions[t].skip)
            {
                std::cout << "\tSKIPPED" << std::endl;
            }
            for(auto& arc : parent->_transitions[t].pre)
            {
                if (arc.weight > 0)
                    std::cout   << "\tInput place " << arc.place
                                << " (" << getPlaceName(arc.place) << ")"
                                << " with arc-weight " << arc.weight << std::endl;
            }
            for(auto& arc : parent->_transitions[t].post)
            {
                if (arc.weight > 0)
                    std::cout   << "\tOutput place " << arc.place
                                << " (" << getPlaceName(arc.place) << ")"
                                << " with arc-weight " << arc.weight << std::endl;
            }
            std::cout << std::endl;
        }
        for (uint32_t i = 0; i < parent->numberOfPlaces(); i++) {
            std::cout <<    "Marking at place "<< i <<
                            " is: " << parent->initMarking()[i] << std::endl;
        }
        for (uint32_t i = 0; i < parent->numberOfPlaces(); i++) {
            std::cout   << "Query count for place " << i
                        << " is: " << context.getQueryPlaceCount()[i] << std::endl;
        }
    }

    shared_const_string Reducer::getTransitionName(uint32_t transition)
    {
        for(auto t : parent->_transitionnames)
        {
            if(t.second == transition) return t.first;
        }
        throw base_error("Unknown transition_id=", transition);
    }

    shared_const_string Reducer::newTransName()
    {
        auto prefix = "CT";
        auto tmp = std::make_shared<const_string>(prefix + std::to_string(_tnameid));
        while(parent->_transitionnames.count(tmp) >= 1)
        {
            ++_tnameid;
            tmp = std::make_shared<const_string>(prefix + std::to_string(_tnameid));
        }
        ++_tnameid;
        return tmp;
    }

    shared_const_string Reducer::getPlaceName(uint32_t place)
    {
        for(auto& t : parent->_placenames)
        {
            if(t.second == place) return t.first;
        }
        throw base_error("Unknown placeid=", place);
    }

    Transition& Reducer::getTransition(uint32_t transition)
    {
        return parent->_transitions[transition];
    }

    ArcIter Reducer::getOutArc(Transition& trans, uint32_t place)
    {
        Arc a;
        a.place = place;
        auto ait = std::lower_bound(trans.post.begin(), trans.post.end(), a);
        if(ait != trans.post.end() && ait->place == place)
        {
            return ait;
        }
        else
        {
            return trans.post.end();
        }
    }

    ArcIter Reducer::getInArc(uint32_t place, Transition& trans)
    {
        Arc a;
        a.place = place;
        auto ait = std::lower_bound(trans.pre.begin(), trans.pre.end(), a);
        if(ait != trans.pre.end() && ait->place == place)
        {
            return ait;
        }
        else
        {
            return trans.pre.end();
        }
    }

    void Reducer::eraseTransition(std::vector<uint32_t>& set, uint32_t el)
    {
        auto lb = std::lower_bound(set.begin(), set.end(), el);
        assert(lb != set.end());
        assert(*lb == el);
        set.erase(lb);
    }

    void Reducer::skipTransition(uint32_t t)
    {
        ++_removedTransitions;
        Transition& trans = getTransition(t);
        assert(!trans.skip);
        for(auto p : trans.post)
        {
            eraseTransition(parent->_places[p.place].producers, t);
        }
        for(auto p : trans.pre)
        {
            eraseTransition(parent->_places[p.place].consumers, t);
        }
        trans.post.clear();
        trans.pre.clear();
        trans.skip = true;
        assert(consistent());
        _skipped_trans.push_back(t);
    }

    void Reducer::skipPlace(uint32_t place)
    {
        ++_removedPlaces;
        Place& pl = parent->_places[place];
        assert(!pl.skip);
        pl.skip = true;
        for(auto& t : pl.consumers)
        {
            Transition& trans = getTransition(t);
            auto ait = getInArc(place, trans);
            if(ait != trans.pre.end() && ait->place == place)
                trans.pre.erase(ait);
        }

        for(auto& t : pl.producers)
        {
            Transition& trans = getTransition(t);
            auto ait = getOutArc(trans, place);
            if(ait != trans.post.end() && ait->place == place)
                trans.post.erase(ait);
        }
        pl.consumers.clear();
        pl.producers.clear();
        assert(consistent());
    }

    bool Reducer::consistent()
    {
#ifndef NDEBUG
        size_t strans = 0;
        for(size_t i = 0; i < parent->numberOfTransitions(); ++i)
        {
            Transition& t = parent->_transitions[i];
            if(t.skip) ++strans;
            assert(std::is_sorted(t.pre.begin(), t.pre.end()));
            assert(std::is_sorted(t.post.end(), t.post.end()));
            assert(!t.skip || (t.pre.size() == 0 && t.post.size() == 0));
            for(Arc& a : t.pre)
            {
                assert(a.weight > 0);
                Place& p = parent->_places[a.place];
                assert(!p.skip);
                assert(std::find(p.consumers.begin(), p.consumers.end(), i) != p.consumers.end());
            }
            for(Arc& a : t.post)
            {
                assert(a.weight > 0);
                Place& p = parent->_places[a.place];
                assert(!p.skip);
                assert(std::find(p.producers.begin(), p.producers.end(), i) != p.producers.end());
            }
        }

        assert(strans == _removedTransitions);

        size_t splaces = 0;
        for(size_t i = 0; i < parent->numberOfPlaces(); ++i)
        {
            Place& p = parent->_places[i];
            if(p.skip) ++splaces;
            assert(std::is_sorted(p.consumers.begin(), p.consumers.end()));
            assert(std::is_sorted(p.producers.begin(), p.producers.end()));
            assert(!p.skip || (p.consumers.size() == 0 && p.producers.size() == 0));

            for(uint c : p.consumers)
            {
                Transition& t = parent->_transitions[c];
                assert(!t.skip);
                auto a = getInArc(i, t);
                assert(a != t.pre.end());
                assert(a->place == i);
            }

            for(uint prod : p.producers)
            {
                Transition& t = parent->_transitions[prod];
                assert(!t.skip);
                auto a = getOutArc(t, i);
                assert(a != t.post.end());
                assert(a->place == i);
            }
        }
        assert(splaces == _removedPlaces);
#endif
        return true;
    }

    bool Reducer::ReducebyRuleA(uint32_t* placeInQuery) {
        // Rule A  - find transition t that has exactly one place in pre and post and remove one of the places (and t)
        bool continueReductions = false;
        const size_t numberoftransitions = parent->numberOfTransitions();
        for (uint32_t t = 0; t < numberoftransitions; t++) {
            if(hasTimedout()) return false;
            Transition& trans = getTransition(t);

            // we have already removed
            if(trans.skip) continue;

            // A2. we have more/less than one arc in pre or post
            // checked first to avoid out-of-bounds when looking up indexes.
            if(trans.pre.size() != 1) continue;

            uint32_t pPre = trans.pre[0].place;

            // A2. Check that pPre goes only to t
            if(parent->_places[pPre].consumers.size() != 1) continue;

            // A3. We have weight of more than one on input
            // and is empty on output (should not happen).
            auto w = trans.pre[0].weight;
            bool ok = true;
            for(auto t : parent->_places[pPre].producers)
            {
                if((getOutArc(parent->_transitions[t], trans.pre[0].place)->weight % w) != 0)
                {
                    ok = false;
                    break;
                }
            }
            if(!ok)
                continue;

            // A4. Do inhibitor check, neither T, pPre or pPost can be involved with any inhibitor
            if(parent->_places[pPre].inhib|| trans.inhib) continue;

            // A5. dont mess with query!
            if(placeInQuery[pPre] > 0) continue;
            // check A1, A4 and A5 for post
            for(auto& pPost : trans.post)
            {
                if(parent->_places[pPost.place].inhib || pPre == pPost.place || placeInQuery[pPost.place] > 0)
                {
                    ok = false;
                    break;
                }
            }
            if(!ok) continue;

            continueReductions = true;
            _ruleA++;

            // here we need to remember when a token is created in pPre (some
            // transition with an output in P is fired), t is fired instantly!.
            if(reconstructTrace) {
                Place& pre = parent->_places[pPre];
                auto tname = getTransitionName(t);
                for(size_t pp : pre.producers)
                {
                    auto prefire = getTransitionName(pp);
                    _postfire[*prefire].push_back(tname);
                }
                _extraconsume[*tname].emplace_back(getPlaceName(pPre), w);
                for(size_t i = 0; i < parent->initMarking()[pPre]; ++i)
                {
                    _initfire.push_back(tname);
                }
            }

            for(auto& pPost : trans.post)
            {
                // UA2. move the token for the initial marking, makes things simpler.
                parent->initialMarking[pPost.place] += ((parent->initialMarking[pPre]/w) * pPost.weight);
            }
            parent->initialMarking[pPre] = 0;

            // Remove transition t and the place that has no tokens in m0
            // UA1. remove transition
            auto toMove = trans.post;
            skipTransition(t);

            // UA2. update arcs
            for(auto& _t : parent->_places[pPre].producers)
            {
                assert(_t != t);
                // move output-arcs to post.
                Transition& src = getTransition(_t);
                auto source = *getOutArc(src, pPre);
                for(auto& pPost : toMove)
                {
                    Arc a;
                    a.place = pPost.place;
                    a.weight = (source.weight/w) * pPost.weight;
                    assert(a.weight > 0);
                    a.inhib = false;
                    auto dest = std::lower_bound(src.post.begin(), src.post.end(), a);
                    if(dest == src.post.end() || dest->place != pPost.place)
                    {
                        dest = src.post.insert(dest, a);
                        auto& prod = parent->_places[pPost.place].producers;
                        auto lb = std::lower_bound(prod.begin(), prod.end(), _t);
                        prod.insert(lb, _t);
                    }
                    else
                    {
                        dest->weight += ((source.weight/w) * pPost.weight);
                    }
                    assert(dest->weight > 0);
                }
            }
            // UA1. remove place
            skipPlace(pPre);
        } // end of Rule A main for-loop
        return continueReductions;
    }

    bool Reducer::ReducebyRuleB(uint32_t* placeInQuery, bool remove_deadlocks, bool remove_consumers) {

        // Rule B - find place p that has exactly one transition in pre and exactly one in post and remove the place
        bool continueReductions = false;
        const size_t numberofplaces = parent->numberOfPlaces();
        for (uint32_t p = 0; p < numberofplaces; p++) {
            if(hasTimedout()) return false;
            Place& place = parent->_places[p];

            if(place.skip) continue;    // already removed
            // B5. dont mess up query
            if(placeInQuery[p] > 0)
                continue;

            // B2. Only one consumer/producer
            if( place.consumers.size() != 1 ||
                place.producers.size() < 1)
                continue; // no orphan removal

            auto tIn = place.consumers[0];

            // B1. producer is not consumer
            bool ok = true;
            for(auto& tOut : place.producers)
            {
                if (tOut == tIn)
                {
                    ok = false;
                    continue; // cannot remove this kind either
                }
            }
            if(!ok)
                continue;
            auto prod = place.producers;
            Transition& in = getTransition(tIn);
            bool added_tIn_extra = false;
            for(auto tOut : prod)
            {
                Transition& out = getTransition(tOut);

                if(out.post.size() != 1 && in.pre.size() != 1)
                    continue; // at least one has to be singular for this to work

                if((!remove_deadlocks || !remove_consumers) && in.pre.size() != 1)
                    // the buffer can mean deadlocks and other interesting things
                    // also we can "hide" tokens, so we need to make sure not
                    // to remove consumers.
                    continue;

                if(parent->initMarking()[p] > 0 && in.pre.size() != 1)
                    continue;

                auto inArc = getInArc(p, in);
                auto outArc = getOutArc(out, p);

                // B3. Output is a multiple of input and nonzero.
                if(outArc->weight < inArc->weight)
                    continue;
                if((outArc->weight % inArc->weight) != 0)
                    continue;

                size_t multiplier = outArc->weight / inArc->weight;

                // B4. Do inhibitor check, neither In, out or place can be involved with any inhibitor
                if(place.inhib || in.inhib || out.inhib)
                    continue;

                // B6. also, none of the places in the post-set of consuming transition can be participating in inhibitors.
                // B7. nor can they appear in the query.
                {
                    bool post_ok = false;
                    for(const Arc& a : in.post)
                    {
                        post_ok |= parent->_places[a.place].inhib;
                        post_ok |= placeInQuery[a.place];
                        if(post_ok) break;
                    }
                    if(post_ok)
                        continue;
                }
                {
                    bool pre_ok = false;
                    for(const Arc& a : in.pre)
                    {
                        pre_ok |= parent->_places[a.place].inhib;
                        pre_ok |= placeInQuery[a.place];
                        if(pre_ok) break;
                    }
                    if(pre_ok)
                        continue;
                }

                bool ok = true;
                if(in.pre.size() > 1)
                    for(const Arc& arc : out.pre)
                        ok &= placeInQuery[arc.place] == 0;
                if(!ok)
                    continue;

                // B2.a Check that there is no other place than p that gives to tPost,
                // tPre can give to other places
                auto& arcs = in.pre.size() < out.post.size() ? in.pre : out.post;
                for (auto& arc : arcs) {
                    if (arc.weight > 0 && arc.place != p) {
                        ok = false;
                        break;
                    }
                }

                if (!ok)
                    continue;

                // UB2. we need to remember initial marking
                uint initm = parent->initMarking()[p];
                initm /= inArc->weight; // integer-devision is floor by default

                if(reconstructTrace)
                {
                    // remember reduction for recreation of trace
                    auto toutname    = getTransitionName(tOut);
                    auto tinname     = getTransitionName(tIn);
                    auto pname       = getPlaceName(p);
                    Arc& a = *getInArc(p, in);
                    if(!added_tIn_extra)
                    {
                        added_tIn_extra = true;
                        _extraconsume[*tinname].emplace_back(pname, a.weight);
                    }
                    for(size_t i = 0; i < multiplier; ++i)
                    {
                        _postfire[*toutname].push_back(tinname);
                    }

                    for(size_t i = 0; initm > 0 && i < initm / inArc->weight; ++i )
                    {
                        _initfire.push_back(tinname);
                    }
                }

                continueReductions = true;
                _ruleB++;
                 // UB1. Remove place p
                parent->initialMarking[p] = 0;
                // We need to remember that when tOut fires, tIn fires just after.
                // this should fix the trace

                // UB3. move arcs from t' to t
                for (auto& arc : in.post) { // remove tPost
                    auto _arc = getOutArc(out, arc.place);
                    // UB2. Update initial marking
                    parent->initialMarking[arc.place] += initm*arc.weight;
                    if(_arc != out.post.end())
                    {
                        _arc->weight += arc.weight*multiplier;
                    }
                    else
                    {
                        out.post.push_back(arc);
                        out.post.back().weight *= multiplier;
                        parent->_places[arc.place].producers.push_back(tOut);

                        std::sort(out.post.begin(), out.post.end());
                        std::sort(parent->_places[arc.place].producers.begin(),
                                  parent->_places[arc.place].producers.end());
                    }
                }
                for (auto& arc : in.pre) { // remove tPost
                    if(arc.place == p)
                        continue;
                    auto _arc = getInArc(arc.place, out);
                    // UB2. Update initial marking
                    parent->initialMarking[arc.place] += initm*arc.weight;
                    if(_arc != out.pre.end())
                    {
                        _arc->weight += arc.weight*multiplier;
                    }
                    else
                    {
                        out.pre.push_back(arc);
                        out.pre.back().weight *= multiplier;
                        parent->_places[arc.place].consumers.push_back(tOut);

                        std::sort(out.pre.begin(), out.pre.end());
                        std::sort(parent->_places[arc.place].consumers.begin(),
                                  parent->_places[arc.place].consumers.end());
                    }
                }

                for(auto it = out.post.begin(); it != out.post.end(); ++it)
                {
                    if(it->place == p)
                    {
                        out.post.erase(it);
                        break;
                    }
                }
                for(auto it = place.producers.begin(); it != place.producers.end(); ++it)
                {
                    if(*it == tOut)
                    {
                        place.producers.erase(it);
                        break;
                    }
                }
            }
            // UB1. remove transition
            if(place.producers.size() == 0)
            {
                skipPlace(p);
                skipTransition(tIn);
            }
        } // end of Rule B main for-loop
        assert(consistent());
        return continueReductions;
    }

    bool Reducer::ReducebyRuleC(uint32_t* placeInQuery) {
        // Rule C - Places in parallel where one accumulates tokens while the others disable their post set
        bool continueReductions = false;

        _pflags.resize(parent->_places.size(), 0);
        std::fill(_pflags.begin(), _pflags.end(), 0);

        for (uint32_t tid_outer = 0; tid_outer < parent->numberOfTransitions(); ++tid_outer) {
            for (size_t aid_outer = 0; aid_outer < parent->_transitions[tid_outer].post.size(); ++aid_outer) {

                auto pid_outer = parent->_transitions[tid_outer].post[aid_outer].place;
                if (_pflags[pid_outer] > 0) continue;
                _pflags[pid_outer] = 1;

                if (hasTimedout()) return false;

                const Place &pout = parent->_places[pid_outer];
                if (pout.skip) continue;

                for (size_t aid_inner = aid_outer + 1; aid_inner < parent->_transitions[tid_outer].post.size(); ++aid_inner) {
                    if (pout.skip) break;
                    auto pid_inner = parent->_transitions[tid_outer].post[aid_inner].place;
                    if (parent->_places[pid_inner].skip) continue;

                    for (size_t swp = 0; swp < 2; ++swp) {
                        if (hasTimedout()) return false;
                        if (parent->_places[pid_inner].skip ||
                            parent->_places[pid_outer].skip)
                            break;

                        uint p1 = pid_outer;
                        uint p2 = pid_inner;

                        assert(p1 != p2);
                        if (swp == 1) std::swap(p1, p2);

                        if (placeInQuery[p2] > 0) continue;

                        Place &place1 = parent->_places[p1];
                        Place &place2 = parent->_places[p2];

                        if (place2.producers.empty() || place1.consumers.empty()) continue;

                        if (place1.consumers.size() < place2.consumers.size() ||
                            place1.producers.size() > place2.producers.size())
                            continue;

                        bool ok = true;

                        double maxDrainRatio = 0;

                        uint32_t i = 0, j = 0;
                        while (i < place1.consumers.size() && j < place2.consumers.size()) {

                            uint32_t p1t = place1.consumers[i];
                            uint32_t p2t = place2.consumers[j];

                            if (p2t < p1t) {
                                // place2.consumers is not a subset of place1.consumers
                                ok = false;
                                break;
                            }

                            i++;
                            if (p2t > p1t) {
                                swp = 2; // We can't remove p1, so don't swap
                                continue;
                            }
                            j++;

                            Transition &tran = getTransition(p1t);
                            const auto &p1Arc = getInArc(p1, tran);
                            const auto &p2Arc = getInArc(p2, tran);

                            maxDrainRatio = std::max(maxDrainRatio, (double)p2Arc->weight / (double)p1Arc->weight);
                        }

                        if (!ok || j != place2.consumers.size()) continue;

                        if (parent->initialMarking[p2] < parent->initialMarking[p1] * maxDrainRatio) continue;

                        i = 0, j = 0;
                        while (i < place1.producers.size() && j < place2.producers.size()) {

                            uint32_t p1t = place1.producers[i];
                            uint32_t p2t = place2.producers[j];

                            if (p1t < p2t) {
                                // place1.producers is not a subset of place2.producers
                                ok = false;
                                break;
                            }

                            j++;
                            if (p1t > p2t) {
                                swp = 2; // We can't remove p1, so don't swap
                                continue;
                            }
                            i++;

                            Transition &tran = getTransition(p2t);
                            const auto &p2Arc = getOutArc(tran, p2);
                            const auto &p1Arc = getOutArc(tran, p1);

                            if (maxDrainRatio > (double)p2Arc->weight / (double)p1Arc->weight) {
                                ok = false;
                                break;
                            }
                        }

                        if (!ok || i != place1.producers.size()) continue;

                        continueReductions = true;
                        _ruleC++;
                        skipPlace(p2);

                        // p2 has now been removed from tid_outer.post, so update arc indexes to not miss any places
                        if (p2 == pid_outer) {
                            aid_outer--;
                            aid_inner--;
                        } else if (p2 == pid_inner) aid_inner--;
                        break;
                    }
                }
            }
        }
        assert(consistent());
        return continueReductions;
    }

    bool Reducer::ReducebyRuleD(uint32_t* placeInQuery, bool all_reach, bool remove_loops_no_branch) {
        // Rule D - two transitions with the same pre and post and same inhibitor arcs
        // This does not alter the trace.
        bool continueReductions = false;
        _tflags.resize(parent->_transitions.size(), 0);
        std::fill(_tflags.begin(), _tflags.end(), 0);
        bool has_empty_trans = false;
        for(size_t t = 0; t < parent->_transitions.size(); ++t)
        {
            auto& trans = parent->_transitions[t];
            if(!trans.skip && trans.pre.size() == 0 && trans.post.size() == 0)
            {
                if(has_empty_trans)
                {
                    ++_ruleD;
                    skipTransition(t);
                }
                has_empty_trans = true;
            }

        }

        for(auto& op : parent->_places)
        for(size_t outer = 0; outer < op.consumers.size(); ++outer)
        {
            auto touter = op.consumers[outer];
            if(hasTimedout()) return false;
            if(_tflags[touter] != 0) continue;
            _tflags[touter] = 1;
            Transition& tout = getTransition(touter);
            if (tout.skip) continue;

            // D2. No inhibitors
            if (tout.inhib) continue;

            for(size_t inner = outer + 1; inner < op.consumers.size(); ++inner) {
                if (tout.skip) break;
                auto tinner = op.consumers[inner];
                Transition& tin = getTransition(tinner);
                if (tin.skip) continue;

                // D2. No inhibitors
                if (tin.inhib) continue;

                for (size_t swp = 0; swp < 2; ++swp) {
                    if(hasTimedout()) return false;

                    uint t1 = touter;
                    uint t2 = tinner;
                    if (swp == 1) std::swap(t1, t2);

                    // D1. not same transition
                    assert(t1 != t2);

                    Transition& trans1 = getTransition(t1);
                    Transition& trans2 = getTransition(t2);

                    // From D3, and D4 we have that pre and post-sets are the same
                    if (trans1.post.size() < trans2.post.size()) { break;}
                    if (trans1.pre.size() > trans2.pre.size()) { break;}
                    if (!remove_loops_no_branch && (trans1.pre.size() != trans2.pre.size() ||
                                          trans1.post.size() != trans2.post.size()))
                    {
                        break; // we require exactness.
                    }

                    int ok = 0;
                    uint mult = std::numeric_limits<uint>::max();
                    bool pre_equal = true;
                    bool post_equal = true;
                    bool some_in_query = false;
                    bool exact = true;
                    // D3. Presets must match
                    size_t j = 0;
                    for (size_t i = 0; i < trans1.pre.size(); ++i, ++j) {
                        Arc& arc = trans1.pre[i];
                        for(; j < trans2.pre.size() && trans2.pre[j].place < arc.place; ++j)
                        {
                            pre_equal &= placeInQuery[trans2.pre[j].place] == 0;
                            exact = false;
                        }

                        if(j >= trans2.pre.size() || trans2.pre[j].place != arc.place)
                        {
                            ok = 1;
                            break;
                        }

                        Arc& arc2 = trans2.pre[j];
                        if (arc2.place != arc.place) {
                            ok = 2;
                            break;
                        }
                        if (arc2.weight < arc.weight) {
                            ok = 1;
                            break;
                        } else {
                            auto old = mult;
                            mult = std::min(arc2.weight / arc.weight, mult);
                            if(old != std::numeric_limits<uint>::max() &&
                               mult != old &&
                               some_in_query)
                            {
                                pre_equal = false;
                                exact = false;
                            }
                        }
                        some_in_query |= placeInQuery[arc2.place] > 0;
                        pre_equal = pre_equal && (placeInQuery[arc2.place] == 0 ||
                                                  arc.weight == arc2.weight*mult);
                        exact &= arc.weight == arc2.weight*mult;
                        if(!pre_equal) break;
                    }
                    for(; j < trans2.pre.size(); ++j)
                    {
                        pre_equal &= placeInQuery[trans2.pre[j].place] == 0;
                        exact = false;
                    }

                    if(!pre_equal) { break;}
                    if (ok == 2) { break;}
                    else if (ok == 1) { continue;}
                    if(mult != 1 && !all_reach) { break;}
                    if(!remove_loops_no_branch && !exact) { break;}
                    ok = 0;
                    // D4. postsets must match
                    j = 0;
                    for (size_t i = 0; i < trans2.post.size(); ++i, ++j) {
                        Arc& arc2 = trans2.post[i];
                        for(; j < trans1.post.size() && trans1.post[j].place < arc2.place; ++j)
                        {
                            post_equal &= placeInQuery[trans1.post[j].place] == 0;
                            exact = false;
                        }
                        if(j >= trans1.post.size() || trans1.post[j].place != arc2.place)
                        {
                            ok = 1;
                            break;
                        }
                        Arc& arc = trans1.post[j];
                        if (arc2.place != arc.place) {
                            ok = 2;
                            break;
                        }
                        post_equal = post_equal && (placeInQuery[arc2.place] == 0 ||
                                                    arc.weight * mult == arc2.weight);
                        exact &= arc.weight * mult == arc2.weight;
                        if (arc2.weight > arc.weight * mult) {
                            ok = 2;
                            break;
                        }
                        if(!post_equal) break;
                    }

                    for(; j < trans1.post.size(); ++j)
                    {
                        post_equal &= placeInQuery[trans1.post[j].place] == 0;
                        exact = false;
                    }

                    if(!post_equal) { break;}
                    if (ok == 2) { break;}
                    else if (ok == 1) {
                        continue;
                    }
                    if(!remove_loops_no_branch && !exact) { break;}

                    // UD1. Remove transition t2
                    continueReductions = true;
                    _ruleD++;
                    skipTransition(t2);
                    _tflags[touter] = 0;

                    // t2 has now been removed from op.consumers, so update indexes to not miss any
                    if (t2 == touter) {
                        outer--;
                        inner--;
                    } else if (t2 == tinner) inner--;
                    break; // break the swap loop
                }
            }
        } // end of main for loop for rule D
        assert(consistent());
        return continueReductions;
    }

    bool Reducer::ReducebyRuleE(uint32_t* placeInQuery) {
        bool continueReductions = false;
        const size_t numberofplaces = parent->numberOfPlaces();
        for(uint32_t p = 0; p < numberofplaces; ++p)
        {
            if(hasTimedout()) return false;
            Place& place = parent->_places[p];
            if(place.skip) continue;
            if(place.inhib) continue;
            if(place.producers.size() > place.consumers.size()) continue;

            bool ok = true;
            // Check for producers without matching consumers first
            for(uint prod : place.producers)
            {
                // Any producer without a matching consumer blocks this rule
                Transition& t = getTransition(prod);
                auto in = getInArc(p, t);
                if(in == t.pre.end())
                {
                    ok = false;
                    break;
                }
            }

            if(!ok) continue;

            std::set<uint32_t> notenabled;
            // Out of the consumers, tally up those that are initially not enabled by place
            // Ensure all the enabled transitions that feed back into place are non-increasing on place.
            for(uint cons : place.consumers)
            {
                Transition& t = getTransition(cons);
                auto in = getInArc(p, t);
                if(in->weight <= parent->initialMarking[p])
                {
                    // This branch happening even once means notenabled.size() != consumers.size()
                    auto out = getOutArc(t, p);
                    // Only increasing loops are not ok
                    if (out != t.post.end() && out->weight > in->weight) {
                        ok = false;
                        break;
                    }
                }
                else
                {
                    notenabled.insert(cons);
                }
            }

            if(!ok || notenabled.empty()) continue;

            bool skipplace = (notenabled.size() == place.consumers.size()) && (placeInQuery[p] == 0);

            for(uint cons : notenabled) {
                skipTransition(cons);
            }

            if(skipplace) {
                skipPlace(p);
            }

            _ruleE++;
            continueReductions = true;

        }
        assert(consistent());
        return continueReductions;
    }

    bool Reducer::ReducebyRuleI(uint32_t* placeInQuery, bool remove_consumers) {
        bool reduced = false;

        auto result = relevant(placeInQuery, remove_consumers);
        if (!result) {
            return false;
        }
        auto[tseen, pseen] = result.value();

        reduced |= remove_irrelevant(placeInQuery, tseen, pseen);

        if(reduced)
            ++_ruleI;

        return reduced;
    }

    bool Reducer::ReducebyRuleF(uint32_t* placeInQuery) {
        bool continueReductions = false;
        const size_t numberofplaces = parent->numberOfPlaces();
        for(uint32_t p = 0; p < numberofplaces; ++p)
        {
            if(hasTimedout()) return false;
            Place& place = parent->_places[p];
            if(place.skip) continue;
            if(place.inhib) continue;
            if(place.producers.size() < place.consumers.size()) continue;
            if(placeInQuery[p] != 0) continue;

            bool ok = true;
            for(uint cons : place.consumers)
            {
                Transition& t = getTransition(cons);
                auto w = getInArc(p, t)->weight;
                if(w > parent->initialMarking[p])
                {
                    ok = false;
                    break;
                }
                else
                {
                    auto it = getOutArc(t, p);
                    if(it == t.post.end() ||
                       it->place != p     ||
                       it->weight < w)
                    {
                        ok = false;
                        break;
                    }
                }
            }

            if(!ok) continue;

            ++_ruleF;

            if((numberofplaces - _removedPlaces) > 1)
            {
                if(reconstructTrace)
                {
                    for(auto t : place.consumers)
                    {
                        auto tname = getTransitionName(t);
                        const ArcIter arc = getInArc(p, getTransition(t));
                        _extraconsume[*tname].emplace_back(getPlaceName(p), arc->weight);
                    }
                }
                skipPlace(p);
                continueReductions = true;
            }

        }
        assert(consistent());
        return continueReductions;
    }


    bool Reducer::ReducebyRuleG(uint32_t* placeInQuery, bool remove_loops, bool remove_consumers) {
        if(!remove_loops) return false;
        bool continueReductions = false;
        for(uint32_t t = 0; t < parent->numberOfTransitions(); ++t)
        {
            if(hasTimedout()) return false;
            Transition& trans = parent->_transitions[t];
            if(trans.skip) continue;
            if(trans.inhib) continue;
            if(trans.pre.size() < trans.post.size()) continue;
            if(!remove_loops && trans.pre.size() == 0) continue;

            auto postit = trans.post.begin();
            auto preit = trans.pre.begin();

            bool ok = true;
            while(true)
            {
                if(preit == trans.pre.end() && postit == trans.post.end())
                    break;
                if(preit == trans.pre.end())
                {
                    ok = false;
                    break;
                }
                if(preit->inhib || parent->_places[preit->place].inhib)
                {
                    ok = false;
                    break;
                }
                if(postit != trans.post.end() && preit->place == postit->place)
                {
                    if(!remove_consumers && preit->weight != postit->weight)
                    {
                        ok = false;
                        break;
                    }
                    if((placeInQuery[preit->place] > 0 && preit->weight != postit->weight) ||
                       (placeInQuery[preit->place] == 0 && preit->weight < postit->weight))
                    {
                        ok = false;
                        break;
                    }
                    ++preit;
                    ++postit;
                }
                else if(postit == trans.post.end() || preit->place < postit->place)
                {
                    if(placeInQuery[preit->place] > 0 || !remove_consumers)
                    {
                        ok = false;
                        break;
                    }
                    ++preit;
                }
                else
                {
                    // could not match a post with a pre
                    ok = false;
                    break;
                }
            }
            if(ok)
            {
                for(preit = trans.pre.begin();preit != trans.pre.end(); ++preit)
                {
                    if(preit->inhib || parent->_places[preit->place].inhib)
                    {
                        ok = false;
                        break;
                    }
                }
            }

            if(!ok) continue;
            ++_ruleG;
            skipTransition(t);
        }
        assert(consistent());
        return continueReductions;
    }

    bool Reducer::ReducebyRuleH(uint32_t* placeInQuery)
    {
        if(reconstructTrace)
            return false; // we don't know where in the loop the tokens are needed
        auto transok = [this](uint32_t t) -> uint32_t {
            auto& trans = parent->_transitions[t];
            if(_tflags[t] != 0)
                return _tflags[t];
            _tflags[t] = 1;
            if(trans.inhib ||
               trans.pre.size() != 1 ||
               trans.post.size() != 1)
            {
                return 2;
            }

            auto p1 = trans.pre[0].place;
            auto p2 = trans.post[0].place;

            // we actually do not need weights to be 1 here.
            // there is a special case when the places are always "inputting"
            // and "outputting" with a GCD that is equal to the weight of the
            // specific transition.
            // Ie, the place always have a number of tokens (disregarding
            // initial tokens) that is dividable with the transition weight

            if(trans.pre[0].weight != 1 ||
               trans.post[0].weight != 1 ||
               p1 == p2 ||
               parent->_places[p1].inhib ||
               parent->_places[p2].inhib)
            {
                return 2;
            }
            return 1;
        };

        auto removeLoop = [this,placeInQuery](std::vector<uint32_t>& loop) -> bool {
            size_t i = 0;
            for(; i < loop.size(); ++i)
                if(loop[i] == loop.back())
                    break;

            assert(_tflags[loop.back()]== 1);
            if(i == loop.size() - 1)
                return false;

            auto p1 = parent->_transitions[loop[i]].pre[0].place;
            bool removed = false;

            for(size_t j = i + 1; j < loop.size() - 1; ++j)
            {
                if(hasTimedout())
                    return removed;
                auto p2 = parent->_transitions[loop[j]].pre[0].place;
                if(placeInQuery[p2] > 0 || placeInQuery[p1] > 0)
                {
                    p1 = p2;
                    continue;
                }
                if(p1 == p2)
                {
                    continue;
                }
                removed = true;
                ++_ruleH;
                skipTransition(loop[j-1]);
                auto& place1 = parent->_places[p1];
                auto& place2 = parent->_places[p2];

                {

                    for(auto p2it : place2.consumers)
                    {
                        auto& t = parent->_transitions[p2it];
                        auto arc = getInArc(p2, t);
                        assert(arc != t.pre.end());
                        assert(arc->place == p2);
                        auto a = *arc;
                        a.place = p1;
                        auto dest = std::lower_bound(t.pre.begin(), t.pre.end(), a);
                        if(dest == t.pre.end() || dest->place != p1)
                        {
                            t.pre.insert(dest, a);
                            auto lb = std::lower_bound(place1.consumers.begin(), place1.consumers.end(), p2it);
                            place1.consumers.insert(lb, p2it);
                        }
                        else
                        {
                            dest->weight += a.weight;
                        }
                        consistent();
                    }
                }

                {
                    auto p2it = place2.producers.begin();

                    for(;p2it != place2.producers.end(); ++p2it)
                    {
                        auto& t = parent->_transitions[*p2it];
                        Arc a = *getOutArc(t, p2);
                        a.place = p1;
                        auto dest = std::lower_bound(t.post.begin(), t.post.end(), a);
                        if(dest == t.post.end() || dest->place != p1)
                        {
                            t.post.insert(dest, a);
                            auto lb = std::lower_bound(place1.producers.begin(), place1.producers.end(), *p2it);
                            place1.producers.insert(lb, *p2it);
                        }
                        else
                        {
                            dest->weight += a.weight;
                        }
                        consistent();
                    }
                }
                parent->initialMarking[p1] += parent->initialMarking[p2];
                skipPlace(p2);
                assert(placeInQuery[p2] == 0);
            }
            return removed;
        };

        bool continueReductions = false;
        for(uint32_t t = 0; t < parent->numberOfTransitions(); ++t)
        {
            if(hasTimedout())
                return continueReductions;
            _tflags.resize(parent->_transitions.size(), 0);
            std::fill(_tflags.begin(), _tflags.end(), 0);
            std::vector<uint32_t> stack;
            {
                if(_tflags[t] != 0) continue;
                auto& trans = parent->_transitions[t];
                if(trans.skip) continue;
                _tflags[t] = transok(t);
                if(_tflags[t] != 1) continue;
                stack.push_back(t);
            }
            bool outer = true;
            while(stack.size() > 0 && outer)
            {
                if(hasTimedout())
                    return continueReductions;
                auto it = stack.back();
                auto post = parent->_transitions[it].post[0].place;
                bool found = false;
                for(auto& nt : parent->_places[post].consumers)
                {
                    if(hasTimedout())
                        return continueReductions;
                    auto& nexttrans = parent->_transitions[nt];
                    if(nt == it || nexttrans.skip)
                        continue; // handled elsewhere
                    if(_tflags[nt] == 1 && stack.size() > 1)
                    {
                        stack.push_back(nt);
                        bool found = removeLoop(stack);
                        continueReductions |= found;

                        if(found)
                        {
                            outer = false;
                            break;
                        }
                        else
                        {
                            stack.pop_back();
                        }
                    }
                    else if(_tflags[nt] == 0)
                    {
                        _tflags[nt] = transok(nt);
                        if(_tflags[nt] == 2)
                        {
                            continue;
                        }
                        else
                        {
                            assert(_tflags[nt] == 1);
                            stack.push_back(nt);
                            found = true;
                            break;
                        }
                    }
                    else
                    {
                        continue;
                    }
                }
                if(!found && outer)
                {
                    _tflags[it] = 2;
                    stack.pop_back();
                }
            }
        }
        return continueReductions;
    }

    bool Reducer::ReducebyRuleJ(uint32_t* placeInQuery)
    {
        bool continueReductions = false;
        for(uint32_t t = 0; t < parent->numberOfTransitions(); ++t)
        {
            if(hasTimedout())
                return continueReductions;

            if(parent->_transitions[t].skip ||
               parent->_transitions[t].inhib ||
               parent->_transitions[t].pre.size() != 1)
                continue;
            auto p = parent->_transitions[t].pre[0].place;
            if(placeInQuery[p] > 0)
            {
                continue; // can be relaxed
            }
            if(parent->initialMarking[p] > 0)
            {
                continue; // can be relaxed
            }
            const Place& place = parent->_places[p];
            if(place.skip) continue;
            if(place.inhib) continue;
            if(place.consumers.size() < 1) continue;
            if(place.producers.size() < 1) continue;

            // check that prod and cons are not overlapping
            const auto presize = place.producers.size(); // can be relaxed >= 2
            const auto postsize = place.consumers.size(); // can be relaxed >= 2
            bool ok = true;
            for(size_t i = 0; i < postsize; ++i)
            {   // this can be done smarter than a quadratic loop!
                for(size_t j = 0; j < presize; ++j)
                {
                    ok &= place.consumers[i] != place.producers[j];
                }
            }
            if(!ok) continue;
            // check that post of consumer is not messing with query or inhib
            // if either all pre or all post are query-free, we are ok.
            bool inquery = false;
            for(auto t : place.consumers)
            {
                Transition& trans = parent->_transitions[t];
                if(trans.pre.size() == 1) // can be relaxed
                {
                    // check that weights match
                    // can be relaxed
                    ok &= trans.pre[0].weight == 1;
                    ok &= !trans.pre[0].inhib;
                }
                else
                {
                    ok = false;
                    break;
                }
                for(auto& pp : trans.post)
                {
                    ok &= !parent->_places[pp.place].inhib;
                    inquery |= placeInQuery[pp.place] > 0;
                    ok &= pp.weight == 1; // can be relaxed
                }
                if(!ok)
                    break;
            }
            if(!ok) continue;
            // check that pre of producing do not mess with query or inhib
            for(auto& t : place.producers)
            {
                Transition& trans = parent->_transitions[t];
                for(const auto& arc : trans.post)
                {
                    ok &= !inquery || placeInQuery[arc.place] == 0;
                    ok &= !parent->_places[arc.place].inhib;
                }
            }
            if(!ok) continue;
            ++_ruleJ;
            continueReductions = true;
            // otherwise we can skip the place by merging up the two transitions
            // constructing 4 new transitions, one for each combination.
            // In the binary case, we want to achieve the following four transitions
            // post[n] = pre[n] + post[n]
            // pre[0] = pre[0] + post[1]
            // pre[1] = pre[1] + post[0]

            // start by copying out the post of each of the posts
            Place pp = place;
            skipPlace(p);
            std::vector<std::vector<Arc>> posts;
            std::vector<Transition> pres;

            for(auto t : pp.consumers)
                posts.push_back(parent->_transitions[t].post);

            for(auto t : pp.producers)
                pres.push_back(parent->_transitions[t]);

            // remove old transitions, we will create new ones
            for(auto t : pp.consumers)
                skipTransition(t);

            for(auto t : pp.producers)
                skipTransition(t);

            // compute all permutations
            for(auto& trans : pres)
            {
                for(auto& postset : posts)
                {
                    auto id = parent->_transitions.size();
                    if(!_skipped_trans.empty())
                        id = _skipped_trans.back();
                    else
                    {
                        continue;
                        parent->_transitions.emplace_back();
                    }
                    parent->_transitions[id] = trans;
                    auto& target = parent->_transitions[id];
                    for(auto& arc : postset)
                        target.addPostArc(arc);

                    // add to places
                    if(_skipped_trans.empty())
                        parent->_transitionnames[newTransName()] = id;

                    for(auto& arc : target.pre)
                        parent->_places[arc.place].addConsumer(id);
                    for(auto& arc : target.post)
                        parent->_places[arc.place].addProducer(id);
                    if(!_skipped_trans.empty())
                    {
                        --_removedTransitions; // recycling
                        _skipped_trans.pop_back();
                    }
                    parent->_transitions[id].skip = false;
                    parent->_transitions[id].inhib = false;
                    consistent();
                }
            }
            consistent();
        }
        return continueReductions;
    }

    bool Reducer::ReducebyRuleK(uint32_t *placeInQuery, bool remove_consumers) {
        bool reduced = false;
        auto opt = relevant(placeInQuery, remove_consumers);
        if (!opt)
            return false;
        auto[tseen, pseen] = opt.value();
        for (std::size_t p = 0; p < parent->numberOfPlaces(); ++p) {
            if (placeInQuery[p] != 0)
                pseen[p] = true;
        }

        for (std::size_t t = 0; t < parent->numberOfTransitions(); ++t) {
            auto transition = parent->_transitions[t];
            if (!tseen[t] && !transition.skip && !transition.inhib && transition.pre.size() == 1 &&
                transition.post.size() == 1
                && transition.pre[0].place == transition.post[0].place) {
                auto p = transition.pre[0].place;
                if (!pseen[p] && !parent->_places[p].inhib) {
                    if (parent->initialMarking[p] >= transition.pre[0].weight){
                        //Mark the initially marked self loop as relevant.
                        tseen[t] = true;
                        pseen[p] = true;
                        reduced |= remove_irrelevant(placeInQuery, tseen, pseen);
                        _ruleK++;
                        return reduced;
                    }
                    if (transition.pre[0].weight == 1){
                        for (auto t2 : parent->_places[p].consumers) {
                            auto transition2 = parent->_transitions[t2];
                            if (t != t2 && !tseen[t2] && !transition2.skip) {
                                skipTransition(t2);
                                reduced = true;
                                _ruleK++;
                            }
                        }
                    }
                }
            }
        }
        return reduced;
    }

    std::optional<std::pair<std::vector<bool>, std::vector<bool>>>
    Reducer::relevant(const uint32_t *placeInQuery, bool remove_consumers) {
        std::vector<uint32_t> wtrans;
        std::vector<bool> tseen(parent->numberOfTransitions(), false);
        for (uint32_t p = 0; p < parent->numberOfPlaces(); ++p) {
            if (hasTimedout()) return std::nullopt;
            if (placeInQuery[p] > 0) {
                const Place &place = parent->_places[p];
                for (auto t : place.consumers) {
                    if (!tseen[t]) {
                        wtrans.push_back(t);
                        tseen[t] = true;
                    }
                }
                for (auto t : place.producers) {
                    if (!tseen[t]) {
                        wtrans.push_back(t);
                        tseen[t] = true;
                    }
                }
            }
        }
        std::vector<bool> pseen(parent->numberOfPlaces(), false);

        while (!wtrans.empty()) {
            if (hasTimedout()) return std::nullopt;
            auto t = wtrans.back();
            wtrans.pop_back();
            const Transition &trans = parent->_transitions[t];
            for (const Arc &arc : trans.pre) {
                const Place &place = parent->_places[arc.place];
                if (arc.inhib) {
                    for (auto pt : place.consumers) {
                        if (!tseen[pt]) {
                            // Summary of block: 'pt' is seen unless it:
                            // - Is inhibited by 'place'
                            // - Forms a decreasing loop on 'place' that cannot lower the marking of 'place' below the weight of 'arc'
                            // - Forms a non-decreasing loop on 'place'
                            Transition &trans = parent->_transitions[pt];
                            auto it = trans.post.begin();
                            for (; it != trans.post.end(); ++it)
                                if (it->place >= arc.place) break;

                            if (it != trans.post.end() && it->place == arc.place) {
                                auto it2 = trans.pre.begin();
                                // Find the arc from place to trans we know to exist because that is how we found trans in the first place
                                for (; it2 != trans.pre.end(); ++it2)
                                    if (it2->place >= arc.place) break;
                                // No need for a || it2->place != arc.place condition because we know the loop will always break on it2->place == arc.place
                                if (it2->inhib || it->weight >= arc.weight || it->weight >= it2->weight) continue;
                            }
                            tseen[pt] = true;
                            wtrans.push_back(pt);
                        }
                    }
                } else {
                    for (auto pt : place.producers) {
                        if (!tseen[pt]) {
                            // Summary of block: pt is seen unless it forms a non-increasing loop on place
                            Transition &trans = parent->_transitions[pt];
                            auto it = trans.pre.begin();
                            for (; it != trans.pre.end(); ++it)
                                if (it->place >= arc.place) break;

                            if (it != trans.pre.end() && it->place == arc.place && !it->inhib) {
                                auto it2 = trans.post.begin();
                                for (; it2 != trans.post.end(); ++it2)
                                    if (it2->place >= arc.place) break;
                                if (it->weight >= it2->weight) continue;
                            }

                            tseen[pt] = true;
                            wtrans.push_back(pt);
                        }
                    }

                    for (auto pt : place.consumers) {
                        if (!tseen[pt] && (!remove_consumers || placeInQuery[arc.place] > 0)) {
                            tseen[pt] = true;
                            wtrans.push_back(pt);
                        }
                    }
                }
                pseen[arc.place] = true;
            }
        }
        return std::make_optional(std::pair(tseen, pseen));
    }

    bool Reducer::remove_irrelevant(const uint32_t* placeInQuery, const std::vector<bool> &tseen, const std::vector<bool> &pseen) {
        bool reduced = false;
        for(size_t t = 0; t < parent->numberOfTransitions(); ++t)
        {
            if(!tseen[t] && !parent->_transitions[t].skip)
            {
                skipTransition(t);
                reduced = true;
            }
        }

        for(size_t p = 0; p < parent->numberOfPlaces(); ++p)
        {
            if(!pseen[p] && !parent->_places[p].skip && placeInQuery[p] == 0)
            {
                assert(placeInQuery[p] == 0);
                skipPlace(p);
                reduced = true;
            }
        }
        return reduced;
    }

    void Reducer::Reduce(QueryPlaceAnalysisContext& context, int enablereduction, bool reconstructTrace, int timeout, bool remove_loops, bool all_reach, bool all_ltl, bool contains_next, std::vector<uint32_t>& reduction) {
        this->_timeout = timeout;
        _timer = std::chrono::high_resolution_clock::now();
        assert(consistent());
        this->reconstructTrace = reconstructTrace;
        if(reconstructTrace && enablereduction >= 1 && enablereduction <= 2)
            std::cout << "Rule H disabled when a trace is requested." << std::endl;
        if (enablereduction == 2) { // for k-boundedness checking only rules A, D and H are applicable
            bool changed = true;
            while (changed && !hasTimedout()) {
                changed = false;
                if(!contains_next)
                {
                    while(ReducebyRuleA(context.getQueryPlaceCount())) changed = true;
                    while(ReducebyRuleD(context.getQueryPlaceCount(), all_reach, false)) changed = true;
                    while(ReducebyRuleH(context.getQueryPlaceCount())) changed = true;
                }
            }
        }
        else if (enablereduction == 1) { // in the aggressive reduction all four rules are used as long as they remove something
            bool changed = false;
            do
            {
                if(remove_loops && !contains_next)
                    while(ReducebyRuleI(context.getQueryPlaceCount(), all_reach)) changed = true;
                do{
                    do { // start by rules that do not move tokens
                        changed = false;
                        while(ReducebyRuleE(context.getQueryPlaceCount())) changed = true;
                        while(ReducebyRuleC(context.getQueryPlaceCount())) changed = true;
                        if(remove_loops && !contains_next)
                            while(ReducebyRuleF(context.getQueryPlaceCount())) changed = true;
                        if(!contains_next)
                        {
                            while(ReducebyRuleG(context.getQueryPlaceCount(), remove_loops, all_reach)) changed = true;
                            while(ReducebyRuleD(context.getQueryPlaceCount(), all_reach, remove_loops && all_ltl)) changed = true;
                            //changed |= ReducebyRuleK(context.getQueryPlaceCount(), remove_consumers); //Rule disabled as correctness has not been proved. Experiments indicate that it is not correct for CTL.
                        }
                    } while(changed && !hasTimedout());
                    if(!contains_next)
                    { // then apply tokens moving rules
                        //while(ReducebyRuleJ(context.getQueryPlaceCount())) changed = true;
                        while(ReducebyRuleB(context.getQueryPlaceCount(), remove_loops, all_reach)) changed = true;
                        while(ReducebyRuleA(context.getQueryPlaceCount())) changed = true;
                    }
                } while(changed && !hasTimedout());
                if(!contains_next && !changed)
                {
                    // Only try RuleH last. It can reduce applicability of other rules.
                    while(ReducebyRuleH(context.getQueryPlaceCount())) changed = true;
                }
            } while(!hasTimedout() && changed);

        }
        else
        {
            const char* rnames = "ABCDEFGHIJK";
            for(int i = reduction.size() - 1; i >= 0; --i)
            {
                if(contains_next)
                {
                    if(reduction[i] != 2 && reduction[i] != 4 && reduction[i] != 5)
                    {
                        std::cerr << "Skipping Rule" << rnames[reduction[i]] << " due to NEXT operator in proposition" << std::endl;
                        reduction.erase(reduction.begin() + i);
			            continue;
                    }
                }
                if(!remove_loops && (reduction[i] == 5 || reduction[i] == 8))
                {
                    std::cerr << "Skipping Rule" << rnames[reduction[i]] << " as proposition is loop sensitive" << std::endl;
                    reduction.erase(reduction.begin() + i);
                }
            }
            bool changed = true;
            while(changed && !hasTimedout())
            {
                changed = false;
                for(auto r : reduction)
                {
#ifndef NDEBUG
                    auto c = std::chrono::high_resolution_clock::now();
                    auto op = _removedPlaces;
                    auto ot = _removedTransitions;
#endif
                    switch(r)
                    {
                        case 0:
                            while(ReducebyRuleA(context.getQueryPlaceCount())) changed = true;
                            break;
                        case 1:
                            while(ReducebyRuleB(context.getQueryPlaceCount(), remove_loops, all_reach)) changed = true;
                            break;
                        case 2:
                            while(ReducebyRuleC(context.getQueryPlaceCount())) changed = true;
                            break;
                        case 3:
                            while(ReducebyRuleD(context.getQueryPlaceCount(), all_reach, remove_loops && all_ltl)) changed = true;
                            break;
                        case 4:
                            while(ReducebyRuleE(context.getQueryPlaceCount())) changed = true;
                            break;
                        case 5:
                            while(ReducebyRuleF(context.getQueryPlaceCount())) changed = true;
                            break;
                        case 6:
                            while(ReducebyRuleG(context.getQueryPlaceCount(), remove_loops, all_reach)) changed = true;
                            break;
                        case 7:
                            while(ReducebyRuleH(context.getQueryPlaceCount())) changed = true;
                            break;
                        case 8:
                            while(ReducebyRuleI(context.getQueryPlaceCount(), all_reach)) changed = true;
                            break;
                        case 9:
                            while(ReducebyRuleJ(context.getQueryPlaceCount())) changed = true;
                            break;
                        case 10:
                            if (ReducebyRuleK(context.getQueryPlaceCount(), all_reach)) changed = true;
                            break;
                    }
#ifndef NDEBUG
                    auto end = std::chrono::high_resolution_clock::now();
                    auto diff = std::chrono::duration_cast<std::chrono::seconds>(end - c);
                    std::cout << "SPEND " << diff.count()  << " ON " << rnames[r] << std::endl;
                    std::cout << "REM " << ((int64_t)_removedPlaces-(int64_t)op) << " "
                        << ((int64_t)_removedTransitions-(int64_t)ot) << std::endl;
#endif
                    if(hasTimedout())
                        break;
                }
            }
        }

        return;
    }

    void Reducer::postFire(std::ostream& out, const std::string& transition) const
    {
        auto it = _postfire.find(transition);
        if(it != std::end(_postfire))
        {

            for(const auto& el : it->second)
            {
                out << "\t<transition id=\"" << el << "\">\n";
                extraConsume(out, *el);
                out << "\t</transition>\n";
                postFire(out, *el);
            }
        }
    }

    void Reducer::initFire(std::ostream& out) const
    {
        for(const auto& init : _initfire)
        {
            out << "\t<transition id=\"" << *init << "\">\n";
            extraConsume(out, *init);
            out << "\t</transition>\n";
            postFire(out, *init);
        }
    }

    void Reducer::extraConsume(std::ostream& out, const std::string& transition) const
    {
        auto it = _extraconsume.find(transition);
        if(it != std::end(_extraconsume))
        {
            for(const auto& ec : it->second)
            {
                out << ec;
            }
        }
    }

} //PetriNet namespace
