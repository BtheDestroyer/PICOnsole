#include "version.h"
#include <sstream>

[[nodiscard]] std::string Version::to_string() const
{
    std::stringstream stream;
    stream << ::to_string(type) << ' ' << major << '.' << minor << '.' << revision;
    return stream.str();
}

[[nodiscard]] constexpr const char* to_string(Version::ReleaseType release_type)
{
    switch (release_type)
    {
    case Version::ReleaseType::Release:
        return "Release";
    case Version::ReleaseType::PreRelease:
        return "PreRelease";
    case Version::ReleaseType::Beta:
        return "Beta";
    case Version::ReleaseType::Alpha:
        return "Alpha";
    case Version::ReleaseType::PreAlpha:
        return "PreAlpha"; 
    }
    return "???";
}

[[nodiscard]] constexpr char to_char(Version::ReleaseType release_type)
{
    return *to_string(release_type);
}
