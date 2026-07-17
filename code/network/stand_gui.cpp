/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <stdarg.h>

#include "vecmat.h"
#include "tmapper.h"
#include "2d.h"
#include "3d.h"
#include "model.h"
#include "bmpman.h"
#include "lighting.h"
#include "linklist.h"
#include "freespace.h"

#include "stand_gui.h"
#include "pstypes.h"
#include "osapi.h"
#include "key.h"
#include "palman.h"
#include "mouse.h"
#include "outwnd.h"
#include "2d.h"
#include "cfile.h"
#include "sound.h"
#include "freespaceresource.h"
#include "multi.h"
#include "multimsgs.h"
#include "multiutil.h"
#include "missiongoals.h"
#include "systemvars.h"
#include "cmdline.h"
#include "multi_kick.h"
#include "multi_pmsg.h"
#include "chatbox.h"
#include "multi_endgame.h"
#include "gamesequence.h"
#include "osregistry.h"
#include "timer.h"
#include "version.h"

HANDLE Standalone_thread;
DWORD Standalone_thread_id;
static HWND Standalone_hwnd = NULL;

// -----------------------------------------------------------------------------------------
// standalone global defs

#define MAX_STANDALONE_PAGES 5

#define STD_STATS_UPDATE_TIME			500		// ms between updating player stats on the visible controls
#define STD_NG_UPDATE_TIME				100		// ms between updating netgame information are controls

// coords for the "shutdown" button (client window coords)
static int Std_shutdown_coords[GR_NUM_RESOLUTIONS][2] = {
	{ 130, 450 },	// GR_640
	{ 130, 450 }	// GR_640
};

// you should reference Page_handles and Pages with these defines from now on
#define CONNECT_PAGE			0
#define MULTIPLAYER_PAGE	1
#define PLAYER_INFO_PAGE   2
#define GODSTUFF_PAGE      3
#define DEBUG_PAGE         4

// standalone gui property sheet stuff
static HWND Psht;
static HWND Page_handles[MAX_STANDALONE_PAGES];
static PROPSHEETPAGE Pages[MAX_STANDALONE_PAGES];
static PROPSHEETHEADER Sheet;

// index into Page_handles[] representing the currently selected page
static int Active_standalone_page;

// timestamp for updating currently selected player stats on the player info page
int Standalone_stats_stamp;

// timestamp for updating the netgame information are text controls
int Standalone_ng_stamp;

// banned player callsigns
#define STANDALONE_MAX_BAN			50
char Standalone_ban_list[STANDALONE_MAX_BAN][CALLSIGN_LEN+1];
int Standalone_ban_count = 0;

// ----------------------------------------------------------------------------------------
// mission validation dialog
//

static HWND Multi_gen_dialog = NULL;		// the dialog itself

// dialog proc for this dialog
BOOL CALLBACK std_gen_dialog_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg){		
	case WM_INITDIALOG:
		return TRUE;

	// destory command
	case WM_DESTROY:
		Multi_gen_dialog = NULL;
		break;
	}
	return FALSE;
}

// create the validate dialog 
void std_create_gen_dialog(char *title)
{
	// if the dialog is already active, do nothing
	if(Multi_gen_dialog != NULL){
		return;
	}

	// otherwise create the dialog
	Multi_gen_dialog = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_GEN), Psht, (DLGPROC)std_gen_dialog_proc);	
	if(Multi_gen_dialog != NULL){
		SetWindowText(Multi_gen_dialog, title);		
	}
}

// kill the validate dialog();
void std_destroy_gen_dialog()
{
	// if the dialog is not active, do nothing
	if(Multi_gen_dialog == NULL){
		return;
	}

	// kill it
	DestroyWindow(Multi_gen_dialog);
	Multi_gen_dialog = NULL;
}

// set the text in the filename of the validate dialog
// valid values for field_num == 0 .. 2
void std_gen_set_text(char *str, int field_num)
{
	HWND ctrl;

	// if the dialog is not active
	if((Multi_gen_dialog == NULL) || (str == NULL) || (field_num < 0) || (field_num > 2)){
		return;
	}

	// otherwise set the text
	ctrl = GetDlgItem(Multi_gen_dialog, (int)MAKEINTRESOURCE(IDC_FIELD1));
	switch(field_num){
	case 0:
		ctrl = GetDlgItem(Multi_gen_dialog,(int)MAKEINTRESOURCE(IDC_FIELD1));
		break;
	case 1:
		ctrl = GetDlgItem(Multi_gen_dialog,(int)MAKEINTRESOURCE(IDC_FIELD2));
		break;
	case 2:
		ctrl = GetDlgItem(Multi_gen_dialog,(int)MAKEINTRESOURCE(IDC_FIELD3));
		break;	
	}
	SetWindowText(ctrl, str);
}

// is the validate dialog active
int std_gen_is_active()
{
	return Multi_gen_dialog != NULL;
}

// ----------------------------------------------------------------------------------------
// connection page/tab functions
//
static HWND Multi_std_name;					// standalone name text edit control
HWND	Multi_std_host_passwd;					// host password text control
int Multi_std_namechange_force;

// convert the index of an item in the list box into an index into the net players array
int std_connect_lindex_to_npindex(int index);

// set the text box indicating how many players are connected, returning the determined count
int std_connect_set_connect_count()
{
	HWND ctrl;
	char str[40];
	char val[10];	
	int idx,count;

	// setup the text string
	strcpy(str,XSTR("# Connections : ",911));

	// determine how many players are actually connected
	count = 0;
	for(idx=0;idx<MAX_PLAYERS;idx++){
		if(MULTI_CONNECTED(Net_players[idx]) && (Net_player != &Net_players[idx])){
			count++;
		}
	}
	
	// tack on the player count to the end of the string
	sprintf(val,"%d",count);
	strcat(str,val);

	// set the text itself
   ctrl = GetDlgItem(Page_handles[CONNECT_PAGE],(int)MAKEINTRESOURCE(IDC_CON_COUNT));
   SetWindowText(ctrl,str);

	// return the num of players found
	return count;
}

// set the connect status (connected or not) of the game host
void std_connect_set_host_connect_status()
{
	int idx,found;
	HWND ctrl;

	// first try and find the host
	found = 0;
	for(idx=0;idx<MAX_PLAYERS;idx++){
		if(MULTI_CONNECTED(Net_players[idx]) && MULTI_HOST(Net_players[idx])){
			found = 1;
			break;
		}
	}

	// get the control and set the status
	ctrl = GetDlgItem(Page_handles[CONNECT_PAGE],(int)MAKEINTRESOURCE(IDC_HOST_IS));
	if(found){
		SetWindowText(ctrl, XSTR("Host connected ? Yes",912));
	} else {
		SetWindowText(ctrl, XSTR("Host connected ? No",913));
	}
}

// add an ip string to the connect page listbox
void std_connect_add_ip_string(char *string)
{
   HWND ctrl;
	
	// add the item
	ctrl = GetDlgItem(Page_handles[CONNECT_PAGE], (int)MAKEINTRESOURCE(IDC_CONPING));
	SendMessage(ctrl, LB_ADDSTRING, (WPARAM)0, (LPARAM)(LPCTSTR)string);
}

// remove an ip string from the connect page listbox
void std_connect_remove_ip_string(char *string)
{
	HWND ctrl;	
	int loc;
	
	// get the control handle
	ctrl = GetDlgItem(Page_handles[CONNECT_PAGE], (int)MAKEINTRESOURCE(IDC_CONPING));

	// NOTE the use of FINDSTRING and _not_ FINDSTRINGEXACT !!
	// since we've appended the ping to the end of the string, we can only check the 
	// "prefix" which is the net_players name
	loc = SendMessage(ctrl, LB_FINDSTRING, (WPARAM)-1, (LPARAM)(LPCTSTR)string);

	if(loc!=LB_ERR){
		SendMessage(ctrl, LB_DELETESTRING, (WPARAM)loc, (LPARAM)0);
	}
}

// set an ip string on the connect page listbox
void std_connect_set_ip_string(char *lookup,char *string)
{
	HWND ctrl;
	int loc;

	// get the control handle
	ctrl = GetDlgItem(Page_handles[CONNECT_PAGE],(int)MAKEINTRESOURCE(IDC_CONPING));
 
	// NOTE the use of FINDSTRING and _not_ FINDSTRINGEXACT !!
	// since we've appended the ping to the end of the string, we can only check the 
	// "prefix" which is the net_players name
	loc = SendMessage(ctrl,LB_FINDSTRING,(WPARAM)-1,(LPARAM)(LPCTSTR)lookup);

	if(loc!=LB_ERR){
		SendMessage(ctrl,LB_DELETESTRING,(WPARAM)loc,(LPARAM)0);
		SendMessage(ctrl,LB_INSERTSTRING,(WPARAM)loc,(LPARAM)string);
	}
}

