#ifndef COLORED_SEARCH_TYPES_H
#define COLORED_SEARCH_TYPES_H

namespace PetriEngine {
    namespace ExplicitColored {
        class DFSStructure {
        public:
            ColoredPetriNetState& next() {
                return waiting.top();
            }

            void remove() {
                waiting.pop();
            }

            void add(ColoredPetriNetState state) {
                waiting.emplace(std::move(state));
            }

            bool empty() const {
                return waiting.empty();
            }

            uint32_t size() const {
                return waiting.size();
            }
        private:
            std::stack<ColoredPetriNetState> waiting;
        };

        class BFSStructure {
        public:
            ColoredPetriNetState& next() {
                return waiting.front();
            }

            void remove() {
                waiting.pop();
            }

            void add(ColoredPetriNetState state) {
                waiting.emplace(std::move(state));
            }

            bool empty() const {
                return waiting.empty();
            }

            uint32_t size() const {
                return waiting.size();
            }
        private:
            std::queue<ColoredPetriNetState> waiting;
        };

        class RDFSStructure {
        public:
            explicit RDFSStructure(size_t seed) : _has_removed(true), _rng(seed) {}

            ColoredPetriNetState& next() {
                if (!_has_removed || _cache.empty()) {
                    return _stack.top();
                }

                _has_removed = false;
                std::shuffle(_cache.begin(), _cache.end(), _rng);

                for (auto it = _cache.begin(); _cache.end() != it; ++it) {
                    _stack.emplace(std::move(*it));
                }

                _cache.clear();
                return _stack.top();
            }

            void remove() {
                _has_removed = true;
                _stack.pop();
            }

            void add(ColoredPetriNetState state) {
                _cache.push_back(std::move(state));
            }

            bool empty() const {
                return _stack.empty() && _cache.empty();
            }

            uint32_t size() const {
                return _stack.size() + _cache.size();
            }

            class BFSStructure {
            public:
                ColoredPetriNetState& next() {
                    return waiting.front();
                }

                void remove() {
                    waiting.pop();
                }

                void add(ColoredPetriNetState state) {
                    waiting.emplace(std::move(state));
                }

                bool empty() const {
                    return waiting.empty();
                }

                uint32_t size() const {
                    return waiting.size();
                }
            private:
                std::queue<ColoredPetriNetState> waiting;
            };
        private:
            std::stack<ColoredPetriNetState> _stack;
            std::vector<ColoredPetriNetState> _cache;
            std::default_random_engine _rng;
            bool _has_removed = false;
        };

        class DeltaEvaluator : public PQL::Visitor {
        public:
            static MarkingCount_t eval(
                const PQL::Expr_ptr& expr,
                const ColoredPetriNetMarking& marking,
                const std::unordered_map<std::string, uint32_t>& placeNameIndices
            ) {
                DeltaEvaluator visitor{marking, placeNameIndices};
                visit(visitor, expr.get());
                return visitor._count;
            }
        protected:
            explicit DeltaEvaluator(
                const ColoredPetriNetMarking& marking,
                const std::unordered_map<std::string, uint32_t>& placeNameIndices
                ) : _placeNameIndices(placeNameIndices), _marking(marking) {}

            void _accept(const PQL::LiteralExpr *element) override {
                _count = static_cast<MarkingCount_t>(element->value());
            }

            void _accept(const PQL::IdentifierExpr *element) override {
                auto placeIndexIt = _placeNameIndices.find(*(element->name()));
                if (placeIndexIt == _placeNameIndices.end()) {
                    throw base_error("Unknown place in query ", *(element->name()));
                }
                _count = _marking.markings[placeIndexIt->second].totalCount();
            }

