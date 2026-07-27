/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <graphics/2d.hh>
#include <render/3dinternal.hh>

#define MIN_Z 0.0f

//Codes a vector.  Returns the codes of a point.
ubyte
g3_code_vector(vector *p)
{
    ubyte cc = 0;

    if (p->x > p->z)
        cc |= CC_OFF_RIGHT;

    if (p->y > p->z)
        cc |= CC_OFF_TOP;

    if (p->x < -p->z)
        cc |= CC_OFF_LEFT;

    if (p->y < -p->z)
        cc |= CC_OFF_BOT;

    if (p->z < MIN_Z)
        cc |= CC_BEHIND;

    if (G3_user_clip) {
        // Check if behind user plane
        if (g3_point_behind_user_plane(p)) {
            cc |= CC_OFF_USER;
        }
    }

    return cc;
}

//code a point.  fills in the p3_codes field of the point, and returns the codes
ubyte
g3_code_vertex(vertex *p)
{
    ubyte cc = 0;

    if (p->x > p->z)
        cc |= CC_OFF_RIGHT;

    if (p->y > p->z)
        cc |= CC_OFF_TOP;

    if (p->x < -p->z)
        cc |= CC_OFF_LEFT;

    if (p->y < -p->z)
        cc |= CC_OFF_BOT;

    if (p->z < MIN_Z)
        cc |= CC_BEHIND;

    if (G3_user_clip) {
        // Check if behind user plane
        if (g3_point_behind_user_plane((vector *)&p->x)) {
            cc |= CC_OFF_USER;
        }
    }

    return p->codes = cc;
}

MONITOR(NumRotations);

ubyte
g3_rotate_vertex(vertex *dest, vector *src)
{
#if 0
	vector tempv;
	Assert( G3_count == 1 );
	vm_vec_sub(&tempv,src,&View_position);
	vm_vec_rotate( (vector *)&dest->x, &tempv, &View_matrix );
	dest->flags = 0;	//not projected
	return g3_code_vertex(dest);
#else
    float tx, ty, tz, x, y, z;
    ubyte codes;

    MONITOR_INC(NumRotations, 1);

    tx = src->x - View_position.x;
    ty = src->y - View_position.y;
    tz = src->z - View_position.z;

    x = tx * View_matrix.rvec.x;
    x += ty * View_matrix.rvec.y;
    x += tz * View_matrix.rvec.z;

    y = tx * View_matrix.uvec.x;
    y += ty * View_matrix.uvec.y;
    y += tz * View_matrix.uvec.z;

    z = tx * View_matrix.fvec.x;
    z += ty * View_matrix.fvec.y;
    z += tz * View_matrix.fvec.z;

    codes = 0;

    if (x > z)
        codes |= CC_OFF_RIGHT;
    if (x < -z)
        codes |= CC_OFF_LEFT;
    if (y > z)
        codes |= CC_OFF_TOP;
    if (y < -z)
        codes |= CC_OFF_BOT;
    if (z < MIN_Z)
        codes |= CC_BEHIND;

    dest->x = x;
    dest->y = y;
    dest->z = z;

    if (G3_user_clip) {
        // Check if behind user plane
        if (g3_point_behind_user_plane((vector *)&dest->x)) {
            codes |= CC_OFF_USER;
        }
    }

    dest->codes = codes;

    dest->flags = 0; // not projected

    return codes;
#endif
}

ubyte
g3_rotate_faraway_vertex(vertex *dest, vector *src)
{
    Assert(G3_count == 1);

    MONITOR_INC(NumRotations, 1);

    vm_vec_rotate((vector *)&dest->x, src, &View_matrix);
    dest->flags = 0; //not projected
    return g3_code_vertex(dest);
}

//rotates a point. returns codes.  does not check if already rotated
ubyte
g3_rotate_vector(vector *dest, vector *src)
{
    vector tempv;

    Assert(G3_count == 1);

    MONITOR_INC(NumRotations, 1);

    vm_vec_sub(&tempv, src, &View_position);
    vm_vec_rotate(dest, &tempv, &View_matrix);
    return g3_code_vector(dest);
}

ubyte
g3_project_vector(vector *p, float *sx, float *sy)
{
    float w;

    Assert(G3_count == 1);

    if (p->z <= MIN_Z)
        return PF_OVERFLOW;

    w = 1.0f / p->z;

    *sx = (Canvas_width + (p->x * Canvas_width * w)) * 0.5f;
    *sy = (Canvas_height - (p->y * Canvas_height * w)) * 0.5f;
    return PF_PROJECTED;
}

//projects a point. Checks for overflow.

