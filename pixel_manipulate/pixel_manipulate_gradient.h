#ifndef _INCLUDED_PIXEL_MANIPULATE_GRADIENT_H_
#define _INCLUDED_PIXEL_MANIPULATE_GRADIENT_H_

#include "pixel_manipulate_composite.h"

extern void PixelManipulateLinearGradientIteratorInitialize(
	PIXEL_MANIPULATE_IMAGE* image,
	PIXEL_MANIPULATE_ITERATION* iter
);

extern void PixelManipulateRadialGradientIterationInitialize(
	PIXEL_MANIPULATE_IMAGE* image,
	PIXEL_MANIPULATE_ITERATION* iter
);

extern void PixelManipulateConicalGradientIterationInitialize(PIXEL_MANIPULATE_IMAGE* image, PIXEL_MANIPULATE_ITERATION* iter);

#endif	 // #ifndef _INCLUDED_PIXEL_MANIPULATE_GRADIENT_H_
