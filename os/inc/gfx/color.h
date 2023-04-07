#pragma once
#include <algorithm>
#include <cstdint>
#include <concepts>
#include "PICOnsole_defines.h"

inline consteval std::uint8_t operator ""_b(unsigned long long v)
{
    return static_cast<std::uint8_t>(v);
}

class ColorFormat {};

template <typename T>
concept colorformat_t = std::derived_from<T, ColorFormat>;

namespace color
{
    template <colorformat_t TColorFormat>
    inline constexpr TColorFormat black() { return TColorFormat{static_cast<std::uint8_t>(0x00)}; }
    template <colorformat_t TColorFormat>
    inline constexpr TColorFormat dark_grey() { return TColorFormat{static_cast<std::uint8_t>(0x44)}; }
    template <colorformat_t TColorFormat>
    inline constexpr TColorFormat grey() { return TColorFormat{static_cast<std::uint8_t>(0x88)}; }
    template <colorformat_t TColorFormat>
    inline constexpr TColorFormat light_grey() { return TColorFormat{static_cast<std::uint8_t>(0xCC)}; }
    template <colorformat_t TColorFormat>
    inline constexpr TColorFormat white() { return TColorFormat{static_cast<std::uint8_t>(0xFF)}; }

    template <colorformat_t TColorFormat>
    inline constexpr TColorFormat red() { return TColorFormat{0xFF, 0x00, 0x00}; }
    template <colorformat_t TColorFormat>
    inline constexpr TColorFormat green() { return TColorFormat{0x00, 0xFF, 0x00}; }
    template <colorformat_t TColorFormat>
    inline constexpr TColorFormat blue() { return TColorFormat{0x00, 0x00, 0xFF}; }

    template <colorformat_t TColorFormat>
    inline constexpr TColorFormat yellow() { return TColorFormat{0xFF, 0xFF, 0x00}; }
    template <colorformat_t TColorFormat>
    inline constexpr TColorFormat cyan() { return TColorFormat{0x00, 0xFF, 0xFF}; }
    template <colorformat_t TColorFormat>
    inline constexpr TColorFormat magenta() { return TColorFormat{0xFF, 0x00, 0xFF}; }
}

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
