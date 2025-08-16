#ifndef _INCLUDED_GRAPHICS_H_
#define _INCLUDED_GRAPHICS_H_

#include "graphics_types.h"
#include "graphics_state.h"
#include "graphics_surface.h"
#include "graphics_pattern.h"
#include "../pixel_manipulate/pixel_manipulate.h"

typedef struct _GRAPHICS GRAPHICS;

typedef struct _GRAPHICS_BACKEND
{
	void (*destroy)(void* context);
	void (*finish)(void* context);
	struct _GRAPHICS_SURFACE* (*get_original_target)(void* context);
	struct _GRAPHICS_SURFACE* (*get_current_target)(void* context);
	eGRAPHICS_STATUS (*save)(void* context);
	eGRAPHICS_STATUS (*restore)(void* context);

	eGRAPHICS_STATUS (*push_group)(void* context, eGRAPHICS_CONTENT content);
	struct _GRAPHICS_PATTERN* (*pop_group)(void* context);

	eGRAPHICS_STATUS (*set_source_rgba)(void* context, FLOAT_T red, FLOAT_T green, FLOAT_T bule, FLOAT_T alpha);
	eGRAPHICS_STATUS (*set_source_surface)(
		void* context, struct _GRAPHICS_SURFACE* surface, FLOAT_T x, FLOAT_T y, struct _GRAPHICS_SURFACE_PATTERN* surface_pattern);
	eGRAPHICS_STATUS (*set_source)(void* context, struct _GRAPHICS_PATTERN* source);
	struct _GRAPHICS_PATTERN (*get_source)(void* context);

	eGRAPHICS_STATUS (*set_antialias)(void* context, eGRAPHICS_ANTIALIAS antialias);
	eGRAPHICS_STATUS (*set_dash)(void* context, FLOAT_T* dashes, int num_dashes, FLOAT_T offset);
	eGRAPHICS_STATUS (*set_fill_rule)(void* context, eGRAPHICS_FILL_RULE fill_rule);
	eGRAPHICS_STATUS (*set_line_cap)(void* context, eGRAPHICS_LINE_CAP line_cap);
	eGRAPHICS_STATUS (*set_line_join)(void* context, eGRAPHICS_LINE_JOIN line_join);
	eGRAPHICS_STATUS (*set_line_width)(void* context, FLOAT_T line_width);
	eGRAPHICS_STATUS (*set_miter_linet)(void* context, FLOAT_T limit);
	eGRAPHICS_STATUS (*set_opacity)(void* context, FLOAT_T opacity);
	eGRAPHICS_STATUS (*set_operator)(void* context, eGRAPHICS_OPERATOR op);
	eGRAPHICS_STATUS (*set_tolerance)(void* context, FLOAT_T tolerance);

	eGRAPHICS_ANTIALIAS (*get_antialias)(void* context);
	void (*get_dash)(void* context, FLOAT_T* dashes, int* num_dashes, FLOAT_T* offset);
	eGRAPHICS_FILL_RULE (*get_fill_rule)(void* context);
	eGRAPHICS_LINE_CAP (*get_line_cap)(void* context);
	eGRAPHICS_LINE_JOIN (*get_line_join)(void* context);
	FLOAT_T (*get_line_width)(void* context);
	FLOAT_T (*get_miter_limit)(void* context);
	FLOAT_T (*get_opacity)(void* context);
	eGRAPHICS_OPERATOR (*get_operator)(void* context);
	FLOAT_T (*get_torlearnce)(void* context);

	eGRAPHICS_STATUS (*translate)(void* context, FLOAT_T trans_x, FLOAT_T trans_y);
	eGRAPHICS_STATUS (*scale)(void* context, FLOAT_T scale_x, FLOAT_T scale_y);
	eGRAPHICS_STATUS (*rotate)(void* context, FLOAT_T theta);
	eGRAPHICS_STATUS (*transform)(void* context, const GRAPHICS_MATRIX* matrix);
	eGRAPHICS_STATUS (*set_matrix)(void* context, const GRAPHICS_MATRIX* matrix);
	eGRAPHICS_STATUS (*set_identity_matrix)(void* context);
	eGRAPHICS_STATUS (*get_matrix)(void* context, GRAPHICS_MATRIX* matrix);

	void (*user_to_device)(void* context, FLOAT_T* x, FLOAT_T* y);
	void (*user_to_device_distance)(void* context, FLOAT_T* x, FLOAT_T* y);
	void (*device_to_user)(void* context, FLOAT_T* x, FLOAT_T* y);
	void (*device_to_user_distance)(void* context, FLOAT_T* x, FLOAT_T* y);

	void (*user_to_backend)(void* context, FLOAT_T* x, FLOAT_T* y);
	void (*user_to_backend_distance)(void* context, FLOAT_T* x, FLOAT_T* y);
	void (*backend_to_user)(void* context, FLOAT_T* x, FLOAT_T* y);
	void (*backend_to_user_distance)(void* context, FLOAT_T* x, FLOAT_T* y);

	eGRAPHICS_STATUS (*new_path)(void* context);
	eGRAPHICS_STATUS (*new_sub_path)(void* context);
	eGRAPHICS_STATUS (*move_to)(void* context, FLOAT_T x, FLOAT_T y);
	eGRAPHICS_STATUS (*relative_move_to)(void* context, FLOAT_T diff_x, FLOAT_T diff_y);
	eGRAPHICS_STATUS (*line_to)(void* context, FLOAT_T x, FLOAT_T y);
	eGRAPHICS_STATUS (*relative_line_to)(void* context, FLOAT_T diff_x, FLOAT_T diff_y);
	eGRAPHICS_STATUS (*curve_to)(void* context, FLOAT_T x1, FLOAT_T y1, FLOAT_T x2, FLOAT_T y2, FLOAT_T x3, FLOAT_T y3);
	eGRAPHICS_STATUS (*relative_curve_to)(void* context, FLOAT_T diff_x1, FLOAT_T diff_y1, FLOAT_T diff_x2, FLOAT_T diff_y2, FLOAT_T diff_x3, FLOAT_T diff_y3);
	eGRAPHICS_STATUS (*arc_to)(void* context, FLOAT_T x1, FLOAT_T y1, FLOAT_T x2, FLOAT_T y2, FLOAT_T radius);
	eGRAPHICS_STATUS (*relative_arc_to)(void* context, FLOAT_T diff_x, FLOAT_T diff_y, FLOAT_T diff_x2, FLOAT_T diff_y2, FLOAT_T radius);
	eGRAPHICS_STATUS (*close_path)(void* context);

	eGRAPHICS_STATUS (*arc)(void* cotext, FLOAT_T center_x, FLOAT_T center_y, FLOAT_T radius, FLOAT_T angle1, FLOAT_T angle2, int forward);
	eGRAPHICS_STATUS (*rectangle)(void* context, FLOAT_T x, FLOAT_T y, FLOAT_T width, FLOAT_T height);

	void (*path_extents)(void* context, FLOAT_T* x1, FLOAT_T* y1, FLOAT_T* x2, FLOAT_T* y2);
	int (*has_current_point)(void* context);
	int (*get_current_point)(void* contexxt, FLOAT_T* x, FLOAT_T* y);

	GRAPHICS_PATH* (*copy_path)(void* context);
	GRAPHICS_PATH* (*copy_path_flat)(void* context);
	eGRAPHICS_STATUS (*append_path)(void* context, const GRAPHICS_PATH* path);

	eGRAPHICS_STATUS (*stroke_to_path)(void* context);

	eGRAPHICS_STATUS (*clip)(void* context);
	eGRAPHICS_STATUS (*clip_preserve)(void* context);
	eGRAPHICS_STATUS (*in_clip)(void* context, FLOAT_T x, FLOAT_T y, int* inside);
	eGRAPHICS_STATUS (*reset_clip)(void* context);
	GRAPHICS_RECTANGLE_LIST (*clip_copy_rectangle_list)(void* context);

	eGRAPHICS_STATUS (*paint)(void* context);
	eGRAPHICS_STATUS (*paint_with_alpha)(void* context, FLOAT_T opacity);
	eGRAPHICS_STATUS (*mask)(void* context, struct _GRAPHICS_PATTERN* pattern);

	eGRAPHICS_STATUS (*stroke)(void* context);
	eGRAPHICS_STATUS (*stroke_preserve)(void* context);
	eGRAPHICS_STATUS (*in_stroke)(void* context, FLOAT_T x, FLOAT_T y, int* inside);
	eGRAPHICS_STATUS (*stroke_extents)(void* context, FLOAT_T* x1, FLOAT_T* y1, FLOAT_T* x2, FLOAT_T y2);

	eGRAPHICS_STATUS (*fill)(void* context);
	eGRAPHICS_STATUS (*fill_preserve)(void* context);
	eGRAPHICS_STATUS (*in_fill)(void* context, FLOAT_T x, FLOAT_T y, int* inside);
	eGRAPHICS_STATUS (*fill_extents)(void* context, FLOAT_T* x1, FLOAT_T* y1, FLOAT_T* x2, FLOAT_T* y2);
} GRAPHICS_BACKEND;

