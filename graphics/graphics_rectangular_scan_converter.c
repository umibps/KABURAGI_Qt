#include <string.h>
#include <setjmp.h>
#include "graphics_types.h"
#include "graphics_compositor.h"
#include "graphics_surface.h"
#include "graphics.h"
#include "graphics_flags.h"
#include "graphics_matrix.h"
#include "graphics_pen.h"
#include "graphics_memory_pool.h"
#include "graphics_private.h"
#include "graphics_inline.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _RECTANGLE
{
	struct _RECTANGLE *next, *prev;
	GRAPHICS_FLOAT_FIXED left, right;
	GRAPHICS_FLOAT_FIXED top, bottom;
	int32 top_y, bottom_y;
	int direction;
} RECTANGLE;

#define UNROLL3(x) x x x

/* the parent is always given by index/2 */
#define POINT_QUEUE_PARENT_INDEX(i) ((i) >> 1)
#define POINT_QUEUE_FIRST_ENTRY 1

/* left and right children are index * 2 and (index * 2) +1 respectively */
#define POINT_QUEUE_LEFT_CHILD_INDEX(i) ((i) << 1)

typedef struct _POINT_QUEUE
{
	int size, max_size;

	RECTANGLE **elements;
	RECTANGLE *elements_embedded[1024];
} POINT_QUEUE;

typedef struct _SWEEP_LINE
{
#define STACK_BUFFER_SIZE (512 * sizeof(int))
	RECTANGLE **start;
	POINT_QUEUE stop;
	RECTANGLE head, tail;
	RECTANGLE *insert_cursor;
	int32 current_y;
	int32 xmin, xmax;

	struct _COVERAGE
	{
		struct _CELL {
			struct _CELL *prev, *next;
			int x, covered, uncovered;
		} head, tail, *cursor;
		unsigned int count;
		GRAPHICS_MEMORY_POOL pool;
	} coverage;

	GRAPHICS_HALF_OPEN_SPAN spans_stack[STACK_BUFFER_SIZE / sizeof(GRAPHICS_HALF_OPEN_SPAN)];
	GRAPHICS_HALF_OPEN_SPAN *spans;
	unsigned int num_spans;
	unsigned int size_spans;

	jmp_buf jmpbuf;
} SWEEP_LINE;

static INLINE int RectangleCompareStart (const RECTANGLE*a, const RECTANGLE* b)
{
	int cmp;

	cmp = a->top_y - b->top_y;
	if(cmp)
	{
		return cmp;
	}
	return a->left - b->left;
}

static INLINE int RectangleCompareStop(const RECTANGLE* a, const RECTANGLE* b)
{
	return a->bottom_y - b->bottom_y;
}

static INLINE void InitializePointQueue(POINT_QUEUE* pq)
{
	pq->max_size = sizeof(pq->elements_embedded) / sizeof(pq->elements_embedded[0]);
	pq->size = 0;

	pq->elements = pq->elements_embedded;
	pq->elements[POINT_QUEUE_FIRST_ENTRY] = NULL;
}

static INLINE void PointQueueFinish(POINT_QUEUE* pq)
{
	if(pq->elements != pq->elements_embedded)
	{
		MEM_FREE_FUNC(pq->elements);
	}
}

static int PointQueueGrow(POINT_QUEUE* pq)
{
	RECTANGLE **new_elements;
	pq->max_size *= 2;

	if(pq->elements == pq->elements_embedded)
	{
		new_elements = (RECTANGLE**)MEM_ALLOC_FUNC(sizeof(RECTANGLE*) * pq->max_size);
		if(new_elements == NULL)
		{
			return FALSE;
		}
		(void)memcpy(new_elements, pq->elements_embedded,
						sizeof (pq->elements_embedded));
	}
	else
	{
		new_elements = (RECTANGLE**)MEM_REALLOC_FUNC(pq->elements, sizeof(RECTANGLE*) * pq->max_size);
		if(new_elements == NULL)
		{
			return FALSE;
		}
	}

	pq->elements = new_elements;
	return TRUE;
}

