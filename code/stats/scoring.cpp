/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include "freespace.h"
#include "pstypes.h"
#include "object.h"
#include "ship.h"
#include "scoring.h"
#include "missionparse.h"
#include "hud.h"
#include "player.h"
#include "parselo.h"
#include "medals.h"
#include "localize.h"

// what percent of points of total damage to a ship a player has to have done to get an assist (or a kill) when it is killed
#define ASSIST_PERCENTAGE				(0.15f)
#define KILL_PERCENTAGE					(0.30f)

// these tables are overwritten with the values from rank.tbl
rank_stuff Ranks[NUM_RANKS];

// scoring scale factors by skill level
float Scoring_scale_factors[NUM_SKILL_LEVELS] = {
	0.2f,					// very easy
	0.4f,					// easy
	0.7f,					// medium
	1.0f,					// hard
	1.25f					// insane
};

void parse_rank_tbl()
{
	char buf[MULTITEXT_LENGTH];
	int rval, idx;

	if ((rval = setjmp(parse_abort)) != 0) {
		Error(LOCATION, "Error parsing 'rank.tbl'\r\nError code = %i.\r\n", rval);
	} 

	// open localization
	lcl_ext_open();

	read_file_text("rank.tbl");
	reset_parse();

	// parse in all the rank names
	idx = 0;
	skip_to_string("[RANK NAMES]");
	ignore_white_space();
	while ( required_string_either("#End", "$Name:") ) {
		Assert ( idx < NUM_RANKS );
		required_string("$Name:");
		stuff_string( Ranks[idx].name, F_NAME, NULL );
		required_string("$Points:");
		stuff_int( &Ranks[idx].points );
		required_string("$Bitmap:");
		stuff_string( Ranks[idx].bitmap, F_NAME, NULL );
		required_string("$Promotion Voice Base:");
		stuff_string( Ranks[idx].promotion_voice_base, F_NAME, NULL, MAX_FILENAME_LEN - 2 );
		required_string("$Promotion Text:");
		stuff_string(buf, F_MULTITEXT, NULL);
		drop_white_space(buf);
		compact_multitext_string(buf);
		Ranks[idx].promotion_text = strdup(buf);
		idx++;
	}

	required_string("#End");

	// be sure that all rank points are in order
#ifndef NDEBUG
	for ( idx = 0; idx < NUM_RANKS-1; idx++ ) {
		if ( Ranks[idx].points >= Ranks[idx+1].points )
			Int3();
	}
#endif

	// close localization
	lcl_ext_close();
}

// initialize a nice blank scoring element
void init_scoring_element(scoring_struct *s)
{
	int i;

	if (s == NULL) {
		Int3();	//	DaveB -- Fix this!
		// read_pilot_file(char* callsign);
		return;
	}

	memset(s, 0, sizeof(scoring_struct));
	s->score = 0;
	s->rank = RANK_ENSIGN;
	s->assists = 0;
	s->kill_count = 0;
	s->kill_count_ok = 0;

	for (i=0; i<NUM_MEDALS; i++){
		s->medals[i] = 0;
	}

	for (i=0; i<MAX_SHIP_TYPES; i++){
		s->kills[i] = 0;
		s->m_kills[i] = 0;
	}

	s->m_kill_count		= 0;
	s->m_kill_count_ok	= 0;

	s->m_score = 0;
	s->m_assists = 0;
   s->p_bonehead_hits=0; s->mp_bonehead_hits=0;
	s->s_bonehead_hits=0; s->ms_bonehead_hits=0;
	s->m_bonehead_kills=0;
	
	s->bonehead_kills=0;   
   
	s->p_shots_fired=0; s->p_shots_hit=0;
   s->s_shots_fired=0; s->s_shots_hit=0;

   s->mp_shots_fired=0; s->mp_shots_hit=0;
   s->ms_shots_fired=0; s->ms_shots_hit=0;

	s->m_player_deaths = 0;

   s->flags = 0;	

	s->missions_flown = 0;
	s->flight_time = 0;
	s->last_flown = 0;
	s->last_backup = 0;

	for(i=0; i<MAX_PLAYERS; i++){
		s->m_dogfight_kills[i] = 0;
	}
}

