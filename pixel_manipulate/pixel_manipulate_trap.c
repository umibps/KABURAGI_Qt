#include "pixel_manipulate.h"
#include "pixel_manipulate_private.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

PIXEL_MANIPULATE_FIXED PixelManipulateSampleCeilY(PIXEL_MANIPULATE_FIXED y, int n)
{
	PIXEL_MANIPULATE_FIXED f = PIXEL_MANIPULATE_FIXED_FRAC(y);
	PIXEL_MANIPULATE_FIXED i = PIXEL_MANIPULATE_FIXED_FLOOR(y);

	f = DIV(f - Y_FRAC_FIRST(n) + (STEP_Y_SMALL(n) - PIXEL_MANIPULATE_FIXED_E), STEP_Y_SMALL(n)) * STEP_Y_SMALL(n)
			+ Y_FRAC_FIRST(n);

	if(f > Y_FRAC_LAST(n))
	{
		if(PIXEL_MANIPULATE_FIXED_TO_INTEGER(i) == 0x7FFF)
		{
			f = 0xFFFF;
		}
		else
		{
			f = Y_FRAC_FIRST(n);
			i += PIXEL_MANIPULATE_FIXED_1;
		}
	}
	return (i | f);
}

PIXEL_MANIPULATE_FIXED PixelManipulateSampleFloorY(PIXEL_MANIPULATE_FIXED y, int n)
{
	PIXEL_MANIPULATE_FIXED f = PIXEL_MANIPULATE_FIXED_FRAC(y);
	PIXEL_MANIPULATE_FIXED i = PIXEL_MANIPULATE_FIXED_FLOOR(y);

	f = DIV(f - PIXEL_MANIPULATE_FIXED_E - Y_FRAC_FIRST(n), STEP_Y_SMALL(n)) * STEP_Y_SMALL(n)
			+ Y_FRAC_FIRST(n);

	if(f < Y_FRAC_FIRST(n))
	{
		if(PIXEL_MANIPULATE_FIXED_TO_INTEGER(i) == 0x8000)
		{
			f = 0;
		}
		else
		{
			f = Y_FRAC_LAST(n);
			i -= PIXEL_MANIPULATE_FIXED_1;
		}
	}
	return (i | f);
}

void PixelManipulateEdgeStep(PIXEL_MANIPULATE_EDGE* e, int n)
{
	PIXEL_MANIPULATE_FIXED_48_16 ne;

	e->x += n * e->stepx;

	ne = e->e + n * (PIXEL_MANIPULATE_FIXED_48_16)e->dx;

	if(n >= 0)
	{
		if(ne > 0)
		{
			int nx = (ne + e->dy - 1) / e->dy;
			e->e = ne - nx * (PIXEL_MANIPULATE_FIXED_48_16)e->dy;
			e->x += nx * e->signdx;
		}
	}
	else
	{
		if(ne <= -e->dy)
		{
			int nx = (-ne) / e->dy;
			e->e = ne + nx * (PIXEL_MANIPULATE_FIXED_48_16)e->dy;
			e->x -= nx * e->signdx;
		}
	}
}

static void InitializePixelManipulateEdgeMulti(
	PIXEL_MANIPULATE_EDGE* e,
	int n,
	PIXEL_MANIPULATE_FIXED* stepx_p,
	PIXEL_MANIPULATE_FIXED* dx_p
)
{
	PIXEL_MANIPULATE_FIXED stepx;
	PIXEL_MANIPULATE_FIXED_48_16 ne;

	ne = n * (PIXEL_MANIPULATE_FIXED_48_16)e->dx;
	stepx = n * e->stepx;

	if(ne > 0)
	{
		int nx = ne / e->dy;
		ne -= nx * (PIXEL_MANIPULATE_FIXED_48_16)e->dy;
		stepx += nx * e->signdx;
	}

	*dx_p = ne;
	*stepx_p = stepx;
}