static INLINE void PointQueuePush(SWEEP_LINE* sweep, RECTANGLE* rectangle)
{
	RECTANGLE **elements;
	int i, parent;

	if(UNLIKELY(sweep->stop.size + 1 == sweep->stop.max_size))
	{
		if(UNLIKELY(FALSE == PointQueueGrow(&sweep->stop)))
		{
			longjmp (sweep->jmpbuf, GRAPHICS_STATUS_NO_MEMORY);
		}
	}

	elements = sweep->stop.elements;
	for(i = ++sweep->stop.size;
		 i != POINT_QUEUE_FIRST_ENTRY &&
			RectangleCompareStop(rectangle, elements[parent = POINT_QUEUE_PARENT_INDEX (i)]) < 0;
	 i = parent
	)
	{
		elements[i] = elements[parent];
	}

	elements[i] = rectangle;
}

static INLINE void PointQueuePop(POINT_QUEUE* pq)
{
	RECTANGLE **elements = pq->elements;
	RECTANGLE *tail;
	int child, i;

	tail = elements[pq->size--];
	if (pq->size == 0)
	{
		elements[POINT_QUEUE_FIRST_ENTRY] = NULL;
		return;
	}

	for(i = POINT_QUEUE_FIRST_ENTRY;
		(child = POINT_QUEUE_LEFT_CHILD_INDEX (i)) <= pq->size;
	 i = child
	)
	{
		if(child != pq->size &&
			RectangleCompareStop(elements[child+1], elements[child]) < 0)
		{
			child++;
		}

		if(RectangleCompareStop(elements[child], tail) >= 0)
		{
			break;
		}
		elements[i] = elements[child];
	}
	elements[i] = tail;
}

static INLINE RECTANGLE* PeekStop(SWEEP_LINE* sweep)
{
	return sweep->stop.elements[POINT_QUEUE_FIRST_ENTRY];
}

#ifndef GRAPHICS_COMBSORT_DECLARE
static INLINE unsigned int GraphicsCombsortNewgap(unsigned int gap)
{
	gap = 10 * gap / 13;
	if(gap == 9 || gap == 10)
	{
		gap = 11;
	}
	if(gap < 1)
	{
		gap = 1;
	}
	return gap;
}

# define GRAPHICS_COMBSORT_DECLARE(NAME, TYPE, CMP) \
static void \
NAME (TYPE *base, unsigned int nmemb) \
{ \
  unsigned int gap = nmemb; \
  unsigned int i, j; \
  int swapped; \
  do { \
	  gap = GraphicsCombsortNewgap(gap); \
	  swapped = gap > 1; \
	  for (i = 0; i < nmemb-gap ; i++) { \
	  j = i + gap; \
	  if (CMP (base[i], base[j]) > 0 ) { \
		  TYPE tmp; \
		  tmp = base[i]; \
		  base[i] = base[j]; \
		  base[j] = tmp; \
		  swapped = 1; \
	  } \
	  } \
  } while (swapped); \
}
#endif

GRAPHICS_COMBSORT_DECLARE(RectangleSort, RECTANGLE*, RectangleCompareStart)

static void InitializeSweepLine(SWEEP_LINE* sweep)
{
	sweep->head.left = INT_MIN;
	sweep->head.next = &sweep->tail;
	sweep->tail.left = INT_MAX;
	sweep->tail.prev = &sweep->head;
	sweep->insert_cursor = &sweep->tail;

	InitializeGraphicsMemoryPool(&sweep->coverage.pool, sizeof(struct _CELL));

	sweep->spans = sweep->spans_stack;
	sweep->size_spans = sizeof(sweep->spans_stack) / sizeof(sweep->spans_stack[0]);

	sweep->coverage.head.prev = NULL;
	sweep->coverage.head.x = INT_MIN;
	sweep->coverage.tail.next = NULL;
	sweep->coverage.tail.x = INT_MAX;

	InitializePointQueue(&sweep->stop);
}

