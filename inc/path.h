#pragma once
#include <string>
#include <string_view>
#include <utility>

namespace path
{
    std::pair<const std::string_view, const std::string_view> split(const std::string_view path);
    const std::string_view base_name(const std::string_view path);
    const std::string_view dir_name(const std::string_view path);
    std::string join(const std::string_view a, const std::string_view b);
    bool exists(const std::string_view path);
    bool is_file(const std::string_view path);
    bool is_dir(const std::string_view path);
}