struct _GRAPHICS
{
	PIXEL_MANIPULATE pixel_manipulate;
	void (*error_message)(void* error_message_data);
	void *error_message_data;
	GRAPHICS_COLOR color_white;
	GRAPHICS_COLOR color_black;
	GRAPHICS_COLOR color_transparent;
	GRAPHICS_PATTERN error_pattern;
	GRAPHICS_SOLID_PATTERN nil_pattern;
	GRAPHICS_SOLID_PATTERN white_pattern;
	GRAPHICS_SOLID_PATTERN pattern_black;
	GRAPHICS_SOLID_PATTERN clear_pattern;
	// GRAPHICS_COLOR color_magenta;
	GRAPHICS_COMPOSITOR shape_mask_compositor;
	GRAPHICS_SPANS_COMPOSITOR spans_compositor;
	GRAPHICS_MASK_COMPOSITOR mask_compositor;
	GRAPHICS_COMPOSITOR no_compositor;
	GRAPHICS_SURFACE_BACKEND image_surface_backend;
	GRAPHICS_SURFACE_BACKEND image_source_backend;
	GRAPHICS_SURFACE_BACKEND subsurface_backend;
	GRAPHICS_BACKEND context_backend;
	GRAPHICS_RECTANGLE_INT unbounded_rectangle;
	GRAPHICS_RECTANGLE_INT empty_rectangle;
};