static void SweepLineFinish(SWEEP_LINE* sweep)
{
	GraphicsMemoryPoolFinish(&sweep->coverage.pool);
	PointQueueFinish(&sweep->stop);

	if(sweep->spans != sweep->spans_stack)
	{
		MEM_FREE_FUNC(sweep->spans);
	}
}

static INLINE void AddCell(SWEEP_LINE* sweep, int x, int covered, int uncovered)
{
	struct _CELL *cell;

	cell = sweep->coverage.cursor;
	if(cell->x > x)
	{
		do
		{
			UNROLL3({
				if (cell->prev->x < x)
				{
					break;
				}
				cell = cell->prev;
			})
		} while(TRUE);
	}
	else
	{
		if(cell->x == x)
		{
			goto found;
		}
		do
		{
			UNROLL3({
				cell = cell->next;
				if (cell->x >= x)
				{
					break;
				}
			})
		} while(TRUE);
	}

	if(x != cell->x)
	{
		struct _CELL *c;

		sweep->coverage.count++;

		c = GraphicsMemoryPoolAllocation(&sweep->coverage.pool);
		if(UNLIKELY(c == NULL))
		{
			longjmp (sweep->jmpbuf, GRAPHICS_STATUS_NO_MEMORY);
		}

		cell->prev->next = c;
		c->prev = cell->prev;
		c->next = cell;
		cell->prev = c;

		c->x = x;
		c->covered = 0;
		c->uncovered = 0;

		cell = c;
	}

found:
	cell->covered += covered;
	cell->uncovered += uncovered;
	sweep->coverage.cursor = cell;
}

