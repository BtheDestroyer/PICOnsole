#include "path.h"

namespace path
{
    std::pair<const std::string_view, const std::string_view> split(const std::string_view path)
    {
        const std::size_t last_slash{ path.rfind('/') };
        if (last_slash == std::string_view::npos)
        {
            return std::make_pair(path, std::string_view{});
        }
        return std::make_pair(path.substr(0, last_slash), path.substr(last_slash + 1));
    }

    const std::string_view base_name(const std::string_view path)
    {
        return split(path).second;
    }

    const std::string_view dir_name(const std::string_view path)
    {
        return split(path).first;
    }

    std::string join(const std::string_view a, const std::string_view b)
    {
        std::string path{a};
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

    // TODO: after SD card setup
    bool exists(const std::string_view path);
    bool is_file(const std::string_view path);
    bool is_dir(const std::string_view path);
}
