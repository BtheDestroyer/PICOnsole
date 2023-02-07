#pragma once
#include "debug.h"
#include "path.h"
#include "LCD.h"
#include "SD.h"
#include <string_view>
#include <functional>

#define assume(x) do { if (!(x)) { __builtin_unreachable(); } } while (0)

class OS;

extern "C"
{
#ifdef _PICONSOLE_OS
    extern OS __piconsole_os[];
    extern void* __piconsole_os_end[];
#elif _PICONSOLE_PROGRAM
    // TODO: Find a way to link these in from the OS's ELF?
    OS *__piconsole_os{ reinterpret_cast<OS*>(0x20000100) };
    void* __piconsole_os_end { reinterpret_cast<void**>(0x20000498) };
#endif
}

class OS {
public:
    OS();
    virtual ~OS() {}

    static inline OS& get() { return *(OS*)__piconsole_os; }

    void update();

    [[nodiscard]] constexpr bool is_active() const { return true; }

    [[nodiscard]] LCD_MODEL& get_lcd() { return lcd; }
    [[nodiscard]] SDCard& get_sd() { return sd; }
    [[nodiscard]] const LCD_MODEL& get_lcd() const { return lcd; }
    [[nodiscard]] const SDCard& get_sd() const { return sd; }

    [[nodiscard]] std::string_view get_current_program_path() { return {current_program_path, std::strlen(current_program_path)}; }
    [[nodiscard]] std::string_view get_current_program_directory() { return path::dir_name(current_program_path); }

    virtual bool __no_inline_not_in_flash_func(load_program)(std::string_view path);

private:
    void show_color_test();

    LCD_MODEL lcd;
    SDCard sd;

    char current_program_path[SDCard::max_path_length + 1] { 0 };
};