void std_connect_kick_player()
{		
	int player_num,sel;
	HWND ctrl;	

	// get the control handle
	ctrl = GetDlgItem(Page_handles[CONNECT_PAGE],(int)MAKEINTRESOURCE(IDC_CONPING));

	sel = SendMessage(ctrl,LB_GETCURSEL,(WPARAM)0,(LPARAM)0);
	// attempt to get the player index
	if(sel != CB_ERR){
		player_num = std_connect_lindex_to_npindex(sel);

		// if we found him, then kick the bastard
		if(player_num != -1){
			multi_kick_player(player_num,0);
		}
	}
}

// update the ping for this particular player
void std_connect_update_ping(net_player *p)
{
	char str[40],ping[10],sml_ping[10],lookup[50];

	// as long as his ping is not -1, do an update
	if(p->s_info.ping.ping_avg > -1){	
		// get the lookup string
		psnet_addr_to_string(lookup,&p->p_info.addr);
		
		// build the string to replace the ping with
		strcpy(str,lookup); 
		strcat(str,", "); 
		
		// chop it off at pings greater than 1 second
		if(p->s_info.ping.ping_avg > 1000){
			strcat(str,XSTR("> 1 sec",914));
			strcpy(sml_ping,XSTR("> 1 sec",914));
		}
		else {
			sprintf(ping,"%d",p->s_info.ping.ping_avg); 
			strcat(str,ping);
			strcat(str,XSTR(" ms",915));
			strcpy(sml_ping,ping); strcat(sml_ping,XSTR(" ms",915));
		}

		// set the string
		std_connect_set_ip_string(lookup,str);
	}
}

// clear all the controls for this page
void std_connect_clear_controls()
{
	HWND handle;

	// set various connect counts
	std_connect_set_connect_count();
	std_connect_set_host_connect_status();

	// reset the list of players and pings
	handle = GetDlgItem(Page_handles[CONNECT_PAGE],(int)MAKEINTRESOURCE(IDC_CONPING));
	SendMessage(handle,LB_RESETCONTENT,(WPARAM)0,(LPARAM)0);
}

// set the game name for the standalone. passing NULL uses the default
void std_connect_set_gamename(char *name)
{
	char buf[MAX_GAMENAME_LEN+1];

	// use the default name for now
	if(name == NULL){
		// if a permanent name exists, use that instead of the default
		if(strlen(Multi_options_g.std_pname)){
			strcpy(Netgame.name, Multi_options_g.std_pname);
		} else {
			strcpy(Netgame.name,XSTR("Standalone Server",916));
		}
	} else {
		strcpy(Netgame.name,name);
	}

	// update the text control
	strcpy(buf,Netgame.name);
	Multi_std_namechange_force = 0;
	SetWindowText(Multi_std_name,buf);
	Multi_std_namechange_force = 1;
}

// the user has changed the text in the server name text box. handle this
void std_connect_handle_name_change()
{
	char buf[MAX_GAMENAME_LEN+2];
	int max_len = MAX_GAMENAME_LEN+2;

	if(Multi_std_namechange_force){
		memset(buf,0,MAX_GAMENAME_LEN+2);
		memcpy(&buf[0],&max_len,sizeof(int));

		// get the new text
		SendMessage(Multi_std_name,EM_GETLINE,(WPARAM)0,(LPARAM)(LPCSTR)buf);

		// just copy it over for now. we may want to process this more later on
		strcpy(Netgame.name,buf);

		// copy it to the permanent name
		strcpy(Multi_options_g.std_pname, buf);
	}
}

// the user has changed the text in the host password text box
void std_connect_handle_passwd_change()
{
	char buf[STD_PASSWD_LEN+2];
	int max_len = STD_PASSWD_LEN+2;
	
	memset(buf,0,STD_PASSWD_LEN+2);
	memcpy(&buf[0],&max_len,sizeof(int));

	// get the new text
	SendMessage(Multi_std_host_passwd,EM_GETLINE,(WPARAM)0,(LPARAM)(LPCSTR)buf);

	// just copy it over for now. we may want to process this more later on
	strcpy(Multi_options_g.std_passwd, buf);
}

// convert the index of an item in the list box into an index into the net players array
int std_connect_lindex_to_npindex(int index)
{
	HWND ctrl;			
	char list_text[40];	
	char addr_text[40];
	int ret,idx;

	// get the control handle
	ctrl = GetDlgItem(Page_handles[CONNECT_PAGE],(int)MAKEINTRESOURCE(IDC_CONPING));

	// get the string contained at a given index	
	SendMessage(ctrl,LB_GETTEXT,(WPARAM)index,(LPARAM)(LPSTR)list_text);

	// look through the net players array and compare address strings (yuck)
	ret = -1;
	for(idx=0;idx<MAX_PLAYERS;idx++){
		// only look at connected players
		if(MULTI_CONNECTED(Net_players[idx])){
			strcpy(addr_text,"");
			psnet_addr_to_string(addr_text,&Net_players[idx].p_info.addr);

			// if we found the match
			if((strlen(addr_text) != 0) && (strstr(list_text,addr_text) != NULL)){
				ret = idx;
				break;
			}
		}
	}

	return ret;
}

// message handler for the connect tab
BOOL CALLBACK connect_proc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
   switch(uMsg){
	// initialize the dialog
	case WM_INITDIALOG:
		// setup the page handle array for this page
		Page_handles[CONNECT_PAGE] = hwndDlg;		
		
		// create the standalone name text box and limit its text length
		Multi_std_name = GetDlgItem(hwndDlg, (int)MAKEINTRESOURCE(IDC_STD_NAME));
		SendMessage(Multi_std_name, EM_SETLIMITTEXT, (WPARAM)MAX_GAMENAME_LEN-1, (LPARAM)0);
		Multi_std_namechange_force = 1;

		// create the standalone host password input box
		Multi_std_host_passwd = GetDlgItem(hwndDlg, (int)MAKEINTRESOURCE(IDC_STD_HOST_PASSWD));
		SendMessage(Multi_std_host_passwd, EM_SETLIMITTEXT, (WPARAM)STD_PASSWD_LEN, (LPARAM)0);
		memset(Multi_options_g.std_passwd, 0, STD_PASSWD_LEN+1);

		return 1;      

	// process a command of some kind (usually button presses)
	case WM_COMMAND:
		switch(HIWORD(wParam)){
		// a button press
		case BN_CLICKED :
			switch(LOWORD(wParam)){			
			// the reset standalone button
			case IDC_RESET_MULTI : 
				// multi_standalone_quit_game();
				multi_quit_game(PROMPT_NONE);
				break;
			
			// kick the currently selected player
			case IDC_KICK_BUTTON :								
				std_connect_kick_player();
				break;
			
			// refresh file list (PXO)
			case IDC_PXO_REFRESH:				
				if(MULTI_IS_TRACKER_GAME){
					// delete mvalid.cfg if it exists
					cf_delete(MULTI_VALID_MISSION_FILE, CF_TYPE_DATA);

					// refresh missions
					multi_update_valid_missions();
				}
				break;
			}
			break;
		// an edit control text has been changed
		case EN_UPDATE :
			if((HWND)lParam == Multi_std_name){
				// update the standalone name field in Netgame.name
				std_connect_handle_name_change();
			} else if((HWND)lParam == Multi_std_host_passwd){
				// update the standalone host passwd
				std_connect_handle_passwd_change();
			}
			break;
		}
		break;

	// a notification message
	case WM_NOTIFY :
		// notification that this is the current selected page. set our own internal data vars
		if(((LPNMHDR)lParam)->code == PSN_SETACTIVE){
			Active_standalone_page = CONNECT_PAGE;
		} else if ( (((LPNMHDR)lParam)->code == PSN_APPLY) || (((LPNMHDR)lParam)->code == PSN_RESET) ) {
			PostMessage( Psht, WM_DESTROY, 0, 0 );
		}
		break;
	
	default :
		return 0;		
	}
	return 0;
}

	
// ----------------------------------------------------------------------------------------
// multiplayer page/tab functions
//

static HWND			Framecap_trackbar;					// trackbar for capping framerate
static HWND		   Standalone_FPS;						// text control for displaying framerate
static HWND			Standalone_mission_name;			// text control for showing the current mission name
static HWND			Standalone_missiontime;				// text control for showing current mission time
static HIMAGELIST Goal_bitmaps;							// bitmaps array for the goal tree control
static HWND			Standalone_goals;						// goal tree control handle
static HTREEITEM	Goal_items[3];							// primary, secondary, and bonus goal items
static HWND			Std_ng_max_players;					// max players display text control
static HWND			Std_ng_max_observers;				// max observers display text control
static HWND			Std_ng_security;						// netgame security display text control
static HWND			Std_ng_respawns;						// netgame # respawns display text control

