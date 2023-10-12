// -*- mode: c++; -*-

#include <defs.hh>
#include <assert/assert.hh>
#include <log/log.hh>

/**
 * @file
 * The code in here is just for bookeeping, allocating AI slots and linking
 * them to ships. See AiCode.cpp for the actual AI code.
 */

#include <ai/ai.hh>
#include <object/object.hh>
#include <ship/ship.hh>

int Total_goal_target_names = 0;
char Goal_target_names[MAX_GOAL_TARGET_NAMES][NAME_LENGTH];
ai_info Ai_info[MAX_AI_INFO];
ai_info* Player_ai;

/**
 * @brief Returns index of free AI slot.
 * @return Return -1 if no free slot.
 */
int ai_get_slot (int shipnum) {
    int i;

    for (i = 0; i < MAX_AI_INFO; i++)
        if (Ai_info[i].shipnum == -1) {
            Ai_info[i].shipnum = shipnum;
            return i;
        }

    WARNINGF (LOCATION, "Couldn't get AI slot");
    ASSERT (0);

    return -1;
}

/**
 * @brief Frees a currently used AI slot.
 * @details Only modifies in ::Ai_info struct. Does not modify hook in ship.
 */
void ai_free_slot (int ai_index) {
    ASSERT ((ai_index >= 0) && (ai_index < MAX_AI_INFO));

    Ai_info[ai_index].shipnum = -1;
}

int get_wingnum (int objnum) {
    int shipnum, ai_index;

    shipnum = Objects[objnum].instance;

    ai_index = Ships[shipnum].ai_index;

    return Ai_info[ai_index].wing;
}

void set_wingnum (int objnum, int wingnum) {
    int shipnum, ai_index;

    ASSERT (Objects[objnum].type == OBJ_SHIP);

    shipnum = Objects[objnum].instance;

    ASSERT ((shipnum >= 0) && (shipnum < MAX_SHIPS));

    ai_index = Ships[shipnum].ai_index;

    ASSERT ((ai_index >= 0) && (ai_index < MAX_AI_INFO));

    Ai_info[ai_index].wing = wingnum;
}

char* ai_get_goal_target_name (const char* name, int* index) {
    ASSERT (name != NULL);
    ASSERT (index != NULL);
    int i;

    for (i = 0; i < Total_goal_target_names; i++)
        if (!strcasecmp (name, Goal_target_names[i])) {
            *index = i;
            return Goal_target_names[i];
        }

    ASSERT (Total_goal_target_names < MAX_GOAL_TARGET_NAMES);
    ASSERTX (
        strlen (name) <= NAME_LENGTH - 1,
        "Goal target name %s is too long. Needs to be 31 characters or less.",
        name);
    i = Total_goal_target_names++;
    strcpy (Goal_target_names[i], name);
    *index = i;
    return Goal_target_names[i];
}
