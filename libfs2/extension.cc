// The FS2 shim: the one translation unit that includes both a Godot header
// and fs2.hh. It registers class FS2 with ClassDB and forwards its calls to
// the held fs2_t, marshalling to Variant types -- pure translation, no
// logic. GDScript reads `var sim := FS2.new()`; the sim side never learns
// Godot exists.

#include "fs2.hh"

#include <gdextension_interface.h>

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

class FS2 : public godot::RefCounted {
    GDCLASS(FS2, godot::RefCounted)

    fs2_t m_sim;

protected:
    static void _bind_methods()
    {
        godot::ClassDB::bind_method(godot::D_METHOD("version"), &FS2::version);
    }

public:
    godot::String version() const { return m_sim.version(); }
};

static void
initialize_fs2_module(godot::ModuleInitializationLevel level)
{
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE)
        return;

    GDREGISTER_CLASS(FS2);
}

static void
uninitialize_fs2_module(godot::ModuleInitializationLevel level)
{
}

extern "C" GDExtensionBool GDE_EXPORT
fs2_library_init(GDExtensionInterfaceGetProcAddress get_proc_address,
                 GDExtensionClassLibraryPtr library,
                 GDExtensionInitialization *initialization)
{
    godot::GDExtensionBinding::InitObject init_obj(get_proc_address, library,
                                                   initialization);

    init_obj.register_initializer(initialize_fs2_module);
    init_obj.register_terminator(uninitialize_fs2_module);
    init_obj.set_minimum_library_initialization_level(
        godot::MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
