// -*- mode: c++; -*-

#include <defs.hh>

#include <cmdline/cmdline.hh>
#include <log/log.hh>
#include <osapi/osapi.hh>
#include <osapi/osregistry.hh>
#include <shared/types.hh>

#include <filesystem>
#include <mutex>
namespace fs = std::filesystem;

#include <boost/property_tree/ini_parser.hpp>

namespace fs2 {

using registry_type = pt::ptree;
registry_type global_registry;

namespace registry {

namespace {

fs::path
registry_path () {
    static fs::path path;

    static std::once_flag init_flag;
    std::call_once (init_flag, [&]() {
        path = fs2::os::get_config_path () /= "freespace2.ini";
    });

    ASSERT (!path.empty ());

    return path;
}

void do_init () {
    const auto path = registry_path ();
    const auto parent_path = path.parent_path ();

    if (!fs::exists (parent_path))
        fs::create_directories (parent_path);

    if (!fs::exists (path))
        std::ofstream (path.c_str ());

    pt::read_ini (std::string (registry_path ()), global_registry);
}

void init () {
    static std::once_flag init_flag;
    std::call_once (init_flag, do_init);
}

} // anonymous

std::string
read (const std::string& key, const std::string& default_value) {
    init ();
    return global_registry.get (key, default_value);
}

int
read (const std::string& key, int default_value) {
    init ();
    return global_registry.get (key, default_value);
}

float
read (const std::string& key, float default_value) {
    init ();
    return global_registry.get (key, default_value);
}

void
write (const std::string& key, const std::string& value) {
    init ();
    global_registry.put (key, value);
    pt::write_ini (registry_path (), global_registry);
}

void
write (const std::string& key, int value) {
    init ();
    global_registry.put (key, value);
    pt::write_ini (registry_path (), global_registry);
}

void
write (const std::string& key, float value) {
    init ();
    global_registry.put (key, value);
    pt::write_ini (registry_path (), global_registry);
}

}}
