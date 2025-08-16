#include "graphics_types.h"
#include "graphics_matrix.h"
#include "graphics_private.h"
#include "graphics_inline.h"
#include "../pixel_manipulate/pixel_manipulate_matrix.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ISFINITE(x) ((x) * (x) >= 0.0)

INLINE void InitializeGraphicsMatrix(GRAPHICS_MATRIX* matrix,
	FLOAT_T xx, FLOAT_T yx,
	FLOAT_T xy, FLOAT_T yy,
	FLOAT_T x0, FLOAT_T y0
)
{
	matrix->xx = xx,	matrix->yx = yx,
	matrix->xy = xy,	matrix->yy = yy,
	matrix->x0 = x0,	matrix->y0 = y0;
}

INLINE void InitializeGraphicsMatrixIdentity(GRAPHICS_MATRIX* matrix)
{
	InitializeGraphicsMatrix(matrix,
		1, 0,
		0, 1,
		0, 0
	);
}

INLINE void InitializeGraphicsMatrixTranslate(GRAPHICS_MATRIX* matrix, FLOAT_T tx, FLOAT_T ty)
{
	InitializeGraphicsMatrix(matrix, 1, 0, 0, 1, tx, ty);
}

INLINE void InitializeGraphicsMatrixScale(GRAPHICS_MATRIX* matrix, FLOAT_T sx, FLOAT_T sy)
{
	InitializeGraphicsMatrix(matrix,
		sx, 0,
		0, sy,
		0, 0
	);
}

INLINE void InitializeGraphicsMatrixRotate(GRAPHICS_MATRIX* matrix, FLOAT_T radian)
{
	FLOAT_T sin_value, cos_value;

	sin_value = sin(radian);
	cos_value = cos(radian);

	InitializeGraphicsMatrix(matrix,
		cos_value, sin_value,
		-sin_value, cos_value,
		0, 0
	);
}

INLINE void GraphicsMatrixScalarMultiply(GRAPHICS_MATRIX* matrix, FLOAT_T scalar)
{
	matrix->xx *= scalar;
	matrix->yx *= scalar;
	matrix->xy *= scalar;
	matrix->x0 *= scalar;
	matrix->y0 *= scalar;
}

INLINE void GraphicsMatrixGetAffine(const GRAPHICS_MATRIX* matrix,
	FLOAT_T* xx, FLOAT_T *yx,
	FLOAT_T* xy, FLOAT_T *yy,
	FLOAT_T* x0, FLOAT_T *y0
)
{
	*xx = matrix->xx;
	*yx = matrix->yx;

	*xy = matrix->xy;
	*yy = matrix->yy;

	if(x0 != NULL)
	{
		*x0 = matrix->x0;
	}

	if(y0 != NULL)
	{
		*y0 = matrix->y0;
	}
}

static void GraphicsMatrixComputeAdjoint(GRAPHICS_MATRIX* matrix)
{
	GRAPHICS_MATRIX temp = *matrix;

	matrix->xx = temp.yy;
	matrix->yx = - temp.yx;
	matrix->xy = - temp.xy;
	matrix->yy = temp.xx;
	matrix->x0 = temp.xy * temp.y0 - temp.yy * temp.x0;
	matrix->y0 = temp.yx * temp.x0 - temp.xx * temp.y0;
}

void GraphicsMatrixTranslate(GRAPHICS_MATRIX* matrix, FLOAT_T tx, FLOAT_T ty)
{
	GRAPHICS_MATRIX tmp;

	InitializeGraphicsMatrixTranslate(&tmp, tx, ty);

	GraphicsMatrixMultiply(matrix, &tmp, matrix);
}

void GraphicsMatrixMultiply(GRAPHICS_MATRIX* result, const GRAPHICS_MATRIX* a, const GRAPHICS_MATRIX* b)
{
	GRAPHICS_MATRIX r;

	r.xx = a->xx * b->xx + a->yx * b->xy;
	r.yx = a->xx * b->yx + a->yx * b->yy;

	r.xy = a->xy * b->xx + a->yy * b->xy;
	r.yy = a->xy * b->yx + a->yy * b->yy;

	r.x0 = a->x0 * b->xx + a->y0 * b->xy + b->x0;
	r.y0 = a->x0 * b->yx + a->y0 * b->yy + b->y0;

	*result = r;
}

