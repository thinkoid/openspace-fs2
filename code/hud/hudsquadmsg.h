/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef _HUD_SQUADMSG
#define _HUD_SQUADMSG

// defines for messages that can be sent from the player.  Defined at bitfields so that we can enable
// and disable messages on a message by message basis
#define ATTACK_TARGET_ITEM		(1<<0)
#define DISABLE_TARGET_ITEM	(1<<1)
#define DISARM_TARGET_ITEM		(1<<2)
#define PROTECT_TARGET_ITEM	(1<<3)
#define IGNORE_TARGET_ITEM		(1<<4)
#define FORMATION_ITEM			(1<<5)
#define COVER_ME_ITEM			(1<<6)
#define ENGAGE_ENEMY_ITEM		(1<<7)
#define CAPTURE_TARGET_ITEM	(1<<8)

// the next are for the support ship only
#define REARM_REPAIR_ME_ITEM		(1<<9)
#define ABORT_REARM_REPAIR_ITEM	(1<<10)
#define STAY_NEAR_ME_ITEM			(1<<11)
#define STAY_NEAR_TARGET_ITEM		(1<<12)
#define KEEP_SAFE_DIST_ITEM		(1<<13)

// next item for all ships again -- to try to preserve relative order within the message menu
#define DEPART_ITEM					(1<<14)
#define DISABLE_SUBSYSTEM_ITEM	(1<<15)

#define MAX_SHIP_ORDERS				13			// Must sync correctly with Comm_orders array in HUDsquadmsg.cpp

// following defines are the set of possible commands that can be given to a ship.  A mission designer
// might not allow some messages

#define FIGHTER_MESSAGES	(ATTACK_TARGET_ITEM | DISABLE_TARGET_ITEM | DISARM_TARGET_ITEM | PROTECT_TARGET_ITEM | IGNORE_TARGET_ITEM | FORMATION_ITEM | COVER_ME_ITEM | ENGAGE_ENEMY_ITEM | DEPART_ITEM | DISABLE_SUBSYSTEM_ITEM)

#define BOMBER_MESSAGES		FIGHTER_MESSAGES			// bombers can do the same things as fighters

#define TRANSPORT_MESSAGES	(ATTACK_TARGET_ITEM | CAPTURE_TARGET_ITEM | DEPART_ITEM )
#define FREIGHTER_MESSAGES	TRANSPORT_MESSAGES		// freighters can do the same things as transports

#define CRUISER_MESSAGES	(ATTACK_TARGET_ITEM | DEPART_ITEM)

#define CAPITAL_MESSAGES	(DEPART_ITEM)				// can't order capitals to do much!!!!

#define SUPPORT_MESSAGES	(REARM_REPAIR_ME_ITEM | ABORT_REARM_REPAIR_ITEM | STAY_NEAR_ME_ITEM | STAY_NEAR_TARGET_ITEM | KEEP_SAFE_DIST_ITEM | DEPART_ITEM )

// these messages require an active target.  They are also the set of messages
// which cannot be given to a ship when the target is on the same team, or the target
// is not a ship.
#define ENEMY_TARGET_MESSAGES		(ATTACK_TARGET_ITEM | DISABLE_TARGET_ITEM | DISARM_TARGET_ITEM | IGNORE_TARGET_ITEM | STAY_NEAR_TARGET_ITEM | CAPTURE_TARGET_ITEM | DISABLE_SUBSYSTEM_ITEM )
#define FRIENDLY_TARGET_MESSAGES	(PROTECT_TARGET_ITEM)

#define TARGET_MESSAGES	(ENEMY_TARGET_MESSAGES | FRIENDLY_TARGET_MESSAGES)


#define SQUADMSG_HISTORY_MAX 160

typedef struct {
	int ship;  // ship that received the order
	int order;  // order that the ship received (see defines above)
	int target;  // ship that is the target of the order 
} squadmsg_history;

extern int squadmsg_history_index;
extern squadmsg_history Squadmsg_history[SQUADMSG_HISTORY_MAX];

extern void hud_init_squadmsg();
extern void hud_squadmsg_toggle();						// toggles the state of messaging mode
extern void hud_squadmsg_shortcut( int command );	// use of a shortcut key
extern int hud_squadmsg_hotkey_select( int k );	// a hotkey was hit -- maybe send a message to those ship(s)
extern void hud_squadmsg_save_keys( int do_scroll = 0 );					// saves into local area keys which need to be saved/restored when in messaging mode
extern int hud_squadmsg_do_frame();
extern int hud_query_order_issued(char *name, char *order, char *target);
extern int hud_squadmsg_read_key( int k );			// called from high level keyboard code

extern void hud_squadmsg_repair_rearm( int toggle_state );
extern void hud_squadmsg_repair_rearm_abort( int toggle_state );
extern void hud_squadmsg_rearm_shortcut();

extern int hud_squadmsg_send_ship_command( int shipnum, int command, int send_message );
extern int hud_squadmsg_send_wing_command( int wingnum, int command, int send_message );
extern void hud_squadmsg_send_to_all_fighters( int command );
extern void hud_squadmsg_call_reinforcement(int reinforcement_num);

extern int hud_squadmsg_reinforcements_available(int team);

//#ifndef NDEBUG
void hud_enemymsg_toggle();						// debug function to allow messaging of enemies
//#endif

#endif
