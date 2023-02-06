#pragma once
#include "debug.h"
#include "LCD.h"
#include "SD.h"
#include <string_view>
#include <functional>

class OS;

extern "C"
{
    extern OS __piconsole_os[];
    extern void* __piconsole_os_end[];
}

class OS {
public:
    OS();

    static inline OS& get() { return *(OS*)__piconsole_os; }

    void update();

    [[nodiscard]] constexpr bool is_active() const { return true; }

    [[nodiscard]] LCD_MODEL& get_lcd() { return lcd; }
    [[nodiscard]] SDCard& get_sd() { return sd; }
    [[nodiscard]] const LCD_MODEL& get_lcd() const { return lcd; }
    [[nodiscard]] const SDCard& get_sd() const { return sd; }

    [[nodiscard]] std::string_view get_current_program_directory() { return {current_program_directory, std::strlen(current_program_directory)}; }

private:
    void show_color_test();

    LCD_MODEL lcd;
    SDCard sd;

    const char current_program_directory[SDCard::max_path_length] { 0 };
};