typedef struct _GRAPHICS_CONTEXT
{
	GRAPHICS *graphics;
	int ref_count;
	eGRAPHICS_STATUS status;
	int own_memory;
	void *user_data;

	GRAPHICS_BACKEND *backend;
} GRAPHICS_CONTEXT;

typedef struct _GRAPHICS_DEFAULT_CONTEXT
{
	GRAPHICS_CONTEXT base;

	GRAPHICS_STATE *state;
	GRAPHICS_STATE state_tail[2];
	GRAPHICS_STATE *state_free_list;

	GRAPHICS_PATH_FIXED path;
} GRAPHICS_DEFAULT_CONTEXT;

#define GRAPHICS_ALPHA_SHORT_IS_CLEAR(alpha) ((alpha) <= 0x00ff)
#define GRAPHICS_ALPHA_SHORT_IS_OPAQUE(alpha) ((alpha) >= 0xff00)
#define GRAPHICS_COLOR_IS_CLEAR(color) GRAPHICS_ALPHA_SHORT_IS_CLEAR((color)->alpha_short)
#define GRAPHICS_ALPHA_IS_OPAQUE(alpha) ((alpha) >= ((FLOAT_T)0xff00 / (FLOAT_T)0xFFFF))
#define GRAPHICS_ALPHA_IS_ZERO(alpha) ((alpha) <= 0.0)

static INLINE void InitializeGraphicsUnboundedRectangle(GRAPHICS_RECTANGLE_INT* rectangle)
{
	const GRAPHICS_RECTANGLE_INT unbounded_rectanlge
		= {GRAPHICS_RECTANGLE_MIN, GRAPHICS_RECTANGLE_MIN,
		   GRAPHICS_RECTANGLE_MAX - GRAPHICS_RECTANGLE_MIN,
		   GRAPHICS_RECTANGLE_MAX - GRAPHICS_RECTANGLE_MIN};
	*rectangle = unbounded_rectanlge;
}

