#include "OS.h"
#include "pico/stdlib.h"
#include "LCD.h"
#include "SD.h"
#include "debug.h"
#include "memory.h"

constexpr uint LED_PIN{ 25u };

OS::OS()
{
    stdio_init_all();
    print("sizeof(OS): %d\n", sizeof(OS));
    print("os: %x\n", this);
    print("Initializing Memory...\n");
    Memory::init();
    print("Memory initialized; %d bytes available (%d used for first block header)\n", Memory::free_dynamic_memory, Memory::get_total_allocated_dynamic_memory());
    print("Initializing LCD interface (%d bytes)...\n", sizeof(LCD_MODEL));
    lcd = Memory::allocate_OS<LCD_MODEL>();
    if (!lcd)
    {
        print("Failed to create LCD interface!\n");
        exit(-1);
    }
    print("Created LCD interface with a baudrate of %u\n", lcd->get_baudrate());

    print("Displaying color test...\n");
    show_color_test();

    print("Creating SD interface (%d bytes)...\n", sizeof(SDCard));
    sd = Memory::allocate_OS<SDCard>();
    if (!sd)
    {
        print("Failed to create SD interface!\n");
        exit(-2);
    }
    print("Created SD interface\n");

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    string test{"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"};
    print("%s\n", test.c_str());
    const auto free_memory_string{ Memory::human_readable<Memory::OSAllocator<char>>(Memory::free_dynamic_memory) };
    const auto used_memory_string{ Memory::human_readable<Memory::OSAllocator<char>>(Memory::get_total_allocated_dynamic_memory()) };
    print("OS initialized; %s of SRAM available (%s used for OS)\n", free_memory_string.c_str(), used_memory_string.c_str());
}

void OS::show_color_test()
{
    for (std::size_t y{ 0 }; y < lcd->height; ++y)
    {
        const float vertical_ratio{ float(y) / float(lcd->height) };
        const std::uint8_t r{ static_cast<std::uint8_t>(vertical_ratio * 255) };
        for (std::size_t x{ 0 }; x < lcd->width; ++x)
        {
            const float horizontal_ratio{ float(x) / float(lcd->width) };
            const std::uint8_t g{ static_cast<std::uint8_t>(horizontal_ratio * 255) };
            const std::uint8_t b{ static_cast<std::uint8_t>(std::max(255.0f * (1.0f - (horizontal_ratio + vertical_ratio) * 0.5f), 0.0f)) };
            lcd->set_pixel(RGB565(r, g, b), x, y);
        }
    }
    lcd->show();
}

void OS::update()
{
    gpio_put(LED_PIN, 0);
    sleep_ms(250);
    gpio_put(LED_PIN, 1);
    print("Hello, OS!\n");
    sleep_ms(1000);
}
