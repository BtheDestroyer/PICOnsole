#pragma once
#include <algorithm>
#include <concepts>
#include <cstdint>
#include <span>
#include "PICOnsole_defines.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"

extern "C"
{
#if _PICONSOLE_OS
    extern std::uint8_t __piconsole_lcd_buffer[];
    extern void *__piconsole_lcd_buffer_end[];
#elif _PICONSOLE_PROGRAM
    // TODO: Find a way to link these in from the OS's ELF?
    std::uint8_t *__piconsole_lcd_buffer { reinterpret_cast<std::uint8_t*>(0x20000468) };
    void **__piconsole_lcd_buffer_end { (void**)0x2000a468 };
#endif
}

inline consteval std::uint8_t operator ""_b(unsigned long long v)
{
    return static_cast<std::uint8_t>(v);
}

class ColorFormat {};

class RGB565 : public ColorFormat
{
public:
    constexpr RGB565(std::uint8_t r, std::uint8_t g, std::uint8_t b)
    {
        r >>= 3; // 8-bit -> 5 bit
        g >>= 2; // 8-bit -> 6 bit
        b >>= 3; // 8-bit -> 5 bit

        data = r << 3 | (g & 0b000111) << 13 | (g & 0b111000) >> 3 | b << 8;
    }
    constexpr RGB565(std::uint8_t gs = 0x00) : RGB565(gs, gs, gs) {}
    template <std::floating_point TFloat>
    constexpr RGB565(TFloat r, TFloat g, TFloat b)
        : RGB565(
            static_cast<std::uint8_t>(std::clamp(r, TFloat{0.}, TFloat{1.}) * 255_b),
            static_cast<std::uint8_t>(std::clamp(g, TFloat{0.}, TFloat{1.}) * 255_b),
            static_cast<std::uint8_t>(std::clamp(b, TFloat{0.}, TFloat{1.}) * 255_b)
        ) {}
    constexpr RGB565(std::uint16_t color) : data{color} {}
    
    GETTER constexpr explicit operator std::uint16_t&() { return data; }
    GETTER constexpr explicit operator const std::uint16_t&() const { return data; }

    std::uint16_t data{ 0 };
};

class SPILCD
{
public:
    PICONSOLE_MEMBER_FUNC bool init(bool final_step = true);
    PICONSOLE_MEMBER_FUNC bool uninit();
    GETTER PICONSOLE_MEMBER_FUNC const bool& is_initialized() const { return initialized; }
    PICONSOLE_MEMBER_FUNC ~SPILCD();

    GETTER PICONSOLE_MEMBER_FUNC constexpr std::size_t get_width() const = 0;
    GETTER PICONSOLE_MEMBER_FUNC constexpr std::size_t get_height() const = 0;
    GETTER PICONSOLE_MEMBER_FUNC constexpr std::size_t get_bytes_per_pixel() const = 0;
    GETTER PICONSOLE_MEMBER_FUNC constexpr std::size_t get_bit_depth() const { return get_bytes_per_pixel() * 8; }

    PICONSOLE_MEMBER_FUNC void write_data(std::uint8_t byte);
    PICONSOLE_MEMBER_FUNC void write_data(std::span<const std::uint8_t> bytes);
    PICONSOLE_MEMBER_FUNC void write_data(std::initializer_list<std::uint8_t> bytes);
    PICONSOLE_MEMBER_FUNC void write_command(std::uint8_t byte);
    PICONSOLE_MEMBER_FUNC void write_command(std::span<const std::uint8_t> bytes);
    PICONSOLE_MEMBER_FUNC void write_command(std::initializer_list<std::uint8_t> bytes);

    PICONSOLE_MEMBER_FUNC void reset();
    
    PICONSOLE_MEMBER_FUNC void show() = 0;

    GETTER PICONSOLE_MEMBER_FUNC const uint& get_baudrate() const { return baudrate; }
    PICONSOLE_MEMBER_FUNC void set_backlight_strength(float strength);

    constexpr static std::size_t buffer_size{ 0 };

protected:
    static spi_inst_t* get_spi();
    bool initialized{ false };

private:
    uint baudrate{0};

    float backlight_strength{0.0f};
    uint backlight_pwm_slice;
    pwm_config backlight_config;
};

template <typename TColorFormat, std::size_t TWidth, std::size_t THeight>
requires std::derived_from<TColorFormat, ColorFormat>
class ColorLCD : public SPILCD
{
public:
    using ColorFormat = TColorFormat;
    constexpr static std::size_t width{ TWidth };
    constexpr static std::size_t height{ THeight };
    constexpr static std::size_t buffer_size{ width * height * sizeof(ColorFormat) };
    using buffer_type = std::array<ColorFormat, width * height>;

    GETTER PICONSOLE_MEMBER_FUNC constexpr std::size_t get_bytes_per_pixel() const override { return sizeof(ColorFormat); };
    GETTER PICONSOLE_MEMBER_FUNC constexpr std::size_t get_width() const override { return TWidth; }
    GETTER PICONSOLE_MEMBER_FUNC constexpr std::size_t get_height() const override { return THeight; }