#define GOALVIEW_X 5											// goal view control extents
#define GOALVIEW_Y 242										//
#define GOALVIEW_W 160										//
#define GOALVIEW_H 168										//

// handle the user sliding the framerate cap scrollbar around
void std_multi_handle_framecap_scroll(HWND ctrl);

// initialize the framerate cap slide control
void std_multi_init_framecap_slider(HWND hwndDlg);

// initialize all the controls for this page
void std_multi_init_multi_controls(HWND hwndDlg);

// return the handle to the item matching the given parameters
HTREEITEM std_multi_get_goal_item(char *goal_string,int type);

// set the mission time in seconds
void std_multi_set_standalone_missiontime(float mission_time)
{
	char txt[20];
	char time_txt[30];
	fix m_time = fl2f(mission_time);

	// format the time string and set the text
	game_format_time(m_time,time_txt);	
	sprintf(txt,"  :  %.1f", mission_time);
	strcat(time_txt,txt);
	SetWindowText(Standalone_missiontime,time_txt);
}

// set the mission name
void std_multi_set_standalone_mission_name(char *mission_name)
{
	// set the text
	SetWindowText(Standalone_mission_name,mission_name);
}

// initialize the goal tree for this mission 
void std_multi_setup_goal_tree()
{
   TV_ITEM         new_item;
	TV_INSERTSTRUCT tree_insert;
	char goal_name[NAME_LENGTH+1];

	// clear out the tree control
	TreeView_DeleteAllItems(Standalone_goals);

   // add the primary goal tag
   new_item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	new_item.pszText = goal_name;
	strcpy(new_item.pszText,XSTR("Primary Objectives",917));
	new_item.iImage = 0;
	new_item.iSelectedImage = 0;		
	tree_insert.hParent      = NULL;
	tree_insert.hInsertAfter = TVI_FIRST;
	tree_insert.item = new_item;
	Goal_items[0] = TreeView_InsertItem(Standalone_goals,&tree_insert);

	// add the secondary goal tag
	new_item.pszText = goal_name;
	strcpy(new_item.pszText,XSTR("Secondary Objectives",918));
	new_item.iImage = 0;
	new_item.iSelectedImage = 0;	
	tree_insert.hInsertAfter = TVI_LAST;
	tree_insert.item = new_item;
   Goal_items[1] = TreeView_InsertItem(Standalone_goals,&tree_insert);

	// add the bonus goal tag
	new_item.pszText = goal_name;
	strcpy(new_item.pszText,XSTR("Bonus Objectives",919));
	new_item.iImage = 0;
	new_item.iSelectedImage = 0;			
	tree_insert.item = new_item;
   Goal_items[2] = TreeView_InsertItem(Standalone_goals,&tree_insert);
}

// add all the goals from the current mission to the tree control
void std_multi_add_goals()
{
	TV_ITEM new_item;
	TV_INSERTSTRUCT tree_insert;
	int idx,goal_flags,perm_goal_flags;		
	char goal_name[NAME_LENGTH+1];

	// setup data common for every item
	new_item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;	
	tree_insert.hInsertAfter = TVI_LAST;	

	perm_goal_flags = 0;
	for(idx=0;idx<Num_goals;idx++){		
		// reset the goal flags
		goal_flags = 0;

      switch(Mission_goals[idx].type & GOAL_TYPE_MASK){
		// primary goal
		case PRIMARY_GOAL :
			goal_flags |= (1<<1);		// (image index == 1, primary goal)						
			perm_goal_flags |= (1<<1);
			break;
		
		// a secondary goal
		case SECONDARY_GOAL :
			goal_flags |= (1<<2);		// (image index == 1, secondary goal)			
			perm_goal_flags |= (1<<2);
			break;
		
		// a bonus  goal
		case BONUS_GOAL :
			goal_flags |= (1<<3);		// (image index == 1, bonus goal)			
			perm_goal_flags |= (1<<3);
			break;
		
		default :
			goal_flags |= (1<<0);		// (image index == 3, no goal)
			break;
		}
		
      // first select whether to insert under primary, secondary, or bonus tree roots
		tree_insert.hParent = Goal_items[Mission_goals[idx].type & GOAL_TYPE_MASK];  

		// set the goal name
		new_item.pszText = goal_name;
		strcpy(new_item.pszText,Mission_goals[idx].name);
		
		// set the correct image indices
		new_item.iImage = (goal_flags & (1<<0)) ? 3 : 0;
		new_item.iSelectedImage = (goal_flags & (1<<0)) ? 3 : 0;

		// insert the item
		tree_insert.item = new_item;
		TreeView_InsertItem(Standalone_goals,&tree_insert);
	}	

	// check to see if there are any of the three types of mission goals. If not, then 
	// insert "none"
	if(!(perm_goal_flags & (1<<1))){
		// insert the "none" item 
		tree_insert.hParent = Goal_items[0];
		new_item.pszText = goal_name;
		strcpy(new_item.pszText,XSTR("none",920));
		new_item.iImage = 3;
		new_item.iSelectedImage = 3;
		tree_insert.item = new_item;
		TreeView_InsertItem(Standalone_goals,&tree_insert);
	}
	if(!(perm_goal_flags & (1<<2))){
		// insert the "none" item
		tree_insert.hParent = Goal_items[1];
		new_item.pszText = goal_name;
		strcpy(new_item.pszText,XSTR("none",920));
		new_item.iImage = 3;
		new_item.iSelectedImage = 3;
		tree_insert.item = new_item;
		TreeView_InsertItem(Standalone_goals,&tree_insert);
	}
	if(!(perm_goal_flags & (1<<3))){
		// insert the "none" item
		tree_insert.hParent = Goal_items[1];
		new_item.pszText = goal_name;
		strcpy(new_item.pszText,XSTR("none",920));
		new_item.iImage = 3;
		new_item.iSelectedImage = 3;
		tree_insert.item = new_item;
		TreeView_InsertItem(Standalone_goals,&tree_insert);
	}

	// expand out all the tree roots so all goals are shown
	for(idx=0;idx<3;idx++){
		TreeView_Expand(Standalone_goals,Goal_items[idx],TVE_EXPAND);
	}
}

// update all the goals in the goal tree based upon the mission status
void std_multi_update_goals()
{
	HTREEITEM update_item;
	TV_ITEM setting,lookup;
	int idx,should_update;

   setting.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;

	// go through all the goals
	for(idx=0;idx<Num_goals;idx++){
		// get a handle to the tree item
		update_item = NULL;
		update_item = std_multi_get_goal_item(Mission_goals[idx].name,Mission_goals[idx].type & GOAL_TYPE_MASK);

		// continue if we didn't get a valid item
		if(update_item == NULL){
			continue;
		}
		
		// get the tree item itself (as it currently stands)
		lookup.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		lookup.hItem = update_item;
		if(!TreeView_GetItem(Standalone_goals,&lookup)){
			continue;
		}
		
		should_update = 0;		
		// determine what image to set for each one (failed, satisfied, incomplete, etc)
		switch(Mission_goals[idx].satisfied){
		case GOAL_FAILED : 			
			// determine if we should update the item
			if((lookup.iImage != 4) && (lookup.iSelectedImage != 4)){
				setting.iImage = 4;
				setting.iSelectedImage = 4;

				should_update = 1;
			}
			break;
		case GOAL_COMPLETE :
			// determine if we should update the item
			if((lookup.iImage != 2) && (lookup.iSelectedImage != 2)){				
				setting.iImage = 2;
				setting.iSelectedImage = 2;

				should_update = 1;
			}
			break;

		case GOAL_INCOMPLETE :
			// determine if we should update the item
			if((lookup.iImage != 1) && (lookup.iSelectedImage != 1)){				
				setting.iImage = 1;
				setting.iSelectedImage = 1;

				should_update = 1;
			}
			break;
		}
			
		// set the actual image
		if(should_update){
			setting.hItem = update_item;
			TreeView_SetItem(Standalone_goals,&setting);		
		}
	}
}

// set the framerate text box for this page
void std_multi_set_framerate(float f)
{
	char fr[10];

	// set the window text
	sprintf(fr,"%.1f",f);
	SetWindowText(Standalone_FPS,fr);
}

