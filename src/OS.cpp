#include "OS.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "hardware/flash.h"
#include "hardware/dma.h"
#include "hardware/structs/ssi.h"
#include "hardware/structs/xip_ctrl.h"
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

// Taken directly from flash_ssi_dma example
static void __no_inline_not_in_flash_func(flash_bulk_read)(uint32_t memory_address, uint32_t word_count, uint32_t flash_offset, uint dma_channel) {
    // SSI must be disabled to set transfer size. If software is executing
    // from flash right now then it's about to have a bad time
    ssi_hw->ssienr = 0;
    ssi_hw->ctrlr1 = word_count - 1; // NDF, number of data frames
    ssi_hw->dmacr = SSI_DMACR_TDMAE_BITS | SSI_DMACR_RDMAE_BITS;
    ssi_hw->ssienr = 1;
    // Other than NDF, the SSI configuration used for XIP is suitable for a bulk read too.

    // Configure and start the DMA. Note we are avoiding the dma_*() functions
    // as we can't guarantee they'll be inlined
    dma_hw->ch[dma_channel].read_addr = (uint32_t) &ssi_hw->dr0;
    dma_hw->ch[dma_channel].write_addr = memory_address;
    dma_hw->ch[dma_channel].transfer_count = word_count;
    // Must enable DMA byteswap because non-XIP 32-bit flash transfers are
    // big-endian on SSI (we added a hardware tweak to make XIP sensible)
    dma_hw->ch[dma_channel].ctrl_trig =
            DMA_CH0_CTRL_TRIG_BSWAP_BITS |
            DREQ_XIP_SSIRX << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB |
            dma_channel << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB |
            DMA_CH0_CTRL_TRIG_INCR_WRITE_BITS |
            DMA_CH0_CTRL_TRIG_DATA_SIZE_VALUE_SIZE_WORD << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB |
            DMA_CH0_CTRL_TRIG_EN_BITS;

    // Now DMA is waiting, kick off the SSI transfer (mode continuation bits in LSBs)
    ssi_hw->dr0 = (flash_offset << 8u) | 0xa0u;

    // Wait for DMA finish
    while (dma_hw->ch[dma_channel].ctrl_trig & DMA_CH0_CTRL_TRIG_BUSY_BITS);

    // Reconfigure SSI before we jump back into flash!
    ssi_hw->ssienr = 0;
    ssi_hw->ctrlr1 = 0; // Single 32-bit data frame per transfer
    ssi_hw->dmacr = 0;
    ssi_hw->ssienr = 1;
}

struct DeferredCopy
{
    std::size_t segment_size;
    std::size_t memory_size;
    std::size_t virtual_address;
    union
    {
        FSIZE_t file_offset;
        std::size_t physical_address;
    };
    
    enum class Source
    {
        Nothing,
        ELF,
        Flash,
    } source;
};

