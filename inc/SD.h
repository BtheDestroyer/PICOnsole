#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <sstream>
#include <iomanip>
#include <vector>
#include "ff.h"

class SDCard
{
public:
    SDCard();
    virtual ~SDCard();

    [[nodiscard]] bool is_valid() const { return mount_result == FR_OK; }
    operator bool() const { return is_valid(); }

    virtual FILINFO get_file_info(const char* path) const;
    virtual FSIZE_t get_file_size(const char* path) const;

    static std::string human_readable(FSIZE_t byte_count, int decimals = 2)
    {
        if (byte_count < 3000)
        {
            return std::to_string(byte_count) + 'B';
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
        std::ostringstream output_stream;
        output_stream << std::setprecision(decimals) << size << suffix;
        return output_stream.str();
    }

    virtual std::string read_text_file(const char* path) const;
    virtual bool read_text_file(const char* path, std::string& out_contents) const;
    virtual std::vector<std::uint8_t> read_binary_file(const char* path) const;
    virtual bool read_binary_file(const char* path, std::span<std::uint8_t> out_buffer) const;
    virtual bool write_text_file(const char* path, std::string_view contents);
    virtual bool write_binary_file(const char* path, std::span<std::uint8_t> buffer);

private:
    static bool driver_initialized;
    FATFS file_system;
    FRESULT mount_result;
};