// clear all the controls for this page
void std_multi_clear_controls()
{
	// clear out the mission name text static
	SetWindowText(Standalone_mission_name,"");

	// clear out the framerate text box
	SetWindowText(Standalone_FPS,"");

	// clear out the misison time text box
	SetWindowText(Standalone_missiontime,"");		

	// clear out the netgame max players text box
	SetWindowText(Std_ng_max_players,"");

	// clear out the netgame max observer text box
	SetWindowText(Std_ng_max_observers,"");

	// clear out the netgame security text box
	SetWindowText(Std_ng_security,"");

	// clear out the netgame respawns # text box
	SetWindowText(Std_ng_respawns,"");

	// clear the goal tree control
	std_multi_setup_goal_tree();
}

// update the netgame information area controls with the current Netgame settings
void std_multi_update_netgame_info_controls()
{
	char buf[40];

	// update the 
	
	// update the max players control
	sprintf(buf,"%d",Netgame.max_players);
	SetWindowText(Std_ng_max_players,buf);

	// update the max observers control
	sprintf(buf,"%d",Netgame.options.max_observers);
	SetWindowText(Std_ng_max_observers,buf);

	// update the netgame security control
	sprintf(buf,"%d",Netgame.security);
	SetWindowText(Std_ng_security,buf);

	// update the netgame respawns # control
	sprintf(buf,"%d",Netgame.respawn);
	SetWindowText(Std_ng_respawns,buf);
}

// handle the user sliding the framerate cap scrollbar around
void std_multi_handle_framecap_scroll(HWND ctrl)
{
   int pos;
	char pos_text[10];
   
	// determine where the slider now is
	pos = SendMessage(ctrl,TBM_GETPOS,(WPARAM)0,(LPARAM)0);

	// update the text display 
	sprintf(pos_text,"%d",pos);
	SetWindowText(GetDlgItem(Page_handles[MULTIPLAYER_PAGE],(int)MAKEINTRESOURCE(IDC_FRAMECAP_STATIC)),pos_text);
	
	// set the framecap var
	Multi_options_g.std_framecap = pos;
}

// initialize the framerate cap slide control
void std_multi_init_framecap_slider(HWND hwndDlg)
{
	WPARAM wp;
	LPARAM lp;

   // create the trackbar object
	Framecap_trackbar = CreateWindowEx(0,TRACKBAR_CLASS,NULL,WS_CHILD | WS_VISIBLE,
		                              10,10,300,30,hwndDlg,NULL,GetModuleHandle(NULL),NULL);	

	// set the range of the framerate cap
	wp = (WPARAM)(BOOL)TRUE;
	lp = (LPARAM)MAKELONG(1, 100);
   SendMessage(Framecap_trackbar,TBM_SETRANGE,wp,lp);
	
   // set the default framerate cap the be the standalone default
	wp = (WPARAM)(BOOL)TRUE;
	lp = (LPARAM)(LONG)30;
	SendMessage(Framecap_trackbar,TBM_SETPOS,wp,lp);

   // call this to update the standalone framecap on this first run
	std_multi_handle_framecap_scroll(Framecap_trackbar);
}

// initialize all the controls for this page
void std_multi_init_multi_controls(HWND hwndDlg)
{
   HBITMAP ref;
	COLORREF mask;
	
	// create the framecap slider
	std_multi_init_framecap_slider(hwndDlg);

	// create the framerate display text box
	Standalone_FPS = GetDlgItem(hwndDlg,(int)MAKEINTRESOURCE(IDC_STANDALONE_FPS));

	// create the missiontime text box
	Standalone_missiontime = GetDlgItem(hwndDlg,(int)MAKEINTRESOURCE(IDC_STANDALONE_MTIME));

	// create the mission name text box
	Standalone_mission_name = GetDlgItem(hwndDlg,(int)MAKEINTRESOURCE(IDC_MISSION_NAME));

	// create the netgame max players text box
	Std_ng_max_players = GetDlgItem(hwndDlg,(int)MAKEINTRESOURCE(IDC_NG_MAXPLAYERS));

	// create the netgame max observers text box
	Std_ng_max_observers = GetDlgItem(hwndDlg,(int)MAKEINTRESOURCE(IDC_NG_MAXOBSERVERS));

	// create the netgame security text box
	Std_ng_security = GetDlgItem(hwndDlg,(int)MAKEINTRESOURCE(IDC_NG_SECURITY));

	// create the netgame respawns # text box
	Std_ng_respawns = GetDlgItem(hwndDlg,(int)MAKEINTRESOURCE(IDC_NG_RESPAWNS));

   // load the goal tree-view bitmaps
	Goal_bitmaps = ImageList_Create(16,16,ILC_COLOR4 | ILC_MASK,5,0);
	
	mask = 0x00ff00ff;
	ref = LoadBitmap(GetModuleHandle(NULL),MAKEINTRESOURCE(IDB_GOAL_ORD));  
	ImageList_AddMasked(Goal_bitmaps,ref,mask);
			
	ref = LoadBitmap(GetModuleHandle(NULL),MAKEINTRESOURCE(IDB_GOAL_INC));
	mask = 0x00ffffff;
	ImageList_AddMasked(Goal_bitmaps,ref,mask);
	
	ref = LoadBitmap(GetModuleHandle(NULL),MAKEINTRESOURCE(IDB_GOAL_COMP));
	mask = 0x00ffffff;
	ImageList_AddMasked(Goal_bitmaps,ref,mask);
	
	ref = LoadBitmap(GetModuleHandle(NULL),MAKEINTRESOURCE(IDB_GOAL_NONE));
	mask = 0x00ffffff;
	ImageList_AddMasked(Goal_bitmaps,ref,mask);

	ref = LoadBitmap(GetModuleHandle(NULL),MAKEINTRESOURCE(IDB_GOAL_FAIL));
	mask = 0x00ffffff;
	ImageList_AddMasked(Goal_bitmaps,ref,mask);
	
   // create the tree view control and associate its image list
	Standalone_goals = CreateWindowEx(0, WC_TREEVIEW, XSTR("Tree View",921), 
        WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES, 
        GOALVIEW_X,GOALVIEW_Y,GOALVIEW_W,GOALVIEW_H,
        hwndDlg, NULL, GetModuleHandle(NULL), NULL); 
	TreeView_SetImageList(Standalone_goals,Goal_bitmaps,TVSIL_NORMAL);
}

// return the handle to the item matching the given parameters
HTREEITEM std_multi_get_goal_item(char *goal_string,int type)
{
	HTREEITEM ret,moveup;
	TV_ITEM lookup;
	int done;
	char goal_name_text[NAME_LENGTH+1];

	// look under the correct root item
	lookup.mask = TVIF_TEXT;
	lookup.pszText = goal_name_text;
	lookup.cchTextMax = NAME_LENGTH;
	strcpy(lookup.pszText,goal_string);

	// search through all the items
	done=0;
	ret=NULL;
	moveup = TreeView_GetChild(Standalone_goals,Goal_items[type]);
	while(!done && moveup!=NULL){
		lookup.hItem = moveup;
		TreeView_GetItem(Standalone_goals,&lookup);
		if(strcmp(lookup.pszText,goal_string)==0){
			ret = moveup;
			done=1;
		}
		if(!done){
			moveup = TreeView_GetNextItem(Standalone_goals,moveup,TVGN_NEXT);
		}
	}	
	return ret;
}

// message handler for the multiplayer tab
BOOL CALLBACK multi_proc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg){
	// initialize the page
	case WM_INITDIALOG:
		// set the page handle
		Page_handles[MULTIPLAYER_PAGE] = hwndDlg;

		// initialize all the controls
		std_multi_init_multi_controls(hwndDlg);
		return 1;
		break;
	
	// a scroll message from the framerate cap trackbar
	case WM_HSCROLL:
		std_multi_handle_framecap_scroll((HWND)lParam);
		return 1;
		break;

	// notification that this page has been set as active
	case WM_NOTIFY :
		// setup our own internal vars
		if(((LPNMHDR)lParam)->code == PSN_SETACTIVE){
			Active_standalone_page = MULTIPLAYER_PAGE;
		} else if ( (((LPNMHDR)lParam)->code == PSN_APPLY) || (((LPNMHDR)lParam)->code == PSN_RESET) ) {
			// PostMessage( Psht, WM_DESTROY, 0, 0 );
			gameseq_post_event(GS_EVENT_QUIT_GAME);
		}
		break;

	default :
		return 0;
		break;
	}
	return 0;
}


// ---------------------------------------------------------------------------------------
// player info page/tab functions
//

