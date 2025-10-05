#include "graphics_state.h"
#include "graphics_types.h"
#include "graphics_function.h"
#include "graphics_pattern.h"
#include "graphics_private.h"
#include "graphics_matrix.h"
#include "graphics_region.h"
#include "graphics.h"
#include "../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

static void GraphicsStateCopyPattern(GRAPHICS_PATTERN* pattern, const GRAPHICS_PATTERN* original);

void GraphicsStateFinish(GRAPHICS_STATE* state)
{
	GraphicsStrokeStyleFinish(&state->stroke_style);

	// PENDING cairo_font_face_destroy

	// PENDING cairo_scaled_font_destroy

	GraphicsClipDestroy(state->clip);

	// PENDING cairo_list_del(&gstate->device_transform_observer.link);

	DestroyGraphicsSurface(state->target);
	state->target = NULL;

	DestroyGraphicsSurface(state->parent_target);
	state->parent_target = NULL;

	DestroyGraphicsSurface(state->original_target);
	state->original_target = NULL;

	DestroyGraphicsPattern(state->source);
	state->source = NULL;
}

static eGRAPHICS_STATUS InitializeGraphicsStateCopy(GRAPHICS_STATE* state, GRAPHICS_STATE* other)
{
	eGRAPHICS_STATUS status;

	state->op = other->op;
	state->opacity = other->opacity;

	state->tolerance = other->tolerance;
	state->antialias = other->antialias;

	status = GraphicsStrokeStyleInitializeCopy(&state->stroke_style, &other->stroke_style);
	if(UNLIKELY(status))
	{
		return status;
	}

	state->fill_rule = other->fill_rule;

	// PENDING font_face, scaled_font, previous_scaled_font
		// PENDING font_matrix
		// PENDING cairo_font_options_init_copy

	state->clip = GraphicsClipCopy(NULL, other->clip);

	state->target = GraphicsSurfaceReference(other->target);
	state->parent_target = NULL;
	state->original_target = GraphicsSurfaceReference(other->original_target);

	// PENDING device_transform_observer

	state->is_identity = other->is_identity;
	state->matrix = other->matrix;
	state->matrix_inverse = other->matrix_inverse;
	state->source_matrix_inverse = other->source_matrix_inverse;

	state->source = GraphicsPatternReference(other->source);

	state->next = NULL;

	return GRAPHICS_STATUS_SUCCESS;
}

static void GraphicsStateUpdateDeviceTransform(GRAPHICS_OBSERVER* observer, void* argument)
{
	GRAPHICS_STATE *state = GRAPHICS_CONTAINER_OF(observer, GRAPHICS_STATE, device_transform_observer);
	state->is_identity = (GRAPHICS_MATRIX_IS_IDENTITY(&state->matrix) &&
		GRAPHICS_MATRIX_IS_IDENTITY(&state->target->device_transform));
}

eGRAPHICS_STATUS InitializeGraphicsState(GRAPHICS_STATE* state, GRAPHICS_SURFACE* target)
{
	state->next = NULL;

	state->op = GRAPHICS_STATE_OPERATOR_DEFAULT;
	state->opacity = 1;

	state->tolerance =GRAPHICS_STATE_TOLERANCE_DEFAULT;
	state->antialias = GRAPHICS_ANTIALIAS_DEFAULT;

	GraphicsStrokeStyleInitialize(&state->stroke_style);

	state->fill_rule = GRAPHICS_STATE_FILL_RULE_DEFAULT;

	state->clip = NULL;

	state->target = GraphicsSurfaceReference(target);
	state->parent_target = NULL;
	state->original_target = GraphicsSurfaceReference(target);

	state->device_transform_observer.callback = GraphicsStateUpdateDeviceTransform;
	InitializeGraphicsList(&state->device_transform_observer.link);
	GraphicsListAdd(&state->device_transform_observer.link, &state->target->device_transform_observers);

	state->is_identity = GRAPHICS_MATRIX_IS_IDENTITY(&state->target->device_transform);
	InitializeGraphicsMatrixIdentity(&state->matrix);
	state->matrix_inverse = state->matrix;
	state->source_matrix_inverse = state->matrix;

	state->source = (GRAPHICS_PATTERN*)&(((GRAPHICS*)target->graphics)->clear_pattern.base);

	return target->status;
}

eGRAPHICS_STATUS GraphicsStateSave(GRAPHICS_STATE** state, GRAPHICS_STATE** free_list)
{
	GRAPHICS_STATE *top;
	eGRAPHICS_STATUS status;

	// PENDING CAIRO_INJECT_FAULT

	top = *free_list;
	if(top == NULL)
	{
		top = MEM_ALLOC_FUNC(sizeof(*top));
		if(top == NULL)
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}
	}
	else
	{
		*free_list = top->next;
	}

	status = InitializeGraphicsStateCopy(top, *state);
	if(UNLIKELY(status))
	{
		top->next = *free_list;
		*free_list = top;
		return status;
	}

	top->next = *state;
	*state = top;

	return GRAPHICS_STATUS_SUCCESS;
}

