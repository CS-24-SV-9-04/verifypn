#ifndef COLOREDPETRINETSTATE_H
#define COLOREDPETRINETSTATE_H

#include <queue>
#include <utility>
#include <spot/twa/twagraph.hh>

#include "ColoredPetriNetMarking.h"

namespace PetriEngine::ExplicitColored {
    struct ColoredPetriNetStateFixed {
        explicit ColoredPetriNetStateFixed(ColoredPetriNetMarking marking)
            : marking(std::move(marking)) {};
        ColoredPetriNetStateFixed(const ColoredPetriNetStateFixed& oldState) = default;
        ColoredPetriNetStateFixed(ColoredPetriNetStateFixed&&) = default;
        ColoredPetriNetStateFixed& operator=(const ColoredPetriNetStateFixed&) = default;
        ColoredPetriNetStateFixed& operator=(ColoredPetriNetStateFixed&&) = default;

        void shrink() {
            marking.shrink();
        }

        void setDone() {
            _done = true;
        }

        [[nodiscard]] bool done() const {
            return _done;
        }

        [[nodiscard]] const Transition_t& getCurrentTransition() const {
            return _currentTransition;
        }

        [[nodiscard]] const Binding_t& getCurrentBinding() const {
            return _currentBinding;
        }

        void nextTransition() {
            _currentTransition += 1;
            _currentBinding = 0;
        }

        void nextBinding() {
            _currentBinding += 1;
        }

        void nextBinding(const Binding_t bid) {
            _currentBinding = bid + 1;
        }

        ColoredPetriNetMarking marking;
        size_t id = 0;

    private:
        bool _done = false;

        Binding_t _currentBinding = 0;
        Transition_t _currentTransition = 0;
    };

    struct BuchiStateIterDeleter {
        const spot::twa_graph *_automaton;

        void operator()(spot::twa_succ_iterator *iter) const
        {
            _automaton->release_iter(iter);
        }
    };


    struct ColoredPetriNetProductState : ColoredPetriNetStateFixed {
        ColoredPetriNetProductState(ColoredPetriNetMarking marking, size_t buchiState)
            : ColoredPetriNetStateFixed(std::move(marking)), _buchiState(buchiState), currentSuccessor({}) {}
        ColoredPetriNetProductState(ColoredPetriNetStateFixed markingState, size_t buchiState)
            : ColoredPetriNetStateFixed(std::move(markingState)), _buchiState(buchiState), currentSuccessor({}) {}

        ColoredPetriNetProductState(ColoredPetriNetProductState&& state) noexcept = default;
        ColoredPetriNetProductState& operator=(ColoredPetriNetProductState&&) noexcept = default;
        ColoredPetriNetProductState(const ColoredPetriNetProductState& state) = delete;
        ColoredPetriNetProductState& operator=(const ColoredPetriNetProductState&) = delete;
        bool operator==(const ColoredPetriNetProductState& other){
            if (_buchiState == other._buchiState && marking == other.marking){
                return true;
            }
                return false;
        }

        size_t getBuchiState() const {
            return _buchiState;
        }
        std::unique_ptr<spot::twa_succ_iterator, BuchiStateIterDeleter> iterState;
        ColoredPetriNetStateFixed currentSuccessor;
    private:
        size_t _buchiState;

    };

    struct ColoredPetriNetStateEven {
        ColoredPetriNetStateEven(const ColoredPetriNetStateEven& oldState, const size_t& numberOfTransitions) : marking(
            oldState.marking) {
            _map = std::vector<Binding_t>(numberOfTransitions);
        }

        ColoredPetriNetStateEven(ColoredPetriNetMarking marking, const size_t& numberOfTransitions) : marking(
            std::move(marking)) {
            _map = std::vector<Binding_t>(numberOfTransitions);
        }

        ColoredPetriNetStateEven(ColoredPetriNetStateEven&& state) = default;
        ColoredPetriNetStateEven(const ColoredPetriNetStateEven& state) = default;
        ColoredPetriNetStateEven& operator=(const ColoredPetriNetStateEven&) = default;
        ColoredPetriNetStateEven& operator=(ColoredPetriNetStateEven&&) = default;

        std::pair<Transition_t, Binding_t> getNextPair() {
            Transition_t tid = _currentIndex;
            Binding_t bid = std::numeric_limits<Binding_t>::max();
            if (done()) {
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

        void shrink() {
            marking.shrink();
        }

        [[nodiscard]] bool done() const {
            return _done;
        }

        ColoredPetriNetMarking marking;
        bool shuffle = false;
        size_t id;

    private:
        bool _done = false;
        std::vector<Binding_t> _map;
        uint32_t _currentIndex = 0;
        uint32_t _completedTransitions = 0;
    };
}

#endif //COLOREDPETRINETSTATE_H