void InitializePixelManipulateEdge(
	PIXEL_MANIPULATE_EDGE* e,
	int n,
	PIXEL_MANIPULATE_FIXED y_start,
	PIXEL_MANIPULATE_FIXED x_top,
	PIXEL_MANIPULATE_FIXED y_top,
	PIXEL_MANIPULATE_FIXED x_bottom,
	PIXEL_MANIPULATE_FIXED y_bottom
)
{
	PIXEL_MANIPULATE_FIXED dx, dy;

	e->x = x_top;
	e->e = 0;
	dx = x_bottom - x_top;
	dy = y_bottom - y_top;
	e->dy = dy;
	e->dx = 0;

	if(dy)
	{
		if(dx >= 0)
		{
			e->signdx = 1;
			e->stepx = dx / dy;
			e->dx = dx % dy;
			e->e = -dy;
		}
		else
		{
			e->signdx = -1;
			e->stepx = -(-dx / dy);
			e->dx = -dx % dy;
			e->e = 0;
		}

		InitializePixelManipulateEdgeMulti(e, STEP_Y_SMALL(n), &e->stepx_small, &e->dx_small);
		InitializePixelManipulateEdgeMulti(e, STEP_Y_BIG(n), &e->stepx_big, &e->dx_big);
	}
	PixelManipulateEdgeStep(e, y_start - y_top);
}

void InitializePixelManipulateLineFixedEdge(
	PIXEL_MANIPULATE_EDGE* e,
	int n,
	PIXEL_MANIPULATE_FIXED y,
	const PIXEL_MANIPULATE_LINE_FIXED* line,
	int x_offset,
	int y_offset
)
{
	PIXEL_MANIPULATE_FIXED x_offset_fixed = PIXEL_MANIPULATE_INTEGER_TO_FIXED(x_offset);
	PIXEL_MANIPULATE_FIXED y_offset_fixed = PIXEL_MANIPULATE_INTEGER_TO_FIXED(y_offset);
	const PIXEL_MANIPULATE_POINT_FIXED *top, *bottom;

	if(line->point1.y <= line->point2.y)
	{
		top = &line->point1;
		bottom = &line->point2;
	}
	else
	{
		top = &line->point2;
		bottom = &line->point1;
	}

	InitializePixelManipulateEdge(e, n, y, top->x + x_offset_fixed, top->y + x_offset_fixed,
		bottom->x + x_offset_fixed, bottom->y + y_offset_fixed);
}

void PixelManipulateAddTrapezoids(
	PIXEL_MANIPULATE_IMAGE* image,
	int16 x_off,
	int y_off,
	int ntraps,
	const PIXEL_MANIPULATE_TRAPEZOID* traps
)
{
	int i;

#if 0
	dump_image (image, "before");
#endif

	for(i = 0; i < ntraps; ++i)
	{
		const PIXEL_MANIPULATE_TRAPEZOID *trap = &(traps[i]);

		if (! PIXEL_MANIPULATE_TRAPEZPOID_VALID(trap))
			continue;

		PixelManipulateRasterizeTrapezoid(image, trap, x_off, y_off);
	}

#if 0
	dump_image (image, "after");
#endif
}

static int GreaterY(const PIXEL_MANIPULATE_POINT_FIXED* a, const PIXEL_MANIPULATE_POINT_FIXED* b)
{
	if (a->y == b->y)
		return a->x > b->x;
	return a->y > b->y;
}

/*
* Note that the definition of this function is a bit odd because
* of the X coordinate space (y increasing downwards).
*/
static int Clockwise(
	const PIXEL_MANIPULATE_POINT_FIXED* ref,
	const PIXEL_MANIPULATE_POINT_FIXED* a,
	const PIXEL_MANIPULATE_POINT_FIXED* b
)
{
	PIXEL_MANIPULATE_POINT_FIXED ad, bd;

	ad.x = a->x - ref->x;
	ad.y = a->y - ref->y;
	bd.x = b->x - ref->x;
	bd.y = b->y - ref->y;

	return ((PIXEL_MANIPULATE_FIXED_32_32)bd.y * ad.x -
		(PIXEL_MANIPULATE_FIXED_32_32)ad.y * bd.x) < 0;
}

