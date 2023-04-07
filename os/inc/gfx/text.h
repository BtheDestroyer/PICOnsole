#pragma once
#include <array>
#include <bitset>
#include <concepts>
#include <optional>
#include <string_view>
#include "gfx/color.h"
#include "gfx/typeface.h"

namespace gfx::text
{
template <typename TLCD>
struct PrintSettings
{
    enum class WrapMode {
        Clip,
        Wrap
    } wrap_mode{ WrapMode::Clip };
    std::uint32_t x{ 0u };
    std::uint32_t y{ 0u };
    std::uint32_t end_x{ TLCD::width };
    std::uint32_t end_y{ TLCD::height };
    std::uint32_t wrap_x{ x };
    std::uint32_t padding_x{ 0u };
    std::optional<std::uint32_t> padding_y{ std::nullopt };
    typename TLCD::ColorFormat color{ color::white<typename TLCD::ColorFormat>() };
    std::optional<typename TLCD::ColorFormat> background{ std::nullopt };
};

template <typename TLCD, std::size_t TWidth>
PICONSOLE_FUNC void print_character_row(TLCD& lcd, const std::bitset<TWidth>& character_row,
    const PrintSettings<std::remove_cvref_t<TLCD>>& settings = {})
{
    std::uint32_t x{ settings.x };
    for (std::size_t i{ 0 }, b{ TWidth - 1}; i < TWidth; ++i, --b)
    {
        if (character_row.test(b))
        {
            lcd.set_pixel(settings.color, x, settings.y);
        }
        ++x;
        if (x > settings.end_x - settings.padding_x * 2u)
        {
            switch (settings.wrap_mode)
            {
            case PrintSettings<std::remove_cvref_t<TLCD>>::WrapMode::Clip:
                return;
            case PrintSettings<std::remove_cvref_t<TLCD>>::WrapMode::Wrap:
                x = settings.wrap_x;
                break;
            }
        }
    }
}

template <typename TLCD, std::size_t THeight, std::size_t TWidth = THeight>
PICONSOLE_FUNC void print_character(TLCD& lcd, const TextCharacter<THeight, TWidth>& character,
    PrintSettings<std::remove_cvref_t<TLCD>> settings = {})
{
    for (const auto& character_row : character)
    {
        print_character_row(lcd, character_row, settings);
        ++settings.y;
        if (settings.y > settings.end_y - settings.padding_y.value_or(0u) * 2u)
        {
            return;
        }
    }
}

template <typename TLCD, typeface_t TTypeface>
PICONSOLE_FUNC void print_character(TLCD& lcd, char character, const TTypeface& typeface = get_ascii_typeface(),
    const PrintSettings<std::remove_cvref_t<TLCD>>& settings = {})
{
    if (is_character_printable(character, typeface))
    {
        print_character(lcd, typeface[static_cast<std::size_t>(character)], settings);
    }
}

template <typename TLCD, typeface_t TTypeface = std::remove_cvref_t<decltype(get_ascii_typeface())>>
PICONSOLE_FUNC void print_string(TLCD& lcd, std::string_view string, const TTypeface& typeface = get_ascii_typeface(),
    PrintSettings<std::remove_cvref_t<TLCD>> settings = {})
{
    const std::uint32_t character_width{ get_typeface_character_width<TTypeface>() + 1u };
    const std::uint32_t character_height{ get_typeface_character_height<TTypeface>() + 1u };
    const std::uint32_t wrapped_line_width{ (settings.end_x - settings.padding_x * 2u) - (settings.wrap_x + settings.padding_x) };
    for (char character : string)
    {
        switch (character)
        {
            case '\n':
                settings.x = settings.wrap_x;
                settings.y += character_height;
                break;
            case '\t':
                settings.x += character_width * 4;
                break;
            default:
                print_character(lcd, typeface[static_cast<std::size_t>(character)], settings);
                settings.x += character_width;
                break;
        }
        if (settings.x > settings.end_x)
        {
            settings.x -= wrapped_line_width;
        }
    }
}
}
