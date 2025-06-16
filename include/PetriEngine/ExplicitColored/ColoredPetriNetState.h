#ifndef COLOREDPETRINETSTATE_H
#define COLOREDPETRINETSTATE_H

#include <queue>
#include <utility>
#include <spot/twa/twagraph.hh>

#include "ColoredPetriNetMarking.h"

namespace PetriEngine::ExplicitColored {
    struct FixedSuccessorInfo {
        bool done = false;
        Binding_t currentBinding = 0;
        Transition_t currentTransition = 0;
    };

    struct EvenSuccessorInfo {
        bool shuffle = false;
        bool _done = false;
        std::vector<Binding_t> _map;
        uint32_t _currentIndex = 0;
        uint32_t _completedTransitions = 0;

        std::pair<Transition_t, Binding_t> getNextPair() {
            Transition_t tid = _currentIndex;
            Binding_t bid = std::numeric_limits<Binding_t>::max();
            if (_done) {
                return {tid, bid};
            }
            auto it = _map.begin() + _currentIndex;
            while (it != _map.end() && *it == std::numeric_limits<Binding_t>::max()) {
                ++it;
                ++_currentIndex;
            }
            if (it == _map.end()) {
                _currentIndex = 0;
                shuffle = true;
            }
            else {
                tid = _currentIndex;
                bid = *it;
                ++_currentIndex;
            }
            return {tid, bid};
        }

        //Takes last fired transition-binding pair, so next binding is bid + 1
        void updatePair(const Transition_t tid, const Binding_t bid) {
            if (tid < _map.size()) {
                auto& oldBid = _map[tid];
                if (bid == std::numeric_limits<Binding_t>::max()) {
                    if (bid != _map[tid]) {
                        oldBid = bid;
                        _completedTransitions += 1;
                        if (_completedTransitions == _map.size()) {
                            _done = true;
                        }
                    }
                }
                else {
                    oldBid = bid + 1;
                }
            }
        }
    };

    struct BuchiStateIterDeleter {
        const spot::twa_graph *_automaton;

        void operator()(spot::twa_succ_iterator *iter) const
        {
            _automaton->release_iter(iter);
        }
    };

    template<typename MarkingSuccInfo>
    struct CPNProductSuccessorInfo {
        MarkingSuccInfo markingSuccInfo;
        std::unique_ptr<spot::twa_succ_iterator, BuchiStateIterDeleter> iterState = nullptr;
        ColoredPetriNetMarking currentSuccessorMarking;
        bool has_prev_state() const {
            return iterState != nullptr;
        }
    };

    struct CPNProductState {
        ColoredPetriNetMarking marking;
        size_t buchiState;
        bool accepting;
        bool is_accepting() const {
            return accepting;
        }
    };
}

#endif //COLOREDPETRINETSTATE_H
