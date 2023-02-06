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
    print("OS address: 0x%x\n", this);
    print("__piconsole_os: 0x%x\n", __piconsole_os);
    print("__piconsole_os_end: 0x%x\n", __piconsole_os_end);
    print("sizeof(OS): %d\n", sizeof(OS));
    print("Initializing LCD interface (%d bytes)...\n", sizeof(LCD_MODEL));
    print("__piconsole_lcd_buffer: 0x%x\n", __piconsole_lcd_buffer);
    print("__piconsole_lcd_buffer_end: 0x%x\n", __piconsole_lcd_buffer_end);
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

bool __no_inline_not_in_flash_func(OS::load_program)(std::string_view path)
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
        return false;
    }
    std::memset(current_program_path, 0, count_of(current_program_path));
    std::memcpy(current_program_path, path.data(), path.size());
    SDCard::FileReader reader(current_program_path);
    ELFHeader elf_header;
    reader.read<ELFHeader>(elf_header);
    print("Magic Number: %c%c%c%c\n",
        elf_header.identifier.magic_number[0],
        elf_header.identifier.magic_number[1],
        elf_header.identifier.magic_number[2],
        elf_header.identifier.magic_number[3]);
    for (std::size_t i{ 0 }; i < count_of(elf_header.identifier.magic_number); ++i)
    {
        if (elf_header.identifier.magic_number[i] != ELFHeader::Identifier::expected_magic_number[i])
        {
            print("Path provided to OS::load_program is not a valid ELF (magic number mismatch): %s\n", current_program_path);
            std::memset(current_program_path, 0, count_of(current_program_path));
            return false;
        }
    }
    switch (elf_header.identifier.bitcount)
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
    print("Program/Segment elf_header offset: 0x%x\n", elf_header.segment_header_offset);
    print("Entrypoint: 0x%x\n", elf_header.entrypoint);
    reader.seek_absolute(elf_header.segment_header_offset);
    print("Loading %d segments...\n", elf_header.segment_header_count);
    print("             idx: Type Offset     VirtAddr   PhysAddr   FileSize MemSize Flags (Raw)    Alignment\n");
    {
        SegmentHeader segment_header;
        FSIZE_t current_segment_header_offset{ elf_header.segment_header_offset };
        for (std::size_t i{ 0 }; i < elf_header.segment_header_count; ++i)
        {
            if (!reader.read<SegmentHeader>(segment_header))
            {
                print("Failed to read segment header %d!\n", i);
                std::memset(current_program_path, 0, count_of(current_program_path));
                return false;
            }
            constexpr static auto has_flag{
                [](SegmentHeader::Flags flags, SegmentHeader::Flags mask)
                {
                    const std::uint32_t masked{ static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(mask) };
                    return masked == static_cast<std::uint32_t>(mask);
                }
            };
            const FSIZE_t next_segment_header_offset{ current_segment_header_offset + sizeof(SegmentHeader) };
            print("(%10llu)", current_segment_header_offset);
            print(" %3d:", i);
            print(" %.5d", segment_header.type);
            print(" 0x%08x 0x%08x 0x%08x",
                segment_header.content_offset, segment_header.virtual_address, segment_header.physical_address);
            print(" 0x%05x  0x%05x",
                segment_header.segment_size, segment_header.memory_size);
            print(" %c%c%c",
                has_flag(segment_header.flags, SegmentHeader::Flags::R) ? 'R' : ' ',
                has_flag(segment_header.flags, SegmentHeader::Flags::W) ? 'W' : ' ',
                has_flag(segment_header.flags, SegmentHeader::Flags::X) ? 'X' : ' ');
            print("    (0x%04x) 0x%04x -> %llu\n",
                segment_header.flags, segment_header.alignment, next_segment_header_offset);
            
            reader.seek_absolute(segment_header.content_offset);
            // TODO: Load segment
            reader.seek_absolute(next_segment_header_offset);
            current_segment_header_offset = next_segment_header_offset;
        }
    }
    return true;
}
