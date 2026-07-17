/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

/*
 *  All states for game sequencing are defined in GameSequence.h.
 *  States should always be referred to using the macros.
*/

#include "freespace.h"
#include "gamesequence.h"

// local defines
#define MAX_GAMESEQ_EVENTS		20		// maximum number of events on the game sequencing queue
#define GS_STACK_SIZE			10		// maximum number of stacked states

// local variables
typedef struct state_stack {
	int	current_state;
	int	event_queue[MAX_GAMESEQ_EVENTS];
	int	queue_tail, queue_head;
} state_stack;

// DO NOT MAKE THIS NON-STATIC!!!!
LOCAL state_stack gs[GS_STACK_SIZE];
LOCAL int gs_current_stack = -1;						// index of top state on stack.

static int state_reentry = 0;  // set if we are already in state processing
static int state_processing_event_post = 0;  // set if we are already processing an event to switch states
static int state_in_event_processer = 0;

// Text of state, corresponding to #define values for GS_STATE_*
//XSTR:OFF
char *GS_event_text[] =
{
	"GS_EVENT_MAIN_MENU",
	"GS_EVENT_START_GAME",
	"GS_EVENT_ENTER_GAME",
	"GS_EVENT_START_GAME_QUICK",
	"GS_EVENT_END_GAME",									
	"GS_EVENT_QUIT_GAME",								// 5
	"GS_EVENT_PAUSE_GAME",
	"GS_EVENT_PREVIOUS_STATE",
	"GS_EVENT_OPTIONS_MENU",
	"GS_EVENT_BARRACKS_MENU",							
	"GS_EVENT_TRAINING_MENU",							// 10
	"GS_EVENT_TECH_MENU",
	"GS_EVENT_LOAD_MISSION_MENU",
	"GS_EVENT_SHIP_SELECTION",
	"GS_EVENT_TOGGLE_FULLSCREEN",						
	"GS_EVENT_WEAPON_SELECT_HELP",					// 15
	"GS_EVENT_START_BRIEFING",
	"GS_EVENT_DEBUG_PAUSE_GAME",
	"GS_EVENT_HUD_CONFIG",
	"GS_EVENT_MULTI_SETUP",								
	"GS_EVENT_MULTI_JOIN_GAME",						// 20
	"GS_EVENT_CONTROL_CONFIG",
	"GS_EVENT_EVENT_DEBUG",
	"GS_EVENT_MULTI_PROTO_CHOICE",
	"GS_EVENT_SAVE_RESTORE",							
	"GS_EVENT_CHOOSE_SAVE_OR_RESTORE",				// 25
	"GS_EVENT_WEAPON_SELECTION",
	"GS_EVENT_MISSION_LOG_SCROLLBACK",
	"GS_EVENT_MAIN_MENU_HELP",							
	"GS_EVENT_GAMEPLAY_HELP",
	"GS_EVENT_SHIP_SELECT_HELP",						// 30
	"GS_EVENT_DEATH_DIED",
	"GS_EVENT_DEATH_BLEW_UP",
	"GS_EVENT_NEW_CAMPAIGN",
	"GS_EVENT_CREDITS",
	"GS_EVENT_SHOW_GOALS",								// 35
	"GS_EVENT_HOTKEY_SCREEN",
	"GS_EVENT_HOTKEY_SCREEN_HELP",					
	"GS_EVENT_VIEW_MEDALS",
	"GS_EVENT_MULTI_HOST_SETUP",
	"GS_EVENT_MULTI_CLIENT_SETUP",					// 40
	"GS_EVENT_DEBRIEF",
	"GS_EVENT_NAVMAP",									
	"GS_EVENT_MULTI_JOIN_TRACKER",
	"GS_EVENT_GOTO_VIEW_CUTSCENES_SCREEN",
	"GS_EVENT_MULTI_STD_WAIT",							// 45
	"GS_EVENT_STANDALONE_MAIN",
	"GS_EVENT_MULTI_PAUSE",
	"GS_EVENT_BRIEFING_HELP",
	"GS_EVENT_TEAM_SELECT",
	"GS_EVENT_TRAINING_PAUSE",							// 50	
	"GS_EVENT_MULTI_HELP",								
	"GS_EVENT_INGAME_PRE_JOIN",
	"GS_EVENT_PLAYER_WARPOUT_START",
	"GS_EVENT_PLAYER_WARPOUT_START_FORCED",
	"GS_EVENT_PLAYER_WARPOUT_STOP",					// 55
	"GS_EVENT_PLAYER_WARPOUT_DONE_STAGE1",			
	"GS_EVENT_PLAYER_WARPOUT_DONE_STAGE2",
	"GS_EVENT_PLAYER_WARPOUT_DONE",
	"GS_EVENT_STANDALONE_POSTGAME",
	"GS_EVENT_INITIAL_PLAYER_SELECT",				// 60
	"GS_EVENT_GAME_INIT",								
	"GS_EVENT_MULTI_MISSION_SYNC",
	"GS_EVENT_MULTI_CAMPAIGN_SELECT",
	"GS_EVENT_MULTI_SERVER_TRANSFER",
	"GS_EVENT_MULTI_START_GAME",						// 65
	"GS_EVENT_MULTI_HOST_OPTIONS",					
	"GS_EVENT_MULTI_DOGFIGHT_DEBRIEF",
	"GS_EVENT_CAMPAIGN_ROOM",
	"GS_EVENT_CMD_BRIEF",
	"GS_EVENT_TOGGLE_GLIDE",							// 70
	"GS_EVENT_RED_ALERT",								
	"GS_EVENT_SIMULATOR_ROOM",
	"GS_EVENT_EMD_CAMPAIGN",	
};
//XSTR:ON