#ifndef NDEBUG
//XSTR:OFF
void scoring_eval_harbison( ship *shipp )
{
	FILE *fp;

	if ( !stricmp(shipp->ship_name, "alpha 2") && (!stricmp(Game_current_mission_filename, "demo01") || !stricmp(Game_current_mission_filename, "sm1-01")) ) {
		int death_count;

		fp = fopen("i:\\volition\\cww\\harbison.txt", "r+t");
		if ( !fp )
			return;
		fscanf(fp, "%d", &death_count );
		death_count++;
		fseek(fp, 0, SEEK_SET);
		fprintf(fp, "%d\n", death_count);
		fclose(fp);
	}
}
//XSTR:ON
#endif

// initialize the Player's mission-based stats before he goes into a mission
void scoring_level_init( scoring_struct *scp )
{
	int i;

	scp->m_medal_earned = -1;		// hasn't earned a medal yet
	scp->m_promotion_earned = -1;
	scp->m_badge_earned = -1;
   scp->m_score = 0;
	scp->m_assists = 0;
	scp->mp_shots_fired=0;
	scp->mp_shots_hit = 0;
	scp->ms_shots_fired = 0;
	scp->ms_shots_hit = 0;

	scp->mp_bonehead_hits=0;
	scp->ms_bonehead_hits=0;
	scp->m_bonehead_kills=0;

   for (i=0; i<MAX_SHIP_TYPES; i++){
		scp->m_kills[i] = 0;
		scp->m_okKills[i]=0;
	}

	scp->m_kill_count = 0;
	scp->m_kill_count_ok = 0;
	
	scp->m_player_deaths =0;

	for(i=0; i<MAX_PLAYERS; i++){
		scp->m_dogfight_kills[i] = 0;
	}
}

void scoring_eval_rank( scoring_struct *sc )
{
	int i, score, new_rank, old_rank;

	old_rank = sc->rank;
	new_rank = old_rank;

	// first check to see if the promotion flag is set -- if so, return the new rank
	if ( Player->flags & PLAYER_FLAGS_PROMOTED ) {
	
		new_rank = sc->rank;

		// if the player does indeed get promoted, we should change his mission score
		// to reflect the differce between all time and new rank score
		if ( sc->rank < MAX_FREESPACE2_RANK ) {
			new_rank = sc->rank + 1;
			if ( (sc->m_score + sc->score) < Ranks[new_rank].points )
				sc->m_score = (Ranks[new_rank].points - sc->score);
		}
	} else {
		// we get here only if player wasn't promoted automatically.
		score = sc->m_score + sc->score;
		for (i=0; i<NUM_RANKS; i++) {
			if ( score >= Ranks[i].points )
				new_rank = i;
		}
	}

	// if the ranks do not match, then "grant" the new rank
	if ( old_rank != new_rank ) {
		Assert( new_rank >= 0 );
		sc->m_promotion_earned = new_rank;
		sc->rank = new_rank;
	}
}

