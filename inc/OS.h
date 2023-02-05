#pragma once
#include "debug.h"
#include "memory.h"
#include <string_view>
#include <functional>

class LCD_MODEL;
class SDCard;

class OS {
public:
    OS();

    void update();

    [[nodiscard]] constexpr bool is_active() const { return true; }

    [[nodiscard]] LCD_MODEL* get_lcd() { return lcd; }
    [[nodiscard]] SDCard* get_sd() { return sd; }

    [[nodiscard]] std::string_view get_current_program_directory() { return current_program_directory; }

private:
    void show_color_test();

    using string = basic_string<Memory::OSAllocator<char>>;
    template <typename T>
    using vector = std::vector<T, Memory::OSAllocator<T>>;
    
    std::array<std::uint8_t, MemoryMap::launched_program_size> program_memory;
    LCD_MODEL* lcd;
    SDCard* sd;

    string current_program_directory;
    friend Memory;
};
