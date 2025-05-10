set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(ExplicitColored
    ArcCompiler.cpp
    ColoredPetriNetBuilder.cpp
    SuccessorGenerator/ColoredSuccessorGenerator.cpp
    SuccessorGenerator/ProductStateGenerator.cpp
    Algorithms/ExplicitWorklist.cpp
    Algorithms/LTLNdfs.cpp
    ColoredResultPrinter.cpp
    ExpressionCompilers/GuardCompiler.cpp
    ExpressionCompilers/GammaQueryCompiler.cpp
    ColoredPetriNet.cpp
    ColorIgnorantPetriNetBuilder.cpp
    ExplicitColoredModelChecker.cpp
)

target_link_libraries(ExplicitColored Colored)