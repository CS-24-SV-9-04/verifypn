#ifndef COLOREDPETRINETSTATE_H
#define COLOREDPETRINETSTATE_H

#include <queue>
#include <utility>
#include <spot/twa/twagraph.hh>

#include "LTL/Structures/BuchiAutomaton.h"
#include "ColoredPetriNetMarking.h"
#include "CPNProductState.h"

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

        [[nodiscard]] bool isDeadlock() const {
            return _deadlock;
        }

        void setNotDeadlock() {
            _deadlock = false;
        }

        ColoredPetriNetMarking marking;
        size_t id = 0;

    private:
        Binding_t _currentBinding = 0;
        Transition_t _currentTransition = 0;
        bool _done = false;
        bool _deadlock = true;
    };

    struct BuchiStateIterDeleter {
        const spot::twa_graph *_automaton;

        void operator()(spot::twa_succ_iterator *iter) const
        {
            _automaton->release_iter(iter);
        }
    };


    struct CPNProductStateFixed {
        explicit CPNProductStateFixed(CPNProductState productState)
            : _buchiState(productState.buchiState),
            _markingGeneratorState(std::move(productState.marking)),
            _currentSuccessor({}, {})
        { }
        CPNProductStateFixed(CPNProductStateFixed&&) noexcept = default;
        CPNProductStateFixed& operator=(CPNProductStateFixed&&) noexcept = default;
        CPNProductStateFixed(const CPNProductStateFixed&) = delete;
        CPNProductStateFixed& operator=(const CPNProductStateFixed&) = delete;

        [[nodiscard]] CPNProductState makeProductState() const {
            return {_markingGeneratorState.marking, _buchiState};
        }

        [[nodiscard]] bool done() const {
            return _done;
        }
    private:
        size_t _buchiState;
        ColoredPetriNetStateFixed _markingGeneratorState;
        CPNProductState _currentSuccessor;
        std::unique_ptr<spot::twa_succ_iterator, BuchiStateIterDeleter> _iterState = nullptr;
        bool _done = false;
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

        [[nodiscard]] bool isDeadlock() const {
            return _deadlock;
        }

        void setNotDeadlock() {
            _deadlock = false;
        }

        ColoredPetriNetMarking marking;
        bool shuffle = false;
        size_t id = 0;

    private:
        bool _done = false;
        bool _deadlock = true;
        std::vector<Binding_t> _map;
        uint32_t _currentIndex = 0;
        uint32_t _completedTransitions = 0;
    };


    template<typename SuccessorGeneratorState>
    struct CPNProductStateSpecialized {
        explicit CPNProductStateSpecialized(size_t buchiState, SuccessorGeneratorState state)
            : _buchiState(buchiState),
            _successorGeneratorState(std::move(state)),
            _currentSuccessor({}, {})
        { }
        CPNProductStateSpecialized(CPNProductStateSpecialized&&) noexcept = default;
        CPNProductStateSpecialized& operator=(CPNProductStateSpecialized&&) noexcept = default;
        CPNProductStateSpecialized(const CPNProductStateSpecialized&) = delete;
        CPNProductStateSpecialized& operator=(const CPNProductStateSpecialized&) = delete;

        [[nodiscard]] CPNProductState makeProductState() const {
            return {_successorGeneratorState.marking, _buchiState};
        }

        [[nodiscard]] bool done() const {
            return _done;
        }

        [[nodiscard]] void setDone() {
            _done = true;
        }

        spot::twa_succ_iterator* getIterState() {
            return _iterState.get();
        }

        void setIterState(std::unique_ptr<spot::twa_succ_iterator, BuchiStateIterDeleter> iterState) {
            _iterState = std::move(iterState);
        }

        size_t getBuchiState() const {
            return _buchiState;
        }

        void setBuchiState(size_t buchiState) {
            _buchiState = buchiState;
        }

        void setSuccessorMarking(ColoredPetriNetMarking marking) {
            _currentSuccessor.marking = std::move(marking);
        }

        SuccessorGeneratorState& getSuccessorGeneratorState() {
            return _successorGeneratorState;
        }

        ColoredPetriNetMarking& getSuccessorMarking() {
            return _currentSuccessor.marking;
        }

        const CPNProductState& getSuccessor() const {
            return _currentSuccessor;
        }
    private:
        size_t _buchiState;
        SuccessorGeneratorState _successorGeneratorState;
        CPNProductState _currentSuccessor;
        std::unique_ptr<spot::twa_succ_iterator, BuchiStateIterDeleter> _iterState = nullptr;
        bool _done = false;
    };
}

#endif //COLOREDPETRINETSTATE_H