// function to evaluate whether or not a new badge is going to be awarded.  This function returns
// which medal is awarded.
void scoring_eval_badges(scoring_struct *sc)
{
	int i, total_kills, badge;

	// to determine badges, we count kills based on fighter/bomber types.  We must count kills in
	// all time stats + current mission stats.  And, only for enemy fighters/bombers
	total_kills = 0;
	for (i = 0; i < MAX_SHIP_TYPES; i++ ) {
		if ( (Ship_info[i].flags & SIF_FIGHTER) || (Ship_info[i].flags & SIF_BOMBER) ) {
			total_kills += sc->m_okKills[i];
			total_kills += sc->kills[i];
		}
	}

	// total_kills should now reflect the number of kills on hostile fighters/bombers.  Check this number
	// against badge kill numbers, and return the badge index if we would get a new one.
	badge = -1;
	for (i = 0; i < MAX_BADGES; i++ ) {
		if ( total_kills >= Medals[Badge_index[i]].kills_needed ){
			badge = i;
		}
	}

	// if player could have a badge based on kills, and doesn't currently have this badge, then
	// return the badge id.
	if ( (badge != -1 ) && (sc->medals[Badge_index[badge]] < 1) ) {
		sc->medals[Badge_index[badge]] = 1;
		sc->m_badge_earned = badge;
	}
}

// central point for dealing with accepting the score for a misison.
void scoring_do_accept(scoring_struct *score)
{
	int idx;

	// do rank, badges, and medals first since they require the alltime stuff
	// to not be updated yet.	

	// do medal stuff
	if ( score->m_medal_earned != -1 ){
		score->medals[score->m_medal_earned]++;
	}

	// return when in training mission.  We can grant a medal in training, but don't
	// want to calculate any other statistics.
	if (The_mission.game_type == MISSION_TYPE_TRAINING){
		return;
	}	

	scoring_eval_rank(score);
	scoring_eval_badges(score);

	score->kill_count += score->m_kill_count;
	score->kill_count_ok += score->m_kill_count_ok;

	score->score += score->m_score;
	score->assists += score->m_assists;
	score->p_shots_fired += score->mp_shots_fired;
	score->s_shots_fired += score->ms_shots_fired;

	score->p_shots_hit += score->mp_shots_hit;
	score->s_shots_hit += score->ms_shots_hit;

	score->p_bonehead_hits += score->mp_bonehead_hits;
	score->s_bonehead_hits += score->ms_bonehead_hits;
	score->bonehead_kills += score->m_bonehead_kills;

	for(idx=0;idx<MAX_SHIP_TYPES;idx++){
		score->kills[idx] = (unsigned short)(score->kills[idx] + score->m_okKills[idx]);
	}

	// add in mission time
	score->flight_time += (unsigned int)f2fl(Missiontime);
	score->last_backup = score->last_flown;
	score->last_flown = time(NULL);
	score->missions_flown++;
}

// backout the score for a mission.  This function gets called when the player chooses to refly a misison
// after debriefing
void scoring_backout_accept( scoring_struct *score )
{
	int idx;

	// if a badge was earned, take it back
	if ( score->m_badge_earned != -1){
		score->medals[Badge_index[score->m_badge_earned]] = 0;
	}

	// return when in training mission.  We can grant a medal in training, but don't
	// want to calculate any other statistics.
	if (The_mission.game_type == MISSION_TYPE_TRAINING){
		return;
	}

	score->kill_count -= score->m_kill_count;
	score->kill_count_ok -= score->m_kill_count_ok;

	score->score -= score->m_score;
	score->assists -= score->m_assists;
	score->p_shots_fired -= score->mp_shots_fired;
	score->s_shots_fired -= score->ms_shots_fired;

	score->p_shots_hit -= score->mp_shots_hit;
	score->s_shots_hit -= score->ms_shots_hit;

	score->p_bonehead_hits -= score->mp_bonehead_hits;
	score->s_bonehead_hits -= score->ms_bonehead_hits;
	score->bonehead_kills -= score->m_bonehead_kills;

	for(idx=0;idx<MAX_SHIP_TYPES;idx++){
		score->kills[idx] = (unsigned short)(score->kills[idx] - score->m_okKills[idx]);
	}

	// if the player was given a medal, take it back
	if ( score->m_medal_earned != -1 ) {
		score->medals[score->m_medal_earned]--;
		Assert( score->medals[score->m_medal_earned] >= 0 );
	}

	// if the player was promoted, take it back
	if ( score->m_promotion_earned != -1) {
		score->rank--;
		Assert( score->rank >= 0 );
	}	

	score->flight_time -= (unsigned int)f2fl(Missiontime);
	score->last_flown = score->last_backup;	
	score->missions_flown--;
}