#ifdef __cplusplus
extern "C" {
#endif

extern int InitializeGraphics(GRAPHICS* graphics);
extern eGRAPHICS_STATUS InitializeGraphicsDefaultContext(GRAPHICS_DEFAULT_CONTEXT* context, void* target, void* graphics);
extern GRAPHICS_CONTEXT* GraphicsDefaultContextCreate(void* target, void* graphics);
extern void GraphicsClip(GRAPHICS_CONTEXT* context);

extern void GraphicsSetSource(GRAPHICS_CONTEXT* context, GRAPHICS_PATTERN* source);
extern void GraphicsSetSourceRGB(GRAPHICS_CONTEXT* context, FLOAT_T red, FLOAT_T green, FLOAT_T blue);
extern void GraphicsSetSourceRGBA(GRAPHICS_CONTEXT* context, FLOAT_T red, FLOAT_T green, FLOAT_T blue, FLOAT_T alpha);
extern void GraphicsSetSourceSurface(GRAPHICS_CONTEXT* context, GRAPHICS_SURFACE* surface, FLOAT_T x, FLOAT_T y, GRAPHICS_SURFACE_PATTERN* pattern);
extern void GraphicsRectangle(GRAPHICS_CONTEXT* context, FLOAT_T x, FLOAT_T y, FLOAT_T width, FLOAT_T height);
extern void GraphicsPaint(GRAPHICS_CONTEXT* context);
extern void GraphicsPaintWithAlpha(GRAPHICS_CONTEXT* context, FLOAT_T alpha);
extern void GraphicsFill(GRAPHICS_CONTEXT* context);
extern void GraphicsFillPreserve(GRAPHICS_CONTEXT* context);
extern void GraphicsMask(GRAPHICS_CONTEXT* context, GRAPHICS_PATTERN* pattern);
extern void GraphicsMaskSurface(
	GRAPHICS_CONTEXT* context,
	GRAPHICS_SURFACE* surface,
	FLOAT_T surface_x,
	FLOAT_T surface_y
);
extern void GraphicsStroke(GRAPHICS_CONTEXT* context);
extern void GraphicsStrokePreserve(GRAPHICS_CONTEXT* context);

extern void InitializeGraphicsDefaultBackend(GRAPHICS_BACKEND* backend);
extern void GraphicsGetMatrix(GRAPHICS_CONTEXT* context, GRAPHICS_MATRIX* matrix);
extern FLOAT_T GraphicsGetTolerance(GRAPHICS_CONTEXT* context);
extern void GraphicsMoveTo(GRAPHICS_CONTEXT* contect, FLOAT_T x, FLOAT_T y);
extern void GraphicsLineTo(GRAPHICS_CONTEXT* context, FLOAT_T x, FLOAT_T y);
extern void GraphicsCurveTo(
	GRAPHICS_CONTEXT* context,
	FLOAT_T x1, FLOAT_T y1,
	FLOAT_T x2, FLOAT_T y2,
	FLOAT_T x3, FLOAT_T y3
);

extern void GraphicsArcPath(
	GRAPHICS_CONTEXT* context,
	FLOAT_T xc,
	FLOAT_T yc,
	FLOAT_T radius,
	FLOAT_T angle1,
	FLOAT_T angle2
);

extern void GraphicsArcPathNegative(
	GRAPHICS_CONTEXT* context,
	FLOAT_T xc,
	FLOAT_T yc,
	FLOAT_T radius,
	FLOAT_T angle1,
	FLOAT_T angle2
);

extern void GraphicsArc(
	GRAPHICS_CONTEXT* context,
	FLOAT_T xc,
	FLOAT_T yc,
	FLOAT_T radius,
	FLOAT_T angle1,
	FLOAT_T angle2
);

extern void GraphicsArcNegative(
	GRAPHICS_CONTEXT* context,
	FLOAT_T xc,
	FLOAT_T yc,
	FLOAT_T radius,
	FLOAT_T angle1,
	FLOAT_T angle2
);

extern void GraphicsClosePath(GRAPHICS_CONTEXT* context);
extern void GraphicsPathExtents(
	GRAPHICS_CONTEXT* context,
	FLOAT_T* x1,FLOAT_T* y1,
	FLOAT_T* x2,FLOAT_T* y2
);

extern eGRAPHICS_CONTENT GraphicsFormatToContent(eGRAPHICS_FORMAT format);

extern void GraphicsSetOperator(GRAPHICS_CONTEXT* context, eGRAPHICS_OPERATOR op);

extern void GraphicsTranslate(GRAPHICS_CONTEXT* context, FLOAT_T tx, FLOAT_T ty);
extern void GraphicsScale(GRAPHICS_CONTEXT* context, FLOAT_T sx, FLOAT_T sy);
extern void GraphicsRotate(GRAPHICS_CONTEXT* context,FLOAT_T angle);

extern void GraphicsRectangle(
	GRAPHICS_CONTEXT* context,
	FLOAT_T x,FLOAT_T y,
	FLOAT_T width,FLOAT_T height
);

extern void GraphicsSetLineWidth(GRAPHICS_CONTEXT* context,FLOAT_T width);
extern void GraphicsSetLineCap(GRAPHICS_CONTEXT* context,eGRAPHICS_LINE_CAP line_cap);
extern void GraphicsSetLineJoin(GRAPHICS_CONTEXT* context,eGRAPHICS_LINE_JOIN line_join);

extern void GraphicsSetAntialias(GRAPHICS_CONTEXT* context, eGRAPHICS_ANTIALIAS antialias);

extern void GraphicsSave(GRAPHICS_CONTEXT* context);
extern void GraphicsRestore(GRAPHICS_CONTEXT* context);

extern int GraphicsFormatBitsPerPixel(eGRAPHICS_FORMAT format);
extern int GraphicsFormatStrideForWidth(eGRAPHICS_FORMAT format,int width);

extern eGRAPHICS_LINE_CAP GraphicsGetLineCap(GRAPHICS_CONTEXT* context);

#ifdef __cplusplus
}
#endif

#endif  // #ifndef _INCLUDED_GRAPHICS_H_
