// Smoke tests for math/: fixed point, vecmat basics.  Exits nonzero on failure.

#include <math.h>
#include <stdio.h>

#include <globalincs/pstypes.hh>
#include <math/fix.hh>
#include <math/vecmat.hh>

static int failures = 0;

#define CHECK(expr)                                                              \
    do {                                                                         \
        if (!(expr)) {                                                           \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);      \
            failures++;                                                          \
        }                                                                        \
    } while (0)

int
main()
{
    // fixed point: 2.0 * 3.0 == 6.0, 1.0 / 4.0 == 0.25
    CHECK(fixmul(F1_0 * 2, F1_0 * 3) == F1_0 * 6);
    CHECK(fixdiv(F1_0, F1_0 * 4) == F1_0 / 4);
    CHECK(fixmuldiv(F1_0 * 6, F1_0 * 2, F1_0 * 3) == F1_0 * 4);

    // vecmat: magnitude of (3,4,0) == 5
    vector v;
    vm_vec_make(&v, 3.0f, 4.0f, 0.0f);
    CHECK(fabsf(vm_vec_mag(&v) - 5.0f) < 1e-5f);

    // normalize then magnitude == 1
    vm_vec_normalize(&v);
    CHECK(fabsf(vm_vec_mag(&v) - 1.0f) < 1e-5f);

    // cross product of x and y axes is z axis
    vector vx, vy, vz;
    vm_vec_make(&vx, 1.0f, 0.0f, 0.0f);
    vm_vec_make(&vy, 0.0f, 1.0f, 0.0f);
    vm_vec_crossprod(&vz, &vx, &vy);
    CHECK(fabsf(vz.z - 1.0f) < 1e-5f);

    // vm_matrix_to_rot_axis_and_angle at exactly 180 degrees, where the
    // axis is degenerate and gets rebuilt from the largest diagonal term.
    // Retail took the reciprocal of the axis component BEFORE writing it,
    // reading whatever the caller left in the out-param; the resulting NaN
    // axis then slipped through vm_matrix_interpolate's sign dispatch (NaN
    // fails both > 0 and < 0) and aborted inside away(). SM2-02 died on it.
    //
    // A half turn about the (1,1,0) axis: R = 2nn' - I, whose off-diagonal
    // terms are what the reciprocal scales -- a half turn about a COORDINATE
    // axis would multiply zeros and hide the defect.
    //
    // The poison value is the point of the test: the routine must not read
    // the out-param at all, so seeding it with a number that would visibly
    // corrupt the answer makes the old order fail deterministically instead
    // of depending on what the stack happened to hold.
    matrix half_turn;
    vm_set_identity(&half_turn);
    half_turn.a2d[0][0] = 0.0f;
    half_turn.a2d[0][1] = 1.0f;
    half_turn.a2d[1][0] = 1.0f;
    half_turn.a2d[1][1] = 0.0f;
    half_turn.a2d[2][2] = -1.0f;

    float theta = 0.0f;
    vector axis;
    vm_vec_make(&axis, 1.0e9f, 1.0e9f, 1.0e9f); // poison

    vm_matrix_to_rot_axis_and_angle(&half_turn, &theta, &axis);

    const float root_half = 0.70710678f;
    CHECK(fabsf(theta - PI) < 1e-5f);
    CHECK(!_isnan(axis.x) && !_isnan(axis.y) && !_isnan(axis.z));
    CHECK(fabsf(axis.x - root_half) < 1e-5f);
    CHECK(fabsf(axis.y - root_half) < 1e-5f);
    CHECK(fabsf(axis.z) < 1e-5f);

    if (failures == 0)
        printf("math_test: all passed\n");
    return failures ? 1 : 0;
}
