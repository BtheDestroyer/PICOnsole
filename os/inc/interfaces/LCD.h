#pragma once
#include <algorithm>
#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>
#include "PICOnsole_defines.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "gfx/color.h"
#include "gfx/typeface.h"
#include "gfx/typefaces/ascii_5px.h"
#include "gfx/text.h"

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

template <colorformat_t TColorFormat, std::size_t TWidth, std::size_t THeight>
class ColorLCD : public SPILCD
{
public:
    using ColorFormat = TColorFormat;
    constexpr static std::size_t width{ TWidth };
    constexpr static std::size_t height{ THeight };
    constexpr static std::size_t buffer_size{ width * height * sizeof(ColorFormat) };
    using buffer_type = std::array<ColorFormat, width * height>;
    using TextSettings = gfx::text::PrintSettings<ColorLCD<TColorFormat, TWidth, THeight>>;

    PICONSOLE_MEMBER_FUNC ~ColorLCD() {}

    GETTER PICONSOLE_MEMBER_FUNC constexpr std::size_t get_bytes_per_pixel() const override { return sizeof(ColorFormat); };
    GETTER PICONSOLE_MEMBER_FUNC constexpr std::size_t get_width() const override { return TWidth; }
    GETTER PICONSOLE_MEMBER_FUNC constexpr std::size_t get_height() const override { return THeight; }

    // Drawing
    GETTER PICONSOLE_MEMBER_FUNC const ColorFormat& get_pixel(std::size_t x, std::size_t y) const
    {
        return this->get_buffer()[x + y * this->get_width()];
    }
    PICONSOLE_MEMBER_FUNC void set_pixel(ColorFormat color, std::size_t x, std::size_t y)
    {
        this->get_buffer()[x + y * this->get_width()] = color;
    }
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
    PICONSOLE_MEMBER_FUNC void text(std::string_view string, TextSettings settings = {}) = 0;
    PICONSOLE_MEMBER_FUNC void centered_text(std::string_view string, TextSettings settings = {}) = 0;

protected:
    #if _PICONSOLE_OS
    static inline buffer_type &get_buffer() { return *reinterpret_cast<buffer_type*>(&__piconsole_lcd_buffer); }
    #else
    static buffer_type &get_buffer();
    #endif
};

template <std::size_t TWidth, std::size_t THeight>
class ColorLCD_RGB565 : public ColorLCD<RGB565, TWidth, THeight>
{
public:
    using super = ColorLCD<RGB565, TWidth, THeight>; 
    using buffer_type = super::buffer_type;
    using ColorFormat = super::ColorFormat;
    using TextSettings = super::TextSettings;

    PICONSOLE_MEMBER_FUNC ~ColorLCD_RGB565() {}