// Text of state, corresponding to #define values for GS_STATE_*
//XSTR:OFF
char *GS_state_text[] =
{
	"NOT A VALID STATE",
	"GS_STATE_MAIN_MENU",								// 1
	"GS_STATE_GAME_PLAY",
	"GS_STATE_GAME_PAUSED",
	"GS_STATE_QUIT_GAME",
	"GS_STATE_OPTIONS_MENU",							// 5
	"GS_EVENT_WEAPON_SELECT_HELP",
	"GS_STATE_BARRACKS_MENU",
	"GS_STATE_TECH_MENU",
	"GS_STATE_TRAINING_MENU",
	"GS_STATE_LOAD_MISSION_MENU",						// 10
	"GS_STATE_BRIEFING",
	"GS_STATE_SHIP_SELECT",
	"GS_STATE_DEBUG_PAUSED",
	"GS_STATE_HUD_CONFIG",
	"GS_STATE_MULTI_SETUP",								// 15
	"GS_STATE_MULTI_JOIN_GAME",
	"GS_STATE_CONTROL_CONFIG",
	"GS_STATE_MULTI_PROTO_CHOICE",
	"GS_STATE_SAVE_RESTORE",
	"GS_STATE_WEAPON_SELECT",							// 20
	"GS_STATE_MISSION_LOG_SCROLLBACK",
	"GS_STATE_MAIN_MENU_HELP",
	"GS_STATE_GAMEPLAY_HELP",
	"GS_STATE_SHIP_SELECT_HELP",
	"GS_STATE_DEATH_DIED",								// 25
	"GS_STATE_DEATH_BLEW_UP",
	"GS_STATE_SIMULATOR_ROOM",
	"GS_STATE_CREDITS",
	"GS_STATE_SHOW_GOALS",
	"GS_STATE_HOTKEY_SCREEN",							// 30
	"GS_STATE_HOTKEY_SCREEN_HELP",
	"GS_STATE_VIEW_MEDALS",
	"GS_STATE_MULTI_HOST_SETUP",
	"GS_STATE_MULTI_CLIENT_SETUP",
	"GS_STATE_DEBRIEF",									// 35
	"GS_STATE_NAVMAP",
	"GS_STATE_MULTI_JOIN_TRACKER",  
	"GS_STATE_VIEW_CUTSCENES",
	"GS_STATE_MULTI_STD_WAIT",
	"GS_STATE_STANDALONE_MAIN",						// 40
	"GS_STATE_MULTI_PAUSED",  	
	"GS_STATE_BRIEFING_HELP",
	"GS_STATE_TEAM_SELECT",
	"GS_STATE_TRAINING_PAUSED",
	"GS_STATE_MULTI_HELP",  							// 45
	"GS_STATE_INGAME_PRE_JOIN",						
	"GS_STATE_EVENT_DEBUG",
	"GS_STATE_STANDALONE_POSTGAME",
	"GS_STATE_INITIAL_PLAYER_SELECT",
	"GS_STATE_MULTI_MISSION_SYNC",					// 50
	"GS_STATE_MULTI_SERVER_TRANSFER",				
	"GS_STATE_MULTI_START_GAME",
	"GS_STATE_MULTI_HOST_OPTIONS",
	"GS_STATE_MULTI_DOGFIGHT_DEBRIEF",				
	"GS_STATE_CAMPAIGN_ROOM",							// 55
	"GS_STATE_CMD_BRIEF",
	"GS_STATE_RED_ALERT",
	"GS_STATE_END_OF_CAMPAIGN",
};
//XSTR:ON

