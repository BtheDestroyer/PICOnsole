#pragma once
#include <array>
#include <bitset>
#include <concepts>
#include <string_view>
#include "interfaces/LCD.h"
#include "gfx/typeface.h"

#ifdef LCD_MODEL
template <std::size_t TWidth>
void print_character_row(LCD_MODEL& lcd, const std::bitset<TWidth>& character_row,
    std::uint32_t lcd_x, std::uint32_t lcd_y,
    const LCD_MODEL::ColorFormat& color = color::white<LCD_MODEL::ColorFormat>())
{
    for (std::size_t i{ 0 }, b{ TWidth - 1}; i < TWidth; ++i, --b)
    {
        if (character_row.test(b))
        {
            lcd.set_pixel(color, lcd_x, lcd_y);
        }
        ++lcd_x;
    }
}

template <std::size_t THeight, std::size_t TWidth = THeight>
void print_character(LCD_MODEL& lcd, const TextCharacter<THeight, TWidth>& character,
    std::uint32_t lcd_x, std::uint32_t lcd_y,
    const LCD_MODEL::ColorFormat& color = color::white<LCD_MODEL::ColorFormat>())
{
    for (const auto& character_row : character)
    {
        print_character_row(lcd, character_row, lcd_x, lcd_y, color);
        ++lcd_y;
    }
}

template <typeface_t TTypeface>
void print_character(LCD_MODEL& lcd, char character,
    std::uint32_t lcd_x, std::uint32_t lcd_y,
    const LCD_MODEL::ColorFormat& color = color::white<LCD_MODEL::ColorFormat>(), const TTypeface& typeface = get_ascii_typeface())
{
    if (is_character_printable(character, typeface))
    {
        print_character(lcd, typeface[static_cast<std::size_t>(character)], lcd_x, lcd_y, color);
    }
}

template <typeface_t TTypeface = std::remove_cvref_t<decltype(get_ascii_typeface())>>
void print_string(LCD_MODEL& lcd, std::string_view string,
    std::uint32_t lcd_x, std::uint32_t lcd_y,
    const LCD_MODEL::ColorFormat& color = color::white<LCD_MODEL::ColorFormat>(), const TTypeface& typeface = get_ascii_typeface())
{
    const std::uint32_t lcd_start_x{ lcd_x };
    const std::uint32_t character_width{ get_typeface_character_width<TTypeface>() + 1 };
    const std::uint32_t character_height{ get_typeface_character_height<TTypeface>() + 1 };
    for (char character : string)
    {
        switch (character)
        {
            case '\n':
                lcd_x = lcd_start_x;
                lcd_y += character_height;
                break;
            case '\t':
                lcd_x += character_width * 4;
                break;
            default:
                print_character(lcd, typeface[static_cast<std::size_t>(character)], lcd_x, lcd_y, color);
                lcd_x += character_width;
                break;
        }
    }
}
#endif
