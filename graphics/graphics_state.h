#ifndef _INCLUDED_GRAPHICS_STATE_H_
#define _INCLUDED_GRAPHICS_STATE_H_

#include "graphics_types.h"
#include "graphics_clip.h"
#include "graphics_surface.h"

#define GRAPHICS_STATE_OPERATOR_DEFAULT GRAPHICS_OPERATOR_OVER
#define GRAPHICS_STATE_TOLERANCE_DEFAULT 0.1
#define GRAPHICS_STATE_FILL_RULE_DEFAULT GRAPHICS_FILL_RULE_WINDING
#define GRAPHICS_STATE_LINE_WIDTH_DEFAULT 2.0
#define GRAPHICS_STATE_LINE_CAP_DEFAULT GRAPHICS_LINE_CAP_BUTT
#define GRAPHICS_STATE_LINE_JOIN_DEFAULT GRAPHICS_LINE_JOIN_MITER
#define GRAPHICS_STATE_MITER_LIMIT_DEFAULT 10.0
#define GRAPHICS_STATE_DEFAULT_FONT_SIZE 10.0

typedef struct _GRAPHICS_STATE
{
	eGRAPHICS_OPERATOR op;

	FLOAT_T opacity;
	FLOAT_T tolerance;
	eGRAPHICS_ANTIALIAS antialias;
	GRAPHICS_STROKE_STYLE stroke_style;

	eGRAPHICS_FILL_RULE fill_rule;

	GRAPHICS_CLIP *clip;

	GRAPHICS_SURFACE *target;
	GRAPHICS_SURFACE *parent_target;
	GRAPHICS_SURFACE *original_target;

	GRAPHICS_OBSERVER device_transform_observer;

	GRAPHICS_MATRIX matrix;
	GRAPHICS_MATRIX matrix_inverse;
	GRAPHICS_MATRIX source_matrix_inverse;
	int is_identity;

	GRAPHICS_PATTERN *source;

	struct _GRAPHICS_STATE *next;
} GRAPHICS_STATE;

