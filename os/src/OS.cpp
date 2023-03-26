#include "OS.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "hardware/dma.h"
#include "hardware/flash.h"
#include "hardware/structs/ssi.h"
#include "hardware/structs/xip_ctrl.h"
#include "pico/bootrom.h"
#include "debug.h"
#include "program.h"
#include <optional>

#include "RP2040.h"
#include "pico/multicore.h"
#include "hardware/structs/dma.h"
#include "hardware/structs/watchdog.h"
#include "hardware/resets.h"
#include "hardware/watchdog.h"

constexpr uint LED_PIN{ 25u };

bool OS::init()
{
    if (initialized)
    {
        return false;
    }
    stdio_init_all();
    print("time_us_64()=%llu\n", time_us_64());
    print("OS address: 0x%x\n", this);
    print("__piconsole_os: 0x%x\n", __piconsole_os);
    print("__piconsole_os_end: 0x%x\n", __piconsole_os_end);
    print("sizeof(OS): %d\n", sizeof(OS));
    print("Initializing LCD interface (%d bytes)...\n", sizeof(LCD_MODEL));
    print("__piconsole_lcd_buffer: 0x%x\n", __piconsole_lcd_buffer);
    print("__piconsole_lcd_buffer_end: 0x%x\n", __piconsole_lcd_buffer_end);
    print("lcd address: 0x%x\n", &lcd);
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

    if (speaker.is_valid())
    {
        print("Initializing Speaker...\n");
        speaker.init();
        if (speaker.is_active())
        {
            print("Speaker initialized!\n");
        }
        else
        {
            print("Failed to initialize Speaker!\n");
            exit(-3);
        }
    }
    else
    {
        print("No valid Speaker interface to initialize.\n");
    }
    if (input.is_valid())
    {
        print("Initializing input map...\n");
        if (input.init())
        {
            print("Inputs initialized!\n");
        }
        else
        {
            print("Failed to initialize Input!\n");
            exit(-4);
        }
    }
    else
    {
        print("No Input interface to initialize.\n");
    }

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    print("OS initialized\n");
    initialized = true;
    return true;
}

