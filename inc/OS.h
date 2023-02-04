#pragma once
#include <memory>
#include <string>
#include "LCD.h"
#include "SD.h"

class OS {
public:
    OS();

    void update();

    [[nodiscard]] constexpr bool is_active() const { return true; }

    [[nodiscard]] LCD_MODEL* get_lcd() { return lcd.get(); }
    [[nodiscard]] SDCard* get_sd() { return sd.get(); }

    [[nodiscard]] std::string_view get_current_program_directory() { return current_program_directory; }

private:
    void show_color_test();

    std::unique_ptr<LCD_MODEL> lcd;
    std::unique_ptr<SDCard> sd;

    std::string current_program_directory;
};