eGRAPHICS_STATUS GraphicsStateRestore(GRAPHICS_STATE** state, GRAPHICS_STATE** free_list)
{
	GRAPHICS_STATE *top;

	top = *state;
	if(top->next == NULL)
	{
		return GRAPHICS_STATUS_INVALID;
	}

	*state = top->next;

	GraphicsStateFinish(top);
	top->next = *free_list;
	*free_list = top;

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE int GraphicsStateIsGroup(GRAPHICS_STATE* state)
{
	return state->parent_target != NULL;
}

INLINE GRAPHICS_SURFACE* GraphicsStateGetTarget(GRAPHICS_STATE* state)
{
	return state->target;
}

INLINE GRAPHICS_CLIP* GraphicsStateGetClip(GRAPHICS_STATE* state)
{
	return state->clip;
}

static eGRAPHICS_OPERATOR ReduceOperator(GRAPHICS_STATE* state)
{
	eGRAPHICS_OPERATOR op;
	const GRAPHICS_PATTERN *pattern;

	op = state->op;
	if(op != GRAPHICS_OPERATOR_SOURCE)
	{
		return op;
	}

	pattern = state->source;
	if(pattern->type == GRAPHICS_PATTERN_TYPE_SOLID)
	{
		const GRAPHICS_SOLID_PATTERN *solid = (const GRAPHICS_SOLID_PATTERN*)pattern;
		{
			if(solid->color.alpha_short <= 0x00FF)
			{
				op = GRAPHICS_OPERATOR_CLEAR;
			}
			else if((state->target->content & GRAPHICS_CONTENT_ALPHA) == 0)
			{
				if((solid->color.red_short | solid->color.green_short | solid->color.blue_short) <= 0x00FF)
				{
					op = GRAPHICS_OPERATOR_CLEAR;
				}
			}
		}
	}
	else if(pattern->type == GRAPHICS_PATTERN_TYPE_SURFACE)
	{
		const GRAPHICS_SURFACE_PATTERN *surface = (const GRAPHICS_SURFACE_PATTERN*)pattern;
		if(surface->surface->is_clear && surface->surface->content & GRAPHICS_CONTENT_ALPHA)
		{
			op = GRAPHICS_OPERATOR_CLEAR;
		}
	}
	else
	{
		const GRAPHICS_GRADIENT_PATTERN *gradient = (const GRAPHICS_GRADIENT_PATTERN*)pattern;
		if(gradient->num_stops == 0)
		{
			op = GRAPHICS_OPERATOR_CLEAR;
		}
	}

	return op;
}

static eGRAPHICS_STATUS GraphicsStateGetPatternStatus(const GRAPHICS_PATTERN* pattern)
{
	if(UNLIKELY(pattern->type == GRAPHICS_PATTERN_TYPE_MESH && ((const GRAPHICS_MESH_PATTERN*)pattern)->current_patch))
	{
		return GRAPHICS_STATUS_INVALID;
	}

	return pattern->status;
}

static void GraphicsStateCopyTransformedPattern(
	GRAPHICS_STATE* state,
	GRAPHICS_PATTERN* pattern,
	const GRAPHICS_PATTERN* original,
	const GRAPHICS_MATRIX* matrix_inverse
)
{
	GraphicsStateCopyPattern(pattern, original);

	if(original->type == GRAPHICS_PATTERN_TYPE_SURFACE)
	{
		GRAPHICS_SURFACE_PATTERN *surface_pattern;
		GRAPHICS_SURFACE *surface;

		surface_pattern = (GRAPHICS_SURFACE_PATTERN*)original;
		surface = surface_pattern->surface;

		if(FALSE == GRAPHICS_MATRIX_IS_IDENTITY(&surface->device_transform))
		{
			GraphicsPatternPretransform(pattern, &surface->device_transform);
		}

		if(FALSE == GRAPHICS_MATRIX_IS_IDENTITY(matrix_inverse))
		{
			GraphicsPatternTransform(pattern, &state->target->device_transform_inverse);
		}
	}
}

static void GraphicsStateCopyTransformedSource(GRAPHICS_STATE* state, GRAPHICS_PATTERN* pattern)
{
	GraphicsStateCopyTransformedPattern(state, pattern, state->source, &state->source_matrix_inverse);
}

static void GraphicsStateCopyTransformedMask(
	GRAPHICS_STATE* state,
	GRAPHICS_PATTERN* pattern,
	GRAPHICS_PATTERN* mask
)
{
	GraphicsStateCopyTransformedPattern(state, pattern, mask, &state->matrix_inverse);
}

eGRAPHICS_STATUS GraphicsStatePaint(GRAPHICS_STATE* state)
{
	GRAPHICS_PATTERN_UNION source_pattern;
	const GRAPHICS_PATTERN *pattern;
	eGRAPHICS_STATUS status;
	eGRAPHICS_OPERATOR op;

	status = GraphicsStateGetPatternStatus(state->source);
	if(UNLIKELY(status))
	{
		return status;
	}

	if(state->op == GRAPHICS_OPERATOR_DEST)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(GraphicsClipIsAllClipped(state->clip))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	op = ReduceOperator(state);
	if(op == GRAPHICS_OPERATOR_CLEAR)
	{
		pattern = &((GRAPHICS*)state->target->graphics)->clear_pattern.base;
		return GraphicsSurfacePaint(state->target, op, pattern, state->clip);
	}
	else
	{
		GraphicsStateCopyTransformedSource(state, &source_pattern.base);
		pattern = &source_pattern.base;
	}

	status = GraphicsSurfacePaint(state->target, op, pattern, state->clip);
	state->source = &((GRAPHICS*)state->target->graphics)->nil_pattern;

	return status;
}

eGRAPHICS_STATUS GraphicsStateMask(GRAPHICS_STATE* state, GRAPHICS_PATTERN* mask)
{
	GRAPHICS_PATTERN_UNION source_pattern, mask_pattern;
	const GRAPHICS_PATTERN *source;
	eGRAPHICS_OPERATOR op;
	eGRAPHICS_STATUS status;

	status = GraphicsStateGetPatternStatus(mask);
	if(UNLIKELY(status))
	{
		return status;
	}

	status = GraphicsStateGetPatternStatus(state->source);
	if(UNLIKELY(status))
	{
		return status;
	}

	if(state->op == GRAPHICS_OPERATOR_DEST)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(GraphicsClipIsAllClipped(state->clip))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(GraphicsPatternIsClear(mask) && GraphicsOperatorBoundedByMask(state->op))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	op = ReduceOperator(state);
	if(op == GRAPHICS_OPERATOR_CLEAR)
	{
		source = &source_pattern.base;
	}
	else
	{
		GraphicsStateCopyTransformedSource(state, &source_pattern.base);
		source = &source_pattern.base;
	}
	GraphicsStateCopyTransformedMask(state, &mask_pattern.base, mask);

	if(source->type == GRAPHICS_PATTERN_TYPE_SOLID
		&& mask_pattern.base.type == GRAPHICS_PATTERN_TYPE_SOLID
		&& GraphicsOperatorBoundedBySource(op)
	)
	{
		const GRAPHICS_SOLID_PATTERN *solid = (GRAPHICS_SOLID_PATTERN*)source;
		GRAPHICS_COLOR combined;
		if(mask_pattern.base.has_component_alpha)
		{
#define M(R, A, B, c) R.c = A.c * B.c
			M(combined, solid->color, mask_pattern.solid.color, red);
			M(combined, solid->color, mask_pattern.solid.color, green);
			M(combined, solid->color, mask_pattern.solid.color, blue);
			M(combined, solid->color, mask_pattern.solid.color, alpha);
#undef M
		}
		else
		{
			combined = solid->color;
			GraphicsColorMultiplyAlpha(&combined, mask_pattern.solid.color.alpha);
		}

		InitializeGraphicsPatternSolid(&source_pattern.solid, &combined, state->target->graphics);

		status = GraphicsSurfacePaint(state->target, op, &source_pattern.base, state->clip);
	}
	else
	{
		status = GraphicsSurfaceMask(state->target, op, source, &mask_pattern.base, state->clip);
	}

	state->source = &((GRAPHICS*)state->target->graphics)->nil_pattern;

	return status;
}

INLINE void GraphicsStateGetMatrix(GRAPHICS_STATE* state, GRAPHICS_MATRIX* matrix)
{
	*matrix = state->matrix;
}

eGRAPHICS_STATUS GraphicsStateSetSource(GRAPHICS_STATE* state, GRAPHICS_PATTERN* source)
{
	if(source->status)
	{
		return source->status;
	}
	source = GraphicsPatternReference(source);
	DestroyGraphicsPattern(state->source);
	state->source = source;
	state->source_matrix_inverse = state->matrix_inverse;

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE GRAPHICS_PATTERN* GraphicsStateGetSource(GRAPHICS_STATE* state)
{
	if(state->source == &((GRAPHICS*)state->original_target->graphics)->pattern_black.base)
	{
		state->source = CreateGraphicsPatternSolid(
			&((GRAPHICS*)state->original_target->graphics)->color_black, state->target->graphics);
	}

	return state->source;
}

INLINE eGRAPHICS_STATUS GraphicsStateSetTolerance(GRAPHICS_STATE* state, FLOAT_T tolerance)
{
	state->tolerance = tolerance;

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE eGRAPHICS_STATUS GraphicsStateSetOperator(GRAPHICS_STATE* state, eGRAPHICS_OPERATOR op)
{
	state->op = op;

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE eGRAPHICS_STATUS GraphicsStateSetOpacity(GRAPHICS_STATE* state, FLOAT_T opacity)
{
	state->opacity = opacity;

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE eGRAPHICS_STATUS GraphicsStateSetAntialias(GRAPHICS_STATE* state, eGRAPHICS_ANTIALIAS antialias)
{
	state->antialias = antialias;

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE eGRAPHICS_STATUS GraphicsStateSetFillRule(GRAPHICS_STATE* state, eGRAPHICS_FILL_RULE fill_rule)
{
	state->fill_rule = fill_rule;

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE eGRAPHICS_STATUS GraphicsStateSetLineWidth(GRAPHICS_STATE* state, FLOAT_T line_width)
{
	state->stroke_style.line_width = line_width;

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE eGRAPHICS_STATUS GraphicsStateSetLineCap(GRAPHICS_STATE* state, eGRAPHICS_LINE_CAP line_cap)
{
	state->stroke_style.line_cap = line_cap;

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE eGRAPHICS_STATUS GraphicsStateSetLineJoin(GRAPHICS_STATE* state, eGRAPHICS_LINE_JOIN line_join)
{
	state->stroke_style.line_join = line_join;

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE eGRAPHICS_STATUS GraphicsStateSetDash(
	GRAPHICS_STATE* state,
	const GRAPHICS_FLOAT* dash,
	int num_dashes,
	GRAPHICS_FLOAT offset
)
{
	FLOAT_T dash_total, on_total, off_total;
	int i, j;

	MEM_FREE_FUNC(state->stroke_style.dash);

	state->stroke_style.num_dashes = num_dashes;

	if(state->stroke_style.num_dashes == 0)
	{
		state->stroke_style.dash = NULL;
		state->stroke_style.dash_offset = 0.0;
		return GRAPHICS_STATUS_SUCCESS;
	}

	state->stroke_style.dash = (GRAPHICS_FLOAT*)MEM_CALLOC_FUNC(num_dashes, sizeof(*dash));
	if(state->stroke_style.dash == NULL)
	{
		state->stroke_style.num_dashes = 0;
		return GRAPHICS_STATUS_NO_MEMORY;
	}

	on_total = off_total = dash_total = 0.0;
	for(i=j=0; i<num_dashes; i++)
	{
		if(dash[i] < 0)
		{
			return GRAPHICS_STATUS_INVALID_DASH;
		}

		if(dash[i] == 0 && i > 0 && i < num_dashes - 1)
		{
			if(dash[++i] < 0)
			{
				return GRAPHICS_STATUS_INVALID_DASH;
			}

			state->stroke_style.dash[j-1] += dash[i];
			state->stroke_style.num_dashes -= 2;
		}
		else
		{
			state->stroke_style.dash[j++] = dash[i];
		}

		if(dash[i] > 0)
		{
			dash_total += dash[i];
			if((i & 1) == 0)
			{
				on_total += dash[i];
			}
			else
			{
				off_total += dash[i];
			}
		}
	}

	if(dash_total == 0.0)
	{
		return GRAPHICS_STATUS_INVALID_DASH;
	}

	if(state->stroke_style.num_dashes & 1)
	{
		dash_total *= 2;
		on_total += off_total;
	}

	if(dash_total - on_total < GRAPHICS_FIXED_ERROR_FLOAT)
	{
		MEM_FREE_FUNC(state->stroke_style.dash);
		state->stroke_style.dash = NULL;
		state->stroke_style.num_dashes = 0;
		state->stroke_style.dash_offset = 0.0;
		return GRAPHICS_STATUS_SUCCESS;
	}

	offset = fmod(offset, dash_total);
	if(offset < 0.0)
	{
		offset += dash_total;
	}
	if(offset <= 0.0)
	{
		offset = 0.0;
	}
	state->stroke_style.dash_offset = offset;

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE eGRAPHICS_STATUS GraphicsStateSetMiterLimit(GRAPHICS_STATE* state, FLOAT_T limit)
{
	state->stroke_style.miter_limit = limit;

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE eGRAPHICS_ANTIALIAS GraphicsStateGetAntialias(GRAPHICS_STATE* state)
{
	return state->antialias;
}

void GraphicsStateGetDash(
	GRAPHICS_STATE* state,
	GRAPHICS_FLOAT* dashes,
	int* num_dashes,
	GRAPHICS_FLOAT* offset
)
{
	if(dashes != NULL)
	{
		(void)memcpy(dashes, state->stroke_style.dash,
			sizeof(*dashes) * state->stroke_style.num_dashes);
	}

	if(num_dashes != NULL)
	{
		*num_dashes = state->stroke_style.num_dashes;
	}

	if(offset != NULL)
	{
		*offset = state->stroke_style.dash_offset;
	}
}

INLINE eGRAPHICS_FILL_RULE GraphicsStateGetFillRule(GRAPHICS_STATE* state)
{
	return state->fill_rule;
}

INLINE FLOAT_T GraphicsStateGetLineWidth(GRAPHICS_STATE* state)
{
	return state->stroke_style.line_width;
}

INLINE eGRAPHICS_LINE_CAP GraphicsStateGetLineCap(GRAPHICS_STATE* state)
{
	return state->stroke_style.line_cap;
}

INLINE eGRAPHICS_LINE_JOIN GraphicsStateGetLineJoin(GRAPHICS_STATE* state)
{
	return state->stroke_style.line_join;
}

INLINE FLOAT_T GraphicsStateGetMiterLimit(GRAPHICS_STATE* state)
{
	return state->stroke_style.miter_limit;
}

INLINE eGRAPHICS_OPERATOR GraphicsStateGetOperator(GRAPHICS_STATE* state)
{
	return state->op;
}

INLINE FLOAT_T GraphicsStateGetOpacity(GRAPHICS_STATE* state)
{
	return state->opacity;
}

INLINE FLOAT_T GraphicsStateGetTolerance(GRAPHICS_STATE* state)
{
	return state->tolerance;
}

INLINE eGRAPHICS_STATUS GraphicsStateTranslate(GRAPHICS_STATE* state, FLOAT_T tx, FLOAT_T ty)
{
	GRAPHICS_MATRIX temp;

	if(FALSE == IS_FINITE(tx) || FALSE == IS_FINITE(ty))
	{
		return GRAPHICS_STATUS_INVALID_MATRIX;
	}

	// PENDING _cairo_gstate_unset_scaled_font(gstate);

	InitializeGraphicsMatrixTranslate(&temp, tx, ty);
	GraphicsMatrixMultiply(&state->matrix, &temp, &state->matrix);
	state->is_identity = FALSE;

	if(FALSE == GraphicsMatrixIsInvertible(&state->matrix))
	{
		return GRAPHICS_STATUS_INVALID_MATRIX;
	}

	InitializeGraphicsMatrixTranslate(&temp, -tx, -ty);
	GraphicsMatrixMultiply(&state->matrix_inverse, &state->matrix_inverse, &temp);

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE eGRAPHICS_STATUS GraphicsStateScale(GRAPHICS_STATE* state, FLOAT_T sx, FLOAT_T sy)
{
	GRAPHICS_MATRIX temp;

	if(sx * sy == 0.0)
	{
		return GRAPHICS_STATUS_INVALID_MATRIX;
	}
	if(FALSE == IS_FINITE(sx) || FALSE == IS_FINITE(sy))
	{
		return GRAPHICS_STATUS_INVALID_MATRIX;
	}

	// PENDING _cairo_gstate_unset_scaled_font(gstate);
	InitializeGraphicsMatrixScale(&temp, sx, sy);
	GraphicsMatrixMultiply(&state->matrix, &temp, &state->matrix);
	state->is_identity = FALSE;

	if(FALSE == GraphicsMatrixIsInvertible(&state->matrix))
	{
		return GRAPHICS_STATUS_INVALID_MATRIX;
	}

	InitializeGraphicsMatrixScale(&temp, 1/sx, 1/sy);
	GraphicsMatrixMultiply(&state->matrix_inverse, &state->matrix_inverse, &temp);

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE eGRAPHICS_STATUS GraphicsStateRotate(GRAPHICS_STATE* state, FLOAT_T angle)
{
	GRAPHICS_MATRIX temp;

	if(angle == 0.0)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(FALSE == IS_FINITE(angle))
	{
		return GRAPHICS_STATUS_INVALID_MATRIX;
	}

	// PENDING _cairo_gstate_unset_scaled_font(gstate);

	InitializeGraphicsMatrixRotate(&temp, angle);
	GraphicsMatrixMultiply(&state->matrix, &temp, &state->matrix);
	state->is_identity = FALSE;

	if(FALSE == GraphicsMatrixIsInvertible(&state->matrix))
	{
		return GRAPHICS_STATUS_INVALID_MATRIX;
	}

	InitializeGraphicsMatrixRotate(&temp, -angle);
	GraphicsMatrixMultiply(&state->matrix_inverse, &state->matrix_inverse, &temp);

	return GRAPHICS_STATUS_SUCCESS;
}

eGRAPHICS_STATUS GraphicsStateTransform(GRAPHICS_STATE* state, const GRAPHICS_MATRIX* matrix)
{
	GRAPHICS_MATRIX temp;
	eGRAPHICS_STATUS status;

	if(FALSE == GraphicsMatrixIsInvertible(matrix))
	{
		return GRAPHICS_STATUS_INVALID_MATRIX;
	}

	if(GRAPHICS_MATRIX_IS_IDENTITY(matrix))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	temp = *matrix;
	status = GraphicsMatrixInvert(&temp);
	if(UNLIKELY(status))
	{
		return status;
	}

	// PENDIGN _cairo_gstate_unset_scaled_font(gstate);

	GraphicsMatrixMultiply(&state->matrix, matrix, &state->matrix);
	GraphicsMatrixMultiply(&state->matrix_inverse, &state->matrix_inverse, &temp);
	state->is_identity = FALSE;

	if(FALSE == GraphicsMatrixIsInvertible(&state->matrix))
	{
		return GRAPHICS_STATUS_INVALID_MATRIX;
	}

	return GRAPHICS_STATUS_SUCCESS;
}

eGRAPHICS_STATUS GraphicsStateSetMatrix(GRAPHICS_STATE* state, const GRAPHICS_MATRIX* matrix)
{
	eGRAPHICS_STATUS status;

	if(memcmp(matrix, &state->matrix, sizeof(*matrix)) == 0)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(FALSE == GraphicsMatrixIsInvertible(matrix))
	{
		return GRAPHICS_STATUS_INVALID_MATRIX;
	}

	if(GRAPHICS_MATRIX_IS_IDENTITY(matrix))
	{
		GraphicsStateIdentityMatrix(state);
		return GRAPHICS_STATUS_SUCCESS;
	}

	// PENDIGN _cairo_gstate_unset_scaled_font(gstate);

	state->matrix = *matrix;
	state->matrix_inverse = *matrix;
	status = GraphicsMatrixInvert(&state->matrix_inverse);
	ASSERT(status == GRAPHICS_STATUS_SUCCESS);
	state->is_identity = FALSE;

	return GRAPHICS_STATUS_SUCCESS;
}

INLINE void GraphicsStateIdentityMatrix(GRAPHICS_STATE* state)
{
	if(GRAPHICS_MATRIX_IS_IDENTITY(&state->matrix))
	{
		return;
	}

	// PENDIGN _cairo_gstate_unset_scaled_font(gstate);

	InitializeGraphicsMatrixIdentity(&state->matrix);
	InitializeGraphicsMatrixIdentity(&state->matrix_inverse);
	state->is_identity = GRAPHICS_MATRIX_IS_IDENTITY(&state->target->device_transform);
}

INLINE void GraphicsStateUserToDevice(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y)
{
	GraphicsMatrixTransformPoint(&state->matrix, x, y);
}

INLINE void GraphicsStateUserToDeviceDistance(GRAPHICS_STATE* state, FLOAT_T* dx, FLOAT_T* dy)
{
	GraphicsMatrixTransformDistance(&state->matrix, dx, dy);
}

INLINE void GraphicsStateDeviceToUser(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y)
{
	GraphicsMatrixTransformPoint(&state->matrix, x, y);
}

INLINE void GraphicsStateDeviceToUserDistance(GRAPHICS_STATE* state, FLOAT_T* dx, FLOAT_T* dy)
{
	GraphicsMatrixTransformDistance(&state->matrix_inverse, dx, dy);
}

INLINE void DoGraphicsStateBackendToUser(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y)
{
	GraphicsMatrixTransformPoint(&state->target->device_transform, x, y);
	GraphicsMatrixTransformPoint(&state->matrix_inverse, x, y);
}

INLINE void GraphicsStateBackendToUser(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y)
{
	if(FALSE == state->is_identity)
	{
		DoGraphicsStateBackendToUser(state, x, y);
	}
}

INLINE void DoGraphicsStateBackendToUserDistance(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y)
{
	GraphicsMatrixTransformDistance(&state->target->device_transform_inverse, x, y);
	GraphicsMatrixTransformDistance(&state->matrix_inverse, x, y);
}

INLINE void GraphicsStateBackendToUserDistance(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y)
{
	if(FALSE == state->is_identity)
	{
		DoGraphicsStateBackendToUserDistance(state, x, y);
	}
}

INLINE void DoGraphicsStateUserToBackend(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y)
{
	GraphicsMatrixTransformPoint(&state->matrix, x, y);
	GraphicsMatrixTransformPoint(&state->target->device_transform, x, y);
}

INLINE void GraphicsStateUserToBackend(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y)
{
	if(FALSE == state->is_identity)
	{
		DoGraphicsStateUserToBackend(state, x, y);
	}
}

INLINE void DoGraphicsStateUserToBackendDistance(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y)
{
	GraphicsMatrixTransformDistance(&state->matrix, x, y);
	GraphicsMatrixTransformDistance(&state->target->device_transform, x, y);
}

INLINE void GraphicsStateUserToBackendDistance(GRAPHICS_STATE* state, FLOAT_T* x, FLOAT_T* y)
{
	if(FALSE == state->is_identity)
	{
		DoGraphicsStateUserToBackendDistance(state, x, y);
	}
}

void GraphicsStateBackendToUserRectanlge(
	GRAPHICS_STATE* state,
	FLOAT_T* x1,
	FLOAT_T* y1,
	FLOAT_T* x2,
	FLOAT_T* y2,
	int* is_tight
)
{
	GRAPHICS_MATRIX matrix_inverse;

	if(FALSE == GRAPHICS_MATRIX_IS_IDENTITY(&state->target->device_transform_inverse)
			|| FALSE == GRAPHICS_MATRIX_IS_IDENTITY(&state->matrix_inverse))
	{
		GraphicsMatrixMultiply(&matrix_inverse, &state->target->device_transform_inverse,
			&state->matrix_inverse);
		GraphicsMatrixTransformBoundingBox(&matrix_inverse,
			x1, y1, x2, y2, is_tight);
	}
	else
	{
		if(is_tight != NULL)
		{
			*is_tight = TRUE;
		}
	}
}

void GraphicsStatePathExtents(
	GRAPHICS_STATE* state,
	GRAPHICS_PATH_FIXED* path,
	FLOAT_T* x1,
	FLOAT_T* y1,
	FLOAT_T* x2,
	FLOAT_T* y2
)
{
	GRAPHICS_BOX box;
	FLOAT_T point_x1, point_y1, point_x2, point_y2;

	if(GraphicsPathFixedExtents(path, &box))
	{
		point_x1 = GRAPHICS_FIXED_TO_FLOAT(box.point1.x);
		point_y1 = GRAPHICS_FIXED_TO_FLOAT(box.point1.y);
		point_x2 = GRAPHICS_FIXED_TO_FLOAT(box.point2.x);
		point_y2 = GRAPHICS_FIXED_TO_FLOAT(box.point2.y);

		GraphicsStateBackendToUserRectanlge(state,
			&point_x1, &point_y1, &point_x2, &point_y2, NULL);
	}
	else
	{
		point_x1 = 0.0;
		point_y1 = 0.0;
		point_x2 = 0.0;
		point_y2 = 0.0;
	}

	// if(x1 != NULL)
	{
		*x1 = point_x1;
	}
	// if(y1 != NULL)
	{
		*y1 = point_y1;
	}
	// if(x2 != NULL)
	{
		*x2 = point_x2;
	}
	// if(y2 != NULL)
	{
		*y2 = point_y2;
	}
}

static void GraphicsStateCopyPattern(GRAPHICS_PATTERN* pattern, const GRAPHICS_PATTERN* original)
{
	GRAPHICS *graphics = (GRAPHICS*)original->graphics;

	if(GraphicsPatternIsClear(original))
	{
		InitializeGraphicsPatternSolid((GRAPHICS_SOLID_PATTERN*)pattern,
			&graphics->color_transparent, graphics);
	}

	if(original->type == GRAPHICS_PATTERN_TYPE_LINEAR
		|| original->type == GRAPHICS_PATTERN_TYPE_RADIAL)
	{
		GRAPHICS_COLOR color;

		if(GraphicsGradientPatternIsSolid((GRAPHICS_GRADIENT_PATTERN*)original, NULL, &color))
		{
			InitializeGraphicsPatternSolid((GRAPHICS_SOLID_PATTERN*)pattern, &color, graphics);
			return;
		}
	}

	InitializeGraphicsPatternStaticCopy(pattern, original);
}

eGRAPHICS_STATUS GraphicsStateStroke(GRAPHICS_STATE* state, GRAPHICS_PATH_FIXED* path)
{
	GRAPHICS_PATTERN_UNION source_pattern;
	GRAPHICS_STROKE_STYLE style;
	FLOAT_T dash[2];
	eGRAPHICS_STATUS status;
	GRAPHICS_MATRIX aggregate_transform;
	GRAPHICS_MATRIX aggregate_transform_inverse;
	
	status = GraphicsStateGetPatternStatus(state->source);
	if(UNLIKELY(status))
	{
		return status;
	}
	
	if(state->op == GRAPHICS_OPERATOR_DEST)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}
	
	if(state->stroke_style.line_width <= 0.0)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}
	
	if(GraphicsClipIsAllClipped(state->clip))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}
	
	GraphicsMatrixMultiply(&aggregate_transform, &state->matrix, &state->target->device_transform);
	GraphicsMatrixMultiply(&aggregate_transform_inverse, &state->target->device_transform_inverse,
		&state->matrix_inverse);
	
	(void)memcpy(&style, &state->stroke_style, sizeof(style));
	if(GraphicsStrokeStyleDashCanApproximate(&state->stroke_style, &aggregate_transform, state->tolerance))
	{
		style.dash = dash;
		GraphicsStrokeStyleDashApproximate(&state->stroke_style, &state->matrix, state->tolerance,
			&style.dash_offset, style.dash, &style.num_dashes);
	}
	
	GraphicsStateCopyTransformedSource(state, &source_pattern.base);
	
	return GraphicsSurfaceStroke(state->target, state->op, &source_pattern.base, path, &style,
		&aggregate_transform, &aggregate_transform_inverse, state->tolerance, state->antialias, state->clip);
}

eGRAPHICS_STATUS GraphicsStateInStroke(
	GRAPHICS_STATE* state,
	GRAPHICS_PATH_FIXED* path,
	FLOAT_T x,
	FLOAT_T y,
	int* inside_return
)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_RECTANGLE_INT extents;
	GRAPHICS_BOX limit;
	GRAPHICS_TRAPS traps;

	if(state->stroke_style.line_width <= 0.0)
	{
		*inside_return = FALSE;
		return GRAPHICS_STATUS_SUCCESS;
	}

	GraphicsStateUserToBackend(state, &x, &y);

	GraphicsPathFixedApproximateStrokeExtents(path, &state->stroke_style,
		&state->matrix, state->target->is_vector, &extents);
	if(x < extents.x || x > extents.x + extents.width
		|| y < extents.y || y > extents.y + extents.height)
	{
		*inside_return = FALSE;
		return GRAPHICS_STATUS_SUCCESS;
	}

	limit.point1.x = GraphicsFixedFromFloat(x) - 1;
	limit.point1.y = GraphicsFixedFromFloat(y) - 1;
	limit.point2.x = limit.point1.x + 2;
	limit.point2.y = limit.point1.y + 2;

	InitializeGraphicsTraps(&traps);
	GraphicsTrapsLimit(&traps, &limit, 1);

	status = GraphicsPathFixedStrokePolygonToTraps(path, &state->stroke_style,
				&state->matrix, &state->matrix_inverse, state->tolerance, &traps);

	if(UNLIKELY(status))
	{
		goto BAIL;
	}

	*inside_return = GraphicsTrapsContain(&traps, x, y);

BAIL:
	GraphicsTrapsFinish(&traps);

	return status;
}

eGRAPHICS_STATUS GraphicsStateFill(GRAPHICS_STATE* state, GRAPHICS_PATH_FIXED* path)
{
	eGRAPHICS_STATUS status;

	status = GraphicsStateGetPatternStatus(state->source);
	if(UNLIKELY(status))
	{
		return status;
	}

	if(state->op == GRAPHICS_OPERATOR_DEST)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(GraphicsClipIsAllClipped(state->clip))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	ASSERT(state->opacity == 1.0);

	if(path->fill_is_empty)
	{
		if(GraphicsOperatorBoundedByMask(state->op))
		{
			return GRAPHICS_STATUS_SUCCESS;
		}
		status = GraphicsSurfacePaint(state->target, GRAPHICS_OPERATOR_CLEAR,
					&((GRAPHICS*)state->target->graphics)->clear_pattern.base, state->clip);
	}
	else
	{
		GRAPHICS_PATTERN_UNION source_pattern;
		const GRAPHICS_PATTERN *pattern;
		eGRAPHICS_OPERATOR op;
		GRAPHICS_RECTANGLE_INT extents;
		GRAPHICS_BOX box;

		op = ReduceOperator(state);
		if(op == GRAPHICS_OPERATOR_CLEAR)
		{
			pattern = &((GRAPHICS*)state->target->graphics)->clear_pattern.base;
		}
		else
		{
			GraphicsStateCopyTransformedSource(state, &source_pattern.base);
			pattern = &source_pattern.base;
		}

		if(GraphicsSurfaceGetExtents(state->target, &extents)
				&& GraphicsPathFixedIsBox(path, &box)
				&& box.point1.x <= GraphicsFixedFromInteger(extents.x)
				&& box.point1.y <= GraphicsFixedFromInteger(extents.y)
				&& box.point2.x >= GraphicsFixedFromInteger(extents.x + extents.width)
				&& box.point2.y >= GraphicsFixedFromInteger(extents.y + extents.height)
		)
		{
			status = GraphicsSurfacePaint(state->target, op, pattern, state->clip);
		}
		else
		{
			status = GraphicsSurfaceFill(state->target, op, pattern,
						path, state->fill_rule, state->tolerance, state->antialias, state->clip);
		}
	}

	return status;
}

int GraphicsStateInFill(GRAPHICS_STATE* state, GRAPHICS_PATH_FIXED* path, FLOAT_T x, FLOAT_T y)
{
	GraphicsStateUserToBackend(state, &x, &y);

	return GraphicsPathFixedInFill(path,
			state->fill_rule, state->tolerance, x, y);
}

int GraphicsStateInClip(GRAPHICS_STATE* state, FLOAT_T x, FLOAT_T y)
{
	GRAPHICS_CLIP *clip = state->clip;
	int i;

	if(clip == NULL)
	{
		return TRUE;
	}

	if(clip->clip_all)
	{
		return FALSE;
	}

	GraphicsStateUserToBackend(state, &x, &y);

	if(x < clip->extents.x
		|| x >= clip->extents.x + clip->extents.width
		|| y < clip->extents.y
		|| y >= clip->extents.y + clip->extents.height
	)
	{
		return FALSE;
	}

	if(clip->num_boxes != 0)
	{
		int fx, fy;

		fx = GraphicsFixedFromFloat(x);
		fy = GraphicsFixedFromFloat(y);
		for(i=0; i<clip->num_boxes; i++)
		{
			if(fx >= clip->boxes[i].point1.x && fx <= clip->boxes[i].point2.x
					&& fy >= clip->boxes[i].point1.y && fy <= clip->boxes[i].point2.y)
			{
				break;
			}
		}
		if(i == clip->num_boxes)
		{
			return FALSE;
		}
	}

	if(clip->path != NULL)
	{
		GRAPHICS_CLIP_PATH *clip_path = clip->path;
		do
		{
			if(FALSE == GraphicsPathFixedInFill(&clip_path->path,
							clip_path->fill_rule, clip_path->tolerance, x, y))
			{
				return FALSE;
			}
		} while((clip_path = clip_path->prev) != NULL);
	}

	return TRUE;
}

static int GraphicsStateIntegerClipExtents(GRAPHICS_STATE* state, GRAPHICS_RECTANGLE_INT* extents)
{
	int is_bounded;

	is_bounded = GraphicsSurfaceGetExtents(state->target, extents);

	if(state->clip != NULL)
	{
		*extents = state->clip->extents;
		is_bounded = TRUE;
	}

	return is_bounded;
}

static void GraphicsStateExtentsToUserRectangle(
	GRAPHICS_STATE* state,
	const GRAPHICS_BOX* extents,
	FLOAT_T *x1, FLOAT_T* y1,
	FLOAT_T *x2, FLOAT_T* y2
)
{
	FLOAT_T px1, py1, px2, py2;
	int temp;

	px1 = GRAPHICS_FIXED_TO_FLOAT(extents->point1.x);
	py1 = GRAPHICS_FIXED_TO_FLOAT(extents->point1.y);
	px2 = GRAPHICS_FIXED_TO_FLOAT(extents->point2.x);
	py2 = GRAPHICS_FIXED_TO_FLOAT(extents->point2.y);

	GraphicsStateBackendToUserRectanlge(state,
		&px1, &py1, &px2, &py2, &temp);

	*x1 = px1;
	*y1 = py1;
	*x2 = px2;
	*y2 = py2;
}

eGRAPHICS_STATUS GraphicsStateStrokeExtents(
	GRAPHICS_STATE* state,
	GRAPHICS_PATH_FIXED* path,
	FLOAT_T* x1, FLOAT_T* y1,
	FLOAT_T* x2, FLOAT_T* y2
)
{
	eGRAPHICS_INTEGER_STATUS status;
	GRAPHICS_BOX extents;
	int empty;

	*x1 = 0.0;
	*y1 = 0.0;
	*x2 = 0.0;
	*y2 = 0.0;

	if(state->stroke_style.line_width <= 0.0)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	status = GRAPHICS_INTEGER_STATUS_UNSUPPORTED;
	if(path->stroke_is_rectilinear)
	{
		GRAPHICS_BOXES boxes;

		InitializeGraphicsBoxes(&boxes);
		status = GraphicsPathFixedStrokeRectilinearToBoxes(path,
					&state->stroke_style, &state->matrix, state->antialias, &boxes);
		empty = boxes.num_boxes == 0;
		if(FALSE == empty)
		{
			GraphicsBoxesExtents(&boxes, &extents);
		}
		GraphicsBoxesFinish(&boxes);
	}

	if(status == GRAPHICS_INTEGER_STATUS_UNSUPPORTED)
	{
		GRAPHICS_POLYGON polygon;

		InitializeGraphicsPolygon(&polygon, NULL, 0);
		status = GraphicsPathFixedStrokeToPolygon(path, &state->stroke_style,
					&state->matrix, &state->matrix_inverse, state->tolerance, &polygon);
		empty = polygon.num_edges == 0;
		if(FALSE == empty)
		{
			extents = polygon.extents;
		}
		GraphicsPolygonFinish(&polygon);
	}
	if(FALSE == empty)
	{
		GraphicsStateExtentsToUserRectangle(state, &extents,
			x1, y1, x2, y2);
	}

	return status;
}

eGRAPHICS_STATUS GraphicsStateFillExtents(
	GRAPHICS_STATE* state,
	GRAPHICS_PATH_FIXED* path,
	FLOAT_T* x1, FLOAT_T* y1,
	FLOAT_T* x2, FLOAT_T* y2
)
{
	eGRAPHICS_STATUS status;
	GRAPHICS_BOX extents;
	int empty;

	*x1 = 0.0;
	*y1 = 0.0;
	*x2 = 0.0;
	*y2 = 0.0;

	if(path->fill_is_empty)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(path->fill_is_rectilinear)
	{
		GRAPHICS_BOXES boxes;

		InitializeGraphicsBoxes(&boxes);
		status = GraphicsPathFixedFillRectilinearToBoxes(path,
					state->fill_rule, state->antialias, &boxes);
		empty = boxes.num_boxes == 0;
		if(FALSE == empty)
		{
			GraphicsBoxesExtents(&boxes, &extents);
		}
		GraphicsBoxesFinish(&boxes);
	}
	else
	{
		GRAPHICS_TRAPS traps;

		InitializeGraphicsTraps(&traps);

		status = GraphicsPathFixedFillToTraps(path,
					state->fill_rule, state->tolerance, &traps);
		empty = traps.num_traps == 0;
		if(FALSE == empty)
		{
			GraphicsTrapsExtents(&traps, &extents);
		}

		GraphicsTrapsFinish(&traps);
	}

	if(FALSE == empty)
	{
		GraphicsStateExtentsToUserRectangle(state, &extents, x1, y1, x2, y2);
	}

	return status;
}

eGRAPHICS_STATUS GraphicsStateResetClip(GRAPHICS_STATE* state)
{
	GraphicsClipDestroy(state->clip);
	state->clip = NULL;

	return GRAPHICS_STATUS_SUCCESS;
}

static int GraphcisStateIntegerClipExtents(
	GRAPHICS_STATE* state,
	GRAPHICS_RECTANGLE_INT* extents
)
{
	int is_bounded;

	is_bounded = GraphicsSurfaceGetExtents(state->target, extents);

	if(state->clip != NULL)
	{
		GraphicsRectangleIntersect(extents, &state->clip->extents);
		is_bounded = TRUE;
	}

	return is_bounded;
}

int GraphicsStateClipExtents(
	GRAPHICS_STATE* state,
	FLOAT_T* x1,
	FLOAT_T* y1,
	FLOAT_T* x2,
	FLOAT_T* y2
)
{
	GRAPHICS_RECTANGLE_INT extents;
	FLOAT_T point_x1, point_y1, point_x2, point_y2;

	if(FALSE == GraphicsStateIntegerClipExtents(state, &extents))
	{
		return FALSE;
	}

	point_x1 = extents.x;
	point_y1 = extents.y;
	point_x2 = extents.x + (int)extents.width;
	point_y2 = extents.y + (int)extents.height;

	GraphicsStateBackendToUserRectanlge(state,
		&point_x1, &point_y1, &point_x2, &point_y2, NULL);

	*x1 = point_x1;
	*y1 = point_y1;
	*x2 = point_x2;
	*y2 = point_y2;

	return TRUE;
}

eGRAPHICS_STATUS GraphicsStateClip(GRAPHICS_STATE* state, GRAPHICS_PATH_FIXED* path)
{
	state->clip = GraphicsClipIntersectPath(state->clip, path, state->fill_rule,
		state->tolerance, state->antialias);
	return GRAPHICS_STATUS_SUCCESS;
}

GRAPHICS_RECTANGLE_LIST* GraphicsStateCopyClipRectangleList(GRAPHICS_STATE* state)
{
	GRAPHICS_RECTANGLE_INT extents;
	GRAPHICS_RECTANGLE_LIST *list;
	GRAPHICS_CLIP *clip;

	if(GraphicsSurfaceGetExtents(state->target, &extents))
	{
		clip = GraphicsClipCopyIntersectRectangle(state->clip, &extents);
	}
	else
	{
		clip = state->clip;
	}

	list = GraphicsClipCopyIntersectRectangle(clip, state);

	if(clip != state->clip)
	{
		GraphicsClipDestroy(clip);
	}

	return list;
}

#ifdef __cplusplus
}
#endif