bool OS::uninit(bool cleanly /* = true */)
{
    if (!initialized)
    {
        return false;
    }
    if (cleanly)
    {
        lcd.uninit();
        sd.uninit();
        speaker.uninit();
        input.uninit();
        gpio_deinit(LED_PIN);
    }
    print("OS unitialized\n");
    initialized = false;
    return true;
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
    vibrator.update();
    speaker.update();
    input.update();
    gpio_put(LED_PIN, !gpio_get(LED_PIN));
    multicore_fifo_push_timeout_us(FIFOCodes::os_updated, 8'000);
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

#ifdef CALL_WITH_INTERUPTS_DISABLED
#undef CALL_WITH_INTERUPTS_DISABLED
#endif
#define CALL_WITH_INTERUPTS_DISABLED(...) \
    { \
        const std::uint32_t interupts{ save_and_disable_interrupts() }; \
        __VA_ARGS__; \
        restore_interrupts(interupts); \
    }

bool OS::load_program(std::string_view path)
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

    std::optional<std::uint32_t**> program_vectors;
    std::optional<program_init_fn*> program_init;
    std::optional<program_update_fn*> program_update;
    if (0)
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
                if (std::strcmp(section_name, ".piconsole.program.init") == 0)
                {
                    if (program_init.has_value())
                    {
                        print("\t.piconsole.program.init has duplicate sections!");
                        return false;
                    }
                    print("\t%s: Address=0x%08lx\n", section_name, section_header.address);
                    program_init.emplace(reinterpret_cast<program_init_fn*>(section_header.address | 1lu));
                }
                else if (std::strcmp(section_name, ".piconsole.program.update") == 0)
                {
                    if (program_update.has_value())
                    {
                        print("\t\t\t.piconsole.program.update has duplicate sections!");
                        return false;
                    }
                    print("\t%s: Address=0x%08lx\n", section_name, section_header.address);
                    program_update.emplace(reinterpret_cast<program_update_fn*>(section_header.address | 1lu));
                }
                else if (std::strcmp(section_name, ".text.vectors") == 0)
                {
                    if (program_vectors.has_value())
                    {
                        print("\t\t\t.text has duplicate sections!");
                        return false;
                    }
                    print("\t%s: Address=0x%08lx\n", section_name, section_header.address);
                    program_vectors.emplace(reinterpret_cast<std::uint32_t**>(section_header.address));
                }
            }
        }
        // if (!program_init.has_value())
        // {
        //     print("Section missing for .piconsole.program.init\n");
        //     return false;
        // }
        // else if (!program_update.has_value())
        // {
        //     print("Section missing for .piconsole.program.update\n");
        //     return false;
        // }
        // else if (!program_vectors.has_value())
        // {
        //     print("Section missing for .text.vectors\n");
        //     return false;
        // }
    }

    reader.seek_absolute(elf_header.segment_header_offset);
    print("Loading %d segments...\n", elf_header.segment_header_count);
    print("             idx: Type Offset     VirtAddr   PhysAddr   FileSize MemSize Flags (Raw)    Alignment\n");
    std::vector<DeferredCopy> deferred_copies;
    std::vector<SegmentHeader> segment_headers;
    segment_headers.resize(elf_header.segment_header_count);
    for (std::size_t i{ 0 }; i < elf_header.segment_header_count; ++i)
    {
        if (!reader.read<SegmentHeader>(segment_headers[i]))
        {
            print("Failed to read segment header %d!\n", i);
            std::memset(current_program_path, 0, count_of(current_program_path));
            return false;
        }
        print(" %3d:", i);
        if (segment_headers[i].type < SegmentHeader::Type::Count)
        {
            print(" %.4d", segment_headers[i].type);
        }
        else
        {
            print(" ????");
        }
        print(" 0x%08lx 0x%08lx 0x%08lx",
            segment_headers[i].content_offset, segment_headers[i].virtual_address, segment_headers[i].physical_address);
        print(" 0x%05x  0x%05x",
            segment_headers[i].segment_size, segment_headers[i].memory_size);
        constexpr static auto has_flag{
            [](SegmentHeader::Flags flags, SegmentHeader::Flags mask)
            {
                const std::uint32_t masked{ static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(mask) };
                return masked == static_cast<std::uint32_t>(mask);
            }
        };
        print(" %c%c%c",
            has_flag(segment_headers[i].flags, SegmentHeader::Flags::R) ? 'R' : ' ',
            has_flag(segment_headers[i].flags, SegmentHeader::Flags::W) ? 'W' : ' ',
            has_flag(segment_headers[i].flags, SegmentHeader::Flags::X) ? 'X' : ' ');
        print("   (0x%04x) 0x%04x\n", segment_headers[i].flags, segment_headers[i].alignment);
    }
    // Erase existing flash
    for (const SegmentHeader& segment_header : segment_headers)
    {
        if (segment_header.physical_address < piconsole_program_flash_start
            || segment_header.physical_address >= piconsole_program_flash_end)
        {
            // Segment not in flash, nothing to erase
            continue;
        }
        const std::uint32_t unaligned_flash_offset{ segment_header.physical_address - XIP_BASE };
        const std::uint32_t sector_alignment_error_of_segment_start{ unaligned_flash_offset % FLASH_SECTOR_SIZE };
        const std::uint32_t flash_offset{
            unaligned_flash_offset - (sector_alignment_error_of_segment_start > 0u ? 1u : 0u)
        };
        const std::uint32_t sector_alignment_error_of_segment_size{ segment_header.segment_size % FLASH_SECTOR_SIZE };
        // Erase 1 extra segment if division results in truncation
        const std::uint32_t sector_count{
            segment_header.segment_size / FLASH_SECTOR_SIZE
            + (sector_alignment_error_of_segment_size > 0u ? 1u : 0u) 
            + (sector_alignment_error_of_segment_start > 0u ? 1u : 0u)
        };
        print("Erasing %lu sectors on existing flash starting at 0x%.08x...\n", sector_count, segment_header.physical_address);
        CALL_WITH_INTERUPTS_DISABLED(flash_range_erase(flash_offset, sector_count * FLASH_SECTOR_SIZE));
    }
    // Program flash with new data
    for (const SegmentHeader& segment_header : segment_headers)
    {
        // Temporarily borrowing program RAM; whatever was there won't matter anymore anyway
        // TODO: Chunk this in case the segment_size is > 192KB
        if (segment_header.physical_address >= piconsole_program_flash_start)
        {
            if (segment_header.physical_address < piconsole_program_flash_end)
            {
                // Copy directly to flash
                // Flash writes need to be page aligned, so first we need to copy the bytes
                //   from the beginning of the page we're programming to ram...
                const std::size_t leading_byte_count{ segment_header.physical_address % 0x100 };
                std::span<std::uint8_t> leading_flash{reinterpret_cast<std::uint8_t*>(segment_header.physical_address - leading_byte_count), leading_byte_count};
                std::span<std::uint8_t> leading_RAM{reinterpret_cast<std::uint8_t*>(piconsole_program_ram_start), leading_byte_count};
                //std::memcpy(leading_RAM.data(), leading_flash.data(), leading_byte_count);
                std::memset(leading_RAM.data(), 0xA5, leading_byte_count);
                // ...then we can read the data we want to program from the SD card...
                std::span<std::uint8_t> segment_data{reinterpret_cast<std::uint8_t*>(piconsole_program_ram_start + leading_byte_count), segment_header.segment_size};
                reader.seek_absolute(segment_header.content_offset);
                if (!reader.read_bytes(segment_data))
                {
                    print("Failed to read segment data\n");
                    return false;
                }
                // ...then we can write the whole thing back to flash.
                //   See flash programming example
                print("\tCopying directly from RAM to flash...\n");
                // ...then we need to end with the trailing bytes of the last written page...
                const std::size_t trailing_offset{ leading_byte_count + segment_data.size() };
                const std::size_t alignment_error{ trailing_offset % 0x100 };
                const std::size_t trailing_byte_count{ alignment_error == 0 ? 0 : 0x100 - alignment_error };
                std::span<std::uint8_t> trailing_flash{reinterpret_cast<std::uint8_t*>(segment_header.physical_address + trailing_offset), trailing_byte_count};
                std::span<std::uint8_t> trailing_RAM{reinterpret_cast<std::uint8_t*>(piconsole_program_ram_start + trailing_offset), trailing_byte_count};
                std::memcpy(trailing_RAM.data(), trailing_flash.data(), trailing_byte_count);
                // ..and finally we can write everything back to flash.
                const std::size_t total_byte_count{ leading_byte_count + segment_data.size() + trailing_byte_count };
                hard_assert(total_byte_count % 0x100 == 0);
                const std::size_t flash_offset{ reinterpret_cast<std::size_t>(leading_flash.data()) - XIP_BASE };
                //flash_range_program(flash_offset, leading_RAM.data(), total_byte_count);
                CALL_WITH_INTERUPTS_DISABLED(flash_range_program(flash_offset, leading_RAM.data(), total_byte_count));
                print("\t0x%.08x == 0x%.08x\n", *(std::uint32_t*)(segment_data.data()), *(std::uint32_t*)(segment_header.physical_address));
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
        }
    }
    print("\tInitial copies to flash done; doing any deferred copies...\n");
    const int dma_channel{ dma_claim_unused_channel(false) };
    for (const DeferredCopy& copy : deferred_copies)
    {
        if (copy.virtual_address < piconsole_program_ram_start || copy.virtual_address >= piconsole_program_ram_end)
        {
            print("\tAttempted deferred copy into non-program RAM");
            return false;
        }
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
    if (dma_channel != -1)
    {
        dma_channel_unclaim(dma_channel);
    }

#if 0
    print("Uninitializing OS\n");
    uninit();
    print("Calling program's reset vector...\n");
    asm volatile (
    "mov r0, %[start]\n"
    "ldr r1, =%[vtable]\n"
    "str r0, [r1]\n"
    "ldmia r0!, {r1, r2}\n"
    "msr msp, r1\n"
    "blx r2\n"
    :
    : [start] "r" (0x10080000), [vtable] "X" (PPB_BASE + M0PLUS_VTOR_OFFSET)
    :
    );
    // Should never return
#endif
    typedef void program_entrypoint_t(void);
    const program_entrypoint_t* program_entrypoint{ reinterpret_cast<program_entrypoint_t*>(0x10080001) };
    stop_program();
    print("Launching program on core1...\n");
    multicore_launch_core1(program_entrypoint);
    std::uint32_t launch_result{ ~0u };
    multicore_fifo_pop_timeout_us(500'000, &launch_result);
    program_running = launch_result == FIFOCodes::program_launch_success;
    if (!program_running)
    {
        print("Failed to launch program; the program_launch_success constant was not pushed to the fifo.\n");
    }
    else
    {
        print("Program started successfully!\n");
    }
    return program_running;
}

bool OS::stop_program()
{
    if (!program_running)
    {
        return false;
    }
    print("Stopping current program...\n");
    multicore_reset_core1();
    program_running = false;
    return true;
}