int
g3_project_vertex(vertex *p)
{
    float w;

    Assert(G3_count == 1);

    if (p->flags & PF_PROJECTED)
        return p->flags;

    //if ( p->z < MIN_Z ) {
    if (p->z <= MIN_Z) {
        p->flags |= PF_OVERFLOW;
    }
    else {
        // w = (p->z == 0.0f) ? 100.0f : 1.0f / p->z;
        w = 1.0f / p->z;
        p->sx = (Canvas_width + (p->x * Canvas_width * w)) * 0.5f;
        p->sy = (Canvas_height - (p->y * Canvas_height * w)) * 0.5f;

        if (w > 1.0f)
            w = 1.0f;
        p->sw = w;
        p->flags |= PF_PROJECTED;
    }

    return p->flags;
}

//from a 2d point, compute the vector through that point
void
g3_point_to_vec(vector *v, int sx, int sy)
{
    vector tempv;

    Assert(G3_count == 1);

    tempv.x = ((float)sx - Canv_w2) / Canv_w2;
    tempv.y = -((float)sy - Canv_h2) / Canv_h2;
    tempv.z = 1.0f;

    tempv.x = tempv.x * Matrix_scale.z / Matrix_scale.x;
    tempv.y = tempv.y * Matrix_scale.z / Matrix_scale.y;

    vm_vec_normalize(&tempv);
    vm_vec_unrotate(v, &tempv, &Unscaled_matrix);
}

//from a 2d point, compute the vector through that point.
// This can be called outside of a g3_start_frame/g3_end_frame
// pair as long g3_start_frame was previously called.
void
g3_point_to_vec_delayed(vector *v, int sx, int sy)
{
    vector tempv;

    tempv.x = ((float)sx - Canv_w2) / Canv_w2;
    tempv.y = -((float)sy - Canv_h2) / Canv_h2;
    tempv.z = 1.0f;

    tempv.x = tempv.x * Matrix_scale.z / Matrix_scale.x;
    tempv.y = tempv.y * Matrix_scale.z / Matrix_scale.y;

    vm_vec_normalize(&tempv);
    vm_vec_unrotate(v, &tempv, &Unscaled_matrix);
}

vector *
g3_rotate_delta_vec(vector *dest, vector *src)
{
    Assert(G3_count == 1);
    return vm_vec_rotate(dest, src, &View_matrix);
}

//	vms_vector tempv;
//	vms_matrix tempm;
//
//	tempv.x =  fixmuldiv(fixdiv((sx<<16) - Canv_w2,Canv_w2),Matrix_scale.z,Matrix_scale.x);
//	tempv.y = -fixmuldiv(fixdiv((sy<<16) - Canv_h2,Canv_h2),Matrix_scale.z,Matrix_scale.y);
//	tempv.z = f1_0;
//
//	vm_vec_normalize(&tempv);
//
//	vm_copy_transpose_matrix(&tempm,&Unscaled_matrix);
//
//	vm_vec_rotate(v,&tempv,&tempm);

/*

//from a 2d point, compute the vector through that point
void g3_point_2_vec(vector *v,int sx,int sy)
{
	vector tempv;
	matrix tempm;

	tempv.x =  fixmuldiv(fixdiv((sx<<16) - Canv_w2,Canv_w2),Matrix_scale.z,Matrix_scale.x);
	tempv.y = -fixmuldiv(fixdiv((sy<<16) - Canv_h2,Canv_h2),Matrix_scale.z,Matrix_scale.y);
	tempv.z = f1_0;

	vm_vec_normalize(&tempv);

	vm_copy_transpose_matrix(&tempm,&Unscaled_matrix);

	vm_vec_rotate(v,&tempv,&tempm);

}

//delta rotation functions
vms_vector *g3_rotate_delta_x(vms_vector *dest,fix dx)
{
	dest->x = fixmul(View_matrix.rvec.x,dx);
	dest->y = fixmul(View_matrix.uvec.x,dx);
	dest->z = fixmul(View_matrix.fvec.x,dx);

	return dest;
}

vms_vector *g3_rotate_delta_y(vms_vector *dest,fix dy)
{
	dest->x = fixmul(View_matrix.rvec.y,dy);
	dest->y = fixmul(View_matrix.uvec.y,dy);
	dest->z = fixmul(View_matrix.fvec.y,dy);

	return dest;
}

vms_vector *g3_rotate_delta_z(vms_vector *dest,fix dz)
{
	dest->x = fixmul(View_matrix.rvec.z,dz);
	dest->y = fixmul(View_matrix.uvec.z,dz);
	dest->z = fixmul(View_matrix.fvec.z,dz);

	return dest;
}



ubyte g3_add_delta_vec(g3s_point *dest,g3s_point *src,vms_vector *deltav)
{
	vm_vec_add(&dest->p3_vec,&src->p3_vec,deltav);

	dest->p3_flags = 0;		//not projected

	return g3_code_point(dest);
}
*/

// calculate the depth of a point - returns the z coord of the rotated point
float
g3_calc_point_depth(vector *pnt)
{
    float q;

    q = (pnt->x - View_position.x) * View_matrix.fvec.x;
    q += (pnt->y - View_position.y) * View_matrix.fvec.y;
    q += (pnt->z - View_position.z) * View_matrix.fvec.z;

    return q;
}
