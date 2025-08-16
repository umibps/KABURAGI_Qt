#include "graphics_types.h"
#include "graphics_list.h"
#include "graphics_private.h"
#include "graphics_region.h"
#include "../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

eGRAPHICS_FORMAT GetGraphicsFormatFromContent(eGRAPHICS_CONTENT content)
{
	switch(content)
	{
	case GRAPHICS_CONTENT_COLOR_ALPHA:
		return GRAPHICS_FORMAT_ARGB32;
	case GRAPHICS_CONTENT_ALPHA:
		return GRAPHICS_FORMAT_A8;
	}

	return (eGRAPHICS_FORMAT)-1;
}

eGRAPHICS_CONTENT GetGraphicsContentFromFormat(eGRAPHICS_FORMAT format)
{
	switch(format)
	{
	case GRAPHICS_FORMAT_ARGB32:
		return GRAPHICS_CONTENT_COLOR_ALPHA;
	case GRAPHICS_FORMAT_RGB24:
		return GRAPHICS_CONTENT_COLOR;
	case GRAPHICS_FORMAT_A8:
	case GRAPHICS_FORMAT_A1:
		return GRAPHICS_CONTENT_ALPHA;
	}

	return GRAPHICS_CONTENT_INVALID;
}

const void* GraphicsArrayIndexConst(const GRAPHICS_ARRAY* array, unsigned int index)
{
	if(index == 0 && array->num_elements == 0)
	{
		return NULL;
	}

	ASSERT(index < array->num_elements);

	return (const void*)(array->elements + index * array->element_size);
}

void ReleaseGraphcisArray(GRAPHICS_ARRAY* array)
{
	MEM_FREE_FUNC(array->elements);
}

void GraphicsObserversNotify(GRAPHICS_LIST* observers, void* arg)
{
	GRAPHICS_OBSERVER *obs, *next;

	for(obs = (GRAPHICS_OBSERVER*)observers->next; obs != (GRAPHICS_OBSERVER*)observers; obs = next)
	{
		next = (GRAPHICS_OBSERVER*)obs->link.next;
		obs->callback(obs, arg);
	}
}

INLINE uint16 GraphicsColorFloat2Short(FLOAT_T value)
{
	return (uint16)(value * (FLOAT_T)USHRT_MAX + 0.5);
}

static INLINE void GraphicsColorComputeShorts(GRAPHICS_COLOR* color)
{
	color->red_short = GraphicsColorFloat2Short(color->red * color->alpha);
	color->green_short = GraphicsColorFloat2Short(color->green * color->alpha);
	color->blue_short = GraphicsColorFloat2Short(color->blue * color->alpha);
	color->alpha_short = GraphicsColorFloat2Short(color->alpha);
}

void InitializeGraphicsColorRGBA(
	GRAPHICS_COLOR* color,
	FLOAT_T red,
	FLOAT_T green,
	FLOAT_T blue,
	FLOAT_T alpha
)
{
	color->red = red;
	color->green = green;
	color->blue = blue;
	color->alpha = alpha;

	GraphicsColorComputeShorts(color);
}

void InitializeGraphicsColorRGB(
	GRAPHICS_COLOR* color,
	FLOAT_T red,
	FLOAT_T green,
	FLOAT_T blue
)
{
	InitializeGraphicsColorRGBA(color, red, green, blue, 1.0);
}

INLINE int GraphicsColorEqual(const GRAPHICS_COLOR* color1, const GRAPHICS_COLOR* color2)
{
	if(color1 == color2)
	{
		return TRUE;
	}

	if(color1->alpha_short != color2->alpha_short)
	{
		return FALSE;
	}

	if(color1->alpha_short == 0)
	{
		return TRUE;
	}

	return color1->red_short == color2->red_short
			&& color1->green_short == color2->green_short
			&& color1->blue_short == color2->blue_short;
}

INLINE int GraphicsColorStopEqual(const GRAPHICS_COLOR_STOP *a, const GRAPHICS_COLOR_STOP* b)
{
	if(a == b)
	{
		return TRUE;
	}

	return a->alpha_short == b->alpha_short
		&& a->red_short == b->red_short
		&& a->green_short == b->green_short
		&& a->blue_short == b->blue_short;
}

INLINE void GraphicsColorMultiplyAlpha(GRAPHICS_COLOR* color, FLOAT_T alpha)
{
	color->alpha *= alpha;
	GraphicsColorComputeShorts(color);
}

void InitializeGraphicsTraps(GRAPHICS_TRAPS* traps)
{
	const GRAPHICS_TRAPS local_traps = {0};

	*traps = local_traps;
	traps->maybe_region = 1;
	traps->is_rectilinear = 0;
	traps->is_rectangular = 0;

	traps->num_traps = 0;

	traps->traps_size = sizeof(traps->traps_embedded) / sizeof(traps->traps_embedded[0]);
	traps->traps = traps->traps_embedded;

	traps->num_limits = 0;
	traps->has_intersections = FALSE;
}

