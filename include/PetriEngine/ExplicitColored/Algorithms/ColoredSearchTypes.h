#ifndef COLORED_SEARCH_TYPES_H
#define COLORED_SEARCH_TYPES_H

#include <queue>
#include <random>
#include <stack>

#include "PetriEngine/ExplicitColored/Visitors/HeuristicVisitor.h"

namespace PetriEngine::ExplicitColored {
    template <typename T, bool encodeWaitingList>
    class DFSStructure {
    public:
        explicit DFSStructure(ColoredEncoder& encoder):
            _encoder(encodeWaitingList ? &encoder : nullptr) {};

        T& next() {
            return _waiting.top();
        }

        void remove() {
            _waiting.pop();
            if constexpr (encodeWaitingList) {
                if (!_waiting.empty()) {
                    _waiting.top().decode(this->_encoder);
                }
            }
        }

        void add(T state) {
            if constexpr(encodeWaitingList) {
                if (!_waiting.empty()) {
                    _waiting.top().encode();
                }
            }
            _waiting.emplace(std::move(state));
        }

        [[nodiscard]] bool empty() const {
            return _waiting.empty();
        }

        [[nodiscard]] uint32_t size() const {
            return _waiting.size();
        }

        static void shuffle() {};
    private:
        std::stack<T> _waiting;
        ColoredEncoder* _encoder;
    };

    template <typename T, bool encodeWaitingList>
    class BFSStructure  {
    public:
        explicit BFSStructure(ColoredEncoder& encoder) : _encoder(encodeWaitingList ? &encoder : nullptr) {};

        T& next() {
            return waiting.front();
        }

        void remove() {
            if constexpr (encodeWaitingList) {
                waiting.front().decode(this->_encoder);
            }
            waiting.pop();
        }

        void add(T state) {
            if constexpr (encodeWaitingList) {
                state.encode();
            }
            waiting.emplace(std::move(state));
        }

        [[nodiscard]] bool empty() const {
            return waiting.empty();
        }

        [[nodiscard]] uint32_t size() const {
            return waiting.size();
        }
        static void shuffle() {};
    private:
        std::queue<T> waiting;
        ColoredEncoder* _encoder;
    };

    template <typename T, bool encodeWaitingList>
    class RDFSStructure  {
    public:
        explicit RDFSStructure(ColoredEncoder& encoder, const size_t seed) :
            _rng(seed), _encoder(encodeWaitingList ? &encoder : nullptr) {}

        T& next() {
            if (_stack.empty()) {
                shuffle();
            }
            return _stack.top();
        }

        void remove() {
            _stack.pop();
            shuffle();
        }

        void shuffle() {
            if constexpr (encodeWaitingList) {
                if (!_stack.empty()) {
                    _stack.top().encode();
                }
            }
            std::shuffle(_cache.begin(), _cache.end(), _rng);
            for (auto & it : _cache) {
                _stack.emplace(std::move(it));
            }
            if (!_stack.empty()) {
                _cache.clear();
                if constexpr (encodeWaitingList) {
                    _stack.top().decode(this->_encoder);
                }
            }
        }

        void add(T state) {
            if constexpr (encodeWaitingList) {
                state.encode();
            }
            _cache.push_back(std::move(state));
        }

        [[nodiscard]] bool empty() const {
            return _stack.empty() && _cache.empty();
        }

        [[nodiscard]] uint32_t size() const {
            return _stack.size() + _cache.size();
        }
    private:
        std::stack<T> _stack;
        std::vector<T> _cache;
        std::default_random_engine _rng;
        ColoredEncoder* _encoder;
    };

    template<typename T>
    struct WeightedState {
        mutable T cpn;
        MarkingCount_t weight;
        bool operator<(const WeightedState& other) const {
            return weight < other.weight;
        }
    };

    template <typename T, bool>
    class BestFSStructure {
    public:
        explicit BestFSStructure(
            ColoredEncoder& encoder, const size_t seed, std::shared_ptr<CompiledGammaQueryExpression> query, const bool negQuery)
            : _rng(seed), _query(std::move(query)), _negQuery(negQuery), _encoder(&encoder) {}

        T& next() {
            return _queue.top().cpn;
        }

        void remove() {
            _queue.pop();
        }

        //Not implemented encoding
        void add(T state) {
            const MarkingCount_t weight = _query->distance(state.marking, _negQuery);

            _queue.push(WeightedState<T> {
                std::move(state),
                weight
            });
        }

        [[nodiscard]] bool empty() const {
            return _queue.empty();
        }

        [[nodiscard]] uint32_t size() const {
            return _queue.size();
        }
        static void shuffle() {};
    private:
        std::priority_queue<WeightedState<T>> _queue;
        std::default_random_engine _rng;
        std::shared_ptr<CompiledGammaQueryExpression> _query{};
        bool _negQuery;
        ColoredEncoder* _encoder;
    };
}

#endif