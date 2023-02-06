#pragma once
#include <cstdint>
#include <span>
#include <cstring>
#include <string_view>
#include <sstream>
#include <iomanip>
#include <vector>
#include "ff.h"
#include "debug.h"

class SDCard
{
public:
    bool init();
    virtual ~SDCard();

    [[nodiscard]] bool is_valid() const { return mount_result == FR_OK; }
    operator bool() const { return is_valid(); }

    virtual FILINFO get_file_info(const char* path) const;
    virtual FSIZE_t get_file_size(const char* path) const;

    virtual std::string read_text_file(const char* path) const;
    virtual bool read_text_file(const char* path, std::string& out_contents) const;
    virtual std::vector<std::uint8_t> read_binary_file(const char* path) const;
    virtual bool read_binary_file(const char* path, std::span<std::uint8_t> out_buffer) const;
    virtual bool write_text_file(const char* path, std::string_view contents);
    virtual bool write_binary_file(const char* path, std::span<const std::uint8_t> buffer);

    class FileInterface
    {
    protected:
        FileInterface() {}

    public:
        ~FileInterface()
        {
            const FRESULT close_result{ f_close(&file_handle) };
            if (close_result != FR_OK)
            {
                print("FileInterface failed to f_close in read_text_file; Err: %d\n", close_result);
                return;
            }
        }
        bool is_valid() { return last_result == FR_OK; }

        void seek_relative(std::int64_t offset)
        {
            if (offset == 0)
            {
                return;
            }
            else if (offset > 0)
            {
                seek_absolute(file_handle.fptr + static_cast<std::uint64_t>(offset));
                return;
            }
            // else/offset < 0
            seek_absolute(file_handle.fptr - static_cast<std::uint64_t>(std::abs(offset)));
        }

        void seek_absolute(FSIZE_t offset)
        {
            f_lseek(&file_handle, offset);
            current_offset = offset;
        }

        constexpr FSIZE_t get_current_offset() const { return current_offset; }
    
    protected:
        FIL file_handle;
        FRESULT last_result;
        FSIZE_t current_offset{ 0 };
    };

    class FileReader : public FileInterface
    {
    public:
        FileReader(const char* path)
        {
            const FRESULT open_result{ f_open(&file_handle, path, FA_READ) };
            last_result = open_result;
            if (open_result != FR_OK)
            {
                print("FileReader failed to f_open path: %s; Err: %d", path, open_result);
                return;
            }
        }

        template<typename TData>
        bool read(TData& out_object)
        {
            if (!is_valid())
            {
                return false;
            }
            constexpr static std::size_t object_size{ sizeof(TData) };
            std::array<std::uint8_t, object_size> buffer;
            if (!read_bytes(buffer))
            {
                print("FileReader failed to read_bytes for object\n");
                return false;
            }
            std::memcpy(&out_object, buffer.data(), buffer.size());
            return true;
        }

        bool read_bytes(std::span<std::uint8_t> memory)
        {
            const std::size_t total_size{ memory.size() };
            FSIZE_t read_bytes{ 0 };
            while (total_size > read_bytes)
            {
                constexpr static unsigned int max_chunk_size{ std::numeric_limits<unsigned int>::max() };
                const FSIZE_t remaining_bytes{ total_size - read_bytes };
                unsigned int chunk_size{ remaining_bytes > max_chunk_size ? max_chunk_size : static_cast<unsigned int>(remaining_bytes) };
                const FRESULT read_result{ f_read(&file_handle, memory.data() + read_bytes, chunk_size, &chunk_size) };
                current_offset += chunk_size;
                if (read_result != FR_OK)
                {
                    last_result = read_result;
                    print("FileReader failed to f_read; Err: %d\n", read_result);
                    const FRESULT close_result{ f_close(&file_handle) };
                    if (close_result != FR_OK)
                    {
                        last_result = read_result;
                        print("FileReader failed to f_close in read; Err: %d\n", close_result);
                    }
                    return false;
                }
                read_bytes += chunk_size;
            }
            return true;
        }
    };

    constexpr static std::size_t max_path_length{ 256 };

private:
    static bool driver_initialized;
    FATFS file_system;
    FRESULT mount_result;
};
