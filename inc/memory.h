#pragma once
#include "debug.h"
#include <cstdint>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>

#define VERBOSE_MEMORY_DEBUGGING 1

#if VERBOSE_MEMORY_DEBUGGING
#define mem_print(...) print("[MEM] " __VA_ARGS__)
#else
#define mem_print(...)
#endif

template <typename T>
concept trivially_destructable = std::is_trivially_destructible_v<T> || std::is_same_v<T, void>;
template <typename T>
concept trivially_constructable = std::is_trivially_constructible_v<T> || std::is_same_v<T, void>;

class OS;

constexpr static std::size_t memory_block_header_size{ 0x0c };

namespace MemoryMap
{
    constexpr static std::size_t sram_start{ 0x2000'0000 };
    constexpr static std::size_t dynamic_start{ 0x2000'35fc };
    constexpr static std::size_t dynamic_end{ 0x2002'0000 - 8 };
    constexpr static std::size_t dynamic_size{ dynamic_end - dynamic_start };
    constexpr static std::size_t launched_program_start{ 0x2002'0000 };
    constexpr static std::size_t launched_program_size{ 0x0001'D96C };
    constexpr static std::size_t launched_program_end{ launched_program_start + launched_program_size };
};

template <typename TAllocator>
using basic_string = std::basic_string<char, std::char_traits<char>, TAllocator>;

class Memory
{
public: 
    enum class Owner : std::uint8_t {
        None = 0,
        OS = 1,
        Program = 2,
        Unknown = 3
    };

private:
    Memory() {}

    struct BlockHeader
    {
        static std::size_t count;
        std::size_t block_size;
        BlockHeader* previous{ nullptr };
        Owner owner : 7;
        bool is_free : 1;

        BlockHeader(std::size_t block_size) : block_size{block_size}, is_free{true}, owner{Owner::None} {}
        BlockHeader(std::size_t block_size, Owner owner) : block_size{block_size}, is_free{false}, owner{owner} {}

        BlockHeader* get_next()
        {
            return reinterpret_cast<BlockHeader*>(static_cast<std::uint8_t *>(get_data()) + block_size);
        }
        const BlockHeader* get_cnext() const
        {
            return reinterpret_cast<const BlockHeader*>(static_cast<const std::uint8_t *>(get_data()) + block_size);
        }
        const BlockHeader* get_next() const
        {
            return get_cnext();
        }
        void* get_data()
        {
            return reinterpret_cast<std::uint8_t*>(this) + sizeof(BlockHeader);
        }
        const void* get_cdata() const
        {
            return reinterpret_cast<const std::uint8_t*>(this) + sizeof(BlockHeader);
        }
        const void* get_data() const
        {
            return get_cdata();
        }

        void split(std::size_t byte_count)
        {
            if (block_size < byte_count + sizeof(BlockHeader))
            {
                // Don't split if a new header can't be fit
                mem_print("\tSkipping block split as there will only be %u (< %u)bytes left after allocating %u\n", block_size - byte_count, sizeof(BlockHeader), byte_count);
                return;
            }
            std::size_t new_block_size{ block_size - byte_count - sizeof(BlockHeader) };
            mem_print("\tSplitting block; New block size: %u\n", new_block_size);
            block_size = byte_count;
            BlockHeader *new_block = new (get_next()) BlockHeader(new_block_size);
            new_block->is_free = is_free; // Technically should always be `true`
            new_block->previous = this;
            allocated_dynamic_memory_memory_blocks += sizeof(BlockHeader);
            free_dynamic_memory -= sizeof(BlockHeader);
            ++count;
            mem_print("\tTotal block count: %u\n", count);
        }

        static BlockHeader* get_header(void* memory)
        {
            return (BlockHeader*)((std::uint8_t*)memory - sizeof(BlockHeader));
        }

        static BlockHeader* get_first()
        {
            return (BlockHeader*)MemoryMap::dynamic_start;
        }

        static BlockHeader* find_smallest_that_fits(std::size_t byte_count)
        {
            BlockHeader* block_header{ get_first() };
            BlockHeader* best_block{ nullptr };
            while (block_header < (BlockHeader*)MemoryMap::dynamic_end)
            {
                if (block_header->is_free && block_header->block_size >= byte_count)
                {
                    if (block_header->block_size == byte_count)
                    {
                        return block_header;
                    }
                    if (best_block == nullptr || best_block->block_size > block_header->block_size)
                    {
                        best_block = block_header;
                    }
                }
                block_header = block_header->get_next();
            }
            return best_block;
        }

        static BlockHeader* find_first_that_fits(std::size_t byte_count)
        {
            BlockHeader* block_header{ get_first() };
            while (block_header < (BlockHeader*)MemoryMap::dynamic_end)
            {
                if (block_header->is_free && block_header->block_size >= byte_count)
                {
                    return block_header;
                }
                block_header = block_header->get_next();
            }
            return nullptr;
        }
    };
    static_assert(sizeof(BlockHeader) == memory_block_header_size);

    static std::uint8_t dynamic_memory[MemoryMap::dynamic_size];
    static void init();

public:
    template <typename T>
    class ProgramAllocator
    {
    public:
        using value_type = T;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        
        inline pointer address(reference r) { return &r; }
        inline const_pointer address(const_reference r) { return &r; }
        inline pointer allocate(size_type count, void* = 0) { return reinterpret_cast<T*>(Memory::allocate(count * sizeof(T))); }
        inline void deallocate(pointer memory, size_type count) { Memory::release(static_cast<void*>(memory)); }
        inline size_type max_size() const { return Memory::get_largest_free_block_size() / sizeof(T); }
        inline void construct(pointer memory, const T& other) { new (memory) T(other); }
        inline void destroy(pointer memory) { memory->~T(); }
        inline bool operator==(const ProgramAllocator& other) const { return true; }
    };
    
    template <typename TAllocator=ProgramAllocator<char>>
    static basic_string<TAllocator> human_readable(std::size_t byte_count, int decimals = 2)
    {
        if (byte_count < 3000)
        {
            std::size_t digit_count{ 1 };
            for (std::size_t temp_count{ byte_count / 10 }; temp_count > 0; temp_count /= 10)
            {
                ++digit_count;
            }
            basic_string<TAllocator> str(digit_count + 1, '0');
            for (std::size_t i{ digit_count - 1 }; byte_count > 0; ++i, byte_count /= 10)
            {
                str[i] = '0' + byte_count % 10;
            }
            return str;
        }
        const char* suffix{ "KB" };
        float size{ static_cast<float>(byte_count) / 1024.0f };
        if (size > 3000.0f)
        {
            suffix = "MB";
            size /= 1024.0f;
            if (size > 3000.0f)
            {
                suffix = "GB";
                size /= 1024.0f;
            }
        }
        std::basic_ostringstream<char, std::char_traits<char>, TAllocator> output_stream;
        output_stream << std::setprecision(decimals) << size << suffix;
        return output_stream.str();
    }

    [[nodiscard]] static void* allocate(std::size_t byte_count)
    {
        return allocate_implementation(byte_count, Owner::Program);
    }
    template <trivially_constructable T, typename ...Args>
    [[nodiscard]] static T* allocate(std::size_t count = 1, Args&&...args)
    {
        return static_cast<T*>(allocate(sizeof(T)));
    }
    template <typename T, typename ...Args>
    [[nodiscard]] static T* allocate(std::size_t count = 1, Args&&...args)
    {
        T* memory{ static_cast<T*>(allocate(sizeof(T))) };
        for (std::size_t i{ 0 }; i < count; ++i)
        {
            new (memory + i) T(std::forward(args)...);
        }
        return memory;
    }

    template <trivially_destructable T>
    static void release(T* memory) { release_implementation(memory); }
    template <typename T>
    static void release(T* memory) { memory->~T(); release_implementation(memory); }

    static inline std::size_t get_total_allocated_dynamic_memory()
    {
        return allocated_dynamic_memory_OS + allocated_dynamic_memory_program + allocated_dynamic_memory_memory_blocks;
    }

    static std::size_t get_free_byte_count() { return free_dynamic_memory; }
    static std::size_t get_byte_count_owned_by(Owner owner)
    {
        switch (owner)
        {
            case Owner::None:
                return allocated_dynamic_memory_memory_blocks;
            case Owner::OS:
                return allocated_dynamic_memory_OS;
            case Owner::Program:
                return allocated_dynamic_memory_program;
        }
        return free_dynamic_memory;
    }

    static std::size_t get_largest_free_block_size()
    {
        BlockHeader* block_header{ BlockHeader::get_first() };
        BlockHeader* best_block{ nullptr };
        while (block_header < (BlockHeader*)MemoryMap::dynamic_end)
        {
            if (block_header->is_free
                && (best_block == nullptr || best_block->block_size < block_header->block_size))
            {
                best_block = block_header;
            }
            block_header = block_header->get_next();
        }
        return best_block == nullptr ? 0 : best_block->block_size;
    }

private:
    static void *allocate_implementation(std::size_t byte_count, Owner owner)
    {
        mem_print("Allocating %u bytes\n", byte_count);
        constexpr static std::size_t word_alignment{ 4 };
        if (const std::size_t alignment_error{ byte_count % word_alignment }; alignment_error != 0)
        {
            const std::size_t alignment_fix{ word_alignment - alignment_error };
            mem_print("\tByte count isn't word aligned, adding %u (%u total)\n", alignment_fix, byte_count + alignment_fix);
            byte_count += alignment_fix;
        }
        BlockHeader *header{ BlockHeader::find_smallest_that_fits(byte_count) };
        if (header == nullptr)
        {
            print("! OUT OF MEMORY !");
            // TODO: Display error to LCD
            return nullptr;
        }
        header->split(byte_count);
        header->owner = owner;
        switch (owner)
        {
            case Owner::OS:
                mem_print("\tOwner: OS\n");
                allocated_dynamic_memory_OS += header->block_size;
                break;
            case Owner::Program:
                mem_print("\tOwner: Program\n");
                allocated_dynamic_memory_program += header->block_size;
                break;
        }
        header->is_free = false;
        free_dynamic_memory -= header->block_size;
        return header->get_data();
    }

    static void release_implementation(void* memory)
    {
        mem_print("Releasing memory: %x\n", memory);
        BlockHeader *header{ BlockHeader::get_header(memory) };
        mem_print("\tHeader location: %x\n", header);
        header->is_free = true;
        switch (header->owner)
        {
            case Owner::OS:
                mem_print("\tData owner: OS\n");
                allocated_dynamic_memory_OS -= header->block_size;
                break;
            case Owner::Program:
                mem_print("\tData owner: Program\n");
                allocated_dynamic_memory_program -= header->block_size;
                break;
        }
        mem_print("\t%u bytes have been freed\n", header->block_size);
        free_dynamic_memory += header->block_size;
        if (BlockHeader *next_header{ header->get_next() }; next_header < (BlockHeader*)MemoryMap::dynamic_end && next_header->is_free)
        {
            mem_print("\tNext header (%x) is also free; merging forward\n", next_header);
            header->block_size += next_header->block_size + sizeof(BlockHeader);
            allocated_dynamic_memory_memory_blocks -= sizeof(BlockHeader);
            free_dynamic_memory += sizeof(BlockHeader);
            --BlockHeader::count;
        }
        if (header->previous != nullptr && header->previous->is_free)
        {
            mem_print("\tPrevious header (%x) is also free; merging backward\n", header->previous);
            header->previous->block_size += header->block_size + sizeof(BlockHeader);
            allocated_dynamic_memory_memory_blocks -= sizeof(BlockHeader);
            free_dynamic_memory += sizeof(BlockHeader);
            --BlockHeader::count;
        }
        mem_print("\tTotal block count: %u\n", BlockHeader::count);
    }

    [[nodiscard]] static void* allocate_OS(std::size_t byte_count)
    {
        return allocate_implementation(byte_count, Owner::OS);
    }
    template <typename T, typename ...Args>
    [[nodiscard]] static T* allocate_OS(Args&&...args) { return new (allocate_OS(sizeof(T))) T(std::forward(args)...); }


    template <typename T>
    class OSAllocator : public ProgramAllocator<T>
    {
    public:
        using value_type = ProgramAllocator<T>::value_type;
        using pointer = ProgramAllocator<T>::pointer;
        using const_pointer = ProgramAllocator<T>::const_pointer;
        using reference = ProgramAllocator<T>::reference;
        using const_reference = ProgramAllocator<T>::const_reference;
        using size_type = ProgramAllocator<T>::size_type;
        using difference_type = ProgramAllocator<T>::difference_type;

        inline pointer allocate(size_type count, void* = 0) { return reinterpret_cast<T*>(Memory::allocate_OS(count * sizeof(T))); }
    };

    static std::size_t free_dynamic_memory;
    static std::size_t allocated_dynamic_memory_OS;
    static std::size_t allocated_dynamic_memory_program;
    static std::size_t allocated_dynamic_memory_memory_blocks;

    friend OS;
    friend BlockHeader;
};

using program_string = basic_string<Memory::ProgramAllocator<char>>;
using string = program_string;

template <typename T>
using program_vector = std::vector<T, Memory::ProgramAllocator<T>>;
template <typename T>
using vector = program_vector<T>;
