#pragma once
#include <array>
#include <bitset>
#include <cstdint>
#include "PICOnsole_defines.h"
#undef BUTTONS_DEFINED
#ifdef BUTTON_CATHODES
    #ifdef BUTTON_ANODES
        #define BUTTONS_DEFINED 1
    #endif
#endif
#ifndef BUTTONS_DEFINED
    #define BUTTONS_DEFINED 0
#endif

enum class Button {
                // C, A
    A,          // 0, 0
    B,          // 0, 1
    DPad_Down,  // 0, 2
    X,          // 1, 0
    Y,          // 1, 1
    DPad_Up,    // 1, 2
    Start,      // 2, 0
    DPad_Right, // 2, 1
    DPad_Left   // 2, 2
};

class InputMap
{
public:
    PICONSOLE_MEMBER_FUNC ~InputMap() { uninit(); }
    PICONSOLE_MEMBER_FUNC bool init();
    PICONSOLE_MEMBER_FUNC bool uninit();
    PICONSOLE_MEMBER_FUNC void update();
    GETTER consteval static bool is_valid()
    {
#if BUTTONS_DEFINED
        return true;
#else
        return false;
#endif
    };
    GETTER PICONSOLE_MEMBER_FUNC constexpr bool is_initialized() const
    {
        return initialized;
    };
    GETTER PICONSOLE_MEMBER_FUNC constexpr std::size_t get_button_count() const
    {
#if BUTTONS_DEFINED
        return button_states.size();
#else
        return 0;
#endif
    };
    GETTER PICONSOLE_MEMBER_FUNC constexpr bool get_button_state(Button button) const
    {
#if BUTTONS_DEFINED
        return button_states[static_cast<std::size_t>(button)];
#else
        return false;
#endif
    };

private:
#if BUTTONS_DEFINED
    constexpr static std::array cathodes{ 
        BUTTON_CATHODES
    };
    constexpr static std::array anodes{ 
        BUTTON_ANODES
    };
#else
    constexpr static std::array<std::int32_t, 0> cathodes{};
    constexpr static std::array<std::int32_t, 0> anodes{};
#endif
    std::bitset<cathodes.size() * anodes.size()> button_states{ 0 };
    bool initialized{ false };
};
