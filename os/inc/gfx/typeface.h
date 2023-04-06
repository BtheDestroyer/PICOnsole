#pragma once
#include <algorithm>
#include <array>
#include <bitset>
#include <concepts>
#include <cstdint>

template<typename>
struct bitset_size;
template<std::size_t N>
struct bitset_size<std::bitset<N>> : std::integral_constant<std::size_t, N>
{};

template <std::size_t THeight, std::size_t TWidth = THeight>
using TextCharacter = std::array<std::bitset<TWidth>, THeight>;

template <typename T>
concept textcharacter_t = requires (T x) {
    { TextCharacter(x) } -> std::same_as<T>;
};

template <std::size_t THeight, std::size_t TWidth = THeight, std::size_t TCharacterCount = 128>
using Typeface = std::array<TextCharacter<THeight, TWidth>, TCharacterCount>;

template <typename T>
concept typeface_t = requires (T x) {
    { Typeface(x) } -> std::same_as<std::remove_cvref_t<T>>;
};

template <textcharacter_t TCharacter>
[[nodiscard]] consteval std::uint32_t get_character_width() { return bitset_size<typename TCharacter::value_type>::value; }

template <textcharacter_t TCharacter>
[[nodiscard]] consteval std::uint32_t get_character_height() { return std::tuple_size<TCharacter>(); }

template <typeface_t TTypeface>
[[nodiscard]] consteval std::uint32_t get_typeface_character_width() { return get_character_width<typename TTypeface::value_type>(); }

template <typeface_t TTypeface>
[[nodiscard]] consteval std::uint32_t get_typeface_character_height() { return get_character_height<typename TTypeface::value_type>(); }

template <typeface_t TTypeface>
[[nodiscard]] constexpr bool is_character_printable(char character, const TTypeface& typeface)
{
    const std::size_t index{ static_cast<std::size_t>(character) };
    if (index >= typeface.size())
    {
        return false;
    }
    const auto typeface_character{ typeface[index] };
    const auto end{ typeface_character.end() };
    return std::find_if(
            typeface_character.begin(), end,
            [](const auto& character_row) { return character_row.any(); }
        ) != end;
}

template <std::size_t THeight = 5, std::size_t TWidth = THeight>
const Typeface<THeight, TWidth>& get_ascii_typeface();

template <typeface_t TTypeface = std::remove_cvref_t<decltype(get_ascii_typeface())>>
[[nodiscard]] constexpr std::uint32_t get_string_width(std::string_view string)
{
    std::size_t longest_line_length{ 0u };
    std::for_each(string.begin(), string.end(),
        [&longest_line_length, current_line_length=longest_line_length](char c) mutable {
            if (c != '\n')
            {
                ++current_line_length;
                if (current_line_length > longest_line_length)
                {
                    longest_line_length = current_line_length;
                }
            }
            else
            {
                current_line_length = 0u;
            }
        });
    if (longest_line_length == 0)
    {
        return 0;
    }
    return longest_line_length * get_typeface_character_width<TTypeface>()
        + (longest_line_length - 1); // Account for 1 pixel of padding after each character but the last
}

template <typeface_t TTypeface>
std::size_t get_string_width(std::string_view string, const TTypeface&)
{
    return get_string_width<TTypeface>(string);
}

template <typeface_t TTypeface = std::remove_cvref_t<decltype(get_ascii_typeface())>>
std::size_t get_string_height(std::string_view string)
{
    const std::size_t line_count{
        std::count_if(string.begin(), string.end(),
            [](char c) { return c == '\n'; }
        ) + 1u
    };
    return line_count * (get_typeface_character_height<TTypeface>() + (line_count > 1u ? 1u : 0u));
}

template <typeface_t TTypeface>
std::size_t get_string_height(std::string_view string, const TTypeface&)
{
    return get_string_height<TTypeface>(string);
}
