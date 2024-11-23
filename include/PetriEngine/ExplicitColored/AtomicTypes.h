#ifndef ATOMIC_TYPES_H
#define ATOMIC_TYPES_H
#include <cstdint>
#include <limits>
namespace PetriEngine
{
    namespace ExplicitColored
    {
        using Color_t = uint32_t;
        using Transition_t = uint32_t;
        using Variable_t = uint32_t;
        using MarkingCount_t = uint32_t;
        constexpr Color_t DOT_COLOR = 0;
        constexpr Color_t ALL_COLOR = std::numeric_limits<Color_t>::max();
    }
}
#endif