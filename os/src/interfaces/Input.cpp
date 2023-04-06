#include "debug.h"
#include "interfaces/Input.h"
#include "hardware/gpio.h"

bool InputMap::init()
{
    if constexpr (!is_valid())
    {
        return false;
    }
    if (is_initialized())
    {
        return false;
    }
    for (const std::int32_t cathode : cathodes)
    {
        print("  Setting pin %d as cathode (output)\n", cathode);
        gpio_init(cathode);
        gpio_set_dir(cathode, GPIO_OUT);
        gpio_put(cathode, 0);
    }
    for (const std::int32_t anode : anodes)
    {
        print("  Setting pin %d as anode (input)\n", anode);
        gpio_init(anode);
        gpio_set_dir(anode, GPIO_IN);
        gpio_pull_down(anode);
    }
    initialized = true;
    return true;
}

bool InputMap::uninit()
{
    if constexpr (!is_valid())
    {
        return false;
    }
    if (!is_initialized())
    {
        return false;
    }
    for (const std::int32_t cathode : cathodes)
    {
        gpio_deinit(cathode);
    }
    for (const std::int32_t anode : anodes)
    {
        gpio_deinit(anode);
    }
    initialized = false;
    return true;
}

void InputMap::update()
{
    if constexpr (!is_valid())
    {
        return;
    }
    if (!is_initialized())
    {
        return;
    }
    std::size_t button_index{ 0 };
    for (const std::int32_t cathode : cathodes)
    {
        gpio_put(cathode, 1);
        for (const std::int32_t anode : anodes)
        {
            button_states.set(button_index, gpio_get(anode));
            ++button_index;
        }
        gpio_put(cathode, 0);
    }
}
