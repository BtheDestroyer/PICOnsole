#pragma once
#include "PICOnsole_defines.h"
#include <cstdint>
#include "hardware/gpio.h"
#include "hardware/pwm.h"

class Vibrator {
public:
    GETTER PICONSOLE_MEMBER_FUNC bool is_valid() const { return false; };
    PICONSOLE_MEMBER_FUNC void start([[maybe_unused]] float strength, [[maybe_unused]] std::uint32_t runtime_ms) {};
    PICONSOLE_MEMBER_FUNC void stop() {};
    PICONSOLE_MEMBER_FUNC void update() {};
};

class PWMVibrator final : public Vibrator {
public:
    PWMVibrator
#ifndef VIBRATION_PIN
        (std::size_t gpio_pin) : gpio_pin{gpio_pin}
#else
        ()
#endif
    {
        gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
        pwm_slice = pwm_gpio_to_slice_num(gpio_pin);
        pwm_clear_irq(pwm_slice);
        // Could maybe add wrapper functionality for handling pwm later
        pwm_set_irq_enabled(pwm_slice, false);

        config = pwm_get_default_config();
        pwm_config_set_clkdiv(&config, 4.0f);
        pwm_init(pwm_slice, &config, true);
    }

    GETTER PICONSOLE_MEMBER_FUNC bool is_valid() const { return true; };

    PICONSOLE_MEMBER_FUNC void start(float strength, std::uint32_t runtime_ms);
    PICONSOLE_MEMBER_FUNC void stop();
    PICONSOLE_MEMBER_FUNC void update();

private:
    void set_strength(float strength);

#ifdef VIBRATION_PIN
    constexpr static std::size_t gpio_pin{ VIBRATION_PIN };
#else
    std::size_t gpio_pin;
#endif
    pwm_config config;
    std::uint32_t pwm_slice;
    absolute_time_t start_time;
    absolute_time_t end_time;
};