#undef CALL_WITH_INTERUPTS_DISABLED
#define CALL_WITH_INTERUPTS_DISABLED(...) \
    { \
        const std::uint32_t interupts{ save_and_disable_interrupts() }; \
        __VA_ARGS__; \
        restore_interrupts(interupts); \
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
    print("Program/Segment elf_header offset: 0x%08lx\n", elf_header.segment_header_offset);
    print("Section elf_header offset: 0x%08lx\n", elf_header.section_header_offset);

    std::optional<program_init_fn*> program_init;
    std::optional<program_update_fn*> program_update;
    {
        assume(elf_header.section_header_string_table_index > 0);
        const FSIZE_t section_header_string_table_header_offset{ FSIZE_t(elf_header.section_header_string_table_index) * FSIZE_t(elf_header.section_header_entry_size) };
        const FSIZE_t section_header_string_table_section_file_offset{ elf_header.section_header_offset + section_header_string_table_header_offset };
        print("Loading section header string table\n");
        reader.seek_absolute(section_header_string_table_section_file_offset);
        SectionHeader section_header_string_table_header;
        if (!reader.read<SectionHeader>(section_header_string_table_header))
        {
            print("Failed to read string table section header\n");
            return false;
        }
        const FSIZE_t section_header_string_table_file_offset{ section_header_string_table_header.offset };
        reader.seek_absolute(section_header_string_table_file_offset);
        // Temporarily borrowing program RAM; whatever was there won't matter anymore anyway
        StringTable section_header_string_table{
            std::span{
                reinterpret_cast<char*>(piconsole_program_ram_start),
                section_header_string_table_header.size
            }
        };
        if (!reader.read_bytes(section_header_string_table.get_data()))
        {
            print("Failed to read string table\n");
            return false;
        }
        print("Scanning %d sections for program hooks...\n", elf_header.section_header_count);
        const FSIZE_t section_header_table_offset{ static_cast<FSIZE_t>(elf_header.section_header_offset) };
        reader.seek_absolute(section_header_table_offset);
        for (std::size_t section_index{ 0 };
            (!program_init.has_value() || !program_update.has_value()) && section_index < elf_header.section_header_count;
            ++section_index)
        {
            SectionHeader section_header;
            if (!reader.read<SectionHeader>(section_header))
            {
                print("\tFailed to read section header %d\n", section_index);
                return false;
            }
            // Note: This is kinda a hack and should be replaced by finding the symbols and getting their addresses instead
            if (section_header.type == SectionHeader::Type::ProgramData)
            {
                const char* section_name{ reinterpret_cast<const char*>(piconsole_program_ram_start) + section_header.string_table_name_index };
                print("Section %u: %s (%p)\n", section_index, section_name, section_name);
                if (std::strcmp(section_name, ".piconsole_program_init") == 0)
                {
                    if (program_init.has_value())
                    {
                        print("\t.piconsole_program_init has duplicate sections!");
                        return false;
                    }
                    print("\t%s: Address=0x%08lx\n", section_name, section_header.address);
                    const std::size_t adjustment{ section_header.address & 1 == 0 ? 0 : 1};
                    program_init.emplace(reinterpret_cast<program_init_fn*>(section_header.address - adjustment));
                }
                else if (std::strcmp(section_name, ".piconsole_program_update") == 0)
                {
                    if (program_update.has_value())
                    {
                        print("\t\t\t.piconsole_program_update has duplicate sections!");
                        return false;
                    }
                    print("\t%s: Address=0x%08lx\n", section_name, section_header.address);
                    const std::size_t adjustment{ section_header.address & 1 == 0 ? 0 : 1};
                    program_update.emplace(reinterpret_cast<program_update_fn*>(section_header.address - adjustment));
                }
            }
        }
        if (!program_init.has_value())
        {
            print("Section missing for .piconsole_program_init\n");
            return false;
        }
        else if (!program_update.has_value())
        {
            print("Section missing for .piconsole_program_update\n");
            return false;
        }
    }

    print("Erasing %lu sectors on existing flash...\n", std::uint32_t(piconsole_program_flash_size / FLASH_SECTOR_SIZE));
    CALL_WITH_INTERUPTS_DISABLED(flash_range_erase(piconsole_program_flash_offset, piconsole_program_flash_size));
    reader.seek_absolute(elf_header.segment_header_offset);
    print("Loading %d segments...\n", elf_header.segment_header_count);
    print("             idx: Type Offset     VirtAddr   PhysAddr   FileSize MemSize Flags (Raw)    Alignment\n");
    std::vector<DeferredCopy> deferred_copies;
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
            print(" 0x%08lx 0x%08lx 0x%08lx",
                segment_header.content_offset, segment_header.virtual_address, segment_header.physical_address);
            print(" 0x%05x  0x%05x",
                segment_header.segment_size, segment_header.memory_size);
            print(" %c%c%c",
                has_flag(segment_header.flags, SegmentHeader::Flags::R) ? 'R' : ' ',
                has_flag(segment_header.flags, SegmentHeader::Flags::W) ? 'W' : ' ',
                has_flag(segment_header.flags, SegmentHeader::Flags::X) ? 'X' : ' ');
            print("    (0x%04x) 0x%04x -> %llu\n",
                segment_header.flags, segment_header.alignment, next_segment_header_offset);
            
            // Temporarily borrowing program RAM; whatever was there won't matter anymore anyway
            // TODO: Chunk this in case the segment_size is > 192KB
            if (segment_header.physical_address >= piconsole_program_flash_start
                && segment_header.physical_address < piconsole_program_flash_end)
            {
                // Copy directly to flash
                std::span<std::uint8_t> segment_data{reinterpret_cast<std::uint8_t*>(piconsole_program_ram_start), segment_header.segment_size};
                reader.seek_absolute(segment_header.content_offset);
                if (!reader.read_bytes(segment_data))
                {
                    print("Failed to read segment data\n");
                    return false;
                }
                reader.seek_absolute(next_segment_header_offset);
                // See flash programming example
                print("\tCopying directly from RAM to flash...\n");
                CALL_WITH_INTERUPTS_DISABLED(flash_range_program(segment_header.physical_address - XIP_BASE, segment_data.data(), segment_header.segment_size));
                print("\tDone.\n");
            }
            else if (segment_header.physical_address >= piconsole_program_ram_start
                && segment_header.physical_address < piconsole_program_ram_end)
            {
                // Defer segment copy into RAM to the end
                const DeferredCopy copy{
                    .segment_size = segment_header.segment_size,
                    .memory_size = segment_header.memory_size,
                    .virtual_address = segment_header.virtual_address,
                    .file_offset = segment_header.content_offset,
                    .source = DeferredCopy::Source::ELF
                };
                print("\tDeferring copy from ELF (0x%04x) to RAM (0x%08lx)!\n",
                    copy.file_offset, copy.virtual_address);
                deferred_copies.emplace_back(std::move(copy));
            }
            if (segment_header.virtual_address != segment_header.physical_address)
            {
                // May be copying from flash to RAM (preinitialized data)
                if (segment_header.virtual_address < piconsole_program_ram_start
                    || segment_header.virtual_address >= piconsole_program_ram_end)
                {
                    print("\tInvalid segment initialization! virtual_address != physical_address, but virtual_address isn't in program RAM!\n");
                    return false;
                }
                else if (segment_header.physical_address < piconsole_program_flash_start
                        || segment_header.physical_address >= piconsole_program_flash_end)
                {
                    print("\tInvalid segment initialization! virtual_address != physical_address, but physical_address isn't in program Flash!\n");
                    return false;
                }
                // Defer segment copy into RAM to the end
                const DeferredCopy copy{
                    .segment_size = segment_header.segment_size,
                    .memory_size = segment_header.memory_size,
                    .virtual_address = segment_header.virtual_address,
                    .physical_address = segment_header.physical_address,
                    .source = DeferredCopy::Source::Flash
                };
                print("\tDeferring copy from flash (0x%08lx) to RAM (0x%08lx)!\n", copy.physical_address, copy.virtual_address);
                deferred_copies.emplace_back(std::move(copy));
            }
            
            current_segment_header_offset = next_segment_header_offset;
        }
    }
    print("\tInitial copies to flash done; doing any deferred copies...\n");
    __compiler_memory_barrier();
    for (const DeferredCopy& copy : deferred_copies)
    {
        switch (copy.source)
        {
            case DeferredCopy::Source::Nothing:
            {
                print("\tClearing memory region: 0x%08lx-0x%08lx (size: 0x%08lx)\n",
                    copy.virtual_address, copy.virtual_address + copy.memory_size, copy.memory_size);
                std::memset(reinterpret_cast<void*>(copy.virtual_address), 0, copy.memory_size);
                break;
            }
            case DeferredCopy::Source::ELF:
            {
                print("\tCopying memory from ELF offset 0x%04x to RAM address 0x%08lx (size: 0x%08lx)\n",
                    copy.file_offset, copy.virtual_address + copy.memory_size, copy.memory_size);
                std::span<std::uint8_t> segment_data{reinterpret_cast<std::uint8_t*>(copy.virtual_address), copy.segment_size};
                reader.seek_absolute(copy.file_offset);
                if (!reader.read_bytes(segment_data))
                {
                    print("Failed to read segment data in deferred_copy\n");
                    return false;
                }
                const std::size_t remaining_bytes{ copy.memory_size - copy.segment_size };
                if (remaining_bytes > 0)
                {
                    std::memset(reinterpret_cast<void*>(copy.virtual_address + remaining_bytes), 0, remaining_bytes);
                }
                break;
            }
            case DeferredCopy::Source::Flash:
            {
                const std::size_t flash_offset{ copy.physical_address - XIP_BASE };
                print("\tCopying memory from flash address 0x%08lx (offset: 0x%08lx) to RAM address 0x%08lx (size: 0x%08lx)\n",
                    copy.physical_address, flash_offset,
                    copy.virtual_address + copy.memory_size, copy.memory_size);
                const int dma_channel{ dma_claim_unused_channel(false) };
                if (dma_channel != -1)
                {
                    const bool memory_size_is_aligned{ copy.memory_size % 4 == 0 };
                    const std::size_t word_count{ copy.memory_size / 4 + (memory_size_is_aligned ? 0 : 1) };
                    print("\tStarting DMA of 0x%08lx words...\n", word_count);
                    CALL_WITH_INTERUPTS_DISABLED(flash_bulk_read(copy.virtual_address, word_count, flash_offset, dma_channel));
                }
                else
                {
                    print("\tNo DMA available for flash->RAM copy; copying the slow way\n");
                    std::memcpy(reinterpret_cast<void*>(copy.virtual_address), reinterpret_cast<const void*>(flash_offset), copy.segment_size);
                    const std::size_t remaining_bytes{ copy.memory_size - copy.segment_size };
                    if (remaining_bytes > 0)
                    {
                        std::memset(reinterpret_cast<void*>(copy.virtual_address + remaining_bytes), 0, remaining_bytes);
                    }
                }
                break;
            }
        }
    }

    program_init_fn *init_func{ program_init.value() };
    print("Calling piconsole_program_init: 0x%p\n", init_func);
    CALL_WITH_INTERUPTS_DISABLED(bool success{ init_func() });
    
    return true;
}

piconsole_program_init
{
    puts("Good job :D");
    OS::get().get_lcd().fill(color::cyan<RGB565>());
    return true;
}