// merge any mission stats accumulated into the alltime stats (as well as updating per campaign stats)
void scoring_level_close(int accepted)
{
	// want to calculate any other statistics.
	if (The_mission.game_type == MISSION_TYPE_TRAINING){
		// call scoring_do_accept
		// this will grant any potential medals and then early bail, and
		// then we will early bail
		scoring_do_accept(&Player->stats);
		return;
	}

	if(accepted){
		// apply mission stats
		nprintf(("General","Storing stats now\n"));
		scoring_do_accept( &Player->stats );

		// If this mission doesn't allow promotion or badges
		// then be sure that these don't get done.  Don't allow promotions or badges when
		// playing normally and not in a campaign.
		if ( (The_mission.flags & MISSION_FLAG_NO_PROMOTION) || ((Game_mode & GM_NORMAL) && !(Game_mode & GM_CAMPAIGN_MODE)) ) {
			if ( Player->stats.m_promotion_earned != -1) {
				Player->stats.rank--;
				Player->stats.m_promotion_earned = -1;
			}

			// if a badge was earned, take it back
			if ( Player->stats.m_badge_earned != -1){
				Player->stats.medals[Badge_index[Player->stats.m_badge_earned]] = -1;
				Player->stats.m_badge_earned = -1;
			}
		}

	} 	
}

// STATS damage, assists recording stuff
void scoring_add_damage(object *ship_obj,object *other_obj,float damage)
{
	int found_slot, signature;
	int lowest_index,idx;
	object *use_obj;
	ship *sp;

	// if we have no other object, bail
	if(other_obj == NULL){
		return;
	}	

	// for player kill/assist evaluation, we have to know exactly how much damage really mattered. For example, if
	// a ship had 1 hit point left, and the player hit it with a nuke, it doesn't matter that it did 10,000,000 
	// points of damage, only that 1 point would count
	float actual_damage = 0.0f;
	
	// other_obj might not always be the parent of other_obj (in the case of debug code for sure).  See
	// if the other_obj has a parent, and if so, use the parent.  If no parent, see if other_obj is a ship
	// and if so, use that ship.
	if ( other_obj->parent != -1 ){		
		use_obj = &Objects[other_obj->parent];
		signature = use_obj->signature;
	} else {
		signature = other_obj->signature;
		use_obj = other_obj;
	}
	
	// don't count damage done to a ship by himself
	if(use_obj == ship_obj){
		return;
	}

	// get a pointer to the ship and add the actual amount of damage done to it
	// get the ship object, and determine the _actual_ amount of damage done
	sp = &Ships[ship_obj->instance];
	// see comments at beginning of function
	if(ship_obj->hull_strength < 0.0f){
		actual_damage = damage + ship_obj->hull_strength;
	} else {
		actual_damage = damage;
	}
	if(actual_damage < 0.0f){
		actual_damage = 0.0f;
	}
	sp->total_damage_received += actual_damage;

	// go through and clear out all old damagers
	for(idx=0; idx<MAX_DAMAGE_SLOTS; idx++){
		if((sp->damage_ship_id[idx] >= 0) && (ship_get_by_signature(sp->damage_ship_id[idx]) < 0)){
			sp->damage_ship_id[idx] = -1;
			sp->damage_ship[idx] = 0;
		}
	}

	// only evaluate possible kill/assist numbers if the hitting object (use_obj) is a piloted ship (ie, ignore asteroids, etc)
	// don't store damage a ship may do to himself
	if((ship_obj->type == OBJ_SHIP) && (use_obj->type == OBJ_SHIP)){
		found_slot = 0;
		// try and find an open slot
		for(idx=0;idx<MAX_DAMAGE_SLOTS;idx++){
			// if this ship object doesn't exist anymore, use the slot
			if((sp->damage_ship_id[idx] == -1) || (ship_get_by_signature(sp->damage_ship_id[idx]) < 0) || (sp->damage_ship_id[idx] == signature) ){
				found_slot = 1;
				break;
			}
		}

		// if not found (implying all slots are taken), then find the slot with the lowest damage % and use that
		if(!found_slot){
			lowest_index = 0;
			for(idx=0;idx<MAX_DAMAGE_SLOTS;idx++){
				if(sp->damage_ship[idx] < sp->damage_ship[lowest_index]){
				   lowest_index = idx;
				}
			}
		} else {
			lowest_index = idx;
		}

		// fill in the slot damage and damager-index
		if(found_slot){
			sp->damage_ship[lowest_index] += actual_damage;								
		} else {
			sp->damage_ship[lowest_index] = actual_damage;
		}
		sp->damage_ship_id[lowest_index] = signature;
	}	
}