int GraphicsMatrixIsInvertible(const GRAPHICS_MATRIX* matrix)
{
	FLOAT_T det;

	det = GRAPHICS_MATRIX_COMPUTE_DETERMINANT(matrix);

	return IS_FINITE(det) && det != 0.0;
}

eGRAPHICS_STATUS GraphicsMatrixInvert(GRAPHICS_MATRIX* matrix)
{
	FLOAT_T det;

	if(matrix->xy == 0.0 && matrix->yx == 0.0)
	{
		matrix->x0 = - matrix->x0;
		matrix->y0 = - matrix->y0;

		if(matrix->xx != 1.0)
		{
			if(matrix->xx == 0.0)
			{
				return GRAPHICS_STATUS_INVALID_MATRIX;
			}

			matrix->xx = 1.0 / matrix->xx;
			matrix->x0 *= matrix->xx;
		}

		if(matrix->yy != 1.0)
		{
			if(matrix->yy == 0.0)
			{
				return GRAPHICS_STATUS_INVALID_MATRIX;
			}

			matrix->yy = 1.0 / matrix->yy;
			matrix->y0 *= matrix->yy;
		}

		return GRAPHICS_STATUS_SUCCESS;
	}

	det = GRAPHICS_MATRIX_COMPUTE_DETERMINANT(matrix);

	if(!ISFINITE(det))
	{
		return GRAPHICS_STATUS_INVALID_MATRIX;
	}

	if(det == 0)
	{
		return GRAPHICS_STATUS_INVALID_MATRIX;
	}

	GraphicsMatrixComputeAdjoint(matrix);
	GraphicsMatrixScalarMultiply(matrix, 1 / det);

	return GRAPHICS_STATUS_SUCCESS;
}

#define SCALING_EXPILON GRAPHICS_FIXED_TO_FLOAT(1)

int GraphicsMatrixHasUnityScale(const GRAPHICS_MATRIX* matrix)
{
	FLOAT_T det = GRAPHICS_MATRIX_COMPUTE_DETERMINANT(matrix);

	if(FABS(det * det - 1.0) < SCALING_EXPILON)
	{
		if(FABS(matrix->xy) < SCALING_EXPILON && FABS(matrix->yx) < SCALING_EXPILON)
		{
			return TRUE;
		}
		if(FABS(matrix->xx) < SCALING_EXPILON && FABS(matrix->yy) < SCALING_EXPILON)
		{
			return TRUE;
		}
	}
	return FALSE;
}

int GraphicsMatrixIsPixelExact(const GRAPHICS_MATRIX* matrix)
{
	int32 x0_fixed, y0_fixed;

	if(GraphicsMatrixHasUnityScale(matrix) == FALSE)
	{
		return FALSE;
	}

	x0_fixed = GraphicsFixedFromFloat(matrix->x0);
	y0_fixed = GraphicsFixedFromFloat(matrix->y0);

	return GRAPHICS_FIXED_IS_INTEGER(x0_fixed) && GRAPHICS_FIXED_IS_INTEGER(y0_fixed);
}

int GraphicsMatrixIsPixelManipulateTranslation(
	const GRAPHICS_MATRIX* matrix,
	eGRAPHICS_FILTER filter,
	int* x_offset,
	int* y_offset
)
{
	FLOAT_T tx, ty;

	if(GRAPHICS_MATRIX_IS_TRANSLATION(matrix) == FALSE)
	{
		return FALSE;
	}

	if(matrix->x0 == 0.0 && matrix->y0 == 0.0)
	{
		return TRUE;
	}

	tx = matrix->x0 + *x_offset;
	ty = matrix->y0 + *y_offset;

	if(filter == GRAPHICS_FILTER_FAST || filter == GRAPHICS_FILTER_NEAREST)
	{
		tx = PIXEL_MANIPULATE_NEAREST_SAMPLE(tx);
		ty = PIXEL_MANIPULATE_NEAREST_SAMPLE(ty);
	}
	else if(tx != floor(tx) || ty != floor(ty))
	{
		return FALSE;
	}

	if(fabs(tx) > PIXEL_MANIPULATE_MAX_INT || fabs(ty) > PIXEL_MANIPULATE_MAX_INT)
	{
		return FALSE;
	}

	*x_offset = (int)floor(tx + 0.5);
	*y_offset = (int)floor(ty + 0.5);

	return TRUE;
}

