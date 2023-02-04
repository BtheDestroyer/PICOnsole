#pragma once
#include <algorithm>
#include <concepts>
#include <cstdint>
#include <span>
#include "hardware/spi.h"
#include "hardware/pwm.h"

inline constexpr std::uint8_t operator ""_b(unsigned long long v)
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
    
    constexpr operator std::uint16_t&() { return data; }
    constexpr operator const std::uint16_t&() const { return data; }

    std::uint16_t data{ 0 };
};

class SPILCD
{
public:
    SPILCD();
    virtual ~SPILCD();

    [[nodiscard]] virtual constexpr std::size_t get_width() const = 0;
    [[nodiscard]] virtual constexpr std::size_t get_height() const = 0;
    [[nodiscard]] virtual constexpr std::size_t get_bytes_per_pixel() const = 0;
    [[nodiscard]] constexpr std::size_t get_bit_depth() const;

    virtual void write_data(std::uint8_t byte);
    virtual void write_data(std::span<const std::uint8_t> bytes);
    void write_data(std::initializer_list<std::uint8_t> bytes) { write_data(std::span{bytes.begin(), bytes.end()}); }
    virtual void write_command(std::uint8_t byte);
    virtual void write_command(std::span<const std::uint8_t> bytes);
    void write_command(std::initializer_list<std::uint8_t> bytes) { write_command(std::span{bytes.begin(), bytes.end()}); }

    virtual void reset();
    
    virtual void show() = 0;

    constexpr const uint& get_baudrate() const { return baudrate; }
    virtual void set_backlight_strength(float strength);

protected:
    static spi_inst_t* get_spi() { return spi1; }

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

    [[nodiscard]] constexpr std::size_t get_bytes_per_pixel() const override { return sizeof(ColorFormat); };
    [[nodiscard]] constexpr std::size_t get_width() const override { return TWidth; }
    [[nodiscard]] constexpr std::size_t get_height() const override { return THeight; }

    // Drawing
    [[nodiscard]] virtual const ColorFormat& get_pixel(std::size_t x, std::size_t y) const = 0;
    virtual void set_pixel(ColorFormat color, std::size_t x, std::size_t y) = 0;
    virtual void fill(ColorFormat color) = 0;
    virtual void line_horizontal(ColorFormat color, std::size_t x, std::size_t y, std::size_t width) = 0;
    virtual void line_vertical(ColorFormat color, std::size_t x, std::size_t y, std::size_t height) = 0;
    virtual void line(ColorFormat color, std::size_t start_x, std::size_t start_y, std::size_t end_x, std::size_t end_y) = 0;
    virtual void rectangle(ColorFormat color, std::size_t x, std::size_t y, std::size_t width, std::size_t height)
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
    virtual void filled_rectangle(ColorFormat color, std::size_t x, std::size_t y, std::size_t width, std::size_t height)
    {
        const std::size_t end_line{ y + height };
        while (y < end_line)
        {
            line_horizontal(color, x, y, width);
            ++y;
        }
    }
};

class PicoLCD_1_8 : public ColorLCD<RGB565, 160, 128>
{
public:
    PicoLCD_1_8();
    virtual ~PicoLCD_1_8();

    void show() override;

    // Drawing
    [[nodiscard]] inline const ColorFormat& get_pixel(std::size_t x, std::size_t y) const override
    {
        return buffer[x + y * get_width()];
    }
    inline void set_pixel(ColorFormat color, std::size_t x, std::size_t y) override
    {
        buffer[x + y * get_width()] = color;
    }
    void fill(ColorFormat color) override;
    void line_horizontal(ColorFormat color, std::size_t x, std::size_t y, std::size_t width) override;
    void line_vertical(ColorFormat color, std::size_t x, std::size_t y, std::size_t height) override;
    void line(ColorFormat color, std::size_t start_x, std::size_t start_y, std::size_t end_x, std::size_t end_y) override;
    virtual void wait_for_dma() const;

private:
    std::array<RGB565, width * height> buffer;
    int dma_channel;
};

namespace color
{
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    constexpr TColorFormat black() { return TColorFormat{static_cast<std::uint8_t>(0x00)}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    constexpr TColorFormat dark_grey() { return TColorFormat{static_cast<std::uint8_t>(0x44)}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    constexpr TColorFormat grey() { return TColorFormat{static_cast<std::uint8_t>(0x88)}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    constexpr TColorFormat light_grey() { return TColorFormat{static_cast<std::uint8_t>(0xCC)}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    constexpr TColorFormat white() { return TColorFormat{static_cast<std::uint8_t>(0xFF)}; }

    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    constexpr TColorFormat red() { return TColorFormat{0xFF, 0x00, 0x00}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    constexpr TColorFormat green() { return TColorFormat{0x00, 0xFF, 0x00}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    constexpr TColorFormat blue() { return TColorFormat{0x00, 0x00, 0xFF}; }

    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    constexpr TColorFormat yellow() { return TColorFormat{0xFF, 0xFF, 0x00}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    constexpr TColorFormat cyan() { return TColorFormat{0x00, 0xFF, 0xFF}; }
    template <typename TColorFormat>
    requires std::derived_from<TColorFormat, ColorFormat>
    constexpr TColorFormat magenta() { return TColorFormat{0xFF, 0x00, 0xFF}; }
}
