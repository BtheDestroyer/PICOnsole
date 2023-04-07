#pragma once
#include <cstdint>
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"

typedef void program_reset_fn(void);
typedef int program_init_fn(OS&);
typedef void program_update_fn(OS&);

enum FIFOCodes : std::uint32_t {
    program_launch_success = 1,
    os_updated = 2,

    error_generic = 100,
    error_crash = 101,
};

class OS;

#undef piconsole_program_init
#undef piconsole_program_update
#if _PICONSOLE_OS || _PICONSOLE_PROGRAM
#define piconsole_program_init int __attribute__((section(".piconsole.program.init"))) _piconsole_program_init(OS& os)
#define piconsole_program_update void __attribute__((section(".piconsole.program.update"))) _piconsole_program_update(OS& os)
#endif

constexpr std::size_t piconsole_program_flash_offset{ 0x00080000 };
constexpr std::size_t piconsole_program_flash_start{ XIP_BASE + piconsole_program_flash_offset };
constexpr std::size_t piconsole_program_flash_end{ XIP_BASE + 0x00200000 };
constexpr std::size_t piconsole_program_flash_size{ piconsole_program_flash_end - piconsole_program_flash_start };
static_assert(piconsole_program_flash_size % FLASH_SECTOR_SIZE == 0);
constexpr std::size_t piconsole_program_ram_offset{ 0x00018000 };
constexpr std::size_t piconsole_program_ram_start{ SRAM_BASE + piconsole_program_ram_offset };
constexpr std::size_t piconsole_program_ram_end{ SRAM_BASE + 0x0003E000 };
constexpr std::size_t piconsole_program_ram_size{ piconsole_program_ram_end - piconsole_program_ram_start - 1 };

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
        ThreadLocalStorage = 7,
        Count
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
    std::uint32_t alignment;
};

struct SectionHeader
{
    std::uint32_t string_table_name_index;
    enum class Type : std::uint32_t
    {
        Null = 0,
        ProgramData = 1,
        SymbolTable = 2,
        StringTable = 3,
        Relative = 4,
        HashTable = 5,
        Dynamic = 6,
        Note = 7,
        NoBits = 8,
        Relocation = 9,
        SharedLibrary = 10,
        DynamicSymbols = 11
    } type;
    enum class Flags : std::uint32_t
    {
        None = 0b000,
        Write = 0b001,
        Allocate = 0b010,
        ExecuteInstruction = 0b100,
    }flags;
    std::size_t address;
    std::size_t offset;
    std::uint32_t size;
    std::uint32_t linked_section_index;
    std::uint32_t info;
    std::uint32_t address_alignment;
    std::uint32_t entry_size;
};
static_assert(sizeof(SectionHeader) == 40);

struct SymbolTableEntry
{
    std::uint32_t string_table_name_index;
    std::size_t address;
    std::uint32_t size;
    std::uint8_t info;
    std::uint8_t other;
    std::uint8_t shndx; //?
};

class StringTable
{
public:
    StringTable(std::size_t byte_count)
        : data{static_cast<char*>(malloc(byte_count)), byte_count}, owns_data{ true }
    {}
    StringTable(std::span<char> memory)
        : data(memory), owns_data{false}
    {}
    ~StringTable()
    {
        if (owns_data)
        {
            free(data.data());
        }
    }

    class Entry
    {
    public:
        constexpr Entry(const char* string) : string{ string } {}

        [[nodiscard]] constexpr inline operator const char*()const
        {
            return string;
        }

        [[nodiscard]] constexpr inline const char* const& operator *() const
        {
            return string;
        }

        constexpr inline Entry& operator ++()
        {
            while (*string != '\0')
            {
                ++string;
            }
            ++string;
            return *this;
        }
    
    private:
        const char* string;
    };

    [[nodiscard]] std::span<char> get_data() { return data; }
    [[nodiscard]] std::span<const char> get_cdata() const { return data; }
    [[nodiscard]] std::span<const char> get_data() const { return get_cdata(); }

    [[nodiscard]] Entry front() { return Entry{ data.data() }; }
    [[nodiscard]] Entry operator[](std::size_t index)
    {
        Entry entry{ front() };
        while (index > 0)
        {
            ++entry;
            --index;
        }
        return entry;
    }

private:
    std::span<char> data;
    bool owns_data;
};