#define MAX_PLAYER_STAT_FIELDS 11							// the # of stats fields for a given set
static HWND Player_name_list;									// the listbox control with player callsigns in it
static HWND Player_ship_type;									// the current player's ship type
static HWND Player_ping_time;									// the current player's ping time
static HWND Player_stats[MAX_PLAYER_STAT_FIELDS];		// text boxes for player alltime statistics info
static HWND Player_mstats[MAX_PLAYER_STAT_FIELDS];		// text boxes for player mission statistics info

// sprintf and set window text to the passed int
#define STD_ADDSTRING(hwnd,val) { sprintf(txt,"%d",(int)val); SetWindowText(hwnd,txt); }

// intialize all the controls in the player info tab
void std_pinfo_init_player_info_controls(HWND hwndDlg);

// returns true or false depending on whether the passed netplayer is the currently selected guy
int std_pinfo_player_is_active(net_player *p);

// start displaying info for the passed player on this page
void std_pinfo_display_player_info(net_player *p)
{
	char txt[40];		

	// set his ship type
	SetWindowText(Player_ship_type,Ship_info[p->p_info.ship_class].name);

	// display his ping time
	std_pinfo_update_ping(p);
	
	// his alltime stats
	scoring_struct *ptr = &p->player->stats;
	STD_ADDSTRING(Player_stats[0],ptr->p_shots_fired);
   STD_ADDSTRING(Player_stats[1],ptr->p_shots_hit);
	STD_ADDSTRING(Player_stats[2],ptr->p_bonehead_hits);
	STD_ADDSTRING(Player_stats[3],
                (int)((float)100.0*((float)ptr->p_shots_hit/(float)ptr->p_shots_fired)));
	STD_ADDSTRING(Player_stats[4],
		          (int)((float)100.0*((float)ptr->p_bonehead_hits/(float)ptr->p_shots_fired)));
	STD_ADDSTRING(Player_stats[5],ptr->s_shots_fired);
	STD_ADDSTRING(Player_stats[6],ptr->s_shots_hit);
	STD_ADDSTRING(Player_stats[7],ptr->s_bonehead_hits);
	STD_ADDSTRING(Player_stats[8],
					 (int)((float)100.0*((float)ptr->s_shots_hit/(float)ptr->s_shots_fired)));
	STD_ADDSTRING(Player_stats[9],
                (int)((float)100.0*((float)ptr->s_bonehead_hits/(float)ptr->s_shots_fired)));
	STD_ADDSTRING(Player_stats[10],ptr->assists);

   // his stats for the current mission
	STD_ADDSTRING(Player_mstats[0],ptr->mp_shots_fired);
   STD_ADDSTRING(Player_mstats[1],ptr->mp_shots_hit);
	STD_ADDSTRING(Player_mstats[2],ptr->mp_bonehead_hits);
	STD_ADDSTRING(Player_mstats[3],
                (int)((float)100.0*((float)ptr->mp_shots_hit/(float)ptr->mp_shots_fired)));
	STD_ADDSTRING(Player_mstats[4],
		          (int)((float)100.0*((float)ptr->mp_bonehead_hits/(float)ptr->mp_shots_fired)));
	STD_ADDSTRING(Player_mstats[5],ptr->ms_shots_fired);
	STD_ADDSTRING(Player_mstats[6],ptr->ms_shots_hit);
	STD_ADDSTRING(Player_mstats[7],ptr->ms_bonehead_hits);
	STD_ADDSTRING(Player_mstats[8],
					 (int)((float)100.0*((float)ptr->ms_shots_hit/(float)ptr->ms_shots_fired)));
	STD_ADDSTRING(Player_mstats[9],
                (int)((float)100.0*((float)ptr->ms_bonehead_hits/(float)ptr->ms_shots_fired))); 
	STD_ADDSTRING(Player_mstats[10],ptr->m_assists);
}

// check to see if this player is the one being displayed, and if so, then update the display info
// return 1 if the player was updated
int std_pinfo_maybe_update_player_info(net_player *p)
{	
	// only update if this is the currently active player
	if(std_pinfo_player_is_active(p)){	
		std_pinfo_display_player_info(p);
		return 1;
	}
	return 0;
}

// add a player to the list on the player info page
void std_pinfo_add_player_list_item(net_player *p)
{	
   // add the item
	SendMessage(Player_name_list,CB_ADDSTRING,(WPARAM)0,(LPARAM)(LPCTSTR)p->player->callsign);	

	// if this is the first item on the list, then select it and display it
	if(SendMessage(Player_name_list,CB_GETCOUNT,(WPARAM)0,(LPARAM)0) == 1){
		// select the item
		SendMessage(Player_name_list,CB_SETCURSEL,(WPARAM)0,(LPARAM)0);

		// display this players info
		std_pinfo_display_player_info(p);
	}
}

// remove a player from the list on the player info page
void std_pinfo_remove_player_list_item(net_player *p)
{
	int loc;
	
	// lookup thie player
	loc = SendMessage(Player_name_list,CB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)(LPCTSTR)p->player->callsign);

	// if we found the entry, then delete it
	if(loc!=CB_ERR){
		SendMessage(Player_name_list,CB_DELETESTRING,(WPARAM)loc,(LPARAM)0);
	}
}

// update the ping display for this player
void std_pinfo_update_ping(net_player *p)
{
	char sml_ping[30];
	
	// chop it off at pings greater than 1 second
	if(p->s_info.ping.ping_avg > 1000){		
		strcpy(sml_ping,XSTR("> 1 sec",914));
	}
	// use the ping itself
	else {
		sprintf(sml_ping,"%d",p->s_info.ping.ping_avg); 		
		strcat(sml_ping,XSTR(" ms",915));
	}
	
	SetWindowText(Player_ping_time,sml_ping);
}

// clear the player info page controls
void std_pinfo_clear_controls()
{
	int idx;
	
   // clear the player selection list
	SendMessage(Player_name_list,CB_RESETCONTENT,(WPARAM)0,(LPARAM)0);
	
	// clear out misc items	
	SetWindowText(Player_ship_type,"");
	SetWindowText(Player_ping_time,"");

	// clear out the player stats
	for(idx=0;idx<MAX_PLAYER_STAT_FIELDS;idx++){
		SetWindowText(Player_stats[idx],"");
		SetWindowText(Player_mstats[idx],"");
	}
}

// intialize all the controls in the player info tab
void std_pinfo_init_player_info_controls(HWND hwndDlg)
{	
	// create the player callsign listbox
	Player_name_list = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_PLAYER_LIST));

	// create the player ship type text box
	Player_ship_type = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_PSHIP_TYPE));

	// create the player ping time text box
	Player_ping_time = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_PING_TIME));

	// initialize the various and sundry statistics text controls (alltime)
   Player_stats[0] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_PSHOTS));
	Player_stats[1] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_PHITS));
	Player_stats[2] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_PBHHITS));
	Player_stats[3] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_PPCT));
	Player_stats[4] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_PBHPCT));
	Player_stats[5] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_SSHOTS));
	Player_stats[6] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_SECHITS));
	Player_stats[7] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_SBHHITS));
	Player_stats[8] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_SPCT));
	Player_stats[9] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_SBHPCT));
	Player_stats[10] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_ASSISTS));

	// initialize the various and sundry statistics text controls (this mission)
	Player_mstats[0] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_MPSHOTS));
	Player_mstats[1] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_MPHITS));
	Player_mstats[2] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_MPBHHITS));
	Player_mstats[3] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_MPPCT));
	Player_mstats[4] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_MPBHPCT));
	Player_mstats[5] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_MSSHOTS));
	Player_mstats[6] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_MSECHITS));
	Player_mstats[7] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_MSBHHITS));
	Player_mstats[8] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_MSPCT));
	Player_mstats[9] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_MSBHPCT));
	Player_mstats[10] = GetDlgItem(Page_handles[PLAYER_INFO_PAGE],(int)MAKEINTRESOURCE(IDC_MASSISTS));
}

// returns true or false depending on whether the passed netplayer is the currently selected guy
int std_pinfo_player_is_active(net_player *p)
{
	int sel;
	char player[40];

	// get the index of the currently selected item
	sel = SendMessage(Player_name_list,CB_GETCURSEL,(WPARAM)0,(LPARAM)0);

	// if we didn't find the item, return a 0 length string
	if(sel == LB_ERR){
		return 0;
	}

	// otherwise, get the callsign of the given player
	SendMessage(Player_name_list,CB_GETLBTEXT,(WPARAM)sel,(LPARAM)player);
	
	// if there is a valid player selected and he's the guy we want
	return ((strlen(player) != 0) && (strcmp(p->player->callsign,player) == 0)) ? 1 : 0;
}

