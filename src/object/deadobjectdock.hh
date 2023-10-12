// -*- mode: c++; -*-

#ifndef FREESPACE2_OBJECT_DEADOBJECTDOCK_HH
#define FREESPACE2_OBJECT_DEADOBJECTDOCK_HH

#include <defs.hh>

#include <object/objectdock.hh>

// get the first object in objp's dock list
object* dock_get_first_dead_docked_object (object* objp);

// find objp's dockpoint being occupied by other_objp
int dock_find_dead_dockpoint_used_by_object (object* objp, object* other_objp);

// add objp1 and objp2 to each others' dock lists; currently only called by
// ai_deathroll_start
void dock_dead_dock_objects (
    object* objp1, int dockpoint1, object* objp2, int dockpoint2);

// remove objp1 and objp2 from each others' dock lists; currently called by
// do_dying_undock_physics and ship_cleanup
void dock_dead_undock_objects (object* objp1, object* objp2);

/**
 * @brief Undocks all dead docks.
 * @note This is a slow method. Use dock_free_dead_dock_list() when doing
 * object cleanup
 */
void dock_dead_undock_all (object* objp);

// free the entire dock list without undocking anything; should only be used on
// object cleanup
void dock_free_dead_dock_list (object* objp);

#endif // FREESPACE2_OBJECT_DEADOBJECTDOCK_HH
