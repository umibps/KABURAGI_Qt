#ifndef _INCLUDED_GRAPHICS_MATRIX_H_
#define _INCLUDED_GRAPHICS_MATRIX_H_

#include "graphics_types.h"

#define GRAPHICS_MATRIX_IS_TRANSLATION(matrix) \
	((matrix)->xx == 1.0 && (matrix)->yx == 0.0 && (matrix)->xy == 0.0 && (matrix)->yy == 1.0)
#define GRAPHICS_MATRIX_IS_IDENTITY(matrix) \
	((matrix)->xx == 1.0 && (matrix)->yx == 0.0 && (matrix)->xy == 0.0 && (matrix)->yy == 1.0 \
		&& (matrix)->x0 == 0.0 && (matrix)->y0 == 0.0)

#define GRAPHICS_MATRIX_COMPARE(m1, m2) \
	((*(m1)).xx != ((*(m2))).xx \
	| ((*(m1))).yx != ((*(m2))).yx \
	| ((*(m1))).xy != ((*(m2))).xy \
	| ((*(m1))).yy != ((*(m2))).yy \
	| ((*(m1))).x0 != ((*(m2))).x0 \
	| ((*(m1))).y0 != ((*(m2))).y0)

#ifdef __cplusplus
extern "C" {
#endif

extern INLINE void InitializeGraphicsMatrix(
	GRAPHICS_MATRIX* matrix,
	FLOAT_T xx, FLOAT_T yx,
	FLOAT_T xy, FLOAT_T yy,
	FLOAT_T x0, FLOAT_T y0
);

extern INLINE void InitializeGraphicsMatrixIdentity(GRAPHICS_MATRIX* matrix);
extern INLINE void InitializeGraphicsMatrixTranslate(GRAPHICS_MATRIX* matrix, FLOAT_T tx, FLOAT_T ty);
extern INLINE void InitializeGraphicsMatrixScale(GRAPHICS_MATRIX* matrix, FLOAT_T sx, FLOAT_T sy);
extern INLINE void InitializeGraphicsMatrixRotate(GRAPHICS_MATRIX* matrix, FLOAT_T radian);
extern void GraphicsMatrixMultiply(GRAPHICS_MATRIX* result, const GRAPHICS_MATRIX* a, const GRAPHICS_MATRIX* b);
extern void GraphicsMatrixTranslate(GRAPHICS_MATRIX* matrix, FLOAT_T tx, FLOAT_T ty);
extern int GraphicsMatrixIsPixelExact(const GRAPHICS_MATRIX* matrix);
extern int GraphicsMatrixIsInvertible(const GRAPHICS_MATRIX* matrix);
extern int GraphicsMatrixHasUnityScale(const GRAPHICS_MATRIX* matrix);
extern int GraphicsMatrixIsPixelManipulateTranslation(
	const GRAPHICS_MATRIX* matrix,
	eGRAPHICS_FILTER filter,
	int* x_offset,
	int* y_offset
);
extern void GraphicsMatrixTransformDistance(const GRAPHICS_MATRIX* matrix, FLOAT_T* dx, FLOAT_T* dy);
extern void GraphicsMatrixTransformPoint(const GRAPHICS_MATRIX* matrix, FLOAT_T* x, FLOAT_T* y);
extern void GraphicsMatrixTransformBoundingBox(
	const GRAPHICS_MATRIX* matrix,
	FLOAT_T* x1, FLOAT_T* y1,
	FLOAT_T* x2, FLOAT_T* y2,
	int* is_tight
);
extern FLOAT_T GraphicsMatrixTransformedCircleMajorAxis(const GRAPHICS_MATRIX* matrix, FLOAT_T radius);
extern int GraphicsMatrixIsIntegerTranslation(const GRAPHICS_MATRIX* matrix, int* itx, int* ity);
extern eGRAPHICS_STATUS GraphicsMatrixToPixelManipulateMatrixOffset(
	const GRAPHICS_MATRIX* matrix,
	eGRAPHICS_FILTER filter,
	FLOAT_T xc,
	FLOAT_T yc,
	PIXEL_MANIPULATE_TRANSFORM* out_transform,
	int* x_offset,
	int* y_offset
);

#ifdef __cplusplus
}
#endif

static INLINE void GraphicsMatrixScale(GRAPHICS_MATRIX* matrix,FLOAT_T sx,FLOAT_T sy)
{
	GRAPHICS_MATRIX temp;

	InitializeGraphicsMatrixScale(&temp, sx, sy);

	GraphicsMatrixMultiply(matrix, &temp, matrix);
}

#endif // #ifndef _INCLUDED_GRAPHICS_MATRIX_H_
