/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <object/object.hh>
#include <io/timer.hh>
#include <globalincs/systemvars.hh>
#include <globalincs/linklist.hh>
#include <parse/parselo.hh>
#include <weapon/muzzleflash.hh>
#include <bmpman/bmpman.hh>
#include <particle/particle.hh>

// ---------------------------------------------------------------------------------------------------------------------
// MUZZLE FLASH DEFINES/VARS
//

// muzzle flash info - read from a table
#define MAX_MFLASH_NAME_LEN 32
#define MAX_MFLASH_BLOBS 5
typedef struct mflash_info
{
    char name[MAX_MFLASH_NAME_LEN + 1];
    char blob_names[MAX_MFLASH_BLOBS][MAX_MFLASH_NAME_LEN + 1]; // blob anim name
    int blob_anims[MAX_MFLASH_BLOBS]; // blob anim
    float blob_offset[MAX_MFLASH_BLOBS]; // blob offset from muzzle
    float blob_radius[MAX_MFLASH_BLOBS]; // blob radius
    int num_blobs; // # of blobs
} mflash_info;
mflash_info Mflash_info[MAX_MUZZLE_FLASH_TYPES];
int Num_mflash_types = 0;

#define MAX_MFLASH 50

// Stuff for missile trails doesn't need to be saved or restored... or does it?
/*
typedef struct mflash { 
   struct   mflash * prev;
   struct   mflash * next;

   ubyte    type;                                                       // muzzle flash type
   int      blobs[MAX_MFLASH_BLOBS];                                    // blobs
} mflash;

int Num_mflash = 0;
mflash Mflash[MAX_MFLASH];

mflash Mflash_free_list;
mflash Mflash_used_list;
*/

// ---------------------------------------------------------------------------------------------------------------------
// MUZZLE FLASH FUNCTIONS
//

// initialize muzzle flash stuff for the whole game
void
mflash_game_init()
{
    mflash_info bogus;
    mflash_info *m;
    char name[MAX_MFLASH_NAME_LEN];
    float offset, radius;
    int idx;

    read_file_text("mflash.tbl");
    reset_parse();

    // header
    required_string("#Muzzle flash types");

    // read em in
    Num_mflash_types = 0;
    while (optional_string("$Mflash:")) {
        if (Num_mflash_types < MAX_MUZZLE_FLASH_TYPES) {
            m = &Mflash_info[Num_mflash_types++];
        }
        else {
            m = &bogus;
        }
        memset(m, 0, sizeof(mflash_info));
        for (idx = 0; idx < MAX_MFLASH_BLOBS; idx++) {
            m->blob_anims[idx] = -1;
        }

        required_string("+name:");
        stuff_string(m->name, F_NAME, NULL);

        // read in all blobs
        m->num_blobs = 0;
        while (optional_string("+blob_name:")) {
            stuff_string(name, F_NAME, NULL, MAX_MFLASH_NAME_LEN);

            required_string("+blob_offset:");
            stuff_float(&offset);

            required_string("+blob_radius:");
            stuff_float(&radius);

            // if we have room left
            if (m->num_blobs < MAX_MFLASH_BLOBS) {
                strcpy(m->blob_names[m->num_blobs], name);
                m->blob_offset[m->num_blobs] = offset;
                m->blob_radius[m->num_blobs] = radius;

                m->num_blobs++;
            }
        }
    }

    // close
    required_string("#end");
}

// initialize muzzle flash stuff for the level
void
mflash_level_init()
{
    int i, idx;
    int num_frames, fps;

    /*
   Num_mflash = 0;
   list_init( &Mflash_free_list );
   list_init( &Mflash_used_list );

   // Link all object slots into the free list
   for (i=0; i<MAX_MFLASH; i++)  {
      memset(&Mflash[i], 0, sizeof(mflash));
      list_append(&Mflash_free_list, &Mflash[i] );
   }
   */

    // load up all anims
    for (i = 0; i < Num_mflash_types; i++) {
        // blobs
        for (idx = 0; idx < Mflash_info[i].num_blobs; idx++) {
            Mflash_info[i].blob_anims[idx] = -1;
            Mflash_info[i].blob_anims[idx] = bm_load_animation(
                Mflash_info[i].blob_names[idx], &num_frames, &fps, 1);
            Assert(Mflash_info[i].blob_anims[idx] >= 0);
        }
    }
}

// shutdown stuff for the level
void
mflash_level_close()
{ }

// create a muzzle flash on the guy
void
mflash_create(vector *gun_pos, vector *gun_dir, int mflash_type)
{
    // mflash *mflashp;
    mflash_info *mi;
    particle_info p;
    int idx;

    // illegal value
    if ((mflash_type >= Num_mflash_types) || (mflash_type < 0)) {
        return;
    }

    /*
   if (Num_mflash >= MAX_MFLASH ) {
      #ifndef NDEBUG
      mprintf(("Muzzle flash creation failed - too many trails!\n" ));
      #endif
      return;
   }

   // Find next available trail
   mflashp = GET_FIRST(&Mflash_free_list);
   Assert( mflashp != &Mflash_free_list );      // shouldn't have the dummy element

   // remove trailp from the free list
   list_remove( &Mflash_free_list, mflashp );
   
   // insert trailp onto the end of used list
   list_append( &Mflash_used_list, mflashp );

   // store some stuff
   mflashp->type = (ubyte)mflash_type; 
   */

    // create the actual animations
    mi = &Mflash_info[mflash_type];
    for (idx = 0; idx < mi->num_blobs; idx++) {
        // bogus anim
        if (mi->blob_anims[idx] < 0) {
            continue;
        }

        // fire it up
        memset(&p, 0, sizeof(particle_info));
        vm_vec_scale_add(&p.pos, gun_pos, gun_dir, mi->blob_offset[idx]);
        p.vel = vmd_zero_vector;
        p.rad = mi->blob_radius[idx];
        p.type = PARTICLE_BITMAP;
        p.optional_data = mi->blob_anims[idx];
        p.attached_objnum = -1;
        p.attached_sig = 0;
        particle_create(&p);
    }

    // increment counter
    // Num_mflash++;
}

// process muzzle flash stuff
void
mflash_process_all()
{
    /*
   mflash *mflashp;

   // if the timestamp has elapsed recycle it
   mflashp = GET_FIRST(&Mflash_used_list);

   while ( mflashp!=END_OF_LIST(&Mflash_used_list) )  {        
      if((mflashp->stamp == -1) || timestamp_elapsed(mflashp->stamp)){
         // delete it from the list!
         mflash *next_one = GET_NEXT(mflashp);

         // remove objp from the used list
         list_remove( &Mflash_used_list, mflashp );

         // add objp to the end of the free
         list_append( &Mflash_free_list, mflashp );

         // decrement counter
         Num_mflash--;

         Assert(Num_mflash >= 0);
         
         mflashp = next_one;        
      } else { 
         mflashp = GET_NEXT(mflashp);
      }
   }
   */
}

void
mflash_render_all()
{ }

// lookup type by name
int
mflash_lookup(char *name)
{
    int idx;

    // look it up
    for (idx = 0; idx < Num_mflash_types; idx++) {
        if (!stricmp(name, Mflash_info[idx].name)) {
            return idx;
        }
    }

    // couldn't find it
    return -1;
}
