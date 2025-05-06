#ifndef COLOREDPETRINETSTATE_H
#define COLOREDPETRINETSTATE_H

#include <queue>
#include <utility>
#include <utils/structures/binarywrapper.h>

#include "ColoredEncoder.h"
#include "ColoredPetriNetMarking.h"

namespace PetriEngine::ExplicitColored {
    // struct AbstractColoredPetriNetState {
    //     ~AbstractColoredPetriNetState() = default;
    //
    //     AbstractColoredPetriNetState(AbstractColoredPetriNetState&& state) = default;
    //     AbstractColoredPetriNetState& operator=(const AbstractColoredPetriNetState& state) = default;
    //     AbstractColoredPetriNetState(AbstractColoredPetriNetState&) = default;
    //     AbstractColoredPetriNetState& operator=(AbstractColoredPetriNetState&&) = default;
    //
    //     AbstractColoredPetriNetState(const AbstractColoredPetriNetState& state) : id(state.id), _encoding(state._encoding) {};
    //     explicit AbstractColoredPetriNetState(ColoredPetriNetMarking marking) : marking(std::move(marking)) {};
    //
    //     void addEncoding(const ptrie::uchar* encoding, const size_t encodingSize) {
    //         _encodingSize = encodingSize;
    //         //En/decoding is not implemented for encodings bigger than uint16_max
    //         if (UINT16_MAX == encodingSize) {
    //             return;
    //         }
    //         _encoding = new ptrie::uchar[encodingSize];
    //         memcpy(_encoding, encoding, encodingSize);
    //     }
    //
    //     void encode() {
    //         if (_encoded || _encodingSize == UINT16_MAX) return;
    //         marking = ColoredPetriNetMarking{};
    //         _encoded = true;
    //     }
    //
    //     void decode(const ColoredEncoder* encoder) {
    //         if (!_encoded) return;
    //         marking = encoder->decode(_encoding);
    //         _encoded = false;
    //     }
    //
    //     [[nodiscard]] bool done() const {
    //         return _done;
    //     }
    //
    //     void setDone() {
    //         _done = true;
    //     }
    //
    //     void shrink() {
    //         marking.shrink();
    //     }
    //
    //     bool shuffle = false;
    //     size_t id;
    //     ColoredPetriNetMarking marking;
    // protected:
    //     bool _done = false;
    //     ptrie::uchar* _encoding;
    //     size_t _encodingSize = 0;
    // private:
    //     bool _encoded = false;
    // };

    struct ColoredPetriNetStateFixed {
        explicit ColoredPetriNetStateFixed(ColoredPetriNetMarking marking) : marking(std::move(marking)) {
        };
        ColoredPetriNetStateFixed(const ColoredPetriNetStateFixed&) = default;
        ColoredPetriNetStateFixed(ColoredPetriNetStateFixed&&) = default;
        ColoredPetriNetStateFixed(ColoredPetriNetStateFixed&) = default;
        ColoredPetriNetStateFixed& operator=(const ColoredPetriNetStateFixed&) = default;
        ColoredPetriNetStateFixed& operator=(ColoredPetriNetStateFixed&&) = default;

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

        void addEncoding(const ptrie::uchar* encoding, const size_t encodingSize) {
            _encodingSize = encodingSize;
            //En/decoding is not implemented for encodings bigger than uint16_max
            if (UINT16_MAX == encodingSize) {
                return;
            }
            _encoding = new ptrie::uchar[encodingSize];
            memcpy(_encoding, encoding, encodingSize);
        }

        void encode() {
            if (_encoded || _encodingSize == UINT16_MAX) return;
            marking = ColoredPetriNetMarking{};
            _encoded = true;
        }

        void decode(const ColoredEncoder* encoder) {
            if (!_encoded) return;
            marking = encoder->decode(_encoding);
            _encoded = false;
        }

        [[nodiscard]] bool done() const {
            return _done;
        }

        void setDone() {
            _done = true;
        }

        void shrink() {
            marking.shrink();
        }

        bool shuffle = false;
        size_t id;
        ColoredPetriNetMarking marking;
    private:
        Binding_t _currentBinding = 0;
        Transition_t _currentTransition = 0;
        bool _done = false;
        ptrie::uchar* _encoding;
        size_t _encodingSize = 0;
        bool _encoded = false;
    };

    struct ColoredPetriNetStateEven final {
        ColoredPetriNetStateEven(const ColoredPetriNetStateEven& oldState, const size_t& numberOfTransitions) :
            marking(oldState.marking) {
            _map = std::vector<Binding_t>(numberOfTransitions);
        }

        ColoredPetriNetStateEven(const ColoredPetriNetMarking& marking, const size_t& numberOfTransitions) :
            marking(marking)  {
            _map = std::vector<Binding_t>(numberOfTransitions);
        }

        ColoredPetriNetStateEven(ColoredPetriNetStateEven&& state) = default;
        ColoredPetriNetStateEven(const ColoredPetriNetStateEven& state) = default;
        ColoredPetriNetStateEven(ColoredPetriNetStateEven& state) = default;
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

        void addEncoding(const ptrie::uchar* encoding, const size_t encodingSize) {
            _encodingSize = encodingSize;
            //En/decoding is not implemented for encodings bigger than uint16_max
            if (UINT16_MAX == encodingSize) {
                return;
            }
            _encoding = new ptrie::uchar[encodingSize];
            memcpy(_encoding, encoding, encodingSize);
        }

        void encode() {
            if (_encoded || _encodingSize == UINT16_MAX) return;
            marking = ColoredPetriNetMarking{};
            _encoded = true;
        }

        void decode(const ColoredEncoder* encoder) {
            if (!_encoded) return;
            marking = encoder->decode(_encoding);
            _encoded = false;
        }

        [[nodiscard]] bool done() const {
            return _done;
        }

        void setDone() {
            _done = true;
        }

        void shrink() {
            marking.shrink();
        }

        bool shuffle = false;
        size_t id;
        ColoredPetriNetMarking marking;
    private:
        std::vector<Binding_t> _map;
        uint32_t _currentIndex = 0;
        uint32_t _completedTransitions = 0;
        bool _done = false;
        ptrie::uchar* _encoding;
        size_t _encodingSize = 0;
        bool _encoded = false;
    };
}

#endif //COLOREDPETRINETSTATE_H