void InitializeGraphicsTrapsWithClip(GRAPHICS_TRAPS* traps, const GRAPHICS_CLIP* clip)
{
	InitializeGraphicsTraps(traps);
	if(clip != NULL)
	{
		GraphicsTrapsLimit(traps, clip->boxes, clip->num_boxes);
	}
}

eGRAPHICS_STATUS GraphicsTrapsInitializeBoxes(GRAPHICS_TRAPS* traps, const GRAPHICS_BOXES* boxes)
{
	GRAPHICS_TRAPEZOID *trap;
	const GRAPHICS_BOXES_CHUNK *chunk;

	InitializeGraphicsTraps(traps);

	while(traps->traps_size < boxes->num_boxes)
	{
		if(UNLIKELY(FALSE == GraphicsTrapsGrow(traps)))
		{
			GraphicsTrapsFinish(traps);
			return GRAPHICS_STATUS_NO_MEMORY;
		}
	}

	traps->num_traps = boxes->num_boxes;
	traps->is_rectilinear = TRUE;
	traps->is_rectangular = TRUE;
	traps->maybe_region = boxes->is_pixel_aligned;

	trap = &traps->traps[0];
	for(chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next)
	{
		const GRAPHICS_BOX* box;
		int i;

		box = chunk->base;
		for(i = 0; i < chunk->count; i++)
		{
			trap->top = box->point1.y;
			trap->bottom = box->point2.y;

			trap->left.point1 = box->point1;
			trap->left.point2.x = box->point1.x;
			trap->left.point2.y = box->point2.y;

			trap->right.point1.x = box->point2.x;
			trap->right.point1.y = box->point1.y;
			trap->right.point2 = box->point2;

			box++, trap++;
		}
	}

	return GRAPHICS_STATUS_SUCCESS;
}

void GraphicsTrapsClear(GRAPHICS_TRAPS* traps)
{
	traps->status = GRAPHICS_STATUS_SUCCESS;

	traps->maybe_region = TRUE;
	traps->is_rectilinear = FALSE;
	traps->is_rectangular = FALSE;

	traps->num_traps = 0;
	traps->has_intersections = FALSE;
}

void GraphicsTrapsLimit(GRAPHICS_TRAPS* traps, const GRAPHICS_BOX* limits, int num_limits)
{
	int i;

	traps->limits = limits;
	traps->num_limits = num_limits;

	traps->bounds = limits[0];
	for(i=1; i<num_limits; i++)
	{
		GraphicsBoxAddBox(&traps->bounds, &limits[i]);
	}
}

int GraphicsTrapsGrow(GRAPHICS_TRAPS* traps)
{
	GRAPHICS_TRAPEZOID *new_traps;
	int new_size = 4 * traps->traps_size;

	if(traps->traps == traps->traps_embedded)
	{
		new_traps = (GRAPHICS_TRAPEZOID*)MEM_ALLOC_FUNC(new_size * sizeof(*new_traps));
		(void)memcpy(new_traps, traps->traps, sizeof(traps->traps_embedded));
	}
	else
	{
		new_traps = (GRAPHICS_TRAPEZOID*)MEM_REALLOC_FUNC(
			traps->traps, new_size * sizeof(*new_traps));
	}

	if(new_traps == NULL)
	{
		traps->status = GRAPHICS_STATUS_NO_MEMORY;
		return FALSE;
	}

	traps->traps = new_traps;
	traps->traps_size = new_size;
	return TRUE;
}

void GraphicsTrapsAddTrap(
	GRAPHICS_TRAPS* traps,
	GRAPHICS_FLOAT_FIXED top,
	GRAPHICS_FLOAT_FIXED bottom,
	const GRAPHICS_LINE* left,
	const GRAPHICS_LINE* right
)
{
	GRAPHICS_TRAPEZOID *trap;

	if(UNLIKELY(traps->num_traps == traps->traps_size))
	{
		if(UNLIKELY(FALSE == GraphicsTrapsGrow(traps)))
		{
			return;
		}
	}

	trap = &traps->traps[traps->num_traps++];
	trap->top = top;
	trap->bottom = bottom;
	trap->left = *left;
	trap->right = *right;
}

