#pragma once
#include <memory>
#include "LCD.h"

class OS {
public:
    OS();

    void update();

    [[nodiscard]] constexpr bool is_active() const { return true; }

    [[nodiscard]] LCD_MODEL* get_lcd() { return lcd.get(); }

private:
    void show_color_test();

    std::unique_ptr<LCD_MODEL> lcd;
};