// evaluate a kill on a ship
void scoring_eval_kill(object *ship_obj)
{		
	float max_damage_pct;		// the pct% of total damage the max damage object did
	int max_damage_index;		// the index into the dying ship's damage_ship[] array corresponding the greatest amount of damage
	int killer_sig;				// signature of the guy getting credit for the kill (or -1 if none)
	int idx;
	player *plr;					// pointer to a player struct if it was a player who got the kill
	ship *dead_ship;				// the ship which was killed

	// we don't evaluate kills on anything except ships
	if(ship_obj->type != OBJ_SHIP){
		return;	
	}
	if((ship_obj->instance < 0) || (ship_obj->instance >= MAX_SHIPS)){
		return;
	}

	// assign the dead ship
	dead_ship = &Ships[ship_obj->instance];

	// evaluate player deaths
	if(ship_obj == Player_obj){
		Player->stats.m_player_deaths++;
	}

	// if this ship doesn't show up on player sensors, then don't eval a kill
	if ( dead_ship->flags & SF_HIDDEN_FROM_SENSORS ){
		// make sure to set invalid killer id numbers
		dead_ship->damage_ship_id[0] = -1;
		dead_ship->damage_ship[0] = -1.0f;
		return;
	}

#ifndef NDEBUG
	scoring_eval_harbison( dead_ship );
#endif

	// clear out invalid damager ships
	for(idx=0; idx<MAX_DAMAGE_SLOTS; idx++){
		if((dead_ship->damage_ship_id[idx] >= 0) && (ship_get_by_signature(dead_ship->damage_ship_id[idx]) < 0)){
			dead_ship->damage_ship[idx] = 0.0f;
			dead_ship->damage_ship_id[idx] = -1;
		}
	}
			
	// determine which object did the most damage to the dying object, and how much damage that was
	max_damage_index = -1;
	for(idx=0;idx<MAX_DAMAGE_SLOTS;idx++){
		// bogus ship
		if(dead_ship->damage_ship_id[idx] < 0){
			continue;
		}

		// if this slot did more damage then the next highest slot
		if((max_damage_index == -1) || (dead_ship->damage_ship[idx] > dead_ship->damage_ship[max_damage_index])){
			max_damage_index = idx;
		}			
	}
	
	// doh
	if((max_damage_index < 0) || (max_damage_index >= MAX_DAMAGE_SLOTS)){
		return;
	}

	// the pct of total damage applied to this ship
	max_damage_pct = dead_ship->damage_ship[max_damage_index] / dead_ship->total_damage_received;
	if(max_damage_pct < 0.0f){
		max_damage_pct = 0.0f;
	} 
	if(max_damage_pct > 1.0f){
		max_damage_pct = 1.0f;
	}

	// only evaluate if the max damage % is high enough to record a kill and it was done by a valid object
	if((max_damage_pct >= KILL_PERCENTAGE) && (dead_ship->damage_ship_id[max_damage_index] >= 0)){
		// set killer_sig for this ship to the signature of the guy who gets credit for the kill
		killer_sig = dead_ship->damage_ship_id[max_damage_index];

		// null this out for now
		plr = NULL;

		// get the player
		if(Objects[Player->objnum].signature == killer_sig){
			plr = Player;
		}

		// if we found a valid player, evaluate some kill details
		if(plr != NULL){
			int si_index;

			// bogus
			if((plr->objnum < 0) || (plr->objnum >= MAX_OBJECTS)){
				return;
			}			

			// get the ship info index of the ship type of this kill.  we need to take ship
			// copies into account here.
			si_index = dead_ship->ship_info_index;
			if ( Ship_info[si_index].flags & SIF_SHIP_COPY ){
				si_index = ship_info_base_lookup( si_index );
			}

			// if you hit this next Assert, find allender.  If not here, don't worry about it, you may safely
			// continue
			Assert( !(Ship_info[si_index].flags & SIF_SHIP_COPY) );

			// if he killed a guy on his own team increment his bonehead kills
			if(Ships[Objects[plr->objnum].instance].team == dead_ship->team){
				plr->stats.m_bonehead_kills++;
				plr->stats.m_score -= (int)(dead_ship->score * scoring_get_scale_factor());
			}
			// otherwise increment his valid kill count and score
			else {
				plr->stats.m_okKills[si_index]++;
				plr->stats.m_kill_count_ok++;
				plr->stats.m_score += (int)(dead_ship->score * scoring_get_scale_factor());
				hud_gauge_popup_start(HUD_KILLS_GAUGE);
			}

			// increment his all-encompassing kills
			plr->stats.m_kills[si_index]++;
			plr->stats.m_kill_count++;
		}
	} else {
		// set killer_sig for this ship to -1, indicating no one got the kill for it
		killer_sig = -1;
	}		
		
	// pass in the guy who got the credit for the kill (if any), so that he doesn't also
	// get credit for an assist
	scoring_eval_assists(dead_ship,killer_sig);	

	// bash damage_ship_id[0] with the signature of the guy who is getting credit for the kill
	dead_ship->damage_ship_id[0] = killer_sig;
	dead_ship->damage_ship[0] = max_damage_pct;
}

