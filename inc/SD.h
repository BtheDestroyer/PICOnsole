#pragma once
#include <cstdint>
#include <span>
#include <cstring>
#include <string_view>
#include <sstream>
#include <iomanip>
#include <vector>
#include "ff.h"
#include "memory.h"
#include "debug.h"

class SDCard
{
public:
    SDCard();
    virtual ~SDCard();

    [[nodiscard]] bool is_valid() const { return mount_result == FR_OK; }
    operator bool() const { return is_valid(); }

    virtual FILINFO get_file_info(const char* path) const;
    virtual FSIZE_t get_file_size(const char* path) const;

    virtual string read_text_file(const char* path) const;
    virtual bool read_text_file(const char* path, string& out_contents) const;
    virtual vector<std::uint8_t> read_binary_file(const char* path) const;
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
        bool is_valid() { return last_result != FR_OK; }
    
    protected:
        FIL file_handle;
        FRESULT last_result;
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
        bool read(TData* out_object)
        {
            if (!is_valid())
            {
                return false;
            }
            constexpr static std::size_t object_size{ sizeof(TData) };
            std::uint8_t buffer[object_size];
            FSIZE_t read_bytes{ 0 };
            while (object_size > read_bytes)
            {
                constexpr static unsigned int max_chunk_size{ std::numeric_limits<unsigned int>::max() };
                const FSIZE_t remaining_bytes{ object_size - read_bytes };
                unsigned int chunk_size{ remaining_bytes > max_chunk_size ? max_chunk_size : static_cast<unsigned int>(remaining_bytes) };
                const FRESULT read_result{ f_read(&file_handle, buffer + read_bytes, chunk_size, &chunk_size) };
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
            std::memcpy(out_object, buffer, object_size);
            return true;
        }
    };

private:
    static bool driver_initialized;
    FATFS file_system;
    FRESULT mount_result;
};