static INLINE void ActiveEdgesToSpans(SWEEP_LINE* sweep)
{
	int32 y = sweep->current_y;
	RECTANGLE *rectangle;
	int coverage, prev_coverage;
	int prev_x;
	struct _CELL *cell;

	sweep->num_spans = 0;
	if(sweep->head.next == &sweep->tail)
	{
		return;
	}

	sweep->coverage.head.next = &sweep->coverage.tail;
	sweep->coverage.tail.prev = &sweep->coverage.head;
	sweep->coverage.cursor = &sweep->coverage.tail;
	sweep->coverage.count = 0;

	/* XXX cell coverage only changes when a rectangle appears or
	 * disappears. Try only modifying coverage at such times.
	 */
	for(rectangle = sweep->head.next;
		rectangle != &sweep->tail;
		rectangle = rectangle->next
	)
	{
		int height;
		int frac, i;

		if(y == rectangle->bottom_y)
		{
			height = rectangle->bottom & GRAPHICS_FIXED_FRAC_MASK;
			if(height == 0)
			{
				continue;
			}
		}
		else
		{
			height = GRAPHICS_FIXED_ONE;
		}
		if(y == rectangle->top_y)
		{
			height -= rectangle->top & GRAPHICS_FIXED_FRAC_MASK;
		}
		height *= rectangle->direction;

		i = GraphicsFixedIntegerPart(rectangle->left),
		frac = GraphicsFixedFractionalPart(rectangle->left);
		AddCell(sweep, i, (GRAPHICS_FIXED_ONE-frac) * height, frac * height);

		i = GraphicsFixedIntegerPart(rectangle->right),
		frac = GraphicsFixedFractionalPart(rectangle->right);
		AddCell(sweep, i, - (GRAPHICS_FIXED_ONE-frac) * height, - frac * height);
	}

	if(2*sweep->coverage.count >= sweep->size_spans)
	{
		unsigned size;

		size = sweep->size_spans;
		while (size <= 2*sweep->coverage.count)
		{
			size <<= 1;
		}

		if(sweep->spans != sweep->spans_stack)
		{
			MEM_FREE_FUNC(sweep->spans);
		}

		sweep->spans = (GRAPHICS_HALF_OPEN_SPAN*)MEM_ALLOC_FUNC(size * sizeof(GRAPHICS_HALF_OPEN_SPAN));
		if(UNLIKELY(sweep->spans == NULL))
		{
			longjmp(sweep->jmpbuf, GRAPHICS_STATUS_NO_MEMORY);
		}

		sweep->size_spans = size;
	}

	prev_coverage = coverage = 0;
	prev_x = INT_MIN;
	for(cell = sweep->coverage.head.next; cell != &sweep->coverage.tail; cell = cell->next)
	{
		if(cell->x != prev_x && coverage != prev_coverage)
		{
			int n = sweep->num_spans++;
			int c = coverage >> (GRAPHICS_FIXED_FRAC_BITS * 2 - 8);
			sweep->spans[n].x = prev_x;
			sweep->spans[n].inverse = 0;
			sweep->spans[n].coverage = c - (c >> 8);
			prev_coverage = coverage;
		}

		coverage += cell->covered;
		if(coverage != prev_coverage)
		{
			int n = sweep->num_spans++;
			int c = coverage >> (GRAPHICS_FIXED_FRAC_BITS * 2 - 8);
			sweep->spans[n].x = cell->x;
			sweep->spans[n].inverse = 0;
			sweep->spans[n].coverage = c - (c >> 8);
			prev_coverage = coverage;
		}
		coverage += cell->uncovered;
		prev_x = cell->x + 1;
	}
	GraphicsMemoryPoolReset(&sweep->coverage.pool);

	if(sweep->num_spans != 0)
	{
		if(prev_x <= sweep->xmax)
		{
			int n = sweep->num_spans++;
			int c = coverage >> (GRAPHICS_FIXED_FRAC_BITS * 2 - 8);
			sweep->spans[n].x = prev_x;
			sweep->spans[n].inverse = 0;
			sweep->spans[n].coverage = c - (c >> 8);
		}

		if(coverage && prev_x < sweep->xmax)
		{
			int n = sweep->num_spans++;
			sweep->spans[n].x = sweep->xmax;
			sweep->spans[n].inverse = 1;
			sweep->spans[n].coverage = 0;
		}
	}
}

static INLINE void SweepLineDelete(SWEEP_LINE* sweep, RECTANGLE* rectangle)
{
	if(sweep->insert_cursor == rectangle)
	{
		sweep->insert_cursor = rectangle->next;
	}

	rectangle->prev->next = rectangle->next;
	rectangle->next->prev = rectangle->prev;

	PointQueuePop(&sweep->stop);
}

static INLINE void SweepLineInsert(SWEEP_LINE* sweep, RECTANGLE* rectangle)
{
	RECTANGLE *pos;

	pos = sweep->insert_cursor;
	if(pos->left != rectangle->left)
	{
		if(pos->left > rectangle->left)
		{
			do
			{
			UNROLL3({
					if (pos->prev->left < rectangle->left)
					{
						break;
					}
					pos = pos->prev;
				})
			} while(TRUE);
		}
		else
		{
			do
			{
				UNROLL3({
					pos = pos->next;
					if (pos->left >= rectangle->left)
					{
						break;
					}
				});
			} while(TRUE);
		}
	}

	pos->prev->next = rectangle;
	rectangle->prev = pos->prev;
	rectangle->next = pos;
	pos->prev = rectangle;
	sweep->insert_cursor = rectangle;

	PointQueuePush(sweep, rectangle);
}

static void RenderRows(
	SWEEP_LINE* sweep_line,
	GRAPHICS_SPAN_RENDERER* renderer,
	int height
)
{
	eGRAPHICS_STATUS status;

	ActiveEdgesToSpans(sweep_line);

	status = renderer->render_rows(renderer, sweep_line->current_y, height,
					sweep_line->spans, sweep_line->num_spans);
	if(UNLIKELY(status))
	{
		longjmp (sweep_line->jmpbuf, status);
	}
}