static void GraphicsTrapsAddClippedTrap(
	GRAPHICS_TRAPS* traps,
	GRAPHICS_FLOAT_FIXED _top,
	GRAPHICS_FLOAT_FIXED _bottom,
	const GRAPHICS_LINE* _left,
	const GRAPHICS_LINE* _right
)
{
	/* Note: With the goofy trapezoid specification, (where an
	* arbitrary two points on the lines can specified for the left
	* and right edges), these limit checks would not work in
	* general. For example, one can imagine a trapezoid entirely
	* within the limits, but with two points used to specify the left
	* edge entirely to the right of the limits.  Fortunately, for our
	* purposes, cairo will never generate such a crazy
	* trapezoid. Instead, cairo always uses for its points the
	* extreme positions of the edge that are visible on at least some
	* trapezoid. With this constraint, it's impossible for both
	* points to be outside the limits while the relevant edge is
	* entirely inside the limits.
	*/
	if(traps->num_limits)
	{
		const GRAPHICS_BOX *b = &traps->bounds;
		GRAPHICS_FLOAT_FIXED top = _top, bottom = _bottom;
		GRAPHICS_LINE left = *_left, right = *_right;

		/* Trivially reject if trapezoid is entirely to the right or
		* to the left of the limits. */
		if(left.point1.x >= b->point2.x && left.point2.x >= b->point2.x)
		{
			return;
		}

		if(right.point1.x <= b->point1.x && right.point2.x <= b->point1.x)
		{
			return;
		}

		/* And reject if the trapezoid is entirely above or below */
		if(top >= b->point2.y || bottom <= b->point1.y)
		{
			return;
		}

		/* Otherwise, clip the trapezoid to the limits. We only clip
		* where an edge is entirely outside the limits. If we wanted
		* to be more clever, we could handle cases where a trapezoid
		* edge intersects the edge of the limits, but that would
		* require slicing this trapezoid into multiple trapezoids,
		* and I'm not sure the effort would be worth it. */
		if(top < b->point1.y)
		{
			top = b->point1.y;
		}

		if(bottom > b->point2.y)
		{
			bottom = b->point2.y;
		}

		if(left.point1.x <= b->point1.x && left.point2.x <= b->point1.x)
		{
			left.point1.x = left.point2.x = b->point1.x;
		}

		if(right.point1.x >= b->point2.x && right.point2.x >= b->point2.x)
		{
			right.point1.x = right.point2.x = b->point2.x;
		}

		/* Trivial discards for empty trapezoids that are likely to
		* be produced by our tessellators (most notably convex_quad
		* when given a simple rectangle).
		*/
		if(top >= bottom)
		{
			return;
		}

		/* cheap colinearity check */
		if(right.point1.x <= left.point1.x && right.point1.y == left.point1.y &&
			right.point2.x <= left.point2.x && right.point2.y == left.point2.y)
		{
			return;
		}

		GraphicsTrapsAddTrap(traps, top, bottom, &left, &right);
	}
	else
	{
		GraphicsTrapsAddTrap(traps, _top, _bottom, _left, _right);
	}
}

int GraphicsTrapsToBoxes(
	GRAPHICS_TRAPS* traps,
	eGRAPHICS_ANTIALIAS antialias,
	GRAPHICS_BOXES* boxes
)
{
	int i;

	for(i = 0; i < traps->num_traps; i++)
	{
		if(traps->traps[i].left.point1.x != traps->traps[i].left.point2.x
			|| traps->traps[i].right.point1.x != traps->traps[i].right.point2.x)
		{
			return FALSE;
		}
	}

	InitializeGraphicsBoxes(boxes);

	boxes->num_boxes = traps->num_traps;
	boxes->chunks.base = (GRAPHICS_BOX*)traps->traps;
	boxes->chunks.count = traps->num_traps;
	boxes->chunks.size = traps->num_traps;

	if(antialias != GRAPHICS_ANTIALIAS_NONE)
	{
		for(i = 0; i < traps->num_traps; i++)
		{
			GRAPHICS_FLOAT_FIXED x1 = traps->traps[i].left.point1.x;
			GRAPHICS_FLOAT_FIXED x2 = traps->traps[i].right.point1.x;
			GRAPHICS_FLOAT_FIXED y1 = traps->traps[i].top;
			GRAPHICS_FLOAT_FIXED y2 = traps->traps[i].bottom;

			boxes->chunks.base[i].point1.x = x1;
			boxes->chunks.base[i].point1.y = y1;
			boxes->chunks.base[i].point2.x = x2;
			boxes->chunks.base[i].point2.y = y2;

			if(boxes->is_pixel_aligned)
			{
				boxes->is_pixel_aligned =
					GRAPHICS_FIXED_IS_INTEGER(x1) && GRAPHICS_FIXED_IS_INTEGER(y1)
					&& GRAPHICS_FIXED_IS_INTEGER(x2) && GRAPHICS_FIXED_IS_INTEGER(y2);
			}
		}
	}
	else
	{
		boxes->is_pixel_aligned = TRUE;

		for(i = 0; i < traps->num_traps; i++)
		{
			GRAPHICS_FLOAT_FIXED x1 = traps->traps[i].left.point1.x;
			GRAPHICS_FLOAT_FIXED x2 = traps->traps[i].right.point1.x;
			GRAPHICS_FLOAT_FIXED y1 = traps->traps[i].top;
			GRAPHICS_FLOAT_FIXED y2 = traps->traps[i].bottom;

			boxes->chunks.base[i].point1.x = GRAPHICS_FIXED_ROUND_DOWN(x1);
			boxes->chunks.base[i].point1.y = GRAPHICS_FIXED_ROUND_DOWN(y1);
			boxes->chunks.base[i].point2.x = GRAPHICS_FIXED_ROUND_DOWN(x2);
			boxes->chunks.base[i].point2.y = GRAPHICS_FIXED_ROUND_DOWN(y2);
		}
	}

	return TRUE;
}