// message handler for the player info tab
BOOL CALLBACK player_info_proc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	int val,player_num;	
	char callsign[40];

	switch(uMsg){
	// initialize the dialog
	case WM_INITDIALOG:
		// set the page handle
		Page_handles[PLAYER_INFO_PAGE] = hwndDlg;

		// intialize all the control
      std_pinfo_init_player_info_controls(hwndDlg);
		return 1;
		break;

	// a command message of some kind
	case WM_COMMAND:
		switch(HIWORD(wParam)){
		// a listbox selection change message
		case CBN_SELCHANGE :
			// get the newly selected item
			val = SendMessage(Player_name_list,CB_GETCURSEL,(WPARAM)0,(LPARAM)0);
			if(val!=CB_ERR){		
				// get the callsign
				if(SendMessage(Player_name_list,CB_GETLBTEXT,(WPARAM)val,(LPARAM)callsign) != CB_ERR){				
					// lookup the player
					player_num = multi_find_player_by_callsign(callsign);
					
					// if we found him then display his info
					if(player_num != -1){
						std_pinfo_display_player_info(&Net_players[player_num]);
					}
				}
			}
			break;
		}
      break;
	
	// a notification message
	case WM_NOTIFY :
		// set our page to be the active one
		if(((LPNMHDR)lParam)->code == PSN_SETACTIVE){
			Active_standalone_page = PLAYER_INFO_PAGE;
		} else if ( (((LPNMHDR)lParam)->code == PSN_APPLY) || (((LPNMHDR)lParam)->code == PSN_RESET) ) {
			PostMessage( Psht, WM_DESTROY, 0, 0 );
		}
		break;

	default :
		return 0;
		break;
	}
	return 0;
}


// ---------------------------------------------------------------------------------------
// player god stuff page/tab functions
//

#define GODSTUFF_MAX_ITEMS					19				// how many items we can fit on the chatbox at one time

static HWND God_player_list;								// the listbox of player callsigns	
static HWND Godstuff_fps;									// the framerate text box
static HWND Godstuff_broadcast_text;					// the text input box for sending messages to players
static HWND Godstuff_broadcast_button;					// the button to send the text messages
static HWND Godstuff_player_messages;					// handle to the list box containing player chatter

// initialize all the controls in the godstuff tab
void std_gs_init_godstuff_controls(HWND hwndDlg);

// add a player to the listbox on the godstuff page
void std_gs_add_god_player(net_player *p)
{
	// add the item
	SendMessage(God_player_list,CB_ADDSTRING,(WPARAM)0,(LPARAM)(LPCTSTR)p->player->callsign);	

		// if this is the first item on the list, then select it
	if(SendMessage(God_player_list,CB_GETCOUNT,(WPARAM)0,(LPARAM)0) == 1){
		// select the item
		SendMessage(God_player_list,CB_SETCURSEL,(WPARAM)0,(LPARAM)0);		
	}
}

// remove a player from the listbox on the godstuff page
void std_gs_remove_god_player(net_player *p)
{
	int loc;
	
	// lookup the player
	loc = SendMessage(God_player_list,CB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)(LPCTSTR)p->player->callsign);

	// if we found him, them delete the item
	if(loc!=CB_ERR){
		SendMessage(God_player_list,CB_DELETESTRING,(WPARAM)loc,(LPARAM)0);
	}
}

// send a message as if the standalone were a player
void std_gs_send_godstuff_message()
{
	char txt[256];	

	// get the text in the edit control
	memset(txt,0,256);
	txt[0]=120;	
	SendMessage(Godstuff_broadcast_text,EM_GETLINE,(WPARAM)0,(LPARAM)&txt[0]);
	
	// if the string is not zero length
	if(strlen(txt) > 0){		
		// send a game chat packet
		send_game_chat_packet(Net_player, txt, MULTI_MSG_ALL,NULL);		

		// add the text to our own control		
		std_add_chat_text(txt, MY_NET_PLAYER_NUM,1);		

		// clear the text control
		SetWindowText(Godstuff_broadcast_text, "");
	}
}

// set the framerate text box for this page
void std_gs_set_framerate(float f)
{
	char fr[10];

	// set the window text
	sprintf(fr,"%.1f",f);
	SetWindowText(Godstuff_fps,fr);
}

// clear the godstuff page controlsv
void std_gs_clear_controls()
{		
	// clear the framerate area
	SetWindowText(Godstuff_fps, "0");
	
	// clear the text area 
	SetWindowText(Godstuff_broadcast_text,"");

	// reset the combo box	
	SendMessage(God_player_list, CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);

	// clear the player chatter listbox
	SendMessage(Godstuff_player_messages, LB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
}

// initialize all the controls in the godstuff tab
void std_gs_init_godstuff_controls(HWND hwndDlg)
{
	// initialize the player listbox control   
	God_player_list = GetDlgItem(Page_handles[GODSTUFF_PAGE], (int)MAKEINTRESOURCE(IDC_PLAYER_GOD_LIST));
	
	// initialize the framerate text box
	Godstuff_fps = GetDlgItem(Page_handles[GODSTUFF_PAGE], (int)MAKEINTRESOURCE(IDC_GODSTUFF_FPS));

	// initialize the messaging edit control
	Godstuff_broadcast_text = GetDlgItem(Page_handles[GODSTUFF_PAGE], (int)MAKEINTRESOURCE(IDC_GODSTUFF_BROADCAST));
	SendMessage(Godstuff_broadcast_text, EM_SETLIMITTEXT, (WPARAM)CHATBOX_MAX_LEN, (LPARAM)0);
	SendMessage(Godstuff_broadcast_text, EM_FMTLINES, (WPARAM)TRUE, (LPARAM)0);

	// create the player chatter list box
	Godstuff_player_messages = GetDlgItem(Page_handles[GODSTUFF_PAGE], (int)MAKEINTRESOURCE(IDC_GOD_CHAT));			
	
	// initialize the message broadcast button
	Godstuff_broadcast_button = GetDlgItem(Page_handles[GODSTUFF_PAGE], (int)MAKEINTRESOURCE(IDC_GODSTUFF_SENDMESS));
	// hide the button -- we can now process return key
	ShowWindow(Godstuff_broadcast_button, SW_HIDE);

}

// message handler for the godstuff tab
BOOL CALLBACK godstuff_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg){
	// initialize the dialog
	case WM_INITDIALOG:
		// setup the page handle
		Page_handles[GODSTUFF_PAGE] = hwndDlg;

		// initialize the controls for this page
      std_gs_init_godstuff_controls(hwndDlg);
		return 1;
		break;

	// a notification message
	case WM_NOTIFY :
		// set this page to be the currently active one
		if(((LPNMHDR)lParam)->code == PSN_SETACTIVE){
			Active_standalone_page = GODSTUFF_PAGE;
		} else if ( (((LPNMHDR)lParam)->code == PSN_APPLY) || (((LPNMHDR)lParam)->code == PSN_RESET) ) {
			PostMessage( Psht, WM_DESTROY, 0, 0 );
		}
		break;	


	// a command message of some kind
	case WM_COMMAND:		
		switch(HIWORD(wParam)){
		// a button click
		case BN_CLICKED :
			switch(LOWORD(wParam)){
			// send the message to the player
			case IDC_GODSTUFF_SENDMESS : 
				std_gs_send_godstuff_message();   
				break;
			}
			break;

		}
		break;

	default :
		return 0;
		break;
	}
	return 0;
}


// ---------------------------------------------------------------------------------------
// debug page/tab functions
//

static HWND Standalone_state_string;								// the text control box

// initialize the controls for the debug page
void std_debug_init_debug_controls(HWND hwndDlg);

// set the text on the standalones state indicator box
void std_debug_set_standalone_state_string(char *str)
{
   // set the text
	SetWindowText(Standalone_state_string,str);
}

// clear the debug page controls
void std_debug_clear_controls()
{
	// set the current debug state
	std_debug_set_standalone_state_string("");
}

// initialize the controls for the debug page
void std_debug_init_debug_controls(HWND hwndDlg)
{
	// create the state string text box
	Standalone_state_string = GetDlgItem(hwndDlg,(int)MAKEINTRESOURCE(IDC_STANDALONE_STATE));
	
	// standalone state indicator
	SetWindowText(Standalone_state_string,"");
}

// message handler for the godstuff tab
BOOL CALLBACK debug_proc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg){
	// initialize the dialog
	case WM_INITDIALOG:
		// setup the page handle
		Page_handles[DEBUG_PAGE] = hwndDlg;

		// intialize the controls for this page
      std_debug_init_debug_controls(hwndDlg);
		return 1;
		break;

	// a notification message
	case WM_NOTIFY :
		// set the currently active page to this one
		if(((LPNMHDR)lParam)->code == PSN_SETACTIVE){
			Active_standalone_page = DEBUG_PAGE;
		} else if ( (((LPNMHDR)lParam)->code == PSN_APPLY) || (((LPNMHDR)lParam)->code == PSN_RESET) ) {
			PostMessage( Psht, WM_DESTROY, 0, 0 );
		}
		break;

	default :
		return 0;
		break;
	}
	return 0;
}


