// Smoke tests for math/: fixed point, vecmat basics.  Exits nonzero on failure.

#include <math.h>
#include <stdio.h>

#include <globalincs/pstypes.hh>
#include <math/fix.hh>
#include <math/vecmat.hh>

static int failures = 0;

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
		failures++; \
	} \
} while (0)

int main()
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

	if (failures == 0)
		printf("math_test: all passed\n");
	return failures ? 1 : 0;
}