            void _accept(const PetriEngine::PQL::NotCondition *element) override { notSupported("NotCondition"); }
            void _accept(const PetriEngine::PQL::AndCondition *element) override { notSupported("AndCondition"); }
            void _accept(const PetriEngine::PQL::OrCondition *element) override { notSupported("OrCondition"); }
            void _accept(const PetriEngine::PQL::LessThanCondition *element) override { notSupported("LessThanCondition"); }
            void _accept(const PetriEngine::PQL::LessThanOrEqualCondition *element) override { notSupported("LessThanOrEqualCondition"); }
            void _accept(const PetriEngine::PQL::EqualCondition *element) override { notSupported("EqualCondition"); }
            void _accept(const PetriEngine::PQL::NotEqualCondition *element) override { notSupported("NotEqualCondition"); }
            void _accept(const PetriEngine::PQL::DeadlockCondition *element) override { notSupported("DeadlockCondition"); }
            void _accept(const PetriEngine::PQL::EFCondition *condition) override { notSupported("EFCondition"); }
            void _accept(const PetriEngine::PQL::FireableCondition *element) override { notSupported("FireableCondition"); }
            void _accept(const PetriEngine::PQL::CompareConjunction *element) override  { notSupported("CompareConjunction"); }
            void _accept(const PetriEngine::PQL::UnfoldedUpperBoundsCondition *element) override { notSupported("UnfoldedUpperBoundsCondition"); }
            void _accept(const PetriEngine::PQL::CommutativeExpr *element) override  { notSupported("CommutativeExpr"); }
            void _accept(const PetriEngine::PQL::SimpleQuantifierCondition *element) override  { notSupported("SimpleQuantifierCondition"); }
            void _accept(const PetriEngine::PQL::LogicalCondition *element) override  { notSupported("LogicalCondition"); }
            void _accept(const PetriEngine::PQL::CompareCondition *element) override  { notSupported("CompareCondition"); }
            void _accept(const PetriEngine::PQL::UntilCondition *element) override  { notSupported("UntilCondition"); }
            void _accept(const PetriEngine::PQL::ControlCondition *condition) override  { notSupported("ControlCondition"); }
            void _accept(const PetriEngine::PQL::PathQuant *element) override  { notSupported("PathQuant"); }
            void _accept(const PetriEngine::PQL::ExistPath *element) override  { notSupported("ExistPath"); }
            void _accept(const PetriEngine::PQL::AllPaths *element) override  { notSupported("AllPaths"); }
            void _accept(const PetriEngine::PQL::PathSelectCondition *element) override  { notSupported("PathSelectCondition"); }
            void _accept(const PetriEngine::PQL::PathSelectExpr *element) override  { notSupported("PathSelectExpr"); }
            void _accept(const PetriEngine::PQL::EGCondition *condition) override  { notSupported("EGCondition"); }
            void _accept(const PetriEngine::PQL::AGCondition *condition) override  { notSupported("AGCondition"); }
            void _accept(const PetriEngine::PQL::AFCondition *condition) override  { notSupported("AFCondition"); }
            void _accept(const PetriEngine::PQL::EXCondition *condition) override  { notSupported("EXCondition"); }
            void _accept(const PetriEngine::PQL::AXCondition *condition) override  { notSupported("AXCondition"); }
            void _accept(const PetriEngine::PQL::EUCondition *condition) override  { notSupported("EUCondition"); }
            void _accept(const PetriEngine::PQL::AUCondition *condition) override  { notSupported("AUCondition"); }
            void _accept(const PetriEngine::PQL::ACondition *condition) override  { notSupported("ACondition"); }
            void _accept(const PetriEngine::PQL::ECondition *condition) override  { notSupported("ECondition"); }
            void _accept(const PetriEngine::PQL::GCondition *condition) override  { notSupported("GCondition"); }
            void _accept(const PetriEngine::PQL::FCondition *condition) override  { notSupported("FCondition"); }
            void _accept(const PetriEngine::PQL::XCondition *condition) override  { notSupported("XCondition"); }
            void _accept(const PetriEngine::PQL::ShallowCondition *element) override  { notSupported("ShallowCondition"); }
            void _accept(const PetriEngine::PQL::UnfoldedFireableCondition *element) override  { notSupported("UnfoldedFireableCondition"); }
            void _accept(const PetriEngine::PQL::UpperBoundsCondition *element) override  { notSupported("UpperBoundsCondition"); }
            void _accept(const PetriEngine::PQL::LivenessCondition *element) override  { notSupported("LivenessCondition"); }
            void _accept(const PetriEngine::PQL::KSafeCondition *element) override  { notSupported("KSafeCondition"); }
            void _accept(const PetriEngine::PQL::QuasiLivenessCondition *element) override  { notSupported("QuasiLivenessCondition"); }
            void _accept(const PetriEngine::PQL::StableMarkingCondition *element) override  { notSupported("StableMarkingCondition"); }
            void _accept(const PetriEngine::PQL::BooleanCondition *element) override  { notSupported("BooleanCondition"); }
            void _accept(const PetriEngine::PQL::UnfoldedIdentifierExpr *element) override  { notSupported("UnfoldedIdentifierExpr"); }
            void _accept(const PetriEngine::PQL::PlusExpr *element) override  { notSupported("PlusExpr"); }
            void _accept(const PetriEngine::PQL::MultiplyExpr *element) override { notSupported("MultiplyExpr"); }
            void _accept(const PetriEngine::PQL::MinusExpr *element) override  { notSupported("MinusExpr"); }
            void _accept(const PetriEngine::PQL::NaryExpr *element) override  { notSupported("NaryExpr"); }
            void _accept(const PetriEngine::PQL::SubtractExpr *element) override { notSupported("SubtractExpr"); }
        private:
            PetriEngine::ExplicitColored::MarkingCount_t _count;
            const PetriEngine::ExplicitColored::ColoredPetriNetMarking& _marking;
            const std::unordered_map<std::string, uint32_t>& _placeNameIndices;