static int ComparePointFixedByY(const void* av, const void* bv)
{
	const GRAPHICS_POINT *a = av, *b = bv;
	int ret = a->y - b->y;
	if(ret == 0)
	{
		ret = a->x - b->x;
	}
	return ret;
}

void GraphicsTrapsTessellateConvexQuad(
	GRAPHICS_TRAPS* traps,
	const GRAPHICS_POINT q[4]
)
{
	int a, b, c, d;
	int i;
	GRAPHICS_SLOPE ab, ad;
	int b_left_of_d;
	GRAPHICS_LINE left;
	GRAPHICS_LINE right;

	/* Choose a as a point with minimal y */
	a = 0;
	for(i = 1; i < 4; i++)
		if(ComparePointFixedByY(&q[i], &q[a]) < 0)
			a = i;

	/* b and d are adjacent to a, while c is opposite */
	b = (a + 1) % 4;
	c = (a + 2) % 4;
	d = (a + 3) % 4;

	/* Choose between b and d so that b.y is less than d.y */
	if (ComparePointFixedByY(&q[d], &q[b]) < 0) {
		b = (a + 3) % 4;
		d = (a + 1) % 4;
	}

	/* Without freedom left to choose anything else, we have four
	* cases to tessellate.
	*
	* First, we have to determine the Y-axis sort of the four
	* vertices, (either abcd or abdc). After that we need to detemine
	* which edges will be "left" and which will be "right" in the
	* resulting trapezoids. This can be determined by computing a
	* slope comparison of ab and ad to determine if b is left of d or
	* not.
	*
	* Note that "left of" here is in the sense of which edges should
	* be the left vs. right edges of the trapezoid. In particular, b
	* left of d does *not* mean that b.x is less than d.x.
	*
	* This should hopefully be made clear in the lame ASCII art
	* below. Since the same slope comparison is used in all cases, we
	* compute it before testing for the Y-value sort. */

	/* Note: If a == b then the ab slope doesn't give us any
	* information. In that case, we can replace it with the ac (or
	* equivalenly the bc) slope which gives us exactly the same
	* information we need. At worst the names of the identifiers ab
	* and b_left_of_d are inaccurate in this case, (would be ac, and
	* c_left_of_d). */
	if(q[a].x == q[b].x && q[a].y == q[b].y)
	{
		INITIALIZE_GRAPHICS_SLOPE(&ab, &q[a], &q[c]);
	}
	else
	{
		INITIALIZE_GRAPHICS_SLOPE(&ab, &q[a], &q[b]);
	}

	INITIALIZE_GRAPHICS_SLOPE(&ad, &q[a], &q[d]);

	b_left_of_d = GraphicsSlopeCompare(&ab, &ad) > 0;

	if (q[c].y <= q[d].y)
	{
		if (b_left_of_d)
		{
			/* Y-sort is abcd and b is left of d, (slope(ab) > slope (ad))
			*
			*					  top bot left right
			*		_a  a  a
			*	  / /  /|  |\	  a.y b.y  ab   ad
			*	 b /  b |  b \
			*	/ /   | |   \ \	b.y c.y  bc   ad
			*   c /	c |	c \
			*  | /	  \|	 \ \  c.y d.y  cd   ad
			*  d		 d	   d
			*/
			left.point1  = q[a]; left.point2  = q[b];
			right.point1 = q[a]; right.point2 = q[d];
			GraphicsTrapsAddClippedTrap(traps, q[a].y, q[b].y, &left, &right);
			left.point1  = q[b]; left.point2  = q[c];
			GraphicsTrapsAddClippedTrap(traps, q[b].y, q[c].y, &left, &right);
			left.point1  = q[c]; left.point2  = q[d];
			GraphicsTrapsAddClippedTrap(traps, q[c].y, q[d].y, &left, &right);
		}
		else
		{
			/* Y-sort is abcd and b is right of d, (slope(ab) <= slope (ad))
			*
			*	   a  a  a_
			*	  /|  |\  \ \	 a.y b.y  ad  ab
			*	 / b  | b  \ b
			*	/ /   | |   \ \   b.y c.y  ad  bc
			*   / c	| c	\ c
			*  / /	 |/	  \ | c.y d.y  ad  cd
			*  d	   d		 d
			*/
			left.point1  = q[a]; left.point2  = q[d];
			right.point1 = q[a]; right.point2 = q[b];
			GraphicsTrapsAddClippedTrap(traps, q[a].y, q[b].y, &left, &right);
			right.point1 = q[b]; right.point2 = q[c];
			GraphicsTrapsAddClippedTrap(traps, q[b].y, q[c].y, &left, &right);
			right.point1 = q[c]; right.point2 = q[d];
			GraphicsTrapsAddClippedTrap(traps, q[c].y, q[d].y, &left, &right);
		}
	}
	else
	{
		if(b_left_of_d)
		{
			/* Y-sort is abdc and b is left of d, (slope (ab) > slope (ad))
			*
			*		a   a	 a
			*	   //  / \	|\	 a.y b.y  ab  ad
			*	 /b/  b   \   b \
			*	/ /	\   \   \ \   b.y d.y  bc  ad
			*   /d/	  \   d   \ d
			*  //		 \ /	 \|  d.y c.y  bc  dc
			*  c		   c	   c
			*/
			left.point1  = q[a]; left.point2  = q[b];
			right.point1 = q[a]; right.point2 = q[d];
			GraphicsTrapsAddClippedTrap(traps, q[a].y, q[b].y, &left, &right);
			left.point1  = q[b]; left.point2  = q[c];
			GraphicsTrapsAddClippedTrap(traps, q[b].y, q[d].y, &left, &right);
			right.point1 = q[d]; right.point2 = q[c];
			GraphicsTrapsAddClippedTrap(traps, q[d].y, q[c].y, &left, &right);
		}
		else
		{
			/* Y-sort is abdc and b is right of d, (slope (ab) <= slope (ad))
			*
			*	  a	 a   a
			*	 /|	/ \  \\	   a.y b.y  ad  ab
			*	/ b   /   b  \b\
			*   / /   /   /	\ \	b.y d.y  ad  bc
			*  d /   d   /	 \d\
			*  |/	 \ /		 \\  d.y c.y  dc  bc
			*  c	   c	   c
			*/
			left.point1  = q[a]; left.point2  = q[d];
			right.point1 = q[a]; right.point2 = q[b];
			GraphicsTrapsAddClippedTrap(traps, q[a].y, q[b].y, &left, &right);
			right.point1 = q[b]; right.point2 = q[c];
			GraphicsTrapsAddClippedTrap(traps, q[b].y, q[d].y, &left, &right);
			left.point1  = q[d]; left.point2  = q[c];
			GraphicsTrapsAddClippedTrap(traps, q[d].y, q[c].y, &left, &right);
		}
	}
}

