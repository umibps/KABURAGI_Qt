#ifndef _INCLUDED_TEXT_RENDER_H_
#define _INCLUDED_TEXT_RENDER_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

EXTERN void DrawText(
	const char* utf8_text,
	const char* true_type_file_path,
	uint8* pixels,
	int x,
	int y,
	int target_width,
	int target_height,
	int target_stride,
	const uint8 color[4],
	int font_size,
	int dot_per_inch,
	int bold,
	int italic,
	int* drawn_width,
	int* drawn_height
);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _INCLUDED_TEXT_RENDER_H_ */
