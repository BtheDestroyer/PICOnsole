#include "memory.h"

std::size_t Memory::free_dynamic_memory;
std::size_t Memory::allocated_dynamic_memory_OS;
std::size_t Memory::allocated_dynamic_memory_program;
std::size_t Memory::allocated_dynamic_memory_memory_blocks;

std::size_t Memory::BlockHeader::count{ 0 };

std::uint8_t Memory::dynamic_memory[MemoryMap::dynamic_size];

void Memory::init()
{
    free_dynamic_memory = MemoryMap::dynamic_size - sizeof(BlockHeader);
    void *mem{ dynamic_memory };
    new (mem) BlockHeader(free_dynamic_memory);
    BlockHeader::count = 1;
    allocated_dynamic_memory_memory_blocks = sizeof(BlockHeader);
}
