#ifndef _INCLUDED_GRAPHICS_COMPOSITOR_H_
#define _INCLUDED_GRAPHICS_COMPOSITOR_H_

#include "graphics_types.h"
#include "graphics_pattern.h"
#include "graphics_clip.h"

typedef enum _eGRAPHICS_COMPOSITOR_FLAGS
{
	GRAPHICS_COMPOSITOR_FLAGS_INITIALIZED = 0x01,
	GRAPHICS_SPANS_COMPOSITOR_HAS_LERP = 0x02
} eGRAPHICS_COMPOSITOR_FLAGS;

#define IS_GRAPHICS_COMPOSITOR_INITIALIZED(compositor) \
	((((GRAPHICS_COMPOSITOR*)(compositor))->flags & GRAPHICS_COMPOSITOR_FLAGS_INITIALIZED) == 0x01)

#define GRAPHICS_COMPOSITOR_INITIALIZED_FLAG_ON(compositor) \
	(((GRAPHICS_COMPOSITOR*)(compositor))->flags |= GRAPHICS_COMPOSITOR_FLAGS_INITIALIZED)

struct _GRAPHICS_COMPOSITE_RECTANGLES
{
	struct _GRAPHICS_SURFACE *surface;
	eGRAPHICS_OPERATOR op;

	GRAPHICS_RECTANGLE_INT source;
	GRAPHICS_RECTANGLE_INT mask;
	GRAPHICS_RECTANGLE_INT destination;

	GRAPHICS_RECTANGLE_INT bounded;
	GRAPHICS_RECTANGLE_INT unbounded;

	unsigned int is_bounded;

	GRAPHICS_RECTANGLE_INT source_sample_area;
	GRAPHICS_RECTANGLE_INT mask_sample_area;

	GRAPHICS_PATTERN_UNION source_pattern;
	GRAPHICS_PATTERN_UNION mask_pattern;
	const GRAPHICS_PATTERN *original_source_pattern;
	const GRAPHICS_PATTERN *original_mask_pattern;

	GRAPHICS_CLIP *clip;
};

typedef struct _GRAPHICS_COMPOSITOR
{
	const struct _GRAPHICS_COMPOSITOR *delegate;
	eGRAPHICS_INTEGER_STATUS (*paint)(
		const struct _GRAPHICS_COMPOSITOR* compositor,
				GRAPHICS_COMPOSITE_RECTANGLES* extents);
	eGRAPHICS_INTEGER_STATUS (*mask)(
		const struct _GRAPHICS_COMPOSITOR* compositor,
				GRAPHICS_COMPOSITE_RECTANGLES* extents);
	eGRAPHICS_INTEGER_STATUS (*stroke)(
		const struct _GRAPHICS_COMPOSITOR* compositor,
		const GRAPHICS_COMPOSITE_RECTANGLES* extents,
		const struct _GRAPHICS_PATH_FIXED* path,
		const struct _GRAPHICS_STROKE_STYLE* style,
		const GRAPHICS_MATRIX* matrix,
		const GRAPHICS_MATRIX* matrix_inverse,
		GRAPHICS_FLOAT tolerance,
		eGRAPHICS_ANTIALIAS antialias
	   );
	eGRAPHICS_INTEGER_STATUS (*fill)(
		const struct _GRAPHICS_COMPOSITOR* compositor,
		GRAPHICS_COMPOSITE_RECTANGLES* extents,
		const struct _GRAPHICS_PATH_FIXED* path,
		eGRAPHICS_FILL_RULE fill_rule,
		GRAPHICS_FLOAT tolerance,
		eGRAPHICS_ANTIALIAS antialias
	   );
	unsigned int flags;
	//eGRAPHICS_INTEGER_STATUS (*glyphs)(
	//	const struct _GRAPHICS_COMPOSITOR* compositor,
	//	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	//	aaa
	void *graphics;
} GRAPHICS_COMPOSITOR;