static AddTriangle(
	GRAPHICS_TRAPS* traps,
	int y1,
	int y2,
	const GRAPHICS_LINE* left,
	const GRAPHICS_LINE* right
)
{
	if(y2 < y1)
	{
		int temp = y1;
		y1 = y2;
		y2 = temp;
	}

	if(GraphicsLinesCompareAtY(left, right, y1) > 0)
	{
		const GRAPHICS_LINE* temp = left;
		left = right;
		right = temp;
	}

	GraphicsTrapsAddClippedTrap(traps, y1, y2, left, right);
}

void GraphicsTrapsTessellateTriangleWithEdges(
	GRAPHICS_TRAPS* traps,
	const GRAPHICS_POINT t[3],
	const GRAPHICS_POINT edges[4]
)
{
	GRAPHICS_LINE lines[3];

	if(edges[0].y <= edges[1].y)
	{
		lines[0].point1 = edges[0];
		lines[0].point2 = edges[1];
	}
	else
	{
		lines[0].point1 = edges[1];
		lines[0].point2 = edges[0];
	}

	if(edges[2].y <= edges[3].y)
	{
		lines[1].point1 = edges[2];
		lines[1].point2 = edges[3];
	}
	else
	{
		lines[1].point1 = edges[3];
		lines[1].point2 = edges[2];
	}

	if(t[1].y == t[2].y)
	{
		AddTriangle(traps, t[0].y, t[1].y, &lines[0], &lines[1]);
		return;
	}

	if(t[1].y <= t[2].y)
	{
		lines[2].point1 = t[1];
		lines[2].point2 = t[2];
	}
	else
	{
		lines[2].point1 = t[2];
		lines[2].point2 = t[1];
	}

	if(((t[1].y - t[0].y) < 0) ^ ((t[2].y - t[0].y) < 0))
	{
		AddTriangle(traps, t[0].y, t[1].y, &lines[0], &lines[2]);
		AddTriangle(traps, t[0].y, t[2].y, &lines[1], &lines[2]);
	}
	else if(abs(t[1].y - t[0].y) < abs(t[2].y - t[0].y))
	{
		AddTriangle(traps, t[0].y, t[1].y, &lines[0], &lines[1]);
		AddTriangle(traps, t[1].y, t[2].y, &lines[2], &lines[1]);
	}
	else
	{
		AddTriangle(traps, t[0].y, t[2].y, &lines[1], &lines[0]);
		AddTriangle(traps, t[1].y, t[2].y, &lines[2], &lines[0]);
	}
}