void gameseq_init()
{
	int i;

	for (i=0; i<GS_STACK_SIZE; i++ )	{
		// gs[i].current_state = GS_STATE_MAIN_MENU;
		gs[i].current_state = 0;
		gs[i].queue_tail=0;
		gs[i].queue_head=0;
	}

	gs_current_stack = 0;
	state_reentry = 0;
	state_processing_event_post = 0;
	state_in_event_processer = 0;
}

// gameseq_post_event posts a new game sequencing event onto the gameseq
// event queue

void gameseq_post_event( int event )
{
	if (state_processing_event_post) {
		nprintf(("Warning", "Received post for event %s during state transtition. Find Allender if you are unsure if this is bad.\n", GS_event_text[event] ));
	}

	Assert(gs[gs_current_stack].queue_tail < MAX_GAMESEQ_EVENTS);
	gs[gs_current_stack].event_queue[gs[gs_current_stack].queue_tail++] = event;
	if ( gs[gs_current_stack].queue_tail == MAX_GAMESEQ_EVENTS )
		gs[gs_current_stack].queue_tail = 0;
}

// returns one of the GS_EVENT_ id's on the game sequencing queue

int gameseq_get_event()
{
	int event;

	if ( gs[gs_current_stack].queue_head == gs[gs_current_stack].queue_tail )
		return -1;
	event = gs[gs_current_stack].event_queue[gs[gs_current_stack].queue_head++];
	if ( gs[gs_current_stack].queue_head == MAX_GAMESEQ_EVENTS )
		gs[gs_current_stack].queue_head = 0;

	return event;
}	   

// returns one of the GS_STATE_ macros
int gameseq_get_state(int depth)
{	
	Assert(depth <= gs_current_stack);
			
	return gs[gs_current_stack - depth].current_state;
}

int gameseq_get_depth()
{
	return gs_current_stack;
}

void gameseq_set_state(int new_state, int override)
{
	int event, old_state;

	if ( (new_state == gs[gs_current_stack].current_state) && !override )
		return;

	old_state = gs[gs_current_stack].current_state;

	// Flush all events!!
	while ( (event = gameseq_get_event()) != -1 ) {
		mprintf(( "Throwing out event %d because of state set from %d to %d\n", event, old_state, new_state ));
	}

	Assert( state_reentry == 1 );		// Get John! (Invalid state sequencing!)
	Assert( state_in_event_processer == 1 );		// can only call from game_process_event

	state_processing_event_post++;
	state_reentry++;
	game_leave_state(gs[gs_current_stack].current_state,new_state);

	gs[gs_current_stack].current_state = new_state;

	game_enter_state(old_state,gs[gs_current_stack].current_state);
	state_reentry--;
	state_processing_event_post--;
}
	