typedef struct _GRAPHICS_MASK_COMPOSITOR
{
	GRAPHICS_COMPOSITOR base;

	eGRAPHICS_INTEGER_STATUS (*acquire)(void* surface);
	eGRAPHICS_INTEGER_STATUS (*release)(void* surface);

	eGRAPHICS_INTEGER_STATUS (*set_clip_region)(void* surface,
						struct _GRAPHICS_REGION* clip_region);
	struct _GRAPHICS_SURFACE* (*pattern_to_surface)(
		struct _GRAPHICS_SURFACE* dst,
		const struct _GRAPHICS_PATTERN* pattern,
		int is_mask,
		const struct _GRAPHICS_RECTANGLE_INT* extents,
		const struct _GRAPHICS_RECTANGLE_INT* sample,
		int* src_x,
		int* src_y
	   );
	eGRAPHICS_INTEGER_STATUS (*draw_image_boxes)(
		void* surface,
		struct _GRAPHICS_IMAGE_SURFAE* image,
		struct _GRAPHICS_BOXES* boxes,
		int dx,
		int dy
	   );
	eGRAPHICS_INTEGER_STATUS (*copy_boxes)(
		void* surface,
		struct _GRAPHICS_SURFAE* src,
		struct _GRAPHICS_BOXES* boxes,
		struct _GRAPHICS_RECTANGLE_INT* extents,
		int dx,
		int dy
	   );

	eGRAPHICS_INTEGER_STATUS (*fill_rectangles)(
		void* surface,
		eGRAPHICS_OPERATOR op,
		const struct _GRAPHICS_COLOR* color,
		struct _GRAPHICS_RECTANGLE_INT* rectangles,
		int num_rectangles
	   );
	eGRAPHICS_INTEGER_STATUS (*fill_boxes)(
		void* surface,
		eGRAPHICS_OPERATOR op,
		const struct _GRAPHICS_COLOR* color,
		struct _GRAPHICS_BOXES* boxes
	   );
	eGRAPHICS_INTEGER_STATUS
		(*check_composite)(const GRAPHICS_COMPOSITE_RECTANGLES* extents);
	eGRAPHICS_INTEGER_STATUS (*composite)(
		void* dst,
		eGRAPHICS_OPERATOR op,
		struct _GRAPHICS_SURFACE* src,
		struct _GRAPHICS_SURFACE* mask,
		int src_x,
		int src_y,
		int mask_x,
		int mask_y,
		int dst_x,
		int dst_y,
		unsigned int width,
		unsigned int height
	   );
	eGRAPHICS_INTEGER_STATUS (*composite_boxes)(
		void* surface,
		eGRAPHICS_OPERATOR op,
		struct _GRAPHICS_SURFACE* source,
		struct _GRAPHICS_SURFACE* mask,
		int src_x,
		int src_y,
		int mask_x,
		int mask_y,
		int dst_x,
		int dst_y,
		struct _GRAPHICS_BOXES* boxes,
		const struct _GRAPHICS_RECTANGLE_INT* extents
	   );
	//eGRAPHICS_INTEGER_STATUS (*check_composite_glyphs)(
	//	const GRAPHICS_COMPOSITE_RECTANGLES* extents,
	//	)
	//eGRAPHICS_INTEGER_STATUS (*composite_glyphs)(
	//		)
} GRAPHICS_MASK_COMPOSITOR;