            void notSupported() {
                throw base_error("Not supported");
            }

            void notSupported(const std::string& type) {
                throw base_error("Not supported ", type);
            }

            void invalid() {
                throw base_error("Invalid expression");
            }
        };

        class DistQueryVisitor : public PetriEngine::PQL::Visitor {
        public:
            static MarkingCount_t eval(
                const PetriEngine::PQL::Condition_ptr& expr,
                const PetriEngine::ExplicitColored::ColoredPetriNetMarking& marking,
                const std::unordered_map<std::string, uint32_t>& placeNameIndices,
                bool neg
            ) {
                DistQueryVisitor visitor{marking, placeNameIndices, neg};
                visit(visitor, expr);
                return visitor._dist;
            }
        protected:
            explicit DistQueryVisitor(
                const PetriEngine::ExplicitColored::ColoredPetriNetMarking& marking,
                const std::unordered_map<std::string, uint32_t>& placeNameIndices,
                bool neg
            )
                : _marking(marking), _placeNameIndices(placeNameIndices), _neg(neg) { }

            void _accept(const PetriEngine::PQL::NotCondition *element) override {
                _neg = !_neg;
                visit(this, element->getCond().get());
            }

            void _accept(const PetriEngine::PQL::AndCondition *expr) override {
                MarkingCount_t sum = 0;
                for (const auto& subExpr : *expr) {
                    visit(this, subExpr.get());
                    sum = _dist;
                }
                _dist = sum;
            }

            void _accept(const PetriEngine::PQL::OrCondition *expr) override {
                MarkingCount_t min = std::numeric_limits<MarkingCount_t>::max();
                for (const auto& subExpr : *expr) {
                    visit(this, subExpr.get());
                    if (min > _dist) {
                        min = _dist;
                    }
                }
                _dist = min;
            }

            void _accept(const PetriEngine::PQL::LessThanCondition *element) override {
                const auto lhs = DeltaEvaluator::eval(element->getExpr1(), _marking, _placeNameIndices);
                const auto rhs = DeltaEvaluator::eval(element->getExpr2(), _marking, _placeNameIndices);
                int64_t val;
                if (!_neg) {
                    val = lhs - rhs + 1;
                } else {
                    val = rhs - lhs;
                }
                _dist = static_cast<MarkingCount_t>(std::max<int64_t>(val, 0));
            }

            void _accept(const PetriEngine::PQL::LessThanOrEqualCondition *element) override {
                const auto lhs = DeltaEvaluator::eval(element->getExpr1(), _marking, _placeNameIndices);
                const auto rhs = DeltaEvaluator::eval(element->getExpr2(), _marking, _placeNameIndices);
                int64_t val;
                if (!_neg) {
                    val = lhs - rhs;
                } else {
                    val = rhs - lhs + 1;
                }
                _dist = static_cast<MarkingCount_t>(std::max<int64_t>(val, 0));
            }