void GraphicsMatrixTransformDistance(const GRAPHICS_MATRIX* matrix, FLOAT_T* dx, FLOAT_T* dy)
{
	FLOAT_T new_x, new_y;

	new_x = (matrix->xx * *dx + matrix->xy * *dy);
	new_y = (matrix->yx * *dx + matrix->yy * *dy);

	*dx = new_x;
	*dy = new_y;
}

void GraphicsMatrixTransformPoint(const GRAPHICS_MATRIX* matrix, FLOAT_T* x, FLOAT_T* y)
{
	GraphicsMatrixTransformDistance(matrix, x, y);

	*x += matrix->x0;
	*y += matrix->y0;
}

void GraphicsMatrixTransformBoundingBox(
	const GRAPHICS_MATRIX* matrix,
	FLOAT_T* x1, FLOAT_T* y1,
	FLOAT_T* x2, FLOAT_T* y2,
	int* is_tight
)
{
	int i;
	FLOAT_T quad_x[4], quad_y[4];
	FLOAT_T min_x, max_x;
	FLOAT_T min_y, max_y;

	if(matrix->xy == 0.0 && matrix->yx == 0.0)
	{
		if(matrix->xx != 1.0)
		{
			quad_x[0] = *x1 * matrix->xx;
			quad_x[1] = *x2 * matrix->xx;
			if(quad_x[0] < quad_x[1])
			{
				*x1 = quad_x[0];
				*x2 = quad_x[1];
			}
			else
			{
				*x1 = quad_x[1];
				*x2 = quad_x[0];
			}
		}
		if(matrix->x0 != 0.0)
		{
			*x1 += matrix->x0;
			*x2 += matrix->x0;
		}

		if(matrix->yy != 1.0)
		{
			quad_y[0] = *y1 * matrix->yy;
			quad_y[1] = *y2 * matrix->yy;
			if(quad_y[0] < quad_y[1])
			{
				*y1 = quad_y[0];
				*y2 = quad_y[1];
			}
			else
			{
				*y1 = quad_y[1];
				*y2 = quad_y[0];
			}
		}
		if(matrix->y0 != 0.0)
		{
			*y1 += matrix->y0;
			*y2 += matrix->y0;
		}

		if(is_tight != NULL)
		{
			*is_tight = TRUE;
		}

		return;
	}

	quad_x[0] = *x1;
	quad_y[0] = *y1;
	GraphicsMatrixTransformPoint(matrix, &quad_x[0], &quad_y[0]);

	quad_x[1] = *x2;
	quad_y[1] = *y1;
	GraphicsMatrixTransformPoint(matrix, &quad_x[1], &quad_y[1]);

	quad_x[2] = *x1;
	quad_y[2] = *y2;
	GraphicsMatrixTransformPoint(matrix, &quad_x[2], &quad_y[2]);

	quad_x[3] = *x2;
	quad_y[3] = *y2;
	GraphicsMatrixTransformPoint(matrix, &quad_x[3], &quad_y[3]);

	min_x = max_x = quad_x[0];
	min_y = max_y = quad_y[0];

	for(i=1; i<4; i++)
	{
		if(quad_x[i] < min_x)
		{
			min_x = quad_x[i];
		}
		if(quad_x[i] > max_x)
		{
			max_x = quad_x[i];
		}

		if(quad_y[i] < min_y)
		{
			min_y = quad_y[i];
		}
		if(quad_y[i] > max_y)
		{
			max_y = quad_y[i];
		}
	}

	*x1 = min_x;
	*y1 = min_y;
	*x2 = max_x;
	*y2 = max_y;

	if(is_tight != NULL)
	{
		*is_tight =
			(quad_x[1] == quad_x[0] && quad_y[1] == quad_y[3]
				&& quad_x[2] == quad_x[3] && quad_y[2] == quad_y[0])
			||
			(quad_x[1] == quad_x[3] && quad_y[1] == quad_y[0]
				&& quad_x[2] == quad_x[0] && quad_y[2] == quad_y[3]);
	}
}