// kill_id is the object signature of the guy who got the credit for the kill (may be -1, if no one got it)
// this is to insure that you don't also get an assist if you get the kill.
void scoring_eval_assists(ship *sp,int killer_sig)
{
	int idx;
	player *plr;

	// evaluate each damage slot to see if it did enough to give the assis
	for(idx=0;idx<MAX_DAMAGE_SLOTS;idx++){
		// if this slot did enough damage to get an assist
		if((sp->damage_ship[idx]/sp->total_damage_received) >= ASSIST_PERCENTAGE){
			// get the player which did this damage (if any)
			plr = NULL;

			if(Objects[Player->objnum].signature == sp->damage_ship_id[idx]){
				plr = Player;
			}

			// if we found a player, give him the assist if it wasn't on his own team
			if((plr != NULL) && (sp->team != Ships[Objects[plr->objnum].instance].team) && (killer_sig != Objects[plr->objnum].signature)){
				plr->stats.m_assists++;

				nprintf(("Network","-==============GAVE PLAYER %s AN ASSIST=====================-\n",plr->callsign));
				break;
			}
		}
	}
}

// eval a hit on an object (for primary and secondary hit purposes)
void scoring_eval_hit(object *hit_obj, object *other_obj,int from_blast)
{
	// only evaluate hits on ships and asteroids
	if((hit_obj->type != OBJ_SHIP) && (hit_obj->type != OBJ_ASTEROID)){
		return;
	}

	// if the other_obj == NULL, we can't evaluate where it came from, so bail here
	if(other_obj == NULL){
		return;
	}

	// other bogus situtations
	if(other_obj->instance < 0){
		return;
	}
	
	if((other_obj->type == OBJ_WEAPON) && !(Weapons[other_obj->instance].weapon_flags & WF_ALREADY_APPLIED_STATS)){		
		// bogus weapon
		if(other_obj->instance >= MAX_WEAPONS){
			return;
		}

		// bogus parent
		if(other_obj->parent < 0){
			return;
		}
		if(other_obj->parent >= MAX_SHIPS){
			return;
		}
		if(Objects[other_obj->parent].type != OBJ_SHIP){
			return;
		}
		if((Objects[other_obj->parent].instance < 0) || (Objects[other_obj->parent].instance >= MAX_SHIPS)){
			return;
		}		

		int is_bonehead = 0;
		int sub_type = Weapon_info[Weapons[other_obj->instance].weapon_info_index].subtype;

		// determine if this was a bonehead hit or not
		if(hit_obj->type == OBJ_SHIP){
		   is_bonehead = Ships[hit_obj->instance].team==Ships[Objects[other_obj->parent].instance].team ? 1 : 0;
		}
		// can't have a bonehead hit on an asteroid
		else {
			is_bonehead = 0;
		}

		// set the flag indicating that we've already applied a "stats" hit for this weapon
		// Weapons[other_obj->instance].weapon_flags |= WF_ALREADY_APPLIED_STATS;

		if(Player_obj == &(Objects[other_obj->parent])){
			switch(sub_type){
			case WP_LASER : 
				if(is_bonehead){
					Player->stats.mp_bonehead_hits++;
				} else {
					Player->stats.mp_shots_hit++; 
				}
				break;
			case WP_MISSILE :
				// friendly hit, once it hits a friendly, its done
				if(is_bonehead){					
					if(!from_blast){
						Player->stats.ms_bonehead_hits++;
					}					
				}
				// hostile hit
				else {
					// if its a bomb, count every bit of damage it does
					if(Weapons[other_obj->instance].weapon_flags & WIF_BOMB){
						// once we get impact damage, stop keeping track of it
						Player->stats.ms_shots_hit++;
					}
					// if its not a bomb, only count impact damage
					else {
						if(!from_blast){
							Player->stats.ms_shots_hit++;
						}
					}
				}				
				break;
			default : 
				break;
			}
		}
	}
}

