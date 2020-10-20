/*
 * File:   ColoredPetriNetBuilder.h
 * Author: Klostergaard
 *
 * Created on 17. februar 2018, 16:25
 */

#ifndef COLOREDPETRINETBUILDER_H
#define COLOREDPETRINETBUILDER_H

#include <vector>
#include <unordered_map>

#include "ColoredNetStructures.h"
#include "Patterns.h"
#include "../AbstractPetriNetBuilder.h"
#include "../PetriNetBuilder.h"

namespace PetriEngine {

    typedef std::unordered_map<std::string, Colored::ColorType*> ColorTypeMap;
    class BindingGenerator {
    public:
        class Iterator {
        private:
            BindingGenerator* _generator;
                        
        public:
            Iterator(BindingGenerator* generator);
            
            bool operator==(Iterator& other);
            bool operator!=(Iterator& other);
            Iterator& operator++();
            const Colored::ExpressionContext::BindingMap operator++(int);
            Colored::ExpressionContext::BindingMap& operator*();
        };
    private:
        Colored::GuardExpression_ptr _expr;
        Colored::ExpressionContext::BindingMap _bindings;
        ColorTypeMap& _colorTypes;
        Colored::PatternSet _patterns;
        Colored::Transition &_transition;
        bool _isDone;
        
        bool eval();
        
    public:
        BindingGenerator(Colored::Transition& transition,
                ColorTypeMap& colorTypes);

        BindingGenerator(const BindingGenerator& ) = default;
        
        BindingGenerator operator= (const BindingGenerator& b) {
            return BindingGenerator(b);
        }

        Colored::ExpressionContext::BindingMap& nextBinding();
        Colored::ExpressionContext::BindingMap& currentBinding();
        bool isInitial() const;
        bool isFinal() const;
        Iterator begin();
        Iterator end();
    };

    class ColoredPetriNetBuilder : public AbstractPetriNetBuilder {
    public:
        typedef std::unordered_map<std::string, std::unordered_map<uint32_t , std::string>> PTPlaceMap;
        typedef std::unordered_map<std::string, std::vector<std::string>> PTTransitionMap;
        
    public:
        ColoredPetriNetBuilder();
        ColoredPetriNetBuilder(const ColoredPetriNetBuilder& orig);
        virtual ~ColoredPetriNetBuilder();
        
        void addPlace(const std::string& name,
                int tokens,
                double x = 0,
                double y = 0) override ;
        void addPlace(const std::string& name,
                Colored::ColorType* type,
                Colored::Multiset&& tokens,
                double x = 0,
                double y = 0) override;
        void addTransition(const std::string& name,
                double x = 0,
                double y = 0) override;
        void addTransition(const std::string& name,
                const Colored::GuardExpression_ptr& guard,
                double x = 0,
                double y = 0) override;
        void addInputArc(const std::string& place,
                const std::string& transition,
                bool inhibitor,
                int) override;
        void addInputArc(const std::string& place,
                const std::string& transition,
                const Colored::ArcExpression_ptr& expr) override;
        void addOutputArc(const std::string& transition,
                const std::string& place,
                int weight = 1) override;
        void addOutputArc(const std::string& transition,
                const std::string& place,
                const Colored::ArcExpression_ptr& expr) override;
        void addColorType(const std::string& id,
                Colored::ColorType* type) override;


        void sort() override;

        double getUnfoldTime() const {
            return _time;
        }

        uint32_t getPlaceCount() const {
            return _places.size();
        }

        uint32_t getTransitionCount() const {
            return _transitions.size();
        }

        uint32_t getArcCount() const {
            uint32_t sum = 0;
            for (auto& t : _transitions) {
                sum += t.input_arcs.size();
                sum += t.output_arcs.size();
            }
            return sum;
        }

        uint32_t getUnfoldedPlaceCount() const {
            //return _nptplaces;
            return _ptBuilder.numberOfPlaces();
        }

        uint32_t getUnfoldedTransitionCount() const {
            return _ptBuilder.numberOfTransitions();
        }

        uint32_t getUnfoldedArcCount() const {
            return _nptarcs;
        }

        bool isUnfolded() const {
            return _unfolded;
        }

        const PTPlaceMap& getUnfoldedPlaceNames() const {
            return _ptplacenames;
        }

        const PTTransitionMap& getUnfoldedTransitionNames() const {
            return _pttransitionnames;
        }
        
        PetriNetBuilder& unfold();
        PetriNetBuilder& stripColors();
        void computePlaceColorFixpoint();
        
    private:
        std::unordered_map<std::string,uint32_t> _placenames;
        std::unordered_map<std::string,uint32_t> _transitionnames;
        std::unordered_map<uint32_t,std::vector<uint32_t>> _placePostTransitionMap;
        std::unordered_map<uint32_t,BindingGenerator> _bindings;
        PTPlaceMap _ptplacenames;
        PTTransitionMap _pttransitionnames;
        uint32_t _nptplaces = 0;
        uint32_t _npttransitions = 0;
        uint32_t _nptarcs = 0;
        
        std::vector<Colored::Place> _places;
        std::vector<Colored::Transition> _transitions;
        std::vector<Colored::Arc> _arcs;
        std::vector<Colored::ColorFixpoint> _placeColorFixpoints;
        ColorTypeMap _colors;
        PetriNetBuilder _ptBuilder;
        bool _unfolded = false;
        bool _stripped = false;

        std::vector<uint32_t> _placeFixpointQueue;


        double _time;

        std::string arcToString(Colored::Arc& arc) const ;

        void printPlaceTable();
        
        void addArc(const std::string& place,
                const std::string& transition,
                const Colored::ArcExpression_ptr& expr,
                bool input);
       
        void processInputArcs(Colored::Transition& transition, uint32_t currentPlaceId, bool &transitionActivated);
        void processOutputArcs(Colored::Transition& transition);
        
        void unfoldPlace(Colored::Place& place);
        void unfoldTransition(Colored::Transition& transition);
        void unfoldArc(Colored::Arc& arc, Colored::ExpressionContext::BindingMap& binding, std::string& name, bool input);
    };
    
 

}

#endif /* COLOREDPETRINETBUILDER_H */