void gameseq_push_state( int new_state )
{
	if ( new_state == gs[gs_current_stack].current_state )
		return;

	int old_state = gs[gs_current_stack].current_state;

	// Flush all events!!
// I commented out because I'm not sure if we should throw out events when pushing or not.
//	int event;
//	while( (event = gameseq_get_event()) != -1 )	{
//		mprintf(( "Throwing out event %d because of state push from %d to %d\n", event, old_state, new_state ));
//	}

	Assert( state_reentry == 1 );		// Get John! (Invalid state sequencing!)
	Assert( state_in_event_processer == 1 );		// can only call from game_process_event

	gs_current_stack++;
	Assert(gs_current_stack < GS_STACK_SIZE);

	state_processing_event_post++;
	state_reentry++;
	game_leave_state(old_state,new_state);

	gs[gs_current_stack].current_state = new_state;
	gs[gs_current_stack].queue_tail = 0;
	gs[gs_current_stack].queue_head = 0;

	game_enter_state(old_state,gs[gs_current_stack].current_state);
	state_reentry--;
	state_processing_event_post--;
}

void gameseq_pop_state()
{
	int popped_state = 0;

	Assert(state_reentry == 1);		// Get John! (Invalid state sequencing!)

	if (gs_current_stack >= 1) {
		int old_state;

		// set the old state to be the state which is about to be popped off the queue
		old_state = gs[gs_current_stack].current_state;

		// set the popped_state to be the state which is going to be moved into
		popped_state = gs[gs_current_stack-1].current_state;

		// leave the current state
		state_reentry++;
		game_leave_state(gs[gs_current_stack].current_state,popped_state);

		// set the popped_state to be the one we moved into
		gs_current_stack--;
		popped_state = gs[gs_current_stack].current_state;

		// swap all remaining events from the state which just got popped to this new state
		while(gs[gs_current_stack+1].queue_head != gs[gs_current_stack+1].queue_tail){
			gameseq_post_event(gs[gs_current_stack+1].event_queue[gs[gs_current_stack+1].queue_head++]);
		}

		game_enter_state(old_state, gs[gs_current_stack].current_state);
		state_reentry--;

	}

}

// gameseq_pop_and_discard_state() is used to remove a state that was pushed onto the stack, but
// will never need to be popped.  An example of this is entering a state that may require returning
// to the previous state (then you would call gameseq_pop_state).  Or you may simply continue to
// another part of the game, to avoid filling up the stack with states that may never be popped, you
// call this function to discard the top of the gs.
//

void gameseq_pop_and_discard_state()
{
	if (gs_current_stack > 0 ) {
		gs_current_stack--;
	}
}

// Returns the last state pushed on stack
int gameseq_get_pushed_state()
{
	if (gs_current_stack >= 1) {
		return gs[gs_current_stack-1].current_state;
	} else	
		return -1;
}

// gameseq_process_events gets called every time through high level loops
// (i.e. game loops, main menu loop).  Function is responsible for pulling
// game sequence events off the queue and changing the state when necessary.
// Returns the current state.
		// pull events game sequence events off of the queue.  Process one at a time
		// based on the current state and the new event.

int gameseq_process_events()	
{
	int event, old_state;
	old_state = gs[gs_current_stack].current_state;

	Assert(state_reentry == 0);		// Get John! (Invalid state sequencing!)

	while ( (event = gameseq_get_event()) != -1 ) {
		state_reentry++;
		state_in_event_processer++;
		game_process_event(gs[gs_current_stack].current_state, event);
		state_in_event_processer--;
		state_reentry--;
		// break when state changes so that code will get called at
		// least one frame for each state.
		if (old_state != gs[gs_current_stack].current_state)
			break;	
	}

	state_reentry++;
	game_do_state(gs[gs_current_stack].current_state);
	state_reentry--;

	return gs[gs_current_stack].current_state;
} 