#ifdef __cplusplus
extern "C" {
#endif

extern eGRAPHICS_STATUS InitializeGraphicsState(GRAPHICS_STATE* state, GRAPHICS_SURFACE* target);
extern void GraphicsStateFinish(GRAPHICS_STATE* state);
extern eGRAPHICS_STATUS GraphicsStateSave(GRAPHICS_STATE** state, GRAPHICS_STATE** free_list);
extern eGRAPHICS_STATUS GraphicsStateRestore(GRAPHICS_STATE** state, GRAPHICS_STATE** free_list);
extern INLINE int GraphicsStateIsGroup(GRAPHICS_STATE* state);
extern INLINE GRAPHICS_SURFACE* GraphicsStateGetTarget(GRAPHICS_STATE* state);
extern INLINE GRAPHICS_CLIP* GraphicsStateGetClip(GRAPHICS_STATE* state);
extern eGRAPHICS_STATUS GraphicsStateClip(GRAPHICS_STATE* state, GRAPHICS_PATH_FIXED* path);
extern GRAPHICS_RECTANGLE_LIST* GraphicsStateCopyClipRectangleList(GRAPHICS_STATE* state);
extern INLINE void GraphicsStateGetMatrix(GRAPHICS_STATE* state, GRAPHICS_MATRIX* matrix);
extern eGRAPHICS_STATUS GraphicsStateSetSource(GRAPHICS_STATE* state, GRAPHICS_PATTERN* source);
extern INLINE GRAPHICS_PATTERN* GraphicsStateGetSource(GRAPHICS_STATE* state);
extern INLINE eGRAPHICS_STATUS GraphicsStateSetTolerance(GRAPHICS_STATE* state, FLOAT_T tolerance);
extern INLINE eGRAPHICS_STATUS GraphicsStateSetOperator(GRAPHICS_STATE* state, eGRAPHICS_OPERATOR op);
extern INLINE eGRAPHICS_STATUS GraphicsStateSetOpacity(GRAPHICS_STATE* state, FLOAT_T opacity);
extern INLINE eGRAPHICS_STATUS GraphicsStateSetAntialias(GRAPHICS_STATE* state, eGRAPHICS_ANTIALIAS antialias);
extern INLINE eGRAPHICS_STATUS GraphicsStateSetFillRule(GRAPHICS_STATE* state, eGRAPHICS_FILL_RULE fill_rule);
extern INLINE eGRAPHICS_STATUS GraphicsStateSetLineWidth(GRAPHICS_STATE* state, FLOAT_T line_width);
extern INLINE eGRAPHICS_STATUS GraphicsStateSetLineCap(GRAPHICS_STATE* state, eGRAPHICS_LINE_CAP line_cap);
extern INLINE eGRAPHICS_STATUS GraphicsStateSetLineJoin(GRAPHICS_STATE* state, eGRAPHICS_LINE_JOIN line_join);
extern INLINE eGRAPHICS_STATUS GraphicsStateSetDash(
	GRAPHICS_STATE* state,
	const GRAPHICS_FLOAT* dash,
	int num_dashes,
	GRAPHICS_FLOAT offset
);
extern INLINE eGRAPHICS_STATUS GraphicsStateSetMiterLimit(GRAPHICS_STATE* state, FLOAT_T limit);
extern INLINE eGRAPHICS_ANTIALIAS GraphicsStateGetAntialias(GRAPHICS_STATE* state);
extern void GraphicsStateGetDash(
	GRAPHICS_STATE* state,
	GRAPHICS_FLOAT* dashes,
	int* num_dashes,
	GRAPHICS_FLOAT* offset
);
extern INLINE eGRAPHICS_FILL_RULE GraphicsStateGetFillRule(GRAPHICS_STATE* state);
extern INLINE FLOAT_T GraphicsStateGetLineWidth(GRAPHICS_STATE* state);
extern INLINE eGRAPHICS_LINE_CAP GraphicsStateGetLineCap(GRAPHICS_STATE* state);
extern INLINE eGRAPHICS_LINE_JOIN GraphicsStateGetLineJoin(GRAPHICS_STATE* state);
extern INLINE FLOAT_T GraphicsStateGetMiterLimit(GRAPHICS_STATE* state);
extern INLINE eGRAPHICS_OPERATOR GraphicsStateGetOperator(GRAPHICS_STATE* state);
extern INLINE FLOAT_T GraphicsStateGetOpacity(GRAPHICS_STATE* state);
extern INLINE FLOAT_T GraphicsStateGetTolerance(GRAPHICS_STATE* state);
extern INLINE eGRAPHICS_STATUS GraphicsStateTranslate(GRAPHICS_STATE* state, FLOAT_T tx, FLOAT_T ty);
extern INLINE eGRAPHICS_STATUS GraphicsStateScale(GRAPHICS_STATE* state, FLOAT_T sx, FLOAT_T sy);
extern INLINE eGRAPHICS_STATUS GraphicsStateRotate(GRAPHICS_STATE* state, FLOAT_T angle);
extern eGRAPHICS_STATUS GraphicsStateTransform(GRAPHICS_STATE* state, const GRAPHICS_MATRIX* matrix);
extern eGRAPHICS_STATUS GraphicsStateSetMatrix(GRAPHICS_STATE* state, const GRAPHICS_MATRIX* matrix);
extern eGRAPHICS_STATUS GraphicsStatePaint(GRAPHICS_STATE* state);
extern eGRAPHICS_STATUS GraphicsStateMask(GRAPHICS_STATE* state, GRAPHICS_PATTERN* mask);
extern INLINE void GraphicsStateIdentityMatrix(GRAPHICS_STATE* state);
extern INLINE void GraphicsStateUserToDevice(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y);
extern INLINE void GraphicsStateUserToDeviceDistance(GRAPHICS_STATE* state, FLOAT_T* dx, FLOAT_T* dy);
extern INLINE void GraphicsStateDeviceToUser(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y);
extern INLINE void GraphicsStateDeviceToUserDistance(GRAPHICS_STATE* state, FLOAT_T* dx, FLOAT_T* dy);
extern INLINE void GraphicsStateBackendToUser(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y);
extern INLINE void GraphicsStateBackendToUserDistance(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y);
extern INLINE void GraphicsStateUserToBackend(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y);
extern INLINE void GraphicsStateUserToBackendDistance(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y);
extern void GraphicsStatePathExtents(
	GRAPHICS_STATE* state,
	GRAPHICS_PATH_FIXED* path,
	FLOAT_T* x1,
	FLOAT_T* y1,
	FLOAT_T* x2,
	FLOAT_T* y2
);
extern void GraphicsStatePathExtents(
	GRAPHICS_STATE* state,
	GRAPHICS_PATH_FIXED* path,
	double* x1, double* y1,
	double* x2, double* y2
);

#ifdef __cplusplus
}
#endif

#endif  // #ifndef _INCLUDED_GRAPHICS_STATE_H_