typedef struct _GRAPHICS_TRAPS_COMPOSITOR
{
	GRAPHICS_COMPOSITOR base;

	eGRAPHICS_INTEGER_STATUS (*acquire)(void* surface);
	eGRAPHICS_INTEGER_STATUS (*release)(void* surface);
	eGRAPHICS_INTEGER_STATUS (*set_clip_region)(void* surface, struct _GRAPHICS_REGION* clip_region);

	struct _GRAPHICS_SURFAE* (*pattern_to_surface)(
		struct _GRAPHICS_SURFAE* dst,
		const struct _GRAPHICS_PATTERN* pattern,
		int is_mask,
		const struct _GRAPHICS_RECTANGLE_INT* extents,
		const struct _GRAPHICS_RECTANGLE_INT* sample,
		int* src_x,
		int* src_y
	   );
	eGRAPHICS_INTEGER_STATUS (*draw_image_boxes)(
		void* surface,
		struct _GRAPHICS_IMAGE_SURFAE* image,
		struct _GRAPHICS_BOXES* boxes,
		const struct _GRAPHICS_RECTANGLE_INT* extents,
		int dx,
		int dy
	   );
	eGRAPHICS_INTEGER_STATUS (*copy_boxes)(
		void* surface,
		struct _GRAPHICS_SURFAE* src,
		struct _GRAPHICS_BOXES* boxes,
		const struct _GRAPHICS_RECTANGLE_INT* extents,
		int dx,
		int dy
	   );
	eGRAPHICS_INTEGER_STATUS (*fill_boxes)(
		void* surface,
		eGRAPHICS_OPERATOR op,
		const struct _GRAPHICS_COLOR* color,
		struct _GRAPHICS_BOXES* boxes
	   );
	eGRAPHICS_INTEGER_STATUS (*check_composite)(const GRAPHICS_COMPOSITE_RECTANGLES* extents);
	eGRAPHICS_INTEGER_STATUS (*composite)(
		void* dst,
		eGRAPHICS_OPERATOR op,
		struct _GRAPHICS_SURFACE* src,
		struct _GRAPHICS_SURFACE* mask,
		int src_x,
		int src_y,
		int mask_x,
		int mask_y,
		int dst_x,
		int dst_y,
		unsigned int width,
		unsigned int height
	   );
	eGRAPHICS_INTEGER_STATUS (*lerp)(
		void* dst,
		struct _GRAPHICS_SURFACE* abstract_src,
		struct _GRAPHICS_SURFACE* abstract_mask,
		int src_x,
		int src_y,
		int mask_x,
		int mask_y,
		int dst_x,
		int dst_y,
		unsigned int width,
		unsigned int height
	   );
	eGRAPHICS_INTEGER_STATUS (*composite_boxes)(
		void* surface,
		eGRAPHICS_OPERATOR op,
		struct _GRAPHICS_SURFACE* source,
		struct _GRAPHICS_SURFACE* mask,
		int src_x,
		int src_y,
		int mask_x,
		int mask_y,
		int dst_x,
		int dst_y,
		struct _GRAPHICS_BOXES* boxes,
		const struct _GRAPHICS_RECTANGLE_INT* extents
	   );
	eGRAPHICS_INTEGER_STATUS (*composite_traps)(
		void* dst,
		eGRAPHICS_OPERATOR op,
		struct _GRAPHICS_SURFAE* source,
		int src_x,
		int src_y,
		int dst_x,
		int dst_y,
		const struct _GRAPHICS_RECTANGLE_INT* extents,
		eGRAPHICS_ANTIALIAS antialias,
		struct _GRAPHICS_TRAPS *traps
	   );
	eGRAPHICS_INTEGER_STATUS (*composite_tristrip)(
		void* dst,
		eGRAPHICS_OPERATOR op,
		struct _GRAPHICS_SURFAE* source,
		int src_x,
		int src_y,
		int dst_x,
		int dst_y,
		const struct _GRAPHICS_RECTANGLE_INT* extents,
		eGRAPHICS_ANTIALIAS antialias,
		struct _GRAPHICS_TRISTRIP* tristrip
	   );
	//eGRAPHICS_INTEGER_STATUS (*check_composite_glyphs)()
	//eGRAPHICS_INTEGER_STATUS (*composite_glyphs)()
} GRAPHICS_TRAPS_COMPOSITOR;

typedef struct _GRAPHICS_SPANS_COMPOSITOR
{
	GRAPHICS_COMPOSITOR base;

	eGRAPHICS_INTEGER_STATUS (*fill_boxes)(void* surface,
		eGRAPHICS_OPERATOR op, const GRAPHICS_COLOR* color, GRAPHICS_BOXES* boxes);

	eGRAPHICS_INTEGER_STATUS (*draw_image_boxes)(void* surface,
		struct _GRAPHICS_IMAGE_SURFACE* image, GRAPHICS_BOXES* boxes, int dx, int dy);

	eGRAPHICS_INTEGER_STATUS (*copy_boxes)(void* surface,
		struct _GRAPHCIS_SURFACE* source, GRAPHICS_BOXES* boxes, const GRAPHICS_RECTANGLE_INT* extents,
			int dx, int dy);

	struct _GRAPHICS_SURFACE* (*pattern_to_surface)(struct _GRAPHICS_SURFACE* destination,
		const struct _GRAPHICS_PATTERN* pattern, int is_mask,
			const GRAPHICS_RECTANGLE_INT* extents, const GRAPHICS_RECTANGLE_INT* sample, int* src_x, int* src_y, void* graphics);

	eGRAPHICS_INTEGER_STATUS (*composite_boxes)(void* surface,
		eGRAPHICS_OPERATOR op, struct _GRAPHICS_SURFACE* source, struct _GRAPHICS_SURFACE* mask,
			int src_x, int src_y, int mask_x, int mask_y, int dst_x, int dst_y,
				GRAPHICS_BOXES* boxes, const GRAPHICS_RECTANGLE_INT* extents);

	eGRAPHICS_INTEGER_STATUS (*initialize_renderer)(struct _GRAPHICS_ABSTRACT_SPAN_RENDERER* renderer,
			const GRAPHICS_COMPOSITE_RECTANGLES* extents, eGRAPHICS_ANTIALIAS antialias, int needs_clip);

	void (*render_finish)(struct _GRAPHICS_ABSTRACT_SPAN_RENDERER* renderer, eGRAPHICS_INTEGER_STATUS status);
} GRAPHICS_SPANS_COMPOSITOR;

