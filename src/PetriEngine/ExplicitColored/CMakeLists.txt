set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(ExplicitColored 
    GuardExpression.cpp
    ArcExpression.cpp
    CPNMultiSet.cpp
    ColoredPetriNetBuilder.cpp
    ColoredSuccessorGenerator.cpp
    Algorithms/NaiveWorklist.cpp
    ColoredResultPrinter.cpp
)

target_link_libraries(ExplicitColored Colored)