FLOAT_T GraphicsMatrixTransformedCircleMajorAxis(const GRAPHICS_MATRIX* matrix, FLOAT_T radius)
{
	FLOAT_T a, b, c, d, f, g, h, i, j;

	if(GraphicsMatrixHasUnityScale(matrix) != FALSE)
	{
		return radius;
	}

	GraphicsMatrixGetAffine(matrix, &a, &b, &c, &d, NULL, NULL);

	i = a * a + b * b;
	j = c * c + d * d;

	f = 0.5 * (i + j);
	g = 0.5 * (i - j);
	h = a * c + b * d;

	return radius * SQRT(f + HYPOT(g, h));
}

int GraphicsMatrixIsIntegerTranslation(const GRAPHICS_MATRIX* matrix, int* itx, int* ity)
{
	if(GraphicsMatrixIsTranslation(matrix))
	{
		GRAPHICS_FLOAT_FIXED x0_fixed = GraphicsFixedFromFloat(matrix->x0);
		GRAPHICS_FLOAT_FIXED y0_fixed = GraphicsFixedFromFloat(matrix->y0);

		if(GRAPHICS_FIXED_IS_INTEGER(x0_fixed) && GRAPHICS_FIXED_IS_INTEGER(y0_fixed))
		{
			*itx = GraphicsFixedIntegerPart(x0_fixed);
			*ity = GraphicsFixedIntegerPart(y0_fixed);

			return TRUE;
		}
	}

	return FALSE;
}

static const PIXEL_MANIPULATE_TRANSFORM pixman_identity_transform = {{
	{1 << 16,		0,	   0},
{	   0, 1 << 16,	   0},
{	   0,	   0, 1 << 16}
	}};

static eGRAPHICS_STATUS GraphicsMatrixToPixelManipulateMatrix(
	const GRAPHICS_MATRIX* matrix,
	PIXEL_MANIPULATE_TRANSFORM* pixman_transform,
	FLOAT_T xc,
	FLOAT_T yc
)
{
	GRAPHICS_MATRIX inv;
	unsigned max_iterations;

	pixman_transform->matrix[0][0] = GraphicsFixed16_16FromFloat(matrix->xx);
	pixman_transform->matrix[0][1] = GraphicsFixed16_16FromFloat(matrix->xy);
	pixman_transform->matrix[0][2] = GraphicsFixed16_16FromFloat(matrix->x0);

	pixman_transform->matrix[1][0] = GraphicsFixed16_16FromFloat(matrix->yx);
	pixman_transform->matrix[1][1] = GraphicsFixed16_16FromFloat(matrix->yy);
	pixman_transform->matrix[1][2] = GraphicsFixed16_16FromFloat(matrix->y0);

	pixman_transform->matrix[2][0] = 0;
	pixman_transform->matrix[2][1] = 0;
	pixman_transform->matrix[2][2] = 1 << 16;

	/* The conversion above breaks cairo's translation invariance:
	* a translation of (a, b) in device space translates to
	* a translation of (xx * a + xy * b, yx * a + yy * b)
	* for cairo, while pixman uses rounded versions of xx ... yy.
	* This error increases as a and b get larger.
	*
	* To compensate for this, we fix the point (xc, yc) in pattern
	* space and adjust pixman's transform to agree with cairo's at
	* that point.
	*/

	if(GraphicsMatrixHasUnityScale(matrix))
		return GRAPHICS_STATUS_SUCCESS;

	if (UNLIKELY(fabs (matrix->xx) > PIXEL_MANIPULATE_MAX_INT ||
		fabs (matrix->xy) > PIXEL_MANIPULATE_MAX_INT ||
		fabs (matrix->x0) > PIXEL_MANIPULATE_MAX_INT ||
		fabs (matrix->yx) > PIXEL_MANIPULATE_MAX_INT ||
		fabs (matrix->yy) > PIXEL_MANIPULATE_MAX_INT ||
		fabs (matrix->y0) > PIXEL_MANIPULATE_MAX_INT)
	)
	{
		return (GRAPHICS_STATUS_INVALID_MATRIX);
	}

	/* Note: If we can't invert the transformation, skip the adjustment. */
	inv = *matrix;
	if(GraphicsMatrixInvert(&inv) != GRAPHICS_STATUS_SUCCESS)
		return GRAPHICS_STATUS_SUCCESS;

	/* find the pattern space coordinate that maps to (xc, yc) */
	max_iterations = 5;
	do
	{
		FLOAT_T x,y;
		PIXEL_MANIPULATE_VECTOR vector;
		GRAPHICS_FIXED_16_16 dx, dy;

		vector.vector[0] = GraphicsFixed16_16FromFloat(xc);
		vector.vector[1] = GraphicsFixed16_16FromFloat(yc);
		vector.vector[2] = 1 << 16;

		/* If we can't transform the reference point, skip the adjustment. */
		if (! PixelManipulateTransformPoint3d(pixman_transform, &vector))
			return GRAPHICS_STATUS_SUCCESS;
		
		x = PIXEL_MANIPULATE_FIXED_TO_FLOAT(vector.vector[0]);
		y = PIXEL_MANIPULATE_FIXED_TO_FLOAT(vector.vector[1]);
		GraphicsMatrixTransformPoint(&inv, &x, &y);

		/* Ideally, the vector should now be (xc, yc).
		* We can now compensate for the resulting error.
		*/
		x -= xc;
		y -= yc;
		GraphicsMatrixTransformDistance(matrix, &x, &y);
		dx = GraphicsFixed16_16FromFloat(x);
		dy = GraphicsFixed16_16FromFloat(y);
		pixman_transform->matrix[0][2] -= dx;
		pixman_transform->matrix[1][2] -= dy;

		if (dx == 0 && dy == 0)
			return GRAPHICS_STATUS_SUCCESS;
	} while(--max_iterations);

	/* We didn't find an exact match between cairo and pixman, but
	* the matrix should be mostly correct */
	return GRAPHICS_STATUS_SUCCESS;
}

