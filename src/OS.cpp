#include "OS.h"
#include "pico/stdlib.h"
#include "LCD.h"
#include "SD.h"
#include "debug.h"
#include "program.h"

static __attribute__((section(".piconsole.os"))) OS os;

constexpr uint LED_PIN{ 25u };

OS::OS()
{
    stdio_init_all();
    print("OS address: %x\n", this);
    print("__piconsole_os: %x\n", __piconsole_os);
    print("__piconsole_os_end: %x\n", __piconsole_os_end);
    print("sizeof(OS): %d\n", sizeof(OS));
    print("Initializing LCD interface (%d bytes)...\n", sizeof(LCD_MODEL));
    print("__piconsole_lcd_buffer: %x\n", __piconsole_lcd_buffer);
    print("__piconsole_lcd_buffer_end: %x\n", __piconsole_lcd_buffer_end);
    if (!lcd.init())
    {
        print("Failed to create LCD interface!\n");
        exit(-1);
    }
    print("Created LCD interface with a baudrate of %u\n", lcd.get_baudrate());

    print("Displaying color test...\n");
    show_color_test();

    print("Creating SD interface (%d bytes)...\n", sizeof(SDCard));
    if (!sd.init())
    {
        print("Failed to create SD interface!\n");
        exit(-2);
    }
    print("Created SD interface\n");

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    print("OS initialized\n");
}

void OS::show_color_test()
{
    for (std::size_t y{ 0 }; y < lcd.height; ++y)
    {
        const float vertical_ratio{ float(y) / float(lcd.height) };
        const std::uint8_t r{ static_cast<std::uint8_t>(vertical_ratio * 255) };
        for (std::size_t x{ 0 }; x < lcd.width; ++x)
        {
            const float horizontal_ratio{ float(x) / float(lcd.width) };
            const std::uint8_t g{ static_cast<std::uint8_t>(horizontal_ratio * 255) };
            const std::uint8_t b{ static_cast<std::uint8_t>(std::max(255.0f * (1.0f - (horizontal_ratio + vertical_ratio) * 0.5f), 0.0f)) };
            lcd.set_pixel(RGB565(r, g, b), x, y);
        }
    }
    lcd.show();
}

void OS::update()
{
    gpio_put(LED_PIN, 0);
    sleep_ms(250);
    gpio_put(LED_PIN, 1);
    print("Hello, OS!\n");
    sleep_ms(1000);
}

void OS::load_program(std::string_view path)
{
    if (path.size() > SDCard::max_path_length)
    {
        print("Error: Can't load path (length %u > %u)", path.size(), SDCard::max_path_length);
        // Path printed manually to avoid allocations
        for (const char& character : path)
        {
            print("%c", character);
        }
        print("\n");
        return;
    }
    std::memcpy(current_program_directory, path.data(), path.size());
    current_program_directory[path.size()] = '\0';
    SDCard::FileReader reader(current_program_directory);
    ELFHeader header;
    reader.read<ELFHeader>(&header);
    print("Magic Number: %c%c%c%c\n",
        header.identifier.magic_number[0],
        header.identifier.magic_number[1],
        header.identifier.magic_number[2],
        header.identifier.magic_number[3]);
    switch (header.identifier.bitcount)
    {
        case ELFHeader::Identifier::BitCount::_32Bit:
            print("Bit count: 32\n");
            break;
        case ELFHeader::Identifier::BitCount::_64Bit:
            print("Bit count: 64\n");
            break;
        default:
            print("Bit count: ???\n");
            break;
    }
    print("Program/Segment header offset: 0x%x\n", header.program_header_offset);
    print("Section header offset: 0x%x\n", header.section_header_offset);
}
