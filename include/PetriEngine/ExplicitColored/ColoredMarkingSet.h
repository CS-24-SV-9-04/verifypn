#ifndef COLORED_MARKING_SET_H
#define COLORED_MARKING_SET_H

#include "ColoredPetriNetMarking.h"
#include <unordered_set>
#include <vector>
#include <sstream>
#include <string>
#include <algorithm>

#include "Binding.h"
#include "ColoredPetriNetState.h"
#include "ColoredSuccessorGenerator.h"

namespace PetriEngine {
    namespace ExplicitColored {
        class ColoredMarkingSet {
        public:
            void add(const ColoredPetriNetState &state, const ColoredPetriNetState &lastState) {
                std::stringstream stream;
                (state.marking).stableEncode(stream);
                _states.push_back(stream.str());
                _indices.emplace(stream.str(), _states.size() - 1);
                std::stringstream stream2;
                (lastState.marking).stableEncode(stream2);
                _map.emplace(stream.str(), "t" + std::to_string(lastState.lastTrans) + "b" + std::to_string(lastState.lastBinding) + "s" + std::to_string(_indices[stream2.str()]));
                // auto hej = _map.find(stream.str());
                // std::cout << "Just inserted, can find: " << (hej != _map.end()) << "Contains: " << contains(lastState.marking) << "\n";
            }

            void add(const ColoredPetriNetMarking &marking) {
                std::stringstream stream;
                marking.stableEncode(stream);
                _states.push_back(stream.str());
                _indices.emplace(stream.str(), _states.size() - 1);
                _map.emplace(stream.str(), "");
            }

            bool contains(const ColoredPetriNetMarking &marking) {
                std::stringstream stream;
                marking.stableEncode(stream);

                auto hej = _map.find(stream.str()) != _map.end();
                // if (hej) {
                //     std::cout << "comparison good";
                // }else {
                //     std::cout << "comparison bad:" << stream.str() << std::endl;
                // }
                return hej;
            }

            size_t size() const {
                return _map.size();
            }

            void printTrace(const ColoredPetriNetMarking &marking, const std::unordered_map<std::string, uint32_t>& placeNameIndices, const std::unordered_map<std::string, Variable_t>& variableMap, const std::unordered_map<std::string, Transition_t>& transitionIndices, const ColoredSuccessorGenerator& generator) const {
                uint32_t length = 1;
                std::string output = "";
                std::stringstream stream;
                marking.stableEncode(stream);
                std::string currentState = stream.str();
                std::cout << "Hello" << std::endl;
                auto str = _map.at(currentState);
                while (!str.empty()) {
                    length++;
                    Transition_t tid = std::stoi(str.substr(1, str.find('b') - 1),nullptr, 10);
                    std::string transitionName;
                    Binding_t bid = std::stoi(str.substr(str.find('b') + 1, str.find('s') - str.find('b') - 1), nullptr);
                    Binding binding = generator.getBinding(tid, bid);
                    uint32_t lastState = std::stoi(str.substr(str.find('s') + 1, str.size()), nullptr, 10);//beautify
                    currentState = _states[lastState];
                    str = _map.at(currentState);
                    for (auto& [fst,snd] : transitionIndices) {
                        if (snd == tid) {
                            transitionName = fst;
                            break;
                        }
                    }
                    output = "T: " + transitionName + " B: " + binding.toString() + "=> \nS: " + toString(currentState, placeNameIndices) + "\n" + output;
                }
                std::cout << "There is the following trace:\n" << toString(currentState, placeNameIndices) << "\n" << output << "In total: " << "consisting of " << length << " states\n";
            }

            std::string toString(const std::string& marking, const std::unordered_map<std::string, uint32_t>& placeNameIndices) const {
                std::stringstream stream;
                std::vector<std::pair<std::string, std::string>> outputMap;
                uint32_t i = 0;
                for (auto elem = marking.begin(); elem != marking.end();) {
                    std::string name;
                    std::string multiset;
                    while (elem != marking.end() && *elem == '.') {
                        i++;
                        ++elem;
                    }
                    if (elem == marking.end()) {
                        break;
                    }
                    for (const auto& [fst,snd] : placeNameIndices) {
                        if (snd == i) {
                            name = fst;
                            break;
                        }
                    }
                    while (*elem != '.' && elem != marking.end()) {
                        multiset += *elem;
                        if (*elem == ')') {
                            multiset += " ";
                        }
                        ++elem;
                    }
                    multiset +=  " ";
                    outputMap.emplace_back(name,multiset);
                }
                std::sort(outputMap.begin(), outputMap.end(), [](const std::pair<std::string, std::string>& a, const std::pair<std::string, std::string>& b) {
                    return a.first < b.first;
                });

                for (const auto& [fst,snd] : outputMap) {
                    stream << fst << ": " << "{ "<< snd << "} ";
                }

                return stream.str();
            }

            friend std::ostream& operator<<(std::ostream& stream, const std::pair<const ColoredMarkingSet, const std::unordered_map<std::string, uint32_t>&>& pair) {
                uint32_t i = 0;
                auto& marking = pair.first;
                auto& placeNameIndices = pair.second;
                for (const auto& [fst,snd] : marking._map) {
                    for (auto elem = fst.begin(); elem != fst.end();) {
                        while (*elem == '.') {
                            i++;
                            ++elem;
                        }
                        for (const auto& [fst,snd] : placeNameIndices) {
                            if (snd == i) {
                                stream << fst << ": ";
                                break;
                            }
                        }
                        while (*elem != '.') {
                           stream << *elem;
                            ++elem;
                        }
                    }
                }
                return stream;
            }
        private:
            std::unordered_map<std::string,std::string> _map;
            std::map<std::string, uint32_t> _indices;
            std::vector<std::string> _states;
        };
    }
}
#endif