eGRAPHICS_STATUS GraphicsMatrixToPixelManipulateMatrixOffset(
	const GRAPHICS_MATRIX* matrix,
	eGRAPHICS_FILTER filter,
	FLOAT_T xc,
	FLOAT_T yc,
	PIXEL_MANIPULATE_TRANSFORM* out_transform,
	int* x_offset,
	int* y_offset
)
{
	int is_pixman_translation;

	is_pixman_translation = GraphicsMatrixIsPixelManipulateTranslation(
		matrix, filter, x_offset, y_offset);

	if (is_pixman_translation)
	{
		*out_transform = pixman_identity_transform;
		return GRAPHICS_INTEGER_STATUS_NOTHING_TO_DO;
	}
	else
	{
		GRAPHICS_MATRIX m;

		m = *matrix;
		GraphicsMatrixTranslate(&m, *x_offset, *y_offset);
		if(m.x0 != 0.0 || m.y0 != 0.0)
		{
			double tx, ty, norm;
			int i, j;

			/* pixman also limits the [xy]_offset to 16 bits so evenly
			* spread the bits between the two.
			*
			* To do this, find the solutions of:
			*   |x| = |x*m.xx + y*m.xy + m.x0|
			*   |y| = |x*m.yx + y*m.yy + m.y0|
			*
			* and select the one whose maximum norm is smallest.
			*/
			tx = m.x0;
			ty = m.y0;
			norm = MAXIMUM(fabs (tx), fabs (ty));

			for(i = -1; i < 2; i+=2)
			{
				for(j = -1; j < 2; j+=2)
				{
					double x, y, den, new_norm;

					den = (m.xx + i) * (m.yy + j) - m.xy * m.yx;
					if (fabs (den) < DBL_EPSILON)
						continue;

					x = m.y0 * m.xy - m.x0 * (m.yy + j);
					y = m.x0 * m.yx - m.y0 * (m.xx + i);

					den = 1 / den;
					x *= den;
					y *= den;

					new_norm = MAXIMUM(fabs (x), fabs (y));
					if(norm > new_norm)
					{
						norm = new_norm;
						tx = x;
						ty = y;
					}
				}
			}
			
			tx = floor (tx);
			ty = floor (ty);
			*x_offset = (int)-tx;
			*y_offset = (int)-ty;
			GraphicsMatrixTranslate(&m, tx, ty);
		}
		else
		{
			*x_offset = 0;
			*y_offset = 0;
		}

		return GraphicsMatrixToPixelManipulateMatrix(&m, out_transform, xc, yc);
	}
}

#ifdef __cplusplus
}
#endif
