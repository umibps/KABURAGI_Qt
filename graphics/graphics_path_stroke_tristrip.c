#include "graphics_compositor.h"
#include "graphics_surface.h"
#include "graphics_region.h"
#include "graphics_matrix.h"
#include "graphics_private.h"
#include "graphics_inline.h"
#include "graphics_scan_converter_private.h"
#include "graphics.h"

#ifdef __cplusplus
extern "C" {
#endif

struct stroker
{
	GRAPHICS_STROKE_STYLE style;

	GRAPHICS_TRISTRIP *strip;

	const GRAPHICS_MATRIX *ctm;
	const GRAPHICS_MATRIX *ctm_inverse;
	double tolerance;
	int ctm_det_positive;

	GRAPHICS_PEN pen;

	int has_sub_path;

	GRAPHICS_POINT first_point;

	int has_current_face;
	GRAPHICS_STROKE_FACE current_face;

	int has_first_face;
	GRAPHICS_STROKE_FACE first_face;

	GRAPHICS_BOX limit;
	int has_limits;
};

static INLINE double normalize_slope(FLOAT_T* dx, FLOAT_T* dy);

static void compute_face(
	const GRAPHICS_POINT* point,
	const GRAPHICS_SLOPE* dev_slope,
	struct stroker* stroker,
	GRAPHICS_STROKE_FACE* face
);

static void translate_point(GRAPHICS_POINT* point, const GRAPHICS_POINT* offset)
{
	point->x += offset->x;
	point->y += offset->y;
}

static int slope_compare_sgn(FLOAT_T dx1, FLOAT_T dy1, FLOAT_T dx2, FLOAT_T dy2)
{
	FLOAT_T c = (dx1 * dy2 - dx2 * dy1);

	if (c > 0) return 1;
	if (c < 0) return -1;
	return 0;
}

static INLINE int range_step(int i, int step, int max)
{
	i += step;
	if (i < 0)
		i = max - 1;
	if (i >= max)
		i = 0;
	return i;
}

/*
* Construct a fan around the midpoint using the vertices from pen between
* inpt and outpt.
*/
static void add_fan(
	struct stroker* stroker,
	const GRAPHICS_SLOPE* in_vector,
	const GRAPHICS_SLOPE* out_vector,
	const GRAPHICS_POINT* midpt,
	const GRAPHICS_POINT* inpt,
	const GRAPHICS_POINT* outpt,
	int clockwise
)
{
	int start, stop, step, i, npoints;

	if (clockwise)
	{
		step  = 1;
		
		start = GraphicsPenFindActiveClockwiseVertexIndex(&stroker->pen,
			in_vector);
		if (GraphicsSlopeCompare(&stroker->pen.vertices[start].slope_cw,
			in_vector) < 0)
			start = range_step (start, 1, stroker->pen.num_vertices);
		
		stop  = GraphicsPenFindActiveClockwiseVertexIndex(&stroker->pen,
			out_vector);
		if (GraphicsSlopeCompare(&stroker->pen.vertices[stop].slope_ccw,
			out_vector) > 0)
		{
			stop = range_step (stop, -1, stroker->pen.num_vertices);
			if (GraphicsSlopeCompare(&stroker->pen.vertices[stop].slope_cw,
				in_vector) < 0)
				return;
		}

		npoints = stop - start;
	}
	else
	{
		step  = -1;
		
		start = GraphicsPenFindActiveCounterClockwiseVertexIndex(&stroker->pen,
			in_vector);
		if (GraphicsSlopeCompare(&stroker->pen.vertices[start].slope_ccw,
			in_vector) < 0)
			start = range_step (start, -1, stroker->pen.num_vertices);

		stop  = GraphicsPenFindActiveCounterClockwiseVertexIndex(&stroker->pen,
			out_vector);
		if (GraphicsSlopeCompare(&stroker->pen.vertices[stop].slope_cw,
			out_vector) > 0)
		{
			stop = range_step (stop, 1, stroker->pen.num_vertices);
			if (GraphicsSlopeCompare(&stroker->pen.vertices[stop].slope_ccw,
				in_vector) < 0)
				return;
		}

		npoints = start - stop;
	}
	stop = range_step (stop, step, stroker->pen.num_vertices);
	if (npoints < 0)
		npoints += stroker->pen.num_vertices;
	if (npoints <= 1)
		return;

	for (i = start;
		i != stop;
		i = range_step (i, step, stroker->pen.num_vertices))
	{
		GRAPHICS_POINT p = *midpt;
		translate_point (&p, &stroker->pen.vertices[i].point);
		//contour_add_point (stroker, c, &p);
	}
}

static int join_is_clockwise(
	const GRAPHICS_STROKE_FACE* in,
	const GRAPHICS_STROKE_FACE* out
)
{
	return GraphicsSlopeCompare(&in->device_vector, &out->device_vector) < 0;
}

static void inner_join(
	struct stroker *stroker,
	const GRAPHICS_STROKE_FACE* in,
	const GRAPHICS_STROKE_FACE* out,
	int clockwise
)
{
	const GRAPHICS_POINT *outpt;

	if (clockwise)
	{
		outpt = &out->counter_clock_wise;
	}
	else
	{
		outpt = &out->clock_wise;
	}
	//contour_add_point (stroker, inner, &in->point);
	//contour_add_point (stroker, inner, outpt);
}

static void inner_close(
	struct stroker *stroker,
	const GRAPHICS_STROKE_FACE* in,
	GRAPHICS_STROKE_FACE* out
)
{
	const GRAPHICS_POINT *inpt;

	if (join_is_clockwise (in, out))
	{
		inpt = &out->counter_clock_wise;
	}
	else
	{
		inpt = &out->clock_wise;
	}

	//contour_add_point (stroker, inner, &in->point);
	//contour_add_point (stroker, inner, inpt);
	//*_cairo_contour_first_point (&inner->contour) =
	//*_cairo_contour_last_point (&inner->contour);
}

static void outer_close(
	struct stroker* stroker,
	const GRAPHICS_STROKE_FACE* in,
	const GRAPHICS_STROKE_FACE* out
)
{
	const GRAPHICS_POINT	*inpt, *outpt;
	int	clockwise;

	if (in->clock_wise.x == out->clock_wise.x && in->clock_wise.y == out->clock_wise.y &&
		in->counter_clock_wise.x == out->counter_clock_wise.x && in->counter_clock_wise.y == out->counter_clock_wise.y)
	{
		return;
	}
	clockwise = join_is_clockwise (in, out);
	if (clockwise)
	{
		inpt = &in->clock_wise;
		outpt = &out->clock_wise;
	}
	else
	{
		inpt = &in->counter_clock_wise;
		outpt = &out->counter_clock_wise;
	}

	switch (stroker->style.line_join)
	{
	case GRAPHICS_LINE_JOIN_ROUND:
		/* construct a fan around the common midpoint */
		add_fan (stroker,
			&in->device_vector,
			&out->device_vector,
			&in->point, inpt, outpt,
			clockwise);
		break;

	case GRAPHICS_LINE_JOIN_MITER:
	default: {
		/* dot product of incoming slope vector with outgoing slope vector */
		FLOAT_T	in_dot_out = -in->user_vector.x * out->user_vector.x +
			-in->user_vector.y * out->user_vector.y;
		FLOAT_T	ml = stroker->style.miter_limit;

		/* Check the miter limit -- lines meeting at an acute angle
		* can generate long miters, the limit converts them to bevel
		*
		* Consider the miter join formed when two line segments
		* meet at an angle psi:
		*
		*	   /.\
		*	  /. .\
		*	 /./ \.\
		*	/./psi\.\
		*
		* We can zoom in on the right half of that to see:
		*
		*		|\
		*		| \ psi/2
		*		|  \
		*		|   \
		*		|	\
		*		|	 \
		*	  miter	\
		*	 length	 \
		*		|		\
		*		|		.\
		*		|	.	 \
		*		|.   line   \
		*		 \	width  \
		*		  \		   \
		*
		*
		* The right triangle in that figure, (the line-width side is
		* shown faintly with three '.' characters), gives us the
		* following expression relating miter length, angle and line
		* width:
		*
		*	1 /sin (psi/2) = miter_length / line_width
		*
		* The right-hand side of this relationship is the same ratio
		* in which the miter limit (ml) is expressed. We want to know
		* when the miter length is within the miter limit. That is
		* when the following condition holds:
		*
		*	1/sin(psi/2) <= ml
		*	1 <= ml sin(psi/2)
		*	1 <= ml² sin²(psi/2)
		*	2 <= ml² 2 sin²(psi/2)
		*				2·sin²(psi/2) = 1-cos(psi)
		*	2 <= ml² (1-cos(psi))
		*
		*				in · out = |in| |out| cos (psi)
		*
		* in and out are both unit vectors, so:
		*
		*				in · out = cos (psi)
		*
		*	2 <= ml² (1 - in · out)
		*
		*/
		if (2 <= ml * ml * (1 - in_dot_out))
		{
			FLOAT_T x1, y1, x2, y2;
			FLOAT_T mx, my;
			FLOAT_T dx1, dx2, dy1, dy2;
			FLOAT_T ix, iy;
			FLOAT_T fdx1, fdy1, fdx2, fdy2;
			FLOAT_T mdx, mdy;

			/*
			* we've got the points already transformed to device
			* space, but need to do some computation with them and
			* also need to transform the slope from user space to
			* device space
			*/
			/* outer point of incoming line face */
			x1 = GRAPHICS_FIXED_TO_FLOAT(inpt->x);
			y1 = GRAPHICS_FIXED_TO_FLOAT(inpt->y);
			dx1 = in->user_vector.x;
			dy1 = in->user_vector.y;
			GraphicsMatrixTransformDistance(stroker->ctm, &dx1, &dy1);

			/* outer point of outgoing line face */
			x2 = GRAPHICS_FIXED_TO_FLOAT(outpt->x);
			y2 = GRAPHICS_FIXED_TO_FLOAT(outpt->y);
			dx2 = out->user_vector.x;
			dy2 = out->user_vector.y;
			GraphicsMatrixTransformDistance(stroker->ctm, &dx2, &dy2);
			
			/*
			* Compute the location of the outer corner of the miter.
			* That's pretty easy -- just the intersection of the two
			* outer edges.  We've got slopes and points on each
			* of those edges.  Compute my directly, then compute
			* mx by using the edge with the larger dy; that avoids
			* dividing by values close to zero.
			*/
			my = (((x2 - x1) * dy1 * dy2 - y2 * dx2 * dy1 + y1 * dx1 * dy2) /
				(dx1 * dy2 - dx2 * dy1));
			if (fabs (dy1) >= fabs (dy2))
				mx = (my - y1) * dx1 / dy1 + x1;
			else
				mx = (my - y2) * dx2 / dy2 + x2;

			/*
			* When the two outer edges are nearly parallel, slight
			* perturbations in the position of the outer points of the lines
			* caused by representing them in fixed point form can cause the
			* intersection point of the miter to move a large amount. If
			* that moves the miter intersection from between the two faces,
			* then draw a bevel instead.
			*/
			
			ix = GRAPHICS_FIXED_TO_FLOAT(in->point.x);
			iy = GRAPHICS_FIXED_TO_FLOAT(in->point.y);

			/* slope of one face */
			fdx1 = x1 - ix; fdy1 = y1 - iy;

			/* slope of the other face */
			fdx2 = x2 - ix; fdy2 = y2 - iy;

			/* slope from the intersection to the miter point */
			mdx = mx - ix; mdy = my - iy;

			/*
			* Make sure the miter point line lies between the two
			* faces by comparing the slopes
			*/
			if (slope_compare_sgn (fdx1, fdy1, mdx, mdy) !=
				slope_compare_sgn (fdx2, fdy2, mdx, mdy))
			{
				GRAPHICS_POINT p;

				p.x = GraphicsFixedFromFloat(mx);
				p.y = GraphicsFixedFromFloat(my);

				//*_cairo_contour_last_point (&outer->contour) = p;
				//*_cairo_contour_first_point (&outer->contour) = p;
				return;
			}
		}
		break;
	}

	case GRAPHICS_LINE_JOIN_BEVEL:
		break;
	}
	//contour_add_point (stroker, outer, outpt);
}

static void outer_join(
	struct stroker* stroker,
	const GRAPHICS_STROKE_FACE* in,
	const GRAPHICS_STROKE_FACE* out,
	int clockwise
)
{
	const GRAPHICS_POINT	*inpt, *outpt;

	if (in->clock_wise.x == out->clock_wise.x && in->clock_wise.y == out->clock_wise.y &&
		in->counter_clock_wise.x == out->counter_clock_wise.x && in->counter_clock_wise.y == out->counter_clock_wise.y)
	{
		return;
	}
	if (clockwise)
	{
		inpt = &in->clock_wise;
		outpt = &out->clock_wise;
	}
	else
	{
		inpt = &in->counter_clock_wise;
		outpt = &out->counter_clock_wise;
	}

	switch (stroker->style.line_join)
	{
	case GRAPHICS_LINE_JOIN_ROUND:
		/* construct a fan around the common midpoint */
		add_fan (stroker,
			&in->device_vector,
			&out->device_vector,
			&in->point, inpt, outpt,
			clockwise
		);
		break;

	case GRAPHICS_LINE_JOIN_MITER:
	default: {
		/* dot product of incoming slope vector with outgoing slope vector */
		double	in_dot_out = -in->user_vector.x * out->user_vector.x +
			-in->user_vector.y * out->user_vector.y;
		double	ml = stroker->style.miter_limit;

		/* Check the miter limit -- lines meeting at an acute angle
		* can generate long miters, the limit converts them to bevel
		*
		* Consider the miter join formed when two line segments
		* meet at an angle psi:
		*
		*	   /.\
		*	  /. .\
		*	 /./ \.\
		*	/./psi\.\
		*
		* We can zoom in on the right half of that to see:
		*
		*		|\
		*		| \ psi/2
		*		|  \
		*		|   \
		*		|	\
		*		|	 \
		*	  miter	\
		*	 length	 \
		*		|		\
		*		|		.\
		*		|	.	 \
		*		|.   line   \
		*		 \	width  \
		*		  \		   \
		*
		*
		* The right triangle in that figure, (the line-width side is
		* shown faintly with three '.' characters), gives us the
		* following expression relating miter length, angle and line
		* width:
		*
		*	1 /sin (psi/2) = miter_length / line_width
		*
		* The right-hand side of this relationship is the same ratio
		* in which the miter limit (ml) is expressed. We want to know
		* when the miter length is within the miter limit. That is
		* when the following condition holds:
		*
		*	1/sin(psi/2) <= ml
		*	1 <= ml sin(psi/2)
		*	1 <= ml² sin²(psi/2)
		*	2 <= ml² 2 sin²(psi/2)
		*				2·sin²(psi/2) = 1-cos(psi)
		*	2 <= ml² (1-cos(psi))
		*
		*				in · out = |in| |out| cos (psi)
		*
		* in and out are both unit vectors, so:
		*
		*				in · out = cos (psi)
		*
		*	2 <= ml² (1 - in · out)
		*
		*/
		if (2 <= ml * ml * (1 - in_dot_out))
		{
			FLOAT_T x1, y1, x2, y2;
			FLOAT_T mx, my;
			FLOAT_T dx1, dx2, dy1, dy2;
			FLOAT_T ix, iy;
			FLOAT_T fdx1, fdy1, fdx2, fdy2;
			FLOAT_T mdx, mdy;

			/*
			* we've got the points already transformed to device
			* space, but need to do some computation with them and
			* also need to transform the slope from user space to
			* device space
			*/
			/* outer point of incoming line face */
			x1 = GRAPHICS_FIXED_TO_FLOAT(inpt->x);
			y1 = GRAPHICS_FIXED_TO_FLOAT(inpt->y);
			dx1 = in->user_vector.x;
			dy1 = in->user_vector.y;
			GraphicsMatrixTransformDistance(stroker->ctm, &dx1, &dy1);

			/* outer point of outgoing line face */
			x2 = GRAPHICS_FIXED_TO_FLOAT(outpt->x);
			y2 = GRAPHICS_FIXED_TO_FLOAT(outpt->y);
			dx2 = out->user_vector.x;
			dy2 = out->user_vector.y;
			GraphicsMatrixTransformDistance(stroker->ctm, &dx2, &dy2);

			/*
			* Compute the location of the outer corner of the miter.
			* That's pretty easy -- just the intersection of the two
			* outer edges.  We've got slopes and points on each
			* of those edges.  Compute my directly, then compute
			* mx by using the edge with the larger dy; that avoids
			* dividing by values close to zero.
			*/
			my = (((x2 - x1) * dy1 * dy2 - y2 * dx2 * dy1 + y1 * dx1 * dy2) /
				(dx1 * dy2 - dx2 * dy1));
			if (fabs (dy1) >= fabs (dy2))
				mx = (my - y1) * dx1 / dy1 + x1;
			else
				mx = (my - y2) * dx2 / dy2 + x2;

			/*
			* When the two outer edges are nearly parallel, slight
			* perturbations in the position of the outer points of the lines
			* caused by representing them in fixed point form can cause the
			* intersection point of the miter to move a large amount. If
			* that moves the miter intersection from between the two faces,
			* then draw a bevel instead.
			*/

			ix = GRAPHICS_FIXED_TO_FLOAT(in->point.x);
			iy = GRAPHICS_FIXED_TO_FLOAT(in->point.y);

			/* slope of one face */
			fdx1 = x1 - ix; fdy1 = y1 - iy;

			/* slope of the other face */
			fdx2 = x2 - ix; fdy2 = y2 - iy;

			/* slope from the intersection to the miter point */
			mdx = mx - ix; mdy = my - iy;

			/*
			* Make sure the miter point line lies between the two
			* faces by comparing the slopes
			*/
			if (slope_compare_sgn (fdx1, fdy1, mdx, mdy) !=
				slope_compare_sgn (fdx2, fdy2, mdx, mdy))
			{
				GRAPHICS_POINT p;
				
				p.x = GraphicsFixedFromFloat(mx);
				p.y = GraphicsFixedFromFloat(my);

				//*_cairo_contour_last_point (&outer->contour) = p;
				return;
			}
		}
		break;
	}

	case GRAPHICS_LINE_JOIN_BEVEL:
		break;
	}
	//contour_add_point (stroker,outer, outpt);
}

static void add_cap(
	struct stroker* stroker,
	const GRAPHICS_STROKE_FACE* f
)
{
	switch (stroker->style.line_cap)
	{
	case GRAPHICS_LINE_CAP_ROUND:
	{
		GRAPHICS_SLOPE slope;

		slope.dx = -f->device_vector.dx;
		slope.dy = -f->device_vector.dy;

		add_fan (stroker, &f->device_vector, &slope,
			&f->point, &f->counter_clock_wise, &f->clock_wise,
			FALSE);
		break;
	}

	case GRAPHICS_LINE_CAP_SQUARE:
	{
		double dx, dy;
		GRAPHICS_SLOPE	fvector;
		GRAPHICS_POINT	quad[4];

		dx = f->user_vector.x;
		dy = f->user_vector.y;
		dx *= stroker->style.line_width / 2.0;
		dy *= stroker->style.line_width / 2.0;
		GraphicsMatrixTransformDistance(stroker->ctm, &dx, &dy);
		fvector.dx = GraphicsFixedFromFloat(dx);
		fvector.dy = GraphicsFixedFromFloat(dy);

		quad[0] = f->counter_clock_wise;
		quad[1].x = f->counter_clock_wise.x + fvector.dx;
		quad[1].y = f->counter_clock_wise.y + fvector.dy;
		quad[2].x = f->clock_wise.x + fvector.dx;
		quad[2].y = f->clock_wise.y + fvector.dy;
		quad[3] = f->clock_wise;

		//contour_add_point (stroker, c, &quad[1]);
		//contour_add_point (stroker, c, &quad[2]);
	}

	case GRAPHICS_LINE_CAP_BUTT:
	default:
		break;
	}
	//contour_add_point (stroker, c, &f->cw);
}

static void add_leading_cap(
	struct stroker* stroker,
	const GRAPHICS_STROKE_FACE* face
)
{
	GRAPHICS_STROKE_FACE reversed;
	GRAPHICS_POINT t;

	reversed = *face;

	/* The initial cap needs an outward facing vector. Reverse everything */
	reversed.user_vector.x = -reversed.user_vector.x;
	reversed.user_vector.y = -reversed.user_vector.y;
	reversed.device_vector.dx = -reversed.device_vector.dx;
	reversed.device_vector.dy = -reversed.device_vector.dy;

	t = reversed.clock_wise;
	reversed.clock_wise = reversed.counter_clock_wise;
	reversed.counter_clock_wise = t;

	add_cap (stroker, &reversed);
}

static void add_trailing_cap(
	struct stroker* stroker,
	const GRAPHICS_STROKE_FACE* face
)
{
	add_cap (stroker, face);
}

static INLINE FLOAT_T normalize_slope(FLOAT_T* dx, FLOAT_T* dy)
{
	FLOAT_T dx0 = *dx, dy0 = *dy;
	FLOAT_T mag;

	assert(dx0 != 0.0 || dy0 != 0.0);

	if(dx0 == 0.0)
	{
		*dx = 0.0;
		if(dy0 > 0.0)
		{
			mag = dy0;
			*dy = 1.0;
		}
		else
		{
			mag = -dy0;
			*dy = -1.0;
		}
	}
	else if(dy0 == 0.0)
	{
		*dy = 0.0;
		if(dx0 > 0.0)
		{
			mag = dx0;
			*dx = 1.0;
		}
		else
		{
			mag = -dx0;
			*dx = -1.0;
		}
	}
	else
	{
		mag = hypot (dx0, dy0);
		*dx = dx0 / mag;
		*dy = dy0 / mag;
	}

	return mag;
}

static void compute_face(
	const GRAPHICS_POINT* point,
	const GRAPHICS_SLOPE* dev_slope,
	struct stroker* stroker,
	GRAPHICS_STROKE_FACE* face
)
{
	FLOAT_T face_dx, face_dy;
	GRAPHICS_POINT offset_ccw, offset_cw;
	FLOAT_T slope_dx, slope_dy;

	slope_dx = GRAPHICS_FIXED_TO_FLOAT(dev_slope->dx);
	slope_dy = GRAPHICS_FIXED_TO_FLOAT(dev_slope->dy);
	face->length = normalize_slope (&slope_dx, &slope_dy);
	face->device_slope.x = slope_dx;
	face->device_slope.y = slope_dy;

	/*
	* rotate to get a line_width/2 vector along the face, note that
	* the vector must be rotated the right direction in device space,
	* but by 90° in user space. So, the rotation depends on
	* whether the ctm reflects or not, and that can be determined
	* by looking at the determinant of the matrix.
	*/
	if (! GRAPHICS_MATRIX_IS_IDENTITY(stroker->ctm_inverse))
	{
		/* Normalize the matrix! */
		GraphicsMatrixTransformDistance(stroker->ctm_inverse,
			&slope_dx, &slope_dy);
		normalize_slope (&slope_dx, &slope_dy);

		if (stroker->ctm_det_positive)
		{
			face_dx = - slope_dy * (stroker->style.line_width / 2.0);
			face_dy = slope_dx * (stroker->style.line_width / 2.0);
		}
		else
		{
			face_dx = slope_dy * (stroker->style.line_width / 2.0);
			face_dy = - slope_dx * (stroker->style.line_width / 2.0);
		}

		/* back to device space */
		GraphicsMatrixTransformDistance(stroker->ctm, &face_dx, &face_dy);
	}
	else
	{
		face_dx = - slope_dy * (stroker->style.line_width / 2.0);
		face_dy = slope_dx * (stroker->style.line_width / 2.0);
	}

	offset_ccw.x = GraphicsFixedFromFloat(face_dx);
	offset_ccw.y = GraphicsFixedFromFloat(face_dy);
	offset_cw.x = -offset_ccw.x;
	offset_cw.y = -offset_ccw.y;

	face->counter_clock_wise = *point;
	translate_point (&face->counter_clock_wise, &offset_ccw);

	face->point = *point;

	face->clock_wise = *point;
	translate_point (&face->clock_wise, &offset_cw);

	face->user_vector.x = slope_dx;
	face->user_vector.y = slope_dy;

	face->device_vector = *dev_slope;
}

static void add_caps(struct stroker* stroker)
{
	/* check for a degenerative sub_path */
	if (stroker->has_sub_path &&
		! stroker->has_first_face &&
		! stroker->has_current_face &&
		stroker->style.line_cap == GRAPHICS_LINE_CAP_ROUND)
	{
		/* pick an arbitrary slope to use */
		GRAPHICS_SLOPE slope = { GRAPHICS_FIXED_ONE, 0 };
		GRAPHICS_STROKE_FACE face;

		/* arbitrarily choose first_point */
		compute_face (&stroker->first_point, &slope, stroker, &face);

		add_leading_cap (stroker, &face);
		add_trailing_cap (stroker, &face);

		/* ensure the circle is complete */
		//_cairo_contour_add_point (&stroker->ccw.contour,
		//_cairo_contour_first_point (&stroker->ccw.contour));
	}
	else
	{
		if (stroker->has_current_face)
			add_trailing_cap (stroker, &stroker->current_face);

		//_cairo_polygon_add_contour (stroker->polygon, &stroker->ccw.contour);
		//_cairo_contour_reset (&stroker->ccw.contour);

		if (stroker->has_first_face)
		{
			//_cairo_contour_add_point (&stroker->ccw.contour,
			//&stroker->first_face.cw);
			add_leading_cap (stroker, &stroker->first_face);
			//_cairo_polygon_add_contour (stroker->polygon,
			//&stroker->ccw.contour);
			//_cairo_contour_reset (&stroker->ccw.contour);
		}
	}
}

static eGRAPHICS_STATUS move_to(void *closure, const GRAPHICS_POINT *point)
{
	struct stroker *stroker = closure;

	/* Cap the start and end of the previous sub path as needed */
	add_caps (stroker);

	stroker->has_first_face = FALSE;
	stroker->has_current_face = FALSE;
	stroker->has_sub_path = FALSE;

	stroker->first_point = *point;

	stroker->current_face.point = *point;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS line_to(void *closure, const GRAPHICS_POINT *point)
{
	struct stroker *stroker = closure;
	GRAPHICS_STROKE_FACE start;
	GRAPHICS_POINT *p1 = &stroker->current_face.point;
	GRAPHICS_SLOPE dev_slope;

	stroker->has_sub_path = TRUE;

	if (p1->x == point->x && p1->y == point->y)
		return GRAPHICS_STATUS_SUCCESS;

	INITIALIZE_GRAPHICS_SLOPE(&dev_slope, p1, point);
	compute_face (p1, &dev_slope, stroker, &start);

	if(stroker->has_current_face)
	{
		int clockwise = join_is_clockwise (&stroker->current_face, &start);
		/* Join with final face from previous segment */
		outer_join (stroker, &stroker->current_face, &start, clockwise);
		inner_join (stroker, &stroker->current_face, &start, clockwise);
	}
	else
	{
		if (! stroker->has_first_face)
		{
			/* Save sub path's first face in case needed for closing join */
			stroker->first_face = start;
			GraphicsTristripMoveTo(stroker->strip, &start.clock_wise);
			stroker->has_first_face = TRUE;
		}
		stroker->has_current_face = TRUE;

		GraphicsTristripAddPoint(stroker->strip, &start.clock_wise);
		GraphicsTristripAddPoint(stroker->strip, &start.counter_clock_wise);
	}

	stroker->current_face = start;
	stroker->current_face.point = *point;
	stroker->current_face.counter_clock_wise.x += dev_slope.dx;
	stroker->current_face.counter_clock_wise.y += dev_slope.dy;
	stroker->current_face.clock_wise.x += dev_slope.dx;
	stroker->current_face.clock_wise.y += dev_slope.dy;

	GraphicsTristripAddPoint(stroker->strip, &stroker->current_face.clock_wise);
	GraphicsTristripAddPoint(stroker->strip, &stroker->current_face.counter_clock_wise);

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS spline_to(
	void* closure,
	const GRAPHICS_POINT* point,
	const GRAPHICS_SLOPE* tangent
)
{
	struct stroker *stroker = closure;
	GRAPHICS_STROKE_FACE face;

	if (tangent->dx == 0 && tangent->dy == 0)
	{
		const GRAPHICS_POINT *inpt, *outpt;
		GRAPHICS_POINT t;
		int clockwise;

		face = stroker->current_face;

		face.user_vector.x = -face.user_vector.x;
		face.user_vector.y = -face.user_vector.y;
		face.device_vector.dx = -face.device_vector.dx;
		face.device_vector.dy = -face.device_vector.dy;

		t = face.clock_wise;
		face.clock_wise = face.counter_clock_wise;
		face.counter_clock_wise = t;

		clockwise = join_is_clockwise (&stroker->current_face, &face);
		if(clockwise)
		{
			inpt = &stroker->current_face.clock_wise;
			outpt = &face.clock_wise;
		}
		else
		{
			inpt = &stroker->current_face.counter_clock_wise;
			outpt = &face.counter_clock_wise;
		}

		add_fan (stroker,
			&stroker->current_face.device_vector,
			&face.device_vector,
			&stroker->current_face.point, inpt, outpt,
			clockwise
		);
	}
	else
	{
		compute_face (point, tangent, stroker, &face);

		if (face.device_slope.x * stroker->current_face.device_slope.x +
			face.device_slope.y * stroker->current_face.device_slope.y < 0)
		{
			const GRAPHICS_POINT *inpt, *outpt;
			int clockwise = join_is_clockwise (&stroker->current_face, &face);

			stroker->current_face.clock_wise.x += face.point.x - stroker->current_face.point.x;
			stroker->current_face.clock_wise.y += face.point.y - stroker->current_face.point.y;
			//contour_add_point (stroker, &stroker->cw, &stroker->current_face.cw);

			stroker->current_face.counter_clock_wise.x += face.point.x - stroker->current_face.point.x;
			stroker->current_face.counter_clock_wise.y += face.point.y - stroker->current_face.point.y;
			//contour_add_point (stroker, &stroker->ccw, &stroker->current_face.ccw);

			if(clockwise)
			{
				inpt = &stroker->current_face.clock_wise;
				outpt = &face.clock_wise;
			}
			else
			{
				inpt = &stroker->current_face.counter_clock_wise;
				outpt = &face.counter_clock_wise;
			}
			add_fan (stroker,
				&stroker->current_face.device_vector,
				&face.device_vector,
				&stroker->current_face.point, inpt, outpt,
				clockwise
			);
		}

		GraphicsTristripAddPoint(stroker->strip, &face.clock_wise);
		GraphicsTristripAddPoint(stroker->strip, &face.counter_clock_wise);
	}

	stroker->current_face = face;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS curve_to (
	void* closure,
	const GRAPHICS_POINT* b,
	const GRAPHICS_POINT* c,
	const GRAPHICS_POINT* d
)
{
	struct stroker *stroker = closure;
	GRAPHICS_SPLINE spline;
	GRAPHICS_STROKE_FACE face;
	
	if(stroker->has_limits)
	{
		if (! GraphicsSplineIntersects(&stroker->current_face.point, b, c, d,
			&stroker->limit))
			return line_to (closure, d);
	}

	if (! InitializeGraphicsSpline(&spline, spline_to, stroker,
		&stroker->current_face.point, b, c, d))
		return line_to (closure, d);

	compute_face (&stroker->current_face.point, &spline.initial_slope,
		stroker, &face);

	if (stroker->has_current_face)
	{
		int clockwise = join_is_clockwise (&stroker->current_face, &face);
		/* Join with final face from previous segment */
		outer_join (stroker, &stroker->current_face, &face, clockwise);
		inner_join (stroker, &stroker->current_face, &face, clockwise);
	}
	else
	{
		if (! stroker->has_first_face)
		{
			/* Save sub path's first face in case needed for closing join */
			stroker->first_face = face;
			GraphicsTristripMoveTo(stroker->strip, &face.clock_wise);
			stroker->has_first_face = TRUE;
		}
		stroker->has_current_face = TRUE;

		GraphicsTristripAddPoint(stroker->strip, &face.clock_wise);
		GraphicsTristripAddPoint(stroker->strip, &face.counter_clock_wise);
	}
	stroker->current_face = face;

	return GraphicsSplineDecompose(&spline, stroker->tolerance);
}

static eGRAPHICS_STATUS close_path (void *closure)
{
	struct stroker *stroker = closure;
	eGRAPHICS_STATUS status;

	status = line_to (stroker, &stroker->first_point);
	if (UNLIKELY(status))
		return status;

	if (stroker->has_first_face && stroker->has_current_face)
	{
		/* Join first and final faces of sub path */
		outer_close (stroker, &stroker->current_face, &stroker->first_face);
		inner_close (stroker, &stroker->current_face, &stroker->first_face);
	}
	else
	{
		/* Cap the start and end of the sub path as needed */
		add_caps (stroker);
	}

	stroker->has_sub_path = FALSE;
	stroker->has_first_face = FALSE;
	stroker->has_current_face = FALSE;

	return GRAPHICS_STATUS_SUCCESS;
}

eGRAPHICS_INTEGER_STATUS GraphicsPathFixedStrokeToTristrip(
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* matrix,
	const GRAPHICS_MATRIX* matrix_inverse,
	FLOAT_T tolerance,
	GRAPHICS_TRISTRIP* strip
)
{
	struct stroker stroker;
	eGRAPHICS_INTEGER_STATUS status;
	int i;

	if (style->num_dashes)
		return GRAPHICS_INTEGER_STATUS_UNSUPPORTED;

	stroker.style = *style;
	stroker.ctm = matrix;
	stroker.ctm_inverse = matrix_inverse;
	stroker.tolerance = tolerance;

	stroker.ctm_det_positive =
		GRAPHICS_MATRIX_COMPUTE_DETERMINANT(matrix) >= 0.0;

	status = InitializeGraphicsPen(&stroker.pen,
		style->line_width / 2.0,
		tolerance, matrix
	);
	if (UNLIKELY(status))
		return status;

	if (stroker.pen.num_vertices <= 1)
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;

	stroker.has_current_face = FALSE;
	stroker.has_first_face = FALSE;
	stroker.has_sub_path = FALSE;

	stroker.has_limits = strip->num_limits > 0;
	stroker.limit = strip->limits[0];
	for (i = 1; i < strip->num_limits; i++)
		GraphicsBoxAddBox(&stroker.limit, &strip->limits[i]);

	stroker.strip = strip;

	status = GraphicsPathFixedInterpret(path,
		move_to,
		line_to,
		curve_to,
		close_path,
		&stroker
	);
	/* Cap the start and end of the final sub path as needed */
	if (LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
		add_caps (&stroker);

	GraphicsPenFinish(&stroker.pen);

	return status;
}

#ifdef __cplusplus
}
#endif