static eGRAPHICS_STATUS Generate(
	GRAPHICS_RECTANGULAR_SCAN_CONVERTER* self,
	GRAPHICS_SPAN_RENDERER* renderer,
	RECTANGLE** rectangles
)
{
	SWEEP_LINE sweep_line;
	RECTANGLE *start, *stop;
	eGRAPHICS_STATUS status;

	InitializeSweepLine(&sweep_line);
	sweep_line.xmin = GraphicsFixedIntegerPart(self->extents.point1.x);
	sweep_line.xmax = GraphicsFixedIntegerPart(self->extents.point2.x);
	sweep_line.start = rectangles;
	if((status = setjmp (sweep_line.jmpbuf)))
	{
		goto out;
	}

	sweep_line.current_y = GraphicsFixedIntegerPart(self->extents.point1.y);
	start = *sweep_line.start++;
	do
	{
		if(start->top_y != sweep_line.current_y)
		{
			RenderRows(&sweep_line, renderer, start->top_y - sweep_line.current_y);
			sweep_line.current_y = start->top_y;
		}

		do
		{
			SweepLineInsert(&sweep_line, start);
			start = *sweep_line.start++;
			if(start == NULL)
			{
				goto end;
			}
			if(start->top_y != sweep_line.current_y)
			{
				break;
			}
		} while(TRUE);

		RenderRows(&sweep_line, renderer, 1);

		stop = PeekStop(&sweep_line);
		while(stop->bottom_y == sweep_line.current_y)
		{
			SweepLineDelete(&sweep_line, stop);
			stop = PeekStop(&sweep_line);
			if(stop == NULL)
			{
				break;
			}
		}

		sweep_line.current_y++;

		while(stop != NULL && stop->bottom_y < start->top_y)
		{
			if(stop->bottom_y != sweep_line.current_y)
			{
				RenderRows(&sweep_line, renderer, stop->bottom_y - sweep_line.current_y);
				sweep_line.current_y = stop->bottom_y;
			}

			RenderRows(&sweep_line, renderer, 1);

			do
			{
				SweepLineDelete(&sweep_line, stop);
				stop = PeekStop(&sweep_line);
			} while(stop != NULL && stop->bottom_y == sweep_line.current_y);

			sweep_line.current_y++;
		}
	} while(TRUE);

  end:
	RenderRows(&sweep_line, renderer, 1);

	stop = PeekStop(&sweep_line);
	while(stop->bottom_y == sweep_line.current_y)
	{
		SweepLineDelete(&sweep_line, stop);
		stop = PeekStop(&sweep_line);
		if(stop == NULL)
		{
			goto out;
		}
	}

	while(++sweep_line.current_y < GraphicsFixedIntegerPart(self->extents.point2.y))
	{
		if(stop->bottom_y != sweep_line.current_y)
		{
			RenderRows(&sweep_line, renderer, stop->bottom_y - sweep_line.current_y);
			sweep_line.current_y = stop->bottom_y;
		}

		RenderRows(&sweep_line, renderer, 1);

		do
		{
			SweepLineDelete(&sweep_line, stop);
			stop = PeekStop(&sweep_line);
			if(stop == NULL)
			{
				goto out;
			}
		} while(stop->bottom_y == sweep_line.current_y);

	}

  out:
	SweepLineFinish(&sweep_line);

	return status;
}

