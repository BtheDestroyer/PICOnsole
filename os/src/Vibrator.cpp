#include "Vibrator.h"
#include "pico/time.h"
#include <algorithm>
#include <limits>

void PWMVibrator::start(float strength, std::uint32_t runtime_ms)
{
    start_time = get_absolute_time();
    end_time = make_timeout_time_ms(runtime_ms);
    set_strength(std::clamp(strength, 0.0f, 1.0f));
}

void PWMVibrator::stop()
{
    update_us_since_boot(&start_time, 0);
    update_us_since_boot(&end_time, 0);
    set_strength(0);
}

void PWMVibrator::update()
{
    if (absolute_time_diff_us(start_time, end_time) != 0)
    {
        absolute_time_t current_time{ get_absolute_time() };
        if (absolute_time_diff_us(current_time, end_time) <= 0)
        {
            stop();
        }
    }
}

void PWMVibrator::set_strength(float strength)
{
    pwm_set_gpio_level(gpio_pin, static_cast<std::uint16_t>(strength * std::numeric_limits<std::uint16_t>::max()));
}
