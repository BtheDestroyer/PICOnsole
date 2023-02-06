#pragma once
#include <cstdint>

#undef piconsole_program_init
#define piconsole_program_init __attribute__((section(".piconsole.program.init"))) piconsole_program_init
#undef piconsole_program_update
#define piconsole_program_update __attribute__((section(".piconsole.program.update"))) piconsole_program_update

struct ELFHeader
{
    struct Identifier
    {
        constexpr static std::uint8_t expected_magic_number[4] {0x7f, 'E', 'L', 'F'};
        std::uint8_t magic_number[4];
        enum class BitCount : std::uint8_t {
            None   = 0,
            _32Bit = 1,
            _64Bit = 2
        } bitcount;
        enum class DataOrder : std::uint8_t {
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
    enum class Type : std::uint16_t{
        None = 0,
        Relocatable = 1,
        Executable = 2,
        Shared = 3,
        Core = 4
    } type;
    std::uint16_t machine;
    std::uint32_t version;
    std::size_t entrypoint;
    std::size_t program_header_offset;
    std::size_t section_header_offset;
    std::uint32_t flags;
    std::uint16_t ehsize;
    std::uint16_t phentsize;
    std::uint16_t phnum;
    std::uint16_t shentsize;
    std::uint16_t shnum;
    std::uint16_t shstrndx;
};