    // Drawing
    GETTER PICONSOLE_MEMBER_FUNC const ColorFormat& get_pixel(std::size_t x, std::size_t y) const = 0;
    PICONSOLE_MEMBER_FUNC void set_pixel(ColorFormat color, std::size_t x, std::size_t y) = 0;
    PICONSOLE_MEMBER_FUNC void fill(ColorFormat color) = 0;
    PICONSOLE_MEMBER_FUNC void line_horizontal(ColorFormat color, std::size_t x, std::size_t y, std::size_t width) = 0;
    PICONSOLE_MEMBER_FUNC void line_vertical(ColorFormat color, std::size_t x, std::size_t y, std::size_t height) = 0;
    PICONSOLE_MEMBER_FUNC void line(ColorFormat color, std::size_t start_x, std::size_t start_y, std::size_t end_x, std::size_t end_y) = 0;
    PICONSOLE_MEMBER_FUNC void rectangle(ColorFormat color, std::size_t x, std::size_t y, std::size_t width, std::size_t height)
    {
        line_horizontal(color, x, y, width);
        if (height <= 1)
        {
            return;
        }
        line_horizontal(color, x, y + height - 1, width);
        if (height <= 2)
        {
            return;
        }
        const std::size_t side_height{ height - 2 };
        line_vertical(color, x, y + 1, side_height);
        if (width > 1)
        {
            line_vertical(color, x + width - 1, y + 1, side_height);
        }
    }
    PICONSOLE_MEMBER_FUNC void filled_rectangle(ColorFormat color, std::size_t x, std::size_t y, std::size_t width, std::size_t height)
    {
        const std::size_t end_line{ y + height };
        while (y < end_line)
        {
            line_horizontal(color, x, y, width);
            ++y;
        }
    }

protected:
    #if _PICONSOLE_OS
    static inline buffer_type &get_buffer() { return *reinterpret_cast<buffer_type*>(__piconsole_lcd_buffer); }
    #else
    static buffer_type &get_buffer();
    #endif
};

class PicoLCD_1_8 : public ColorLCD<RGB565, 160, 128>
{
public:
    using ColorLCD::buffer_size;
    using ColorLCD::buffer_type;
    using ColorLCD::ColorFormat;

    PICONSOLE_MEMBER_FUNC bool init(bool final_step = true);
    PICONSOLE_MEMBER_FUNC bool uninit();
    PICONSOLE_MEMBER_FUNC ~PicoLCD_1_8();

    PICONSOLE_MEMBER_FUNC void show() override;

    // Drawing
    GETTER PICONSOLE_MEMBER_FUNC const ColorFormat& get_pixel(std::size_t x, std::size_t y) const override
    {
        return get_buffer()[x + y * get_width()];
    }
    PICONSOLE_MEMBER_FUNC void set_pixel(ColorFormat color, std::size_t x, std::size_t y) override
    {
        get_buffer()[x + y * get_width()] = color;
    }
    PICONSOLE_MEMBER_FUNC void fill(ColorFormat color) override;
    PICONSOLE_MEMBER_FUNC void line_horizontal(ColorFormat color, std::size_t x, std::size_t y, std::size_t width) override;
    PICONSOLE_MEMBER_FUNC void line_vertical(ColorFormat color, std::size_t x, std::size_t y, std::size_t height) override;
    PICONSOLE_MEMBER_FUNC void line(ColorFormat color, std::size_t start_x, std::size_t start_y, std::size_t end_x, std::size_t end_y) override;
    PICONSOLE_MEMBER_FUNC void wait_for_dma() const;

private:
    int dma_channel{ -1 };
};

namespace color
{
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    inline constexpr TColorFormat black() { return TColorFormat{static_cast<std::uint8_t>(0x00)}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    inline constexpr TColorFormat dark_grey() { return TColorFormat{static_cast<std::uint8_t>(0x44)}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    inline constexpr TColorFormat grey() { return TColorFormat{static_cast<std::uint8_t>(0x88)}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    inline constexpr TColorFormat light_grey() { return TColorFormat{static_cast<std::uint8_t>(0xCC)}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    inline constexpr TColorFormat white() { return TColorFormat{static_cast<std::uint8_t>(0xFF)}; }

    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    inline constexpr TColorFormat red() { return TColorFormat{0xFF, 0x00, 0x00}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    inline constexpr TColorFormat green() { return TColorFormat{0x00, 0xFF, 0x00}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    inline constexpr TColorFormat blue() { return TColorFormat{0x00, 0x00, 0xFF}; }

    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    inline constexpr TColorFormat yellow() { return TColorFormat{0xFF, 0xFF, 0x00}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    inline constexpr TColorFormat cyan() { return TColorFormat{0x00, 0xFF, 0xFF}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    inline constexpr TColorFormat magenta() { return TColorFormat{0xFF, 0x00, 0xFF}; }
}
