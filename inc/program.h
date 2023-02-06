#pragma once
#include <cstdint>

#undef piconsole_program_init
#define piconsole_program_init __attribute__((section(".piconsole.program.init"))) piconsole_program_init
#undef piconsole_program_update
#define piconsole_program_update __attribute__((section(".piconsole.program.update"))) piconsole_program_update

constexpr std::size_t piconsole_program_flash_start{ 0x10080000 };
constexpr std::size_t piconsole_program_flash_end{ 0x10200000 };
constexpr std::size_t piconsole_program_ram_start{ 0x20010000 };
constexpr std::size_t piconsole_program_ram_end{ 0x20040000 };

struct ELFHeader
{
    struct Identifier
    {
        constexpr static std::uint8_t expected_magic_number[4] { 0x7f, 'E', 'L', 'F' };
        std::uint8_t magic_number[4];
        enum class BitCount : std::uint8_t
        {
            None   = 0,
            _32Bit = 1,
            _64Bit = 2
        } bitcount;
        enum class DataOrder : std::uint8_t
        {
            None = 0,
            LSB  = 1,
            MSB  = 2
        } data_order;
        std::uint8_t version; // Should *always* be 1
        std::uint8_t os_abi; // Should *always* be 0
        std::uint8_t abi_version; // Should *always* be 0; basically never used
        std::uint8_t padding[7];
    } identifier;
    static_assert(sizeof(Identifier) == 16);
    enum class Type : std::uint16_t
    {
        None = 0,
        Relocatable = 1,
        Executable = 2,
        Shared = 3,
        Core = 4
    } type;
    std::uint16_t machine;
    std::uint32_t version;
    std::size_t entrypoint;
    std::size_t segment_header_offset;
    std::size_t section_header_offset;
    std::uint32_t flags;
    std::uint16_t header_size; // Should always be equal to sizeof(ELFHeader)
    std::uint16_t segment_header_entry_size;
    std::uint16_t segment_header_count;
    std::uint16_t section_header_entry_size;
    std::uint16_t section_header_count;
    std::uint16_t section_header_string_table_index;
};

struct SegmentHeader
{
    enum class Type : std::uint32_t
    {
        Null = 0,
        Load = 1,
        Dynamic = 2,
        Interpreter = 3,
        Note = 4,
        SharedLibrary = 5, // Should never be used
        ProgramHeader = 6,
        ThreadLocalStorage = 7
    } type;
    std::size_t content_offset;
    std::size_t virtual_address;
    std::size_t physical_address;
    std::uint32_t segment_size;
    std::uint32_t memory_size;
    enum class Flags : std::uint32_t
    {
        None = 0b000,
        R    = 0b100,
        W    = 0b010,
        X    = 0b001,
        RW = R | W,
        RX = R | X,
        WX = W | X,
        RWX = R | W | X
    } flags; // TODO: Enum this
    std::uint32_t alignment; // Should always be 4
};