int GraphicsSlopeCompare(const GRAPHICS_SLOPE* a, const GRAPHICS_SLOPE* b)
{
	int64 ady_bdx = GRAPHICS_INT32x32_64_MULTI(a->dy, b->dx);
	int64 bdy_adx = GRAPHICS_INT32x32_64_MULTI(b->dy, a->dx);
	int compare;

	compare = (ady_bdx == bdy_adx) ? 0 : (ady_bdx < bdy_adx) ? -1 : 1;
	if(compare != 0)
	{
		return compare;
	}

	if(a->dx == 0 && a->dy == 0 && b->dx == 0 && b->dy == 0)
	{
		return 0;
	}
	if(a->dx == 0 && a->dy == 0)
	{
		return 1;
	}
	if(b->dx == 0 && b->dy == 0)
	{
		return -1;
	}

	if((a->dx ^ b->dx) < 0 || (a->dy ^ b->dy) < 0)
	{
		if(a->dx > 0 || (a->dx == 0 && a->dy > 0))
		{
			return -1;
		}
		else
		{
			return 1;
		}
	}

	return 0;
}

static GRAPHICS_FLOAT_FIXED LineComputeIntersectionXforY(const GRAPHICS_LINE* line, GRAPHICS_FLOAT_FIXED y)
{
	return GraphicsEdgeComputeIntersectionXforY(&line->point1, &line->point2, y);
}

void GraphicsTrapsExtents(const GRAPHICS_TRAPS* traps, GRAPHICS_BOX* extents)
{
	int i;

	if(traps->num_traps == 0)
	{
		extents->point1.x = extents->point1.y = 0;
		extents->point2.x = extents->point2.y = 0;
		return;
	}

	extents->point1.x = extents->point1.y = INT32_MAX;
	extents->point2.x = extents->point2.y = INT32_MIN;

	for(i=0; i<traps->num_traps; i++)
	{
		const GRAPHICS_TRAPEZOID *trap = &traps->traps[i];

		if(trap->top < extents->point1.y)
		{
			extents->point1.y = trap->top;
		}
		if(trap->bottom > extents->point2.y)
		{
			extents->point2.y = trap->bottom;
		}

		if(trap->left.point1.x < extents->point1.x)
		{
			GRAPHICS_FLOAT_FIXED x = trap->left.point1.x;
			if(trap->top != trap->left.point1.y)
			{
				x = LineComputeIntersectionXforY(&trap->left, trap->top);
				if(x < extents->point1.x)
				{
					extents->point1.x = x;
				}
			}
			else
			{
				extents->point1.x = x;
			}
		}
		if(trap->left.point2.x < extents->point1.x)
		{
			GRAPHICS_FLOAT_FIXED x = trap->left.point2.x;
			if(trap->bottom != trap->left.point2.y)
			{
				x = LineComputeIntersectionXforY(&trap->left, trap->bottom);
				if(x < extents->point1.x)
				{
					extents->point1.x = x;
				}
			}
			else
			{
				extents->point1.x = x;
			}
		}

		if(trap->right.point1.x > extents->point2.x)
		{
			GRAPHICS_FLOAT_FIXED x = trap->right.point1.x;
			if(trap->top != trap->right.point1.y)
			{
				x = LineComputeIntersectionXforY(&trap->right, trap->top);
				if(x > extents->point2.x)
				{
					extents->point2.x = x;
				}
			}
		}
		if(trap->right.point2.x > extents->point2.x)
		{
			GRAPHICS_FLOAT_FIXED x = trap->right.point2.x;
			if(trap->bottom != trap->right.point2.y)
			{
				x = LineComputeIntersectionXforY(&trap->right, trap->bottom);
				if(x > extents->point2.x)
				{
					extents->point2.x = x;
				}
			}
			else
			{
				extents->point2.x = x;
			}
		}
	}
}

