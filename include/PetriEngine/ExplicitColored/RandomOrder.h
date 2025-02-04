#ifndef RANDOMORDER_H
#define RANDOMORDER_H

#include <random>
#include <tuple>

namespace PetriEngine
{
    namespace ExplicitColored
    {
        struct RandomOrderFactory
        {
            RandomOrderFactory(const uint64_t seed, const bool randomize) : _engine(std::default_random_engine {seed}), _randomize(randomize) {};

            std::pair<uint32_t,uint32_t> _fillRandomOrder(const uint32_t n) {
                _timesProduced += 1;
                if (_timesProduced % 50000 == 0) {
                    std::cout << "Produced " << _timesProduced << std::endl;
                }
                if (!_randomize) {
                    return std::make_pair(0, 1);
                }
                std::uniform_int_distribution<uint32_t> offset_dist(0, n - 1);
                std::uniform_int_distribution<uint32_t> distance_dist(2, n - 1);
                //The first element is chosen at random
                const uint32_t offset = offset_dist(_engine);
                const std::vector<uint32_t> primeFactors = _getPrimeFactors(n);
                uint32_t distance = distance_dist(_engine);
                bool sharedFactors = false;
                while (true) {
                    for (auto&& primeFactor : primeFactors) {
                        if (distance % primeFactor == 0) {
                            sharedFactors = true;
                            distance = distance_dist(_engine);
                            break;
                        }
                    }
                    if (!sharedFactors) {
                        break;
                    }
                }

                return std::make_pair(offset, distance);
            }
        private:
            std::map<uint32_t, std::vector<uint32_t>> _primeFactors = std::map<uint32_t, std::vector<uint32_t>>{};
            std::default_random_engine _engine;
            bool _randomize;
            uint32_t _timesProduced = 0;

            std::vector<uint32_t> _getPrimeFactors(const uint32_t n) {
                if (_primeFactors.find(n) != _primeFactors.end()) {
                    return _primeFactors[n];
                }
                std::vector<uint32_t> result;
                uint32_t previousFactor = 0;

                uint32_t remainder = n;
                while (remainder % 2 == 0) {
                    if (previousFactor != 2) {
                        result.push_back(2);
                        previousFactor = 2;
                    }
                    remainder = remainder / 2;
                }
                uint32_t i = 2;
                while (i * i <= remainder) {
                    if (remainder % i == 0) {
                        remainder /= 2;
                        if (previousFactor != i) {
                            result.push_back(i);
                            previousFactor = i;
                        }
                    }else {
                        i += 2;
                    }
                }
                if (remainder > 2  && remainder != previousFactor) {
                    result.push_back(remainder);
                }
                _primeFactors[n] = result;
                return result;
            }
        };

        struct RandomOrder
        {
            explicit RandomOrder(const std::shared_ptr<RandomOrderFactory>& factory, const uint32_t n) : _factory(factory), _elements_n(n) {};

            [[nodiscard]] uint32_t fromId(const uint32_t id) {
                if (_distance == 0) {
                    auto [fst, snd] = _factory->_fillRandomOrder(_elements_n);
                    _offset = fst;
                    _distance = snd;
                }
                uint32_t result;
                if (id > _previousIdElementPair.first) {
                    result = (_previousIdElementPair.second + (id - _previousIdElementPair.first) * _distance) % _elements_n;
                }else{
                    result = (_offset + id * _distance) % _elements_n;
                }
                _previousIdElementPair = std::make_pair(id, result);
                return result;
            }
        private:
            uint32_t _offset = 0;
            uint32_t _distance = 0;
            std::shared_ptr<RandomOrderFactory> _factory;
            //Contains the (id,result) from the previous fromId call
            std::pair<uint32_t,uint32_t> _previousIdElementPair = {0,0};
            uint32_t _elements_n;
        };

            }
}
#endif //RANDOMORDER_H
