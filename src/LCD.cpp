#include "LCD.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/binary_info.h"
#include "hardware/dma.h"
#include <stdlib.h>

SPILCD::SPILCD()
    : backlight_config{pwm_get_default_config()}
{
    baudrate = spi_init(get_spi(), 62'500'000u);
    bi_decl(bi_2pins_with_func(LCD_MOSI, LCD_SCK, GPIO_FUNC_SPI));
    gpio_set_function(LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MOSI, GPIO_FUNC_SPI);
    bi_decl(bi_1pin_with_name(LCD_BACKLIGHT, "LCD Backlight (PWM)"));
    gpio_set_function(LCD_BACKLIGHT, GPIO_FUNC_PWM);
    backlight_pwm_slice = pwm_gpio_to_slice_num(LCD_BACKLIGHT);
    pwm_config_set_clkdiv(&backlight_config, 4.0f);
    pwm_init(backlight_pwm_slice, &backlight_config, true);
    set_backlight_strength(0.1f);
    bi_decl(bi_1pin_with_name(LCD_CS, "LCD Chip Select"));
    gpio_init(LCD_CS);
    gpio_set_dir(LCD_CS, GPIO_OUT);
    bi_decl(bi_1pin_with_name(LCD_CS, "LCD Reset"));
    gpio_init(LCD_RST);
    gpio_set_dir(LCD_RST, GPIO_OUT);
    bi_decl(bi_1pin_with_name(LCD_CS, "LCD Data/Command"));
    gpio_init(LCD_DC);
    gpio_set_dir(LCD_DC, GPIO_OUT);
    reset();
}

SPILCD::~SPILCD()
{
    set_backlight_strength(0.0f);
}

[[nodiscard]] constexpr std::size_t SPILCD::get_bit_depth() const
{
    return get_bytes_per_pixel() * 8;
}

void SPILCD::write_data(std::uint8_t byte)
{
    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, LCD_CS_ACTIVE_POLARITY);
    spi_write_blocking(get_spi(), &byte, 1);
    gpio_put(LCD_CS, !LCD_CS_ACTIVE_POLARITY);
}

void SPILCD::write_data(std::span<const std::uint8_t> bytes)
{
    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, LCD_CS_ACTIVE_POLARITY);
    spi_write_blocking(get_spi(), bytes.data(), bytes.size());
    gpio_put(LCD_CS, !LCD_CS_ACTIVE_POLARITY);
}

void SPILCD::write_command(std::uint8_t byte)
{
    gpio_put(LCD_DC, 0);
    gpio_put(LCD_CS, LCD_CS_ACTIVE_POLARITY);
    spi_write_blocking(get_spi(), &byte, 1);
    gpio_put(LCD_CS, !LCD_CS_ACTIVE_POLARITY);
}

void SPILCD::write_command(std::span<const std::uint8_t> bytes)
{
    gpio_put(LCD_DC, 0);
    gpio_put(LCD_CS, LCD_CS_ACTIVE_POLARITY);
    spi_write_blocking(get_spi(), bytes.data(), bytes.size());
    gpio_put(LCD_CS, !LCD_CS_ACTIVE_POLARITY);
}

void SPILCD::reset()
{
    gpio_put(LCD_CS, !LCD_CS_ACTIVE_POLARITY);
    gpio_put(LCD_DC, 0);
    gpio_put(LCD_RST, 1);
    sleep_ms(20);
    gpio_put(LCD_RST, 0);
    sleep_ms(20);
    gpio_put(LCD_RST, 1);
}

void SPILCD::set_backlight_strength(float strength)
{
    backlight_strength = std::clamp(strength, 0.0f, 1.0f);
    const std::uint16_t level{ static_cast<std::uint16_t>(strength * 65535.0f) };
    pwm_set_gpio_level(LCD_BACKLIGHT, level);
}