// ---------------------------------------------------------------------------------------
// general functions
// 

// add a player and take care of updating all gui/data details
void std_add_player(net_player *p)
{	
	char ip_string[60];	
	
	// get his ip string and add it to the list
	psnet_addr_to_string(ip_string,&p->p_info.addr);
	std_connect_add_ip_string(ip_string);

	// add to the player info player list box, and update his info
	std_pinfo_add_player_list_item(p);
	std_pinfo_maybe_update_player_info(p);

	// add to the god stuff player list box
	std_gs_add_god_player(p);	

	// check to see if this guy is the host. 	
	std_connect_set_host_connect_status();	

	// set the connection count
	std_connect_set_connect_count();
}

// remove a player and take care of updateing all gui/data details
int std_remove_player(net_player *p)
{	
	int count;
	char ip_string[60];	
   
	// determine his ip string and remove it from the list
	psnet_addr_to_string(ip_string,&p->p_info.addr);
	std_connect_remove_ip_string(ip_string);

	// remove from the player info player list box
	std_pinfo_remove_player_list_item(p);

	// remove from the godstuff list box
	std_gs_remove_god_player(p);

	// update the host connect count	
	std_connect_set_host_connect_status();	

	// update the currently connected players
	count = std_connect_set_connect_count();

	if(count == 0){
		// multi_standalone_quit_game();      
		multi_quit_game(PROMPT_NONE);
		return 1;
	}

	return 0;
}

// set any relevant controls which display the framerate of the standalone
void std_set_standalone_fps(float fps)
{
	// set the framerate in the multiplayer dialog
	std_multi_set_framerate(fps);

	// set the framerate in the godstuff dialog
	std_gs_set_framerate(fps);
}

// update any relveant controls which display the ping for the given player
void std_update_player_ping(net_player *p)
{
	// update the ping on the connect page
	std_connect_update_ping(p);

	// update the ping on the player info page
	std_pinfo_update_ping(p);	
}

// reset everything in the standalone gui (all the controls)
void std_reset_standalone_gui()
{
	// clear the connect page controls
	std_connect_clear_controls();

	// clear the multi page controls
	std_multi_clear_controls();

	// clear the player info page controls
	std_pinfo_clear_controls();

	// clear the godstuff page controls
	std_gs_clear_controls();

	// clear the debug page controls
	std_debug_clear_controls();

	// set all framerate displays to 0
	std_set_standalone_fps((float)0);
	
	// set the mission time
	std_multi_set_standalone_missiontime((float)0);	

	// reset the stats update timestamp
	Standalone_stats_stamp = -1;

	// reset the netgame info timestamp
	Standalone_ng_stamp = -1;	
}

// do any gui related issues on the standalone (like periodically updating player stats, etc...)
void std_do_gui_frame()
{
	int idx;
	
	// check to see if the timestamp for updating player selected stats has popped
	if((Standalone_stats_stamp == -1) || timestamp_elapsed(Standalone_stats_stamp)){
		// reset the timestamp
		Standalone_stats_stamp = timestamp(STD_STATS_UPDATE_TIME);

		// update any player currently selected
		// there's probably a nicer way to do this, but...
		for(idx=0;idx<MAX_PLAYERS;idx++){
			if(MULTI_CONNECTED(Net_players[idx]) && (Net_player != &Net_players[idx])){
				if(std_pinfo_maybe_update_player_info(&Net_players[idx])){
					break;
				}
			}
		}
	}

	// check to see if the timestamp for updating the netgame information controls has popped
	if((Standalone_ng_stamp == -1) || timestamp_elapsed(Standalone_ng_stamp)){
		// reset the timestamp
		Standalone_ng_stamp = timestamp(STD_NG_UPDATE_TIME);

		// update the controls
		std_multi_update_netgame_info_controls();
	}
}

// notify the user that the standalone has failed to login to the tracker on startup
void std_notify_tracker_login_fail()
{
	MessageBox(Psht,XSTR("The standalone server has failed to log in to Parallax Online!",922),XSTR("VMT Warning!",923),MB_OK);
}

// attempt to log the standalone into the tracker
void std_tracker_login()
{
}

// reset all stand gui timestamps
void std_reset_timestamps()
{
	// reset the stats update stamp
	Standalone_stats_stamp = timestamp(STD_STATS_UPDATE_TIME);

	// reset the netgame controls update timestamp
	Standalone_ng_stamp = timestamp(STD_NG_UPDATE_TIME);
}

// add a line of text chat to the standalone
void std_add_chat_text(char *text,int player_index,int add_id)
{
	int num_items,ret_val;
	char format[512];

	// invalid player ?
	if(player_index == -1){
		return;
	}

	// format the chat text nicely
	if(add_id){
		if(MULTI_STANDALONE(Net_players[player_index])){
			sprintf(format,XSTR("<SERVER> %s",924),text);
		} else {
			sprintf(format,"%s: %s",Net_players[player_index].player->callsign,text);
		}
	} else {
		strcpy(format,text);
	}

	// insert the text string into the godstuff chat box and scroll it down to the bottom
	SendMessage(Godstuff_player_messages,LB_INSERTSTRING,(WPARAM)-1,(LPARAM)format);
	num_items = SendMessage(Godstuff_player_messages,LB_GETCOUNT,(WPARAM)0,(LPARAM)0);
	if(num_items > 19){
		ret_val = SendMessage(Godstuff_player_messages,LB_SETTOPINDEX,(WPARAM)num_items - GODSTUFF_MAX_ITEMS,(LPARAM)0);		
	}
}

// if the standalone is host password protected
int std_is_host_passwd()
{
	return (strlen(Multi_options_g.std_passwd) > 0) ? 1 : 0;
}

// change the default property sheet interface into something more useful
void std_mutate_sheet()
{
	HWND ok_button = NULL;
	HWND cancel_button = NULL;
	HWND apply_button = NULL;
	HWND help_button = NULL;	
	char lookup[512];
	
	// get the buttons on the property sheet itself
	HWND child = GetWindow(Psht,GW_CHILD);
	while(child != NULL){
		// get the text of the window
		memset(lookup,0,512);
		GetWindowText(child,lookup,511);
		
		// if its the OK button
		if(!stricmp(lookup,XSTR("ok",925))){
			ok_button = child;
		} 

		// if its the cancel button
		if(!stricmp(lookup,XSTR("cancel",926))){
			cancel_button = child;
		} 

		// if its the apply button
		if(!stricmp(lookup,XSTR("&apply",927))){
			apply_button = child;
		} 

		// if its the help button
		if(!stricmp(lookup,XSTR("help",928))){
			help_button = child;
		} 

		child = GetWindow(child,GW_HWNDNEXT);
	}

	// kill em all
	if(apply_button != NULL){		
		DestroyWindow(apply_button);
	}
	
	// rename the shutdown button and move it over a bit
	if(ok_button != NULL){				
		// set the text
		SetWindowText(ok_button,XSTR("Shutdown",929));

		// move it
		SetWindowPos(ok_button,
			HWND_TOP,
			Std_shutdown_coords[gr_screen.res][0],
			Std_shutdown_coords[gr_screen.res][1],
			0,0,
			SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
			);
	}

	// kill em all
	if(cancel_button != NULL){
		DestroyWindow(cancel_button);
	}	

	// kill em all
	if(help_button != NULL){
		DestroyWindow(help_button);
	}	

	// now we want to mess with the titlebar controls	
}

// if the given callsign is banned from the server
int std_player_is_banned(char *name)
{
	int idx;

	// go through the ban list
	for(idx=0;idx<Standalone_ban_count;idx++){
		if(!stricmp(name,Standalone_ban_list[idx])){
			return 1;
		}
	}

	// not banned
	return 0;
}

// add a callsign to the ban list
void std_add_ban(char *name)
{
	// if we've reached the max bans
	if(Standalone_ban_count >= STANDALONE_MAX_BAN){
		return;
	}

	// otherwise add it
	memset(Standalone_ban_list[Standalone_ban_count],0,CALLSIGN_LEN+1);
	strcpy(Standalone_ban_list[Standalone_ban_count++],name);
}


// -------------------------------------------------------------------------------
// property sheet/page creation and handling 
//