            void _accept(const PetriEngine::PQL::EqualCondition *element) override {
                const int64_t lhs = DeltaEvaluator::eval(element->getExpr1(), _marking, _placeNameIndices);
                const int64_t rhs = DeltaEvaluator::eval(element->getExpr2(), _marking, _placeNameIndices);
                if (!_neg) {
                    _dist = std::abs(lhs - rhs);
                } else {
                    _dist = lhs == rhs ? 1 : 0;
                }
            }

            void _accept(const PetriEngine::PQL::NotEqualCondition *element) override {
                const int64_t lhs = DeltaEvaluator::eval(element->getExpr1(), _marking, _placeNameIndices);
                const int64_t rhs = DeltaEvaluator::eval(element->getExpr2(), _marking, _placeNameIndices);
                if (!_neg) {
                    _dist = lhs == rhs ? 1 : 0;
                } else {
                    _dist = std::abs(lhs - rhs);
                }
            }

            void _accept(const PetriEngine::PQL::DeadlockCondition *element) override {
                _dist = 1000000;
            }

            void _accept(const PetriEngine::PQL::FireableCondition *element) override {
                notSupported("Does not support fireable");
            }

            void _accept(const PetriEngine::PQL::EFCondition *condition) override {
                notSupported("Does not supported nested quantifiers");
            }

            void _accept(const PetriEngine::PQL::AGCondition *condition) override {
                notSupported("Does not supported nested quantifiers");
            }

            void _accept(const PetriEngine::PQL::LiteralExpr *element) override {
                invalid();
            }

            void _accept(const PetriEngine::PQL::IdentifierExpr *element) override {
                invalid();
            }

            void _accept(const PetriEngine::PQL::CompareConjunction *element) override  { notSupported("CompareConjunction"); }
            void _accept(const PetriEngine::PQL::UnfoldedUpperBoundsCondition *element) override { notSupported("UnfoldedUpperBoundsCondition"); }
            void _accept(const PetriEngine::PQL::CommutativeExpr *element) override  { notSupported("CommutativeExpr"); }
            void _accept(const PetriEngine::PQL::SimpleQuantifierCondition *element) override  { notSupported("SimpleQuantifierCondition"); }
            void _accept(const PetriEngine::PQL::LogicalCondition *element) override  { notSupported("LogicalCondition"); }
            void _accept(const PetriEngine::PQL::CompareCondition *element) override  { notSupported("CompareCondition"); }
            void _accept(const PetriEngine::PQL::UntilCondition *element) override  { notSupported("UntilCondition"); }
            void _accept(const PetriEngine::PQL::ControlCondition *condition) override  { notSupported("ControlCondition"); }
            void _accept(const PetriEngine::PQL::PathQuant *element) override  { notSupported("PathQuant"); }
            void _accept(const PetriEngine::PQL::ExistPath *element) override  { notSupported("ExistPath"); }
            void _accept(const PetriEngine::PQL::AllPaths *element) override  { notSupported("AllPaths"); }
            void _accept(const PetriEngine::PQL::PathSelectCondition *element) override  { notSupported("PathSelectCondition"); }
            void _accept(const PetriEngine::PQL::PathSelectExpr *element) override  { notSupported("PathSelectExpr"); }
            void _accept(const PetriEngine::PQL::EGCondition *condition) override  { notSupported("EGCondition"); }
            void _accept(const PetriEngine::PQL::AFCondition *condition) override  { notSupported("AFCondition"); }
            void _accept(const PetriEngine::PQL::EXCondition *condition) override  { notSupported("EXCondition"); }
            void _accept(const PetriEngine::PQL::AXCondition *condition) override  { notSupported("AXCondition"); }
            void _accept(const PetriEngine::PQL::EUCondition *condition) override  { notSupported("EUCondition"); }
            void _accept(const PetriEngine::PQL::AUCondition *condition) override  { notSupported("AUCondition"); }
            void _accept(const PetriEngine::PQL::ACondition *condition) override  { notSupported("ACondition"); }
            void _accept(const PetriEngine::PQL::ECondition *condition) override  { notSupported("ECondition"); }
            void _accept(const PetriEngine::PQL::GCondition *condition) override  { notSupported("GCondition"); }
            void _accept(const PetriEngine::PQL::FCondition *condition) override  { notSupported("FCondition"); }
            void _accept(const PetriEngine::PQL::XCondition *condition) override  { notSupported("XCondition"); }
            void _accept(const PetriEngine::PQL::ShallowCondition *element) override  { notSupported("ShallowCondition"); }
            void _accept(const PetriEngine::PQL::UnfoldedFireableCondition *element) override  { notSupported("UnfoldedFireableCondition"); }
            void _accept(const PetriEngine::PQL::UpperBoundsCondition *element) override  { notSupported("UpperBoundsCondition"); }
            void _accept(const PetriEngine::PQL::LivenessCondition *element) override  { notSupported("LivenessCondition"); }
            void _accept(const PetriEngine::PQL::KSafeCondition *element) override  { notSupported("KSafeCondition"); }
            void _accept(const PetriEngine::PQL::QuasiLivenessCondition *element) override  { notSupported("QuasiLivenessCondition"); }
            void _accept(const PetriEngine::PQL::StableMarkingCondition *element) override  { notSupported("StableMarkingCondition"); }
            void _accept(const PetriEngine::PQL::BooleanCondition *element) override  { notSupported("BooleanCondition"); }
            void _accept(const PetriEngine::PQL::UnfoldedIdentifierExpr *element) override  { notSupported("UnfoldedIdentifierExpr"); }
            void _accept(const PetriEngine::PQL::PlusExpr *element) override  { notSupported("PlusExpr"); }
            void _accept(const PetriEngine::PQL::MultiplyExpr *element) override { notSupported("MultiplyExpr"); }
            void _accept(const PetriEngine::PQL::MinusExpr *element) override  { notSupported("MinusExpr"); }
            void _accept(const PetriEngine::PQL::NaryExpr *element) override  { notSupported("NaryExpr"); }
            void _accept(const PetriEngine::PQL::SubtractExpr *element) override { notSupported("SubtractExpr"); }
        private:
            void notSupported() {
                throw base_error("Not supported");
            }
            void notSupported(const std::string& type) {
                throw base_error("Not supported ", type);
            }