PicoLCD_1_8::PicoLCD_1_8()
{
    //MX, MY, RGB mode (???)
    write_command(0x36);
    // Horizontal mode
    write_data(0x70);
    //write_data(0x00); // Vertical mode
    
    // ???
    write_command(0x3A);
    write_data(0x05);

    // ST7735R Frame Rate (???)
    write_command(0xB1);
    write_data({0x01, 0x2C, 0x2D});

    // ???
    write_command(0xB2);
    write_data({0x01, 0x2C, 0x2D});

    // ???
    write_command(0xB3);
    write_data({0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D});

    // Column inversion
    write_command(0xB4);
    write_data(0x07);

    // ST7735R Power Sequence (???)
    write_command(0xC0);
    write_data({0xA2, 0x02, 0x84});
    // ???
    write_command(0xC1);
    write_data(0xC5);

    // ???
    write_command(0xC2);
    write_data({0x0A, 0x00});

    // ???
    write_command(0xC3);
    write_data({0x8A, 0x2A});
    // ???
    write_command(0xC4);
    write_data({0x8A, 0xEE});

    // VCOM (???)
    write_command(0xC5);
    write_data(0x0E);

    // ST7735R Gamma Sequence (???)
    write_command(0xe0);
    write_data({0x0f, 0x1a, 0x0f, 0x18, 0x2f, 0x28, 0x20, 0x22, 0x1f, 0x1b, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10});

    // ???
    write_command(0xe1);
    write_data({0x0f, 0x1b, 0x0f, 0x17, 0x33, 0x2c, 0x29, 0x2e, 0x30, 0x30, 0x39, 0x3f, 0x00, 0x07, 0x03, 0x10});

    // Enable test command (???)
    write_command(0xF0);
    write_data(0x01);

    // Disable ram power save mode
    write_command(0xF6);
    write_data(0x00);

    // Sleep out (???)
    write_command(0x11);
    sleep_ms(120);

    // Turn on the LCD display
    write_command(0x29);
    
    dma_channel = dma_claim_unused_channel(true);
    fill(color::black<ColorFormat>());
    show();
}

PicoLCD_1_8::~PicoLCD_1_8()
{
    dma_channel_unclaim(dma_channel);
}

void PicoLCD_1_8::show()
{
    // ???
    write_command(0x2A);
    write_data({0x00, 0x01, 0x00, 0xA0});
    
    // ???
    write_command(0x2B);
    write_data({ 0x00, 0x02, 0x00, 0x81 });
    
    // ???
    // - Maybe tell the LCD it's about to get pixel data?
    write_command(0x2C);
    
    dma_channel_wait_for_finish_blocking(dma_channel);
    write_data(std::span<std::uint8_t>{
        reinterpret_cast<std::uint8_t*>(buffer.data()),
        buffer.size() * get_bytes_per_pixel()
        });
}

void PicoLCD_1_8::fill(ColorFormat color)
{
    dma_channel_abort(dma_channel); // We don't care about the last transfer when filling the whole screen
    static ColorFormat source_color{};
    source_color = color;
    dma_channel_config config{ dma_channel_get_default_config(dma_channel) };
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    dma_channel_configure(
        dma_channel,
        &config,
        buffer.data(),
        &source_color.data,
        buffer.size(),
        true
    );
}

void PicoLCD_1_8::line_horizontal(ColorFormat color, std::size_t x, std::size_t y, std::size_t width)
{
    dma_channel_wait_for_finish_blocking(dma_channel);
    static ColorFormat source_color{};
    source_color = color;
    dma_channel_config config{ dma_channel_get_default_config(dma_channel) };
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    const std::size_t start_offset{ x + y * get_width() };
    dma_channel_configure(
        dma_channel,
        &config,
        buffer.data() + start_offset,
        &source_color.data,
        width,
        true
    );
}

void PicoLCD_1_8::line_vertical(ColorFormat color, std::size_t x, std::size_t y, std::size_t height)
{
    const std::size_t end_y{ y + height };
    while (y < end_y)
    {
        set_pixel(color, x, y);
        ++y;
    }
}

void PicoLCD_1_8::line(ColorFormat color, std::size_t start_x, std::size_t start_y, std::size_t end_x, std::size_t end_y)
{
    // TODO
}

void PicoLCD_1_8::wait_for_dma() const
{
    dma_channel_wait_for_finish_blocking(dma_channel);
}