void InitializeGraphicsContour(GRAPHICS_CONTOUR* contour, int direction)
{
	contour->direction = direction;
	contour->chain.points = contour->embedded_points;
	contour->chain.next = NULL;
	contour->chain.num_points = 0;
	contour->chain.size_points = sizeof(contour->embedded_points)/ sizeof(contour->embedded_points[0]);
	contour->tail = &contour->chain;
}

void GraphicsContourFinish(GRAPHICS_CONTOUR* contour)
{
	GRAPHICS_CONTOUR_CHAIN *chain, *next;

	for(chain = contour->chain.next; chain != NULL; chain = next)
	{
		next = chain->next;
		MEM_FREE_FUNC(chain);
	}
}

void GraphicsContourReset(GRAPHICS_CONTOUR* contour)
{
	GraphicsContourFinish(contour);
	InitializeGraphicsContour(contour, contour->direction);
}

static int B_BoxCompare(const GRAPHICS_LINE* a, const GRAPHICS_LINE* b)
{
	int32 a_min, a_max;
	int32 b_min, b_max;

	if(a->point1.x < a->point2.x)
	{
		a_min = a->point1.x;
		a_max = a->point2.x;
	}
	else
	{
		a_min = a->point2.x;
		a_max = a->point1.x;
	}

	if(b->point1.x < b->point2.x)
	{
		b_min = b->point1.x;
		b_max = b->point2.x;
	}
	else
	{
		b_min = b->point2.x;
		b_max = b->point1.x;
	}

	if(a_max < b_min)
	{
		return -1;
	}

	if(a_min > b_max)
	{
		return +1;
	}

	return 0;
}

int GraphicsLinesCompareAtY(
	const GRAPHICS_LINE* a,
	const GRAPHICS_LINE* b,
	int y
)
{
	GRAPHICS_SLOPE slope_a, slope_b;
	int result;

	if(GraphicsLinesEqual(a, b) != FALSE)
	{
		return 0;
	}

	result = B_BoxCompare(a, b);
	if(result != 0)
	{
		return result;
	}

	INITIALIZE_GRAPHICS_SLOPE(&slope_a, &a->point1, &a->point2);
	INITIALIZE_GRAPHICS_SLOPE(&slope_b, &b->point1, &b->point2);

	return GraphicsSlopeCompare(&slope_b, &slope_a);
}

#define UINT64_HI(i) ((i) >> 32)
#define UINT64_CARRY32 (((uint64)1) << 32)
#define UINT64_SHIFT32(i) ((i) << 32)

uint128 GraphicsUnsignedInteger64x64_128Multi(uint64 a, uint64 b)
{
	uint128 s;
	uint32 ah, al, bh, bl;
	uint64 r0, r1, r2, r3;

	al = a & 0xFFFFFFFF;
	ah = a >> 32;
	bl = b & 0xFFFFFFFF;
	bh = b >> 32;

	r0 = GRAPHICS_UINT32x32_64_MULTI(al, bl);
	r1 = GRAPHICS_UINT32x32_64_MULTI(al, bh);
	r2 = GRAPHICS_UINT32x32_64_MULTI(ah, bl);
	r3 = GRAPHICS_UINT32x32_64_MULTI(ah, bh);

	r1 = r1 + UINT64_HI(r0);
	r1 = r1 + r2;
	if(r1 < r2)
	{
		r3 = r3 + UINT64_CARRY32;
	}

	s.hi = r3 + UINT64_HI(r1);
	s.lo = UINT64_SHIFT32(r1) + (r0 & 0xFFFFFFFF);

	return s;
}

uint128 GraphicsInteger64x64_128Multi(int64 a, int64 b)
{
	uint128 s;
	s = GraphicsUnsignedInteger64x64_128Multi((uint64)a, (uint64)b);
	if(a < 0)
	{
		s.hi = s.hi - (uint64)b;
	}
	if(b < 0)
	{
		s.hi = s.hi - (uint64)a;
	}

	return s;
}

uint128 GraphicsUnsignedInteger128Negate(uint128 a)
{
	uint128 _1;
	_1.hi = 0,  _1.lo = 1;
	a.lo = ~a.lo;
	a.hi = ~a.hi;

	return UnsignedInteger128Add(a, _1);
}

uint128 GraphicsUnsignedInteger128RightShift(uint128 a, int shift)
{
	if(shift >= 64)
	{
		a.lo = a.hi;
		a.hi = 0;
		shift -= 64;
	}
	if(shift != 0)
	{
		a.lo = (a.lo >> shift) + (a.hi >> (64 - shift));
		a.hi = a.hi >> shift;
	}
	return a;
}