typedef struct _GRAPHICS_HALF_OPEN_SPAN
{
	int32 x;
	uint8 coverage;
	uint8 inverse;
} GRAPHICS_HALF_OPEN_SPAN;

typedef struct _GRAPHICS_SPAN_REDNDERER
{
	eGRAPHICS_STATUS status;

	void (*destroy)(void*);

	eGRAPHICS_STATUS (*render_rows)(void* abstract_renderer, int y, int height,
		const GRAPHICS_HALF_OPEN_SPAN* coverages, unsigned num_coverages);

	eGRAPHICS_STATUS (*finish)(void* abstract_renderer);
} GRAPHICS_SPAN_RENDERER;

typedef struct _GRAPHICS_SCAN_CONVERTER
{
	void (*destroy)(void*);
	eGRAPHICS_STATUS (*generate)(void* abstract_converter, GRAPHICS_SPAN_RENDERER* renderer);
	eGRAPHICS_STATUS status;
	int own_memory;
} GRAPHICS_SCAN_CONVERTER;


#if PIXEL_MANIPULATE_HAS_COMPOSITOR

typedef struct _GRAPHICS_IMAGE_SPAN_RENDERER {
	cairo_span_renderer_t base;

	pixman_image_compositor_t* compositor;
	PIXEL_MANIPULATE_IMAGE* src, * mask;
	float opacity;
	GRAPHICS_RECTANGLE_INT extents;
} GRAPHICS_IMAGE_SPAN_RENDERER;
COMPILE_TIME_ASSERT(sizeof(GRAPHICS_IMAGE_SPAN_RENDERER) <= sizeof(GRAPHICS_ABSTRACT_SPAN_RENDERER));

#else

typedef struct _GRAPHICS_IMAGE_SPAN_RENDERER
{
	GRAPHICS_SPAN_RENDERER base;
	const GRAPHICS_COMPOSITE_RECTANGLES *composite;

	float opacity;
	uint8 op;
	int bpp;

	PIXEL_MANIPULATE_IMAGE *src, *mask;
	union
	{
		struct fill
		{
			int64 stride;
			uint8 *data;
			uint32 pixel;
		} fill;
		struct blit
		{
			int stride;
			uint8 *data;
			int src_stride;
			uint8 *src_data;
		} blit;
		struct composite
		{
			PIXEL_MANIPULATE_IMAGE *dst;
			int src_x, src_y;
			int mask_x, mask_y;
			int run_length;
		} composite;
		struct finish
		{
			GRAPHICS_RECTANGLE_INT extents;
			int src_x, src_y;
			int64 stride;
			uint8 *data;
		} mask;
	} u;
	uint8 _buf[4096];
#define SZ_BUF (int)(sizeof(GRAPHICS_ABSTRACT_SPAN_RENDERER) - sizeof (GRAPHICS_IMAGE_SPAN_RENDERER))
} GRAPHICS_IMAGE_SPAN_RENDERER;

#endif

typedef struct _GRAPHICS_RECTANGULAR_SCAN_CONVERTER
{
#define STACK_BUFFER_SIZE (512 * sizeof(int))
	GRAPHICS_SCAN_CONVERTER base;

	GRAPHICS_BOX extents;

	struct _GRAPHICS_RECTANGULAR_SCAN_CONVERTER_CHUNK
	{
		struct _GRAPHICS_RECTANGULAR_SCAN_CONVERTER_CHUNK *next;
		void *base;
		int count;
		int size;
	} chunks, *tail;
	char buffer[STACK_BUFFER_SIZE];
	int num_rectangles;
#undef STAK_BUFFER_SIZE
} GRAPHICS_RECTANGULAR_SCAN_CONVERTER;

typedef union _GRAPHICS_SPAN_RENDER_UNION
{
	GRAPHICS_SPAN_RENDERER base;
	struct _GRAPHICS_IMAGE_SPAN_RENDERER image_renderer;
} GRAPHICS_SPAN_RENDER_UNION;