void std_init_property_pages()
{
	PROPSHEETPAGE *p;
	
	// connect tab
	p = &Pages[CONNECT_PAGE];
	p->dwSize = sizeof(PROPSHEETPAGE);
	p->dwFlags = PSP_DEFAULT;
	p->hInstance = GetModuleHandle(NULL);
	p->pszTemplate = MAKEINTRESOURCE(IDD_CONNECT);
	p->pszIcon = NULL;
	p->pfnDlgProc = (DLGPROC)connect_proc;
	p->pszTitle = XSTR("Connections",930);
	p->lParam = 0;
	p->pfnCallback = NULL;

   // multiplayer tab
	p = &Pages[MULTIPLAYER_PAGE];
	p->dwSize = sizeof(PROPSHEETPAGE);
	p->dwFlags = PSP_DEFAULT;
	p->hInstance = GetModuleHandle(NULL);
	p->pszTemplate = MAKEINTRESOURCE(IDD_MULTI);
	p->pszIcon = NULL;
	p->pfnDlgProc = (DLGPROC)multi_proc;
	p->pszTitle = XSTR("Multi-Player",931);
	p->lParam = 0;
	p->pfnCallback = NULL;

   // player tab
	p = &Pages[PLAYER_INFO_PAGE];
	p->dwSize = sizeof(PROPSHEETPAGE);
	p->dwFlags = PSP_DEFAULT;
	p->hInstance = GetModuleHandle(NULL);
	p->pszTemplate = MAKEINTRESOURCE(IDD_PLAYER_DIALOG);
	p->pszIcon = NULL;
	p->pfnDlgProc = (DLGPROC)player_info_proc;
	p->pszTitle = XSTR("Player info",932);
	p->lParam = 0;
	p->pfnCallback = NULL;

   // godstuff tab
	p = &Pages[GODSTUFF_PAGE];
	p->dwSize = sizeof(PROPSHEETPAGE);
	p->dwFlags = PSP_DEFAULT;
	p->hInstance = GetModuleHandle(NULL);
	p->pszTemplate = MAKEINTRESOURCE(IDD_GODSTUFF);
	p->pszIcon = NULL;
	p->pfnDlgProc = (DLGPROC)godstuff_proc;
	p->pszTitle = XSTR("God Stuff",933);
	p->lParam = 0;
	p->pfnCallback = NULL;

	// debug tab
	p = &Pages[DEBUG_PAGE];
	p->dwSize = sizeof(PROPSHEETPAGE);
	p->dwFlags = PSP_DEFAULT;
	p->hInstance = GetModuleHandle(NULL);
	p->pszTemplate = MAKEINTRESOURCE(IDD_DEBUG_DIALOG);
	p->pszIcon = NULL;
	p->pfnDlgProc = (DLGPROC)debug_proc;
	p->pszTitle = XSTR("Debug",934);
	p->lParam = 0;
	p->pfnCallback = NULL;
}

// build a title string
void std_build_title_string(char *str)
{
	char part1[256];
	char cat[256];
	char temp[256];

	// build the version #
	memset(part1, 0, 256);
	memset(cat, 0, 256);
	memset(temp, 0, 256);

	sprintf(part1, "%s %d.", XSTR("Freespace Standalone",935), FS_VERSION_MAJOR);
	if(FS_VERSION_MINOR < 10){
		strcpy(cat, "0");
		strcat(cat, itoa(FS_VERSION_MINOR, temp, 10));
	} else {
		sprintf(cat, "%d", FS_VERSION_MINOR);
	}
	strcat(part1, cat);
	strcat(part1, ".");
	if(FS_VERSION_BUILD < 10){
		strcpy(cat, "0");
		strcat(cat, itoa(FS_VERSION_BUILD, temp, 10));
	} else {
		sprintf(cat, "%d", FS_VERSION_BUILD);
	}
	strcat(part1, cat);	

	// first part
	strcpy(str, part1);

#ifdef STANDALONE_ONLY_BUILD
	sprintf(cat, "   %s %d", "Release", STANDALONE_ONLY_RELEASE_VERSION);
	strcat(str, cat);
#endif
}

// initialize the property sheet itself
HWND std_init_property_sheet(HWND hwndDlg)
{
	LONG styles;

	// initialize the property pages
	std_init_property_pages();
	
	// create the property sheet
	Sheet.dwSize = sizeof(PROPSHEETHEADER);
	Sheet.dwFlags = PSH_PROPSHEETPAGE | PSH_MODELESS | PSH_NOAPPLYNOW;
	Sheet.hwndParent = hwndDlg;
	Sheet.hInstance = GetModuleHandle(NULL);
	Sheet.nPages = MAX_STANDALONE_PAGES;

	// set the title bar appropriately
	char title_str[512];
	memset(title_str, 0, 512);
	std_build_title_string(title_str);
	Sheet.pszCaption = title_str;

	Sheet.nStartPage = 0;
	Sheet.ppsp = &Pages[0];
	Sheet.pfnCallback = NULL;
	Psht = (HWND)PropertySheet(&Sheet);

	// set the window style to include a minimize button
	//styles = GetWindowLong(Psht, GWL_STYLE );
	//if ( styles != 0 ) {
	//	SetWindowLong(Psht, GWL_STYLE, styles | WS_MINIMIZEBOX );
	//}

	styles = GetWindowLong(Psht, GWL_EXSTYLE );
	if ( styles != 0 ) {
		SetWindowLong( Psht, GWL_EXSTYLE, (styles & ~WS_EX_CONTEXTHELP) );
	}

	memset(Multi_options_g.std_pname, 0, MAX_GAMENAME_LEN+1);

	// change the default property sheet interface into something more useful
	std_mutate_sheet();

	// return a handle to this property sheet
	return Psht;
}

extern int Lighting_flag;

BOOL std_create_standalone_window()
{		
	Standalone_hwnd = std_init_property_sheet(NULL);

	// this is kind of a big-ass hack. But here's what it does. Property sheets only 
	// initialize their individual pages the first time (and ONLY the first time) they
	// are selected. So before any of this happens, their control handles are bogus. So,
	// by calling PropSheet_SetCurSel for all the pages, WM_INITDIALOG is sent to all of
	// them, and their controls become valid. However, its kind of silly because each 
	// page will blink into existence for a second. I can't see another way arount this
	// problem.
	int idx;
	for(idx=MAX_STANDALONE_PAGES-1;idx>=0;idx--){
	   PropSheet_SetCurSel(Psht,(HPROPSHEETPAGE)&Pages[idx],idx);
	}

//   main_window_inited = 1;

	// turn off lighting effects
	Lighting_flag = 0;

	// turn off all sound and music
	Cmdline_freespace_no_sound = 1;
	Cmdline_freespace_no_music = 1;

   // reset all standalone gui items
	std_reset_standalone_gui();

	// initialize the debug outwindow
	#ifndef NDEBUG
		outwnd_init();
	#endif

	return TRUE;
}

// just like the osapi version for the nonstandalone mode of Freespace
DWORD standalone_process(WORD lparam)
{
	MSG msg;

	if ( !std_create_standalone_window() )
		return 0;

	while (1)	{	
#if 0
	   if(PropSheet_GetCurrentPageHwnd(Psht)==NULL){
			mprintf(("prop sheet is destroyed -- exiting\n"));
			DestroyWindow(Psht);
			PostQuitMessage(0);
			os_close();
			std_deinit_standalone();
			break;
		}
#endif
		if (WaitMessage())	{
			while(PeekMessage(&msg,0,0,0,PM_REMOVE))	{

				// if the dialog should be destroyed, then exit.
				if ( msg.message == WM_DESTROY )	{
					DestroyWindow(Psht);
					PostQuitMessage(0);
					gameseq_post_event(GS_EVENT_QUIT_GAME);
					return 0;
				}

				// see if the message is destined for the edit control, and what the message is.
				// intercept if a return
				if ( msg.hwnd == Godstuff_broadcast_text ) {
					int virt_key;

					virt_key = (int)msg.wParam;
					if ( (msg.message == WM_KEYDOWN) && (virt_key == VK_RETURN) ) {
						std_gs_send_godstuff_message();
						continue;
					}
				}

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
	return 0;
}

// called when freespace initialized
void std_init_standalone()
{
	// start the main thread
	Standalone_thread = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)standalone_process, NULL, 0, &Standalone_thread_id );

	os_init_registry_stuff(Osreg_company_name, Osreg_app_name,NULL);
	
	// set the close functions
	atexit(std_deinit_standalone);
}

// called when freespace closes
void std_deinit_standalone()
{	
	if (Standalone_thread)	{
		CloseHandle(Standalone_thread);
		Standalone_thread = NULL;
	}
}

