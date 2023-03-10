#pragma once
#include "memory.h"
#include <string_view>
#include <utility>
#include <string>

class path
{
private:
    path();

public:
    static std::pair<const std::string_view, const std::string_view> split(const std::string_view path)
    {
        const std::size_t last_slash{ path.rfind('/') };
        if (last_slash == std::string_view::npos)
        {
            return std::make_pair(path, std::string_view{});
        }
        return std::make_pair(path.substr(0, last_slash), path.substr(last_slash + 1));
    }

    static const std::string_view dir_name(const std::string_view path) { return split(path).first; }
    static const std::string_view base_name(const std::string_view path) { return split(path).second; }

    static std::string join(const std::string_view a, const std::string_view b)
    {
        std::string path{ a };
        if (!a.ends_with('/'))
        {
            path += '/';
        }
        if (b.starts_with('/'))
        {
            path += b.substr(1);
        }
        else
        {
            path += b;
        }
        return path;
    }
};