static void GenerateRow(
	GRAPHICS_SPAN_RENDERER* renderer,
	const RECTANGLE* r,
	int y,
	int h,
	uint16 coverage
)
{
	GRAPHICS_HALF_OPEN_SPAN spans[4];
	unsigned int num_spans = 0;
	int x1 = GraphicsFixedIntegerPart(r->left);
	int x2 = GraphicsFixedIntegerPart(r->right);
	if (x2 > x1)
	{
		if(FALSE == GRAPHICS_FIXED_IS_INTEGER(r->left))
		{
			spans[num_spans].x = x1;
			spans[num_spans].coverage =
			coverage * (256 - GraphicsFixedFractionalPart(r->left)) >> 8;
			num_spans++;
			x1++;
		}

		if(x2 > x1)
		{
			spans[num_spans].x = x1;
			spans[num_spans].coverage = coverage - (coverage >> 8);
			num_spans++;
		}

		if(FALSE == GRAPHICS_FIXED_IS_INTEGER(r->right))
		{
			spans[num_spans].x = x2++;
			spans[num_spans].coverage =
			coverage * GraphicsFixedFractionalPart(r->right) >> 8;
			num_spans++;
		}
	}
	else
	{
		spans[num_spans].x = x2++;
		spans[num_spans].coverage = coverage * (r->right - r->left) >> 8;
		num_spans++;
	}

	spans[num_spans].x = x2;
	spans[num_spans].coverage = 0;
	num_spans++;

	renderer->render_rows(renderer, y, h, spans, num_spans);
}