GRAPHICS_UNSIGNED_QUOREM64 GraphicsUnsignedInteger96by64_32x64_DivideRemain(uint128 num, uint64 den)
{
	GRAPHICS_UNSIGNED_QUOREM64 result;
	uint64 B = 1 << 32;
	uint64 x = GraphicsUnsignedInteger128RightShift(num, 32).lo;

	result.quo = (uint64)(-1U) << 32 | (-1U);
	result.rem = den;

	if(x >= den)
	{
		return result;
	}

	if(x < B)
	{
		return GraphicsUnsignedQuorem64DivideRemain(num.lo, den);
	}
	else
	{
		uint32 y = (uint32)num.lo;
		uint32 u = den >> 32;
		uint32 v = (uint32)den;

		GRAPHICS_UNSIGNED_QUOREM64 quorem;
		uint64 remainder;
		uint32 quotient;
		uint32 q;
		uint32 r;

		if(u+1)
		{
			quorem = GraphicsUnsignedQuorem64DivideRemain(x, u+1);
			q = (uint32)quorem.quo;
			r = (uint32)quorem.rem;
		}
		else
		{
			q = x >> 32;
			r = (uint32)x;
		}
		quotient = q;

		if(v)
		{
			quorem = GraphicsUnsignedQuorem64DivideRemain(GRAPHICS_INT32x32_64_MULTI(q, -v), den);
		}
		else
		{
			quorem = GraphicsUnsignedQuorem64DivideRemain(q << 32, den);
		}
		quotient += (uint32)quorem.quo;

		remainder = (r << 32) | y;
		if(remainder >= den)
		{
			remainder = remainder - den;
			quotient++;
		}
		result.quo = quotient;
		result.rem = remainder;
	}

	return result;
}

GRAPHICS_QUOREM64 GraphicsInteger96by64_32x64_DivideRemain(int128 num, int64 den)
{
	int num_negative = INTEGER128_NEGATIVE(num);
	int den_negative = den < 0;
	uint64 nonnegative_den;
	GRAPHICS_UNSIGNED_QUOREM64 uqr;
	GRAPHICS_QUOREM64 qr;

	if(num_negative)
	{
		num = GraphicsUnsignedInteger128Negate(num);
	}
	if(den_negative)
	{
		nonnegative_den = ~den;
	}
	else
	{
		nonnegative_den = den;
	}

	uqr = GraphicsUnsignedInteger96by64_32x64_DivideRemain(num, nonnegative_den);
	if(uqr.rem == nonnegative_den)
	{
		qr.quo = (0x7FFFFFFF << 32) | (-1U);
		qr.rem = den;
		return qr;
	}

	if(num_negative)
	{
		qr.rem = ~uqr.rem;
	}
	else
	{
		qr.rem = uqr.rem;
	}
	if(num_negative != den_negative)
	{
		qr.quo = ~uqr.quo;
	}
	else
	{
		qr.quo = uqr.quo;
	}
	return qr;
}

static int GraphicsTrapContains(GRAPHICS_TRAPEZOID* t, GRAPHICS_POINT* point)
{
	GRAPHICS_SLOPE slope_left, slope_point, slope_right;

	if(t->top > point->y)
	{
		return FALSE;
	}
	if(t->bottom < point->y)
	{
		return FALSE;
	}

	INITIALIZE_GRAPHICS_SLOPE(&slope_left, &t->left.point1, &t->left.point2);
	INITIALIZE_GRAPHICS_SLOPE(&slope_point, &t->left.point1, point);

	if(GraphicsSlopeCompare(&slope_left, &slope_point) < 0)
	{
		return FALSE;
	}

	INITIALIZE_GRAPHICS_SLOPE(&slope_right, &t->right.point1, &t->right.point2);
	INITIALIZE_GRAPHICS_SLOPE(&slope_point, &t->right.point1, point);

	if(GraphicsSlopeCompare(&slope_point, &slope_right) < 0)
	{
		FALSE;
	}

	return TRUE;
}

int GraphicsTrapsContain(const GRAPHICS_TRAPS* traps, FLOAT_T x, FLOAT_T y)
{
	int i;
	GRAPHICS_POINT point;

	point.x = GraphicsFixedFromFloat(x);
	point.y = GraphicsFixedFromFloat(y);

	for(i=0; i<traps->num_traps; i++)
	{
		if(GraphicsTrapContains(&traps->traps[i], &point) != FALSE)
		{
			return TRUE;
		}
	}

	return FALSE;
}

void GraphicsTrapsFinish(GRAPHICS_TRAPS* traps)
{
	if(traps->traps != traps->traps_embedded)
	{
		MEM_FREE_FUNC(traps->traps);
	}
}

#ifdef __cplusplus
}
#endif
