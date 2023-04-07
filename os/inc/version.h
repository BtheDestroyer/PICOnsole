#pragma once
#include <cstdint>
#include <string>
#include "PICOnsole_defines.h"

struct Version
{
    enum class ReleaseType : std::uint32_t {
        Release,
        PreRelease,
        Beta,
        Alpha,
        PreAlpha,
    } type{ ReleaseType::Release };

    std::uint32_t major{ 0 };
    std::uint32_t minor{ 0 };
    std::uint32_t revision{ 0 };
    
    GETTER PICONSOLE_FUNC std::string to_string() const;
};

PICONSOLE_FUNC [[nodiscard]] constexpr const char* to_string(Version::ReleaseType release_type);
PICONSOLE_FUNC [[nodiscard]] constexpr char to_char(Version::ReleaseType release_type);
