/* pbm.h:
 *
 * PBM header file
 *
 * jim frost 10.15.89
 */

typedef struct {
	byte width[2];
	byte height[2];
} PBMCompact;

#define PM_SCALE(a, b, c) (long)((a) * (c))/(b)