static void TriangleToTrapezoids(const PIXEL_MANIPULATE_TRIANGLE* tri, PIXEL_MANIPULATE_TRAPEZOID* traps)
{
	const PIXEL_MANIPULATE_POINT_FIXED *top, *left, *right, *tmp;

	top = &tri->point1;
	left = &tri->point2;
	right = &tri->point3;

	if(GreaterY(top, left))
	{
		tmp = left;
		left = top;
		top = tmp;
	}

	if(GreaterY(top, right))
	{
		tmp = right;
		right = top;
		top = tmp;
	}

	if(Clockwise(top, right, left))
	{
		tmp = right;
		right = left;
		left = tmp;
	}

	/*
	* Two cases:
	*
	*		+		+
	*		   / \			 / \
	*		  /   \		   /	  \
	*		 /	 +		 +	   \
	*	  /	--		   --	\
	*	 /   --			   --   \
	*	/ ---				   --- \
	*	 +--						 --+
	*/

	traps->top = top->y;
	traps->left.point1 = *top;
	traps->left.point2 = *left;
	traps->right.point1 = *top;
	traps->right.point2 = *right;

	if (right->y < left->y)
		traps->bottom = right->y;
	else
		traps->bottom = left->y;

	traps++;

	*traps = *(traps - 1);

	if(right->y < left->y)
	{
		traps->top = right->y;
		traps->bottom = left->y;
		traps->right.point1 = *right;
		traps->right.point2 = *left;
	}
	else
	{
		traps->top = left->y;
		traps->bottom = right->y;
		traps->left.point1 = *left;
		traps->left.point2 = *right;
	}
}

static PIXEL_MANIPULATE_TRAPEZOID* ConvertTriangles(int n_tris, const PIXEL_MANIPULATE_TRIANGLE* tris)
{
	PIXEL_MANIPULATE_TRAPEZOID *traps;
	int i;

	if (n_tris <= 0)
		return NULL;

	traps = MEM_ALLOC_FUNC(n_tris * 2 * sizeof(PIXEL_MANIPULATE_TRAPEZOID));
	if (!traps)
		return NULL;

	for(i = 0; i < n_tris; ++i)
		TriangleToTrapezoids(&(tris[i]), traps + 2 * i);

	return traps;
}

void PixelManipulateRasterizeTrapezoid(
	PIXEL_MANIPULATE_IMAGE* image,
	const PIXEL_MANIPULATE_TRAPEZOID* trap,
	int x_offset,
	int y_offset
)
{
	int bpp;
	int height;

	PIXEL_MANIPULATE_FIXED y_offset_fixed;
	PIXEL_MANIPULATE_EDGE l, r;
	PIXEL_MANIPULATE_FIXED t, b;

	PixelManipulateImageValidate(image);

	if(FALSE == PIXEL_MANIPULATE_TRAPEZPOID_VALID(trap))
	{
		return;
	}

	height = image->bits.height;
	bpp = PIXEL_MANIPULATE_BIT_PER_PIXEL(image->bits.format);

	y_offset_fixed = PIXEL_MANIPULATE_INTEGER_TO_FIXED(y_offset);

	t = trap->top + y_offset_fixed;
	if(t < 0)
	{
		t = 0;
	}
	t = PixelManipulateSampleCeilY(t, bpp);

	b = trap->top + y_offset_fixed;
	if(PIXEL_MANIPULATE_FIXED_TO_INTEGER(b) >= height)
	{
		b = PIXEL_MANIPULATE_FIXED_TO_INTEGER(height) - 1;
	}
	b = PixelManipulateSampleFloorY(b, bpp);

	if(b >= t)
	{
		InitializePixelManipulateLineFixedEdge(&l, bpp, t, &trap->left, x_offset, y_offset);
		InitializePixelManipulateLineFixedEdge(&r, bpp, t, &trap->right, x_offset, y_offset);

		PixelManipulateRasterizeEdges(image, &l, &r, t, b);
	}
}

void PixelManipulateAddTriangles(
	PIXEL_MANIPULATE_IMAGE* image,
	int32 x_offset,
	int32 y_offset,
	int n_tris,
	const PIXEL_MANIPULATE_TRIANGLE* tris
)
{
	PIXEL_MANIPULATE_TRAPEZOID *traps;

	if((traps = ConvertTriangles(n_tris, tris)))
	{
		PixelManipulateAddTrapezoids(image, x_offset, y_offset,
			n_tris * 2, traps);

		MEM_FREE_FUNC(traps);
	}
}

#ifdef __cplusplus
}
#endif