// get a scaling factor for adding/subtracting from mission score
float scoring_get_scale_factor()
{
	// check for bogus Skill_level values
	Assert((Game_skill_level >= 0) && (Game_skill_level < NUM_SKILL_LEVELS));
	if((Game_skill_level < 0) || (Game_skill_level > NUM_SKILL_LEVELS-1)){
		return Scoring_scale_factors[0];
	}

	// return the correct scale value
	return Scoring_scale_factors[Game_skill_level];
}


// ----------------------------------------------------------------------------------------
// DCF functions
//

// bash the passed player to the specified rank
void scoring_bash_rank(player *pl,int rank)
{	
	// if this is an invalid rank, do nothing
	if((rank < RANK_ENSIGN) || (rank > RANK_ADMIRAL)){
		nprintf(("General","Could not bash player rank - invalid value!!!\n"));
		return;
	}

	// set the player's score and rank
	pl->stats.score = Ranks[rank].points + 1;
	pl->stats.rank = rank;
}

DCF(rank, "changes scoring vars")
{
	if(Dc_command){		
		dc_get_arg(ARG_INT);		
		
		// parse the argument and change things around accordingly		
		if((Dc_arg_type & ARG_INT) && (Player != NULL)){							
			scoring_bash_rank(Player,Dc_arg_int);
		}		
	}
	dc_printf("Usage\n0 : Ensign\n1 : Lieutenant Junior Grade\n");
	dc_printf("2 : Lietenant\n3 : Lieutenant Commander\n");
	dc_printf("4 : Commander\n5 : Captain\n6 : Commodore\n");
	dc_printf("7 : Rear Admiral\n8 : Vice Admiral\n9 : Admiral");
}