    // Drawing
    PICONSOLE_MEMBER_FUNC void fill(ColorFormat color) override
    {
        if (dma_channel_is_busy(dma_channel))
        {
            dma_channel_abort(dma_channel); // We don't care about the last transfer when filling the whole screen
        }
        static ColorFormat fill_source_color{};
        fill_source_color = color;
        dma_channel_config config{ dma_channel_get_default_config(dma_channel) };
        channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
        channel_config_set_read_increment(&config, false);
        channel_config_set_write_increment(&config, true);
        wait_for_dma();
        dma_channel_configure(
            dma_channel,
            &config,
            this->get_buffer().data(),
            &fill_source_color.data,
            this->get_buffer().size(),
            true
        );
        wait_for_dma();
    }
    PICONSOLE_MEMBER_FUNC void line_horizontal(ColorFormat color, std::size_t x, std::size_t y, std::size_t width) override
    {
        static ColorFormat source_color{};
        source_color = color;
        dma_channel_config config{ dma_channel_get_default_config(dma_channel) };
        channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
        channel_config_set_read_increment(&config, false);
        channel_config_set_write_increment(&config, true);
        const std::size_t start_offset{ x + y * this->get_width() };
        wait_for_dma();
        dma_channel_configure(
            dma_channel,
            &config,
            this->get_buffer().data() + start_offset,
            &source_color.data,
            width,
            true
        );
    }
    PICONSOLE_MEMBER_FUNC void line_vertical(ColorFormat color, std::size_t x, std::size_t y, std::size_t height) override
    {
        const std::size_t end_y{ y + height };
        while (y < end_y)
        {
            this->set_pixel(color, x, y);
            ++y;
        }
    }
    PICONSOLE_MEMBER_FUNC void line(ColorFormat color, std::size_t start_x, std::size_t start_y, std::size_t end_x, std::size_t end_y) override
    {
        // TODO
    }
    PICONSOLE_MEMBER_FUNC void text(std::string_view string, TextSettings settings = {}) override
    {
        if (settings.background.has_value())
        {
            this->filled_rectangle(settings.background.value(), settings.x, settings.y,
                settings.padding_x * 2u + get_string_width(string),
                settings.padding_y.value_or(settings.padding_x) * 2u + get_string_height(string));
        }
        const auto& typeface{ get_ascii_typeface() };
        using Typeface = std::remove_cvref_t<decltype(typeface)>;
        const std::uint32_t character_width{ get_typeface_character_width<Typeface>() + 1u };
        const std::uint32_t character_height{ get_typeface_character_height<Typeface>() + 1u };
        while (!string.empty() && settings.y < settings.end_y)
        {
            const std::uint32_t max_line_width_px{ settings.end_x - settings.x };
            const std::size_t newline_index{ string.find('\n') };
            const std::uint32_t line_width_characters{
                std::min<std::uint32_t>(
                    std::min<std::uint32_t>(max_line_width_px / character_width, string.length()),
                    newline_index
                )
            };
            const bool has_newline{ newline_index != std::string_view::npos };
            const std::uint32_t line_width_px{ std::min<std::uint32_t>(line_width_characters * character_width, max_line_width_px) };
            std::string_view line{ string.substr(0, line_width_characters) };
            gfx::text::print_string<super>(*this, line, typeface, settings);
            if (settings.wrap_mode == TextSettings::WrapMode::Clip && !has_newline)
            {
                return;
            }
            string = string.substr(line_width_characters + (has_newline ? 1u : 0u));
            settings.y += character_height;
            settings.x = settings.wrap_x;
        }
    }
    PICONSOLE_MEMBER_FUNC void centered_text(std::string_view string, TextSettings settings = {}) override
    {
        // TODO: Add background based on widest line
        const auto& typeface{ get_ascii_typeface() };
        using Typeface = std::remove_cvref_t<decltype(typeface)>;
        const std::uint32_t character_width{ get_typeface_character_width<Typeface>() + 1u };
        const std::uint32_t character_height{ get_typeface_character_height<Typeface>() + 1u };
        while (!string.empty() && settings.y < settings.end_y)
        {
            const std::uint32_t max_line_width_px{ (settings.end_x - settings.x) * 2u };
            const std::size_t newline_index{ string.find('\n') };
            const std::uint32_t line_width_characters{
                std::min<std::uint32_t>(
                    std::min<std::uint32_t>(max_line_width_px / character_width, string.length()),
                    newline_index
                )
            };
            const bool has_newline{ newline_index != std::string_view::npos };
            const std::uint32_t line_width_px{ std::min<std::uint32_t>(line_width_characters * character_width, max_line_width_px) };
            TextSettings line_settings{ settings };
            line_settings.x -= line_width_px / 2u;
            std::string_view line{ string.substr(0, line_width_characters) };
            gfx::text::print_string<super>(*this, line, typeface, line_settings);
            if (settings.wrap_mode == TextSettings::WrapMode::Clip && !has_newline)
            {
                return;
            }
            string = string.substr(line_width_characters + (has_newline ? 1u : 0u));
            settings.y += character_height;
            settings.x = settings.wrap_x;
        }
    }

    PICONSOLE_MEMBER_FUNC void wait_for_dma() const
    {
        if (dma_channel != -1)
        {
            dma_channel_wait_for_finish_blocking(dma_channel);
        }
    }

protected:
    int dma_channel{ -1 };
};

class PicoLCD_1_8 : public ColorLCD_RGB565<160, 128>
{
public:
    using ColorLCD_RGB565::buffer_size;
    using ColorLCD_RGB565::buffer_type;
    using ColorLCD_RGB565::ColorFormat;

    PICONSOLE_MEMBER_FUNC bool init(bool final_step = true);
    PICONSOLE_MEMBER_FUNC bool uninit();
    PICONSOLE_MEMBER_FUNC ~PicoLCD_1_8();

    PICONSOLE_MEMBER_FUNC void show() override;
};
