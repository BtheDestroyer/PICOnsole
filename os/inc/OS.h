#pragma once
#include <string_view>
#include <functional>
#include "debug.h"
#include "path.h"
#include "PICOnsole_defines.h"
#include "LCD.h"
#include "SD.h"

#define assume(x) do { if (!(x)) { __builtin_unreachable(); } } while (0)
#if _PICONSOLE_OS_LIB
#define os_main __attribute__((section(".piconsole.os.main"))) main
#endif

class OS;

extern "C"
{
#if _PICONSOLE_OS
    extern OS __piconsole_os[];
    extern void* __piconsole_os_end[];
#elif _PICONSOLE_PROGRAM
    // TODO: Find a way to link these in from the OS's ELF?
    OS *__piconsole_os{ reinterpret_cast<OS*>(0x200000c0) };
    void* __piconsole_os_end { reinterpret_cast<void**>(0x20000468) };
#endif
}

class OS {
public:
    PICONSOLE_MEMBER_FUNC ~OS() {}

    PICONSOLE_FUNC static OS& get() { return *reinterpret_cast<OS*>(0x200000c0); }

    PICONSOLE_MEMBER_FUNC bool init();
    PICONSOLE_MEMBER_FUNC bool uninit(bool cleanly = true);
    GETTER PICONSOLE_MEMBER_FUNC const bool& is_initialized() const { return initialized; }

    PICONSOLE_MEMBER_FUNC void update();

    GETTER PICONSOLE_MEMBER_FUNC bool is_active() const { return true; }

    GETTER PICONSOLE_MEMBER_FUNC LCD_MODEL& get_lcd() { return lcd; }
    GETTER PICONSOLE_MEMBER_FUNC SDCard& get_sd() { return sd; }
    GETTER PICONSOLE_MEMBER_FUNC const LCD_MODEL& get_lcd() const { return lcd; }
    GETTER PICONSOLE_MEMBER_FUNC const SDCard& get_sd() const { return sd; }

    GETTER PICONSOLE_MEMBER_FUNC std::string_view get_current_program_path() { return {current_program_path, std::strlen(current_program_path)}; }
    GETTER PICONSOLE_MEMBER_FUNC std::string_view get_current_program_directory() { return path::dir_name(current_program_path); }

    KEEP bool __no_inline_not_in_flash_func(load_program)(std::string_view path);

private:
    PICONSOLE_MEMBER_FUNC void show_color_test();

    LCD_MODEL lcd;
    SDCard sd;

    char current_program_path[SDCard::max_path_length + 1] { 0 };
    bool initialized{ false };
};
