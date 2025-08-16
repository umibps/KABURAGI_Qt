#ifndef _INCLUDED_PIXEL_MANIPULATE_MATRIX_H_
#define _INCLUDED_PIXEL_MANIPULATE_MATRIX_H_

#include "pixel_manipulate.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int PixelManipulateTransformPoint(const PIXEL_MANIPULATE_TRANSFORM* transform, PIXEL_MANIPULATE_VECTOR* vector);

extern int PixelManipulateTransformPoint3d (
	const PIXEL_MANIPULATE_TRANSFORM* transform,
	PIXEL_MANIPULATE_VECTOR*		 vector
);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_PIXEL_MANIPULATE_MATRIX_H_
