#ifndef COLORED_SEARCH_TYPES_H
#define COLORED_SEARCH_TYPES_H

#include <queue>
#include <random>
#include <stack>

#include "PetriEngine/ExplicitColored/Visitors/HeuristicVisitor.h"

namespace PetriEngine::ExplicitColored {

    //Remove supertype
    // template <typename T>
    // class ExplicitSearchStructure {
    // public:
    //     virtual ~ExplicitSearchStructure() = default;
    //     explicit ExplicitSearchStructure(ColoredEncoder& encoder) : _encoder(&encoder) {};
    //
    //     virtual T& next() = 0;
    //     virtual void remove() = 0;
    //     virtual void add(T) = 0;
    //     [[nodiscard]] virtual bool empty() const = 0;
    //     [[nodiscard]] virtual uint32_t size() const = 0;
    //     virtual void shuffle() = 0;
    // protected:
    //     ColoredEncoder* _encoder{};
    // };

    template <typename T>
    class DFSStructure {
    public:
        explicit DFSStructure(ColoredEncoder& encoder) : _encoder(&encoder) {};

        T& next() {
            return waiting.top();
        }

        void remove() {
            waiting.pop();
            if (!waiting.empty()) {
                waiting.top().decode(this->_encoder);
            }
        }

        void add(T state) {
            if (!waiting.empty()) {
                waiting.top().encode();
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
        std::stack<T> waiting;
        ColoredEncoder* _encoder;
    };

    template <typename T>
    class BFSStructure  {
    public:
        explicit BFSStructure(ColoredEncoder& encoder) : _encoder(&encoder) {};

        T& next() {
            return waiting.front();
        }

        void remove() {
            waiting.front().decode(this->_encoder);
            waiting.pop();
        }

        void add(T state) {
            state.encode();
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

    template<typename T>
    class RDFSStructure  {
    public:
        explicit RDFSStructure(ColoredEncoder& encoder, const size_t seed) :  _rng(seed), _encoder(&encoder) {}
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
            if (!_stack.empty()) {
                _stack.top().encode();
            }
            std::shuffle(_cache.begin(), _cache.end(), _rng);
            for (auto & it : _cache) {
                _stack.emplace(std::move(it));
            }
            if (!_stack.empty()) {
                _cache.clear();
                _stack.top().decode(this->_encoder);
            }
        }

        void add(T state) {
            state.encode();
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

    template <typename T>
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