static eGRAPHICS_STATUS GenerateBox(
	GRAPHICS_RECTANGULAR_SCAN_CONVERTER* self,
	GRAPHICS_SPAN_RENDERER* renderer
)
{
	const RECTANGLE *r = self->chunks.base;
	int y1 = GraphicsFixedIntegerPart(r->top);
	int y2 = GraphicsFixedIntegerPart(r->bottom);
	if (y2 > y1)
	{
		if(FALSE == GRAPHICS_FIXED_IS_INTEGER(r->top))
		{
			GenerateRow(renderer, r, y1, 1,
			 256 - GraphicsFixedFractionalPart(r->top));
			y1++;
		}

		if(y2 > y1)
		{
			GenerateRow(renderer, r, y1, y2-y1, 256);
		}
		if(FALSE == GRAPHICS_FIXED_IS_INTEGER(r->bottom))
		{
			GenerateRow(renderer, r, y2, 1, GraphicsFixedFractionalPart(r->bottom));
		}
	}
	else
	{
		GenerateRow(renderer, r, y1, 1, r->bottom - r->top);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsRectangularScanConverterGenerate (
	void* converter,
	GRAPHICS_SPAN_RENDERER* renderer
)
{
	GRAPHICS_RECTANGULAR_SCAN_CONVERTER *self = (GRAPHICS_RECTANGULAR_SCAN_CONVERTER*)converter;
	RECTANGLE *rectangles_stack[STACK_BUFFER_SIZE / sizeof(RECTANGLE*)];
	RECTANGLE **rectangles;
	struct _GRAPHICS_RECTANGULAR_SCAN_CONVERTER_CHUNK *chunk;
	eGRAPHICS_STATUS status;
	int i, j;

	if(UNLIKELY(self->num_rectangles == 0))
	{
		return renderer->render_rows(renderer, GraphicsFixedIntegerPart(self->extents.point1.y),
					  GraphicsFixedIntegerPart(self->extents.point2.y - self->extents.point1.y), NULL, 0);
	}

	if(self->num_rectangles == 1)
	{
		return GenerateBox(self, renderer);
	}

	rectangles = rectangles_stack;
	if(UNLIKELY(self->num_rectangles >= sizeof(rectangles_stack) / sizeof(rectangles_stack[0])))
	{
		rectangles = (RECTANGLE**)MEM_ALLOC_FUNC(sizeof(RECTANGLE*) * (self->num_rectangles + 1));
		if(UNLIKELY(rectangles == NULL))
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}
	}

	j = 0;
	for(chunk = &self->chunks; chunk != NULL; chunk = chunk->next)
	{
		RECTANGLE *rectangle;

		rectangle = chunk->base;
		for(i = 0; i < chunk->count; i++)
		{
			rectangles[j++] = &rectangle[i];
		}
	}
	RectangleSort(rectangles, j);
	rectangles[j] = NULL;

	status = Generate(self, renderer, rectangles);

	if(rectangles != rectangles_stack)
	{
		MEM_FREE_FUNC(rectangles);
	}

	return status;
}

static RECTANGLE* AllocateRectangle(GRAPHICS_RECTANGULAR_SCAN_CONVERTER* self)
{
	RECTANGLE *rectangle;
	struct _GRAPHICS_RECTANGULAR_SCAN_CONVERTER_CHUNK *chunk;

	chunk = self->tail;
	if(chunk->count == chunk->size)
	{
		int size;

		size = chunk->size * 2;
		chunk->next = (struct _GRAPHICS_RECTANGULAR_SCAN_CONVERTER_CHUNK*)MEM_ALLOC_FUNC(
			size * sizeof(RECTANGLE) + sizeof(struct _GRAPHICS_RECTANGULAR_SCAN_CONVERTER_CHUNK));
		if(chunk->next == NULL)
		{
			return NULL;
		}

		chunk = chunk->next;
		chunk->next = NULL;
		chunk->count = 0;
		chunk->size = size;
		chunk->base = chunk + 1;
		self->tail = chunk;
	}

	rectangle = chunk->base;
	return rectangle + chunk->count++;
}

eGRAPHICS_STATUS GraphicsRectangularScanConverterAddBox(
	GRAPHICS_RECTANGULAR_SCAN_CONVERTER* self,
	const GRAPHICS_BOX* box,
	int direction
)
{
	RECTANGLE *rectangle;

	rectangle = AllocateRectangle(self);
	if(UNLIKELY(rectangle == NULL))
	{
		return GRAPHICS_STATUS_NO_MEMORY;
	}

	rectangle->direction = direction;
	rectangle->left  = MAXIMUM(box->point1.x, self->extents.point1.x);
	rectangle->right = MINIMUM(box->point2.x, self->extents.point2.x);
	if(UNLIKELY(rectangle->right <= rectangle->left))
	{
		self->tail->count--;
		return GRAPHICS_STATUS_SUCCESS;
	}

	rectangle->top = MAXIMUM(box->point1.y, self->extents.point1.y);
	rectangle->top_y  = GraphicsFixedIntegerFloor(rectangle->top);
	rectangle->bottom = MINIMUM(box->point2.y, self->extents.point2.y);
	rectangle->bottom_y = GRAPHICS_FIXED_INTEGER_FLOOR(rectangle->bottom);
	if(rectangle->bottom > rectangle->top)
	{
		self->num_rectangles++;
	}
	else
	{
		self->tail->count--;
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static void GraphicsRectangularScanConverterDestroy(void* converter)
{
	GRAPHICS_RECTANGULAR_SCAN_CONVERTER *self = converter;
	struct _GRAPHICS_RECTANGULAR_SCAN_CONVERTER_CHUNK *chunk, *next;

	for(chunk = self->chunks.next; chunk != NULL; chunk = next)
	{
		next = chunk->next;
		MEM_FREE_FUNC(chunk);
	}
}

void InitializeGraphicsRectangularScanConverter(
	GRAPHICS_RECTANGULAR_SCAN_CONVERTER* self,
	GRAPHICS_RECTANGLE_INT* extents
)
{
	self->base.destroy = GraphicsRectangularScanConverterDestroy;
	self->base.generate = GraphicsRectangularScanConverterGenerate;

	GraphicsBoxFromRectangle(&self->extents, extents);

	self->chunks.base = self->buffer;
	self->chunks.next = NULL;
	self->chunks.count = 0;
	self->chunks.size = sizeof (self->buffer) / sizeof (RECTANGLE);
	self->tail = &self->chunks;

	self->num_rectangles = 0;
}

#ifdef __cplusplus
}
#endif