typedef struct _GRAPHICS_ABSTRACT_SPAN_RENDERER
{
	union _GRAPHICS_SPAN_RENDER_UNION renderer_union;
} GRAPHICS_ABSTRACT_SPAN_RENDERER;

extern eGRAPHICS_INTEGER_STATUS GraphicsCompositorPaint(
	const GRAPHICS_COMPOSITOR* compositor,
	struct _GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_CLIP* clip
);

extern eGRAPHICS_INTEGER_STATUS GraphicsCompositorMask(
	const GRAPHICS_COMPOSITOR* compositor,
	struct _GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATTERN* mask,
	const GRAPHICS_CLIP* clip
);

extern eGRAPHICS_INTEGER_STATUS GraphicsCompositorStroke(
	const GRAPHICS_COMPOSITOR* compositor,
	struct _GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	const GRAPHICS_STROKE_STYLE* style,
	const GRAPHICS_MATRIX* ctm,
	const GRAPHICS_MATRIX* ctm_inverse,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
);

extern eGRAPHICS_INTEGER_STATUS GraphicsCompositorFill(
	const GRAPHICS_COMPOSITOR* compositor,
	struct _GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_PATH_FIXED* path,
	eGRAPHICS_FILL_RULE fill_rule,
	FLOAT_T tolerance,
	eGRAPHICS_ANTIALIAS antialias,
	const GRAPHICS_CLIP* clip
);

extern int GraphicsCompositeRectanglesCanReduceClip(
	GRAPHICS_COMPOSITE_RECTANGLES* composite,
	GRAPHICS_CLIP* clip
);

extern void InitializeGraphicsRectangularScanConverter(
	GRAPHICS_RECTANGULAR_SCAN_CONVERTER* self,
	GRAPHICS_RECTANGLE_INT* extents
);

extern eGRAPHICS_STATUS GraphicsRectangularScanConverterAddBox(
	GRAPHICS_RECTANGULAR_SCAN_CONVERTER* self,
	const GRAPHICS_BOX* box,
	int direction
);

extern GRAPHICS_SCAN_CONVERTER* GraphicsMonoScanConverterCreate(
	int xmin,
	int ymin,
	int xmax,
	int ymax,
	eGRAPHICS_FILL_RULE fill_rule
);

extern GRAPHICS_SCAN_CONVERTER* GraphicsTorScanConverterCreate(
	int xmin,
	int ymin,
	int xmax,
	int ymax,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias
);

extern GRAPHICS_SCAN_CONVERTER* GraphicsTor22ScanConverterCreate(
	int xmin,
	int ymin,
	int xmax,
	int ymax,
	eGRAPHICS_FILL_RULE	fill_rule,
	eGRAPHICS_ANTIALIAS	antialias
);

extern eGRAPHICS_INTEGER_STATUS InitializeGraphicsCompositeRectanglesForBoxes(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	struct _GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_BOXES* boxes,
	const GRAPHICS_CLIP* clip
);

extern eGRAPHICS_STATUS InitializeGraphicsCompositeRectanglesForPolygon(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	struct _GRAPHICS_SURFACE* surface,
	eGRAPHICS_OPERATOR op,
	const GRAPHICS_PATTERN* source,
	const GRAPHICS_POLYGON* polygon,
	const GRAPHICS_CLIP* clip
);

extern void GraphicsCompositeRectanglesFinish(GRAPHICS_COMPOSITE_RECTANGLES* extents);

extern eGRAPHICS_INTEGER_STATUS GraphicsCompositeRectanglesIntersectMaskExtents(
	GRAPHICS_COMPOSITE_RECTANGLES* extents,
	const GRAPHICS_BOX* box
);

extern void GraphicsShapeMaskCompositorInitialize(GRAPHICS_COMPOSITOR* compositor, const GRAPHICS_COMPOSITOR* delegate, void* graphics);

extern void InitialzieGraphicsSpansCompositor(
	GRAPHICS_SPANS_COMPOSITOR* compositor,
	const GRAPHICS_COMPOSITOR* delegate,
	void* graphics
);

extern void SetGraphicsImageCompositorCallbacks(GRAPHICS_SPANS_COMPOSITOR* compositor);

#endif // #ifndef _INCLUDED_GRAPHICS_COMPOSITOR_H_