            void invalid() {
                throw base_error("Invalid expression");
            }

            MarkingCount_t _dist = true;
            bool _neg;
            const PetriEngine::ExplicitColored::ColoredPetriNetMarking& _marking;
            const std::unordered_map<std::string, uint32_t>& _placeNameIndices;
        };

        struct WeightedNet {
            mutable ColoredPetriNetState cpn;
            MarkingCount_t weight;
            bool operator<(const WeightedNet& other) const {
                return weight < other.weight;
            }
        };

        class BestFSStructure {
        public:
            explicit BestFSStructure(size_t seed, PQL::Condition_ptr query, const std::unordered_map<std::string, uint32_t>& placeNameIndices, bool negQuery)
                : _rng(seed), _query(std::move(query)), _placeNameIndices(placeNameIndices), _negQuery(negQuery) {}

            ColoredPetriNetState& next() {
                return _queue.top().cpn;
            }

            void remove() {
                _queue.pop();
            }

            void add(ColoredPetriNetState state) {
                MarkingCount_t weight = DistQueryVisitor::eval(_query, state.marking, _placeNameIndices, _negQuery);
                if (weight <_minDist) {
                    _minDist = weight;
                    std::cout << "New min dist: " << weight << std::endl;
                }
                _queue.push(WeightedNet {
                    std::move(state),
                    weight
                });
            }

            bool empty() const {
                return _queue.empty();
            }

            uint32_t size() const {
                return _queue.size();
            }
        private:
            std::priority_queue<WeightedNet> _queue;
            std::default_random_engine _rng;
            PQL::Condition_ptr _query;
            bool _negQuery;
            const std::unordered_map<std::string, uint32_t>& _placeNameIndices;
            MarkingCount_t _minDist = std::numeric_limits<MarkingCount_t>::max();

        };
    }
}

#endif