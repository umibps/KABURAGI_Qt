#include <setjmp.h>
#include "graphics.h"
#include "graphics_private.h"
#include "graphics_scan_converter_private.h"
#include "graphics_inline.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STACK_BUFFER_SIZE (512 * sizeof(int))
#define POINT_QUEUE_FIRST_ENTRY 1
#define POINT_QUEUE_LEFT_CHILD_INDEX(i) ((i) << 1)
#define POINT_QUEUE_PARENT_INDEX(i) ((i) >> 1)

typedef struct _EDGE
{
	struct _EDGE *next, *previous;
	struct _EDGE *right;
	GRAPHICS_FLOAT_FIXED x, top;
	int a_or_b;
	int direction;
} EDGE;

typedef struct _RECTANGLE
{
	EDGE left, right;
	int32 top, bottom;
} RECTANGLE;

typedef struct _POINT_QUEUE
{
	int size, max_size;

	RECTANGLE **elements;
	RECTANGLE *elements_embedded[1024];
} POINT_QUEUE;

typedef struct _SWEEP_LINE
{
	RECTANGLE **rectangles;
	POINT_QUEUE point_queue;
	EDGE head, tail;
	EDGE *insert_left, *insert_right;
	int32 current_y;
	int32 last_y;

	jmp_buf unwind;
} SWEEP_LINE;

int GraphicsRectangleIntersect(GRAPHICS_RECTANGLE_INT* dst, GRAPHICS_RECTANGLE_INT* src)
{
	int x1, y1, x2, y2;

	x1 = MAXIMUM(dst->x, src->x);
	y1 = MAXIMUM(dst->y, src->y);

	x2 = MINIMUM(dst->x + (int)dst->width, src->x + (int)src->width);
	y2 = MINIMUM(dst->y + (int)dst->height, src->y + (int)src->height);

	if(x1 >= x2 || y1 >= y2)
	{
		dst->x = 0, dst->y = 0;
		dst->width = 0, dst->height = 0;
		return FALSE;
	}
	else
	{
		dst->x = x1,	dst->y = y1;
		dst->width = x2 - x1,   dst->height = y2 - y1;
	}

	return TRUE;
}

void GraphicsBoxRoundToRectangle(const GRAPHICS_BOX* box, GRAPHICS_RECTANGLE_INT* rectangle)
{
	rectangle->x = GraphicsFixedIntegerFloor(box->point1.x);
	rectangle->y = GraphicsFixedIntegerFloor(box->point1.y);
	rectangle->width = GraphicsFixedIngegerCeil(box->point2.x) - rectangle->x;
	rectangle->height = GraphicsFixedIngegerCeil(box->point2.y) - rectangle->y;
}

static eGRAPHICS_STATUS GraphicsBoxAddSplinePoint(
	void* closure,
	const GRAPHICS_POINT* point,
	const GRAPHICS_SLOPE* tangent
)
{
	GraphicsBoxAddPoint(closure, point);

	return GRAPHICS_STATUS_SUCCESS;
}

void GraphicsBoxAddCurveTo(
	GRAPHICS_BOX* extents,
	const GRAPHICS_POINT* a,
	const GRAPHICS_POINT* b,
	const GRAPHICS_POINT* c,
	const GRAPHICS_POINT* d
)
{
	GraphicsBoxAddPoint(extents, d);
	if(FALSE == GraphicsBoxContainsPoint(extents, b) ||
		FALSE == GraphicsBoxContainsPoint(extents, c))
	{
		eGRAPHICS_STATUS status;

		status = GraphicsSplineBound(GraphicsBoxAddSplinePoint,
			extents, a, b, c, d);
		ASSERT(status == GRAPHICS_STATUS_SUCCESS);
	}
}

void GraphicsBoxFromRectangle(GRAPHICS_BOX* box, const GRAPHICS_RECTANGLE_INT* rectangle)
{
	box->point1.x = GraphicsFixedFromInteger(rectangle->x);
	box->point1.y = GraphicsFixedFromInteger(rectangle->y);
	box->point1.x = GraphicsFixedFromInteger(rectangle->x + rectangle->width);
	box->point2.y = GraphicsFixedFromInteger(rectangle->x + rectangle->height);
}

int GraphicsBoxIntersectsLineSegment(const GRAPHICS_BOX* box, GRAPHICS_LINE* line)
{
	GRAPHICS_FLOAT_FIXED t1 = 0, t2 = 0, t3 = 0, t4 = 0;
	int64 t1y, t2y, t3x, t4x;
	GRAPHICS_FLOAT_FIXED x_length, y_length;

	if(GraphicsBoxContainsPoint(box, &line->point1) || GraphicsBoxContainsPoint(box, &line->point2))
	{
		return TRUE;
	}

	x_length = line->point2.x - line->point1.x;
	y_length = line->point2.y - line->point1.y;

	if(x_length)
	{
		if(x_length > 0)
		{
			t1 = box->point1.x - line->point1.x;
			t2 = box->point2.x - line->point1.x;
		}
		else
		{
			t1 = line->point1.x - box->point2.x;
			t2 = line->point1.x - box->point1.x;
			x_length = - x_length;
		}
	}

	if((t1 < 0 || t1 > x_length) && (t2 < 0 || t2 > x_length))
	{
		return FALSE;
	}
	else
	{
		if(line->point1.x < box->point1.x || line->point1.x > box->point2.x)
		{
			return FALSE;
		}
	}

	if(y_length)
	{
		if(y_length > 0)
		{
			t3 = box->point1.y - line->point1.y;
			t4 = box->point2.y - line->point1.y;
		}
		else
		{
			t3 = line->point1.y - box->point2.y;
			t4 = line->point1.y - box->point1.y;
			y_length = - y_length;
		}

		if((t3 < 0 || t3 > y_length) && (t4 < 0 || t4 > y_length))
		{
			return FALSE;
		}
		else
		{
			if(line->point1.y < box->point1.y || line->point1.y > box->point2.y)
			{
				return FALSE;
			}
		}
	}

	if(line->point1.x == line->point2.x || line->point1.y == line->point2.y)
	{
		return TRUE;
	}

	t1y = GRAPHICS_INT32x32_64_MULTI(t1, y_length);
	t2y = GRAPHICS_INT32x32_64_MULTI(t2, y_length);
	t3x = GRAPHICS_INT32x32_64_MULTI(t3, x_length);
	t4x = GRAPHICS_INT32x32_64_MULTI(t4, x_length);

	if(t1y < t4x && t3x < t2y)
	{
		return TRUE;
	}

	return FALSE;
}

void InitializeGraphicsBoxes(GRAPHICS_BOXES* boxes)
{
	boxes->status = GRAPHICS_STATUS_SUCCESS;
	boxes->num_limits = 0;
	boxes->num_boxes = 0;

	boxes->tail = &boxes->chunks;
	boxes->chunks.next = NULL;
	boxes->chunks.base = boxes->boxes_embedded;
	boxes->chunks.size = sizeof(boxes->boxes_embedded) / sizeof(boxes->boxes_embedded[0]);
	boxes->chunks.count = 0;

	boxes->is_pixel_aligned = TRUE;
}

void InitializeGraphicsBoxesForArray(GRAPHICS_BOXES* boxes, GRAPHICS_BOX* array, int num_boxes)
{
	int n;

	boxes->status = GRAPHICS_STATUS_SUCCESS;
	boxes->num_limits = 0;
	boxes->num_boxes = num_boxes;

	boxes->tail = &boxes->chunks;
	boxes->chunks.next = NULL;
	boxes->chunks.base = array;
	boxes->chunks.size = num_boxes;
	boxes->chunks.count = num_boxes;

	for(n=0; n<num_boxes; n++)
	{
		if(FALSE == GRAPHICS_FIXED_IS_INTEGER(array[n].point1.x)
			|| FALSE == GRAPHICS_FIXED_IS_INTEGER(array[n].point1.y)
			|| FALSE == GRAPHICS_FIXED_IS_INTEGER(array[n].point2.x)
			|| FALSE == GRAPHICS_FIXED_IS_INTEGER(array[n].point2.y)
		)
		{
			break;
		}
	}

	boxes->is_pixel_aligned = n == num_boxes;
}

void InitializeGraphicsBoxesFromRectangle(
	GRAPHICS_BOXES* boxes,
	int x,
	int y,
	int width,
	int height
)
{
	InitializeGraphicsBoxes(boxes);

	GraphicsBoxFromIntegers(&boxes->chunks.base[0], x, y, width, height);
	boxes->num_boxes = 1;
}

void InitializeGraphicsBoxesWithClip(GRAPHICS_BOXES* boxes, GRAPHICS_CLIP* clip)
{
	InitializeGraphicsBoxes(boxes);
	if(clip != NULL)
	{
		GraphicsBoxesLimit(boxes, clip->boxes, clip->num_boxes);
	}
}

void GraphicsBoxesFinish(GRAPHICS_BOXES* boxes)
{
	struct _GRAPHICS_BOXES_CHUNK *chunk, *next;

	for(chunk=boxes->chunks.next; chunk!=NULL; chunk = next)
	{
		next = chunk->next;
		MEM_FREE_FUNC(chunk);
	}
}

void GraphicsBoxesGetExtents(const GRAPHICS_BOX* boxes, int num_boxes, GRAPHICS_BOX* extents)
{
	ASSERT(num_boxes > 0);
	*extents = *boxes;
	while(--num_boxes != 0)
	{
		GraphicsBoxAddBox(extents, ++boxes);
	}
}

static void GraphicsBoxesAddInternal(GRAPHICS_BOXES* boxes, const GRAPHICS_BOX* box)
{
	struct _GRAPHICS_BOXES_CHUNK *chunk;

	if(UNLIKELY(boxes->status))
	{
		return;
	}

	chunk = boxes->tail;
	if(UNLIKELY(chunk->count == chunk->size))
	{
		int size;

		size = chunk->size * 2;
		chunk->next = (struct _GRAPHICS_BOXES_CHUNK*)MEM_ALLOC_FUNC(
						size * sizeof(GRAPHICS_BOX) + sizeof(struct _GRAPHICS_BOXES_CHUNK));
		if(UNLIKELY(chunk->next == NULL))
		{
			boxes->status = GRAPHICS_STATUS_NO_MEMORY;
			return;
		}

		chunk = chunk->next;
		boxes->tail = chunk;

		chunk->next = NULL;
		chunk->count = 0;
		chunk->size = size;
		chunk->base = (GRAPHICS_BOX*)(chunk + 1);
	}

	chunk->base[chunk->count++] = *box;
	boxes->num_boxes++;

	if(boxes->is_pixel_aligned)
	{
		boxes->is_pixel_aligned = GraphicsBoxIsPixelAligned(box);
	}
}

eGRAPHICS_STATUS GraphicsBoxesAdd(GRAPHICS_BOXES* boxes, eGRAPHICS_ANTIALIAS antialias, const GRAPHICS_BOX* box)
{
	GRAPHICS_BOX b;

	if(antialias == GRAPHICS_ANTIALIAS_NONE)
	{
		b.point1.x = GRAPHICS_FIXED_ROUND_DOWN(box->point1.x);
		b.point1.y = GRAPHICS_FIXED_ROUND_DOWN(box->point1.y);
		b.point2.x = GRAPHICS_FIXED_ROUND_DOWN(box->point2.x);
		b.point2.y = GRAPHICS_FIXED_ROUND_DOWN(box->point2.y);
		box = &b;
	}

	if(box->point1.y == box->point2.y)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(box->point1.x == box->point2.x)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(boxes->num_limits != 0)
	{
		GRAPHICS_POINT point1, point2;
		int reversed = FALSE;
		int n;

		if(box->point1.x < box->point2.x)
		{
			point1.x = box->point1.x;
			point2.x = box->point2.x;
		}
		else
		{
			point2.x = box->point1.x;
			point1.x = box->point2.x;
			reversed = TRUE;
		}

		if(point1.x >= boxes->limit.point2.x || point2.x <= boxes->limit.point1.x)
		{
			return GRAPHICS_STATUS_SUCCESS;
		}

		if(box->point1.y < box->point2.y)
		{
			point1.y = box->point1.y;
			point2.y = box->point2.y;
		}
		else
		{
			point2.y = box->point1.y;
			point1.y = box->point2.y;
			reversed = ! reversed;
		}

		if(point1.y >= boxes->limit.point2.y || point2.y <= boxes->limit.point1.y)
		{
			return GRAPHICS_STATUS_SUCCESS;
		}

		for(n=0; n<boxes->num_limits; n++)
		{
			const GRAPHICS_BOX *limits = &boxes->limits[n];
			GRAPHICS_BOX _box;
			GRAPHICS_POINT p1, p2;

			if(point1.x >= limits->point2.x || point2.x <= limits->point1.x)
			{
				continue;
			}
			if(point1.y >= limits->point2.y || point2.y <= limits->point1.y)
			{
				continue;
			}

			p1 = point1;
			if(p1.x < limits->point1.x)
			{
				p1.x = limits->point1.x;
			}
			if(p1.y < limits->point1.y)
			{
				p1.y = limits->point1.y;
			}

			p2 = point2;
			if(p2.x > limits->point2.x)
			{
				p2.x = limits->point2.x;
			}
			if(p2.y > limits->point2.y)
			{
				p2.y = limits->point2.y;
			}

			if(p2.y <= p1.y || p2.x <= p1.x)
			{
				continue;
			}

			_box.point1.y = p1.y;
			_box.point2.y = p2.y;
			if(reversed)
			{
				_box.point1.x = p2.x;
				_box.point2.x = p1.x;
			}
			else
			{
				_box.point1.x = p1.x;
				_box.point2.x = p2.x;
			}

			GraphicsBoxesAddInternal(boxes, &_box);
		}
	}
	else
	{
		GraphicsBoxesAddInternal(boxes, box);
	}

	return boxes->status;
}

void GraphicsBoxesClear(GRAPHICS_BOXES* boxes)
{
	struct _GRAPHICS_BOXES_CHUNK *chunk, *next;

	for(chunk=boxes->chunks.next; chunk!=NULL; chunk = next)
	{
		next = chunk->next;
		MEM_FREE_FUNC(chunk);
	}

	boxes->tail = &boxes->chunks;
	boxes->chunks.next = 0;
	boxes->chunks.count = 0;
	boxes->chunks.base = boxes->boxes_embedded;
	boxes->chunks.size = sizeof(boxes->boxes_embedded) / sizeof(boxes->boxes_embedded[0]);
	boxes->num_boxes = 0;

	boxes->is_pixel_aligned = TRUE;
}

void GraphicsBoxesExtents(const GRAPHICS_BOXES* boxes, GRAPHICS_BOX* box)
{
	const struct _GRAPHICS_BOXES_CHUNK *chunk;
	GRAPHICS_BOX b;
	int i;

	if(boxes->num_boxes == 0)
	{
		box->point1.x = box->point1.y = box->point2.x = box->point2.y = 0;
		return;
	}

	b = boxes->chunks.base[0];
	for(chunk=&boxes->chunks; chunk!=NULL; chunk=chunk->next)
	{
		for(i=0; i<chunk->count; i++)
		{
			if(chunk->base[i].point1.x < b.point1.x)
			{
				b.point1.x = chunk->base[i].point1.x;
			}

			if(chunk->base[i].point1.y < b.point1.y)
			{
				b.point1.y = chunk->base[i].point1.y;
			}

			if(chunk->base[i].point2.x > b.point2.x)
			{
				b.point2.x = chunk->base[i].point2.x;
			}

			if(chunk->base[i].point2.y > b.point2.y)
			{
				b.point2.y = chunk->base[i].point2.y;
			}
		}
	}
	*box = b;
}

void GraphicsBoxesLimit(GRAPHICS_BOXES* boxes, const GRAPHICS_BOX* limits, int num_limits)
{
	int n;

	boxes->limits = limits;
	boxes->num_limits = num_limits;

	if(boxes->num_limits != 0)
	{
		boxes->limit = limits[0];
		for(n=1; n<num_limits; n++)
		{
			if(limits[n].point1.x < boxes->limit.point1.x)
			{
				boxes->limit.point1.x = limits[n].point1.x;
			}

			if(limits[n].point1.y < boxes->limit.point1.y)
			{
				boxes->limit.point1.y = limits[n].point1.y;
			}

			if(limits[n].point2.x > boxes->limit.point2.x)
			{
				boxes->limit.point2.x = limits[n].point2.x;
			}

			if(limits[n].point2.y > boxes->limit.point2.y)
			{
				boxes->limit.point2.y = limits[n].point2.y;
			}
		}
	}
}

static INLINE int RectangleCompareStart(const RECTANGLE* a, const RECTANGLE* b)
{
	return a->top - b->top;
}

static INLINE int RectangleCompareStop(const RECTANGLE* a, const RECTANGLE* b)
{
	return a->bottom - b->bottom;
}

static INLINE RECTANGLE* RectanglePopStart(SWEEP_LINE* sweep_line)
{
	return *sweep_line->rectangles++;
}

static INLINE RECTANGLE* RectanglePeekStop(SWEEP_LINE* sweep_line)
{
	return sweep_line->point_queue.elements[POINT_QUEUE_FIRST_ENTRY];
}

static INLINE int IsZero(const int* winding)
{
	return winding[0] == 0 || winding[1] == 0;
}

static void EndBox(
	SWEEP_LINE* sweep_line,
	EDGE* left,
	int32 bottom,
	GRAPHICS_BOXES* out
)
{
	if(LIKELY(left->top < bottom))
	{
		eGRAPHICS_STATUS status;
		GRAPHICS_BOX box;

		box.point1.x = left->x;
		box.point1.y = left->top;
		box.point2.x = left->right->x;
		box.point2.y = bottom;

		status = GraphicsBoxesAdd(out, GRAPHICS_ANTIALIAS_DEFAULT, &box);
		if(UNLIKELY(status))
		{
			longjmp(sweep_line->unwind, status);
		}
	}
	left->right = NULL;
}

static INLINE void StartOrContinueBox(
	SWEEP_LINE* sweep_line,
	EDGE* left,
	EDGE* right,
	int top,
	GRAPHICS_BOXES* out
)
{
	if(left->right == right)
	{
		return;
	}

	if(left->right != NULL)
	{
		if(right != NULL && left->right->x == right->x)
		{
			left->right = right;
			return;
		}
		EndBox(sweep_line, left, top, out);
	}

	if(right != NULL && left->x != right->x)
	{
		left->top = top;
		left->right = right;
	}
}

static INLINE void ActiveEdges(SWEEP_LINE* sweep, GRAPHICS_BOXES* out)
{
	int top = sweep->current_y;
	int winding[2] = {0};
	EDGE *position;

	if(sweep->last_y == sweep->current_y)
	{
		return;
	}

	position = sweep->head.next;
	if(position == &sweep->tail)
	{
		return;
	}

	do
	{
		EDGE *left, *right;

		left = position;
		do
		{
			winding[left->a_or_b] += left->direction;
			if(FALSE == IsZero(winding))
			{
				break;
			}
			if(left->next == &sweep->tail)
			{
				goto out;
			}

			if(UNLIKELY(left->right != NULL))
			{
				EndBox(sweep, left, top, out);
			}

			left = left->next;
		} while(1);

		right = left->next;
		do
		{
			if(UNLIKELY(right->right != NULL))
			{
				EndBox(sweep, right, top, out);
			}

			winding[right->a_or_b] += right->direction;
			if(IsZero(winding))
			{
				if(LIKELY(right->x != right->next->x))
				{
					break;
				}
			}

			right = right->next;
		} while(1);

		StartOrContinueBox(sweep, left, right, top, out);

		position = right->next;
	} while(position != &sweep->tail);

out:
	sweep->last_y = sweep->current_y;
}

static INLINE void InitializePointQueue(POINT_QUEUE* point_queue)
{
	point_queue->max_size = sizeof(point_queue->elements_embedded) / sizeof(point_queue->elements_embedded[0]);
	point_queue->size = 0;

	point_queue->elements = point_queue->elements_embedded;
	point_queue->elements[POINT_QUEUE_FIRST_ENTRY] = NULL;
}

static int PointQueueGrow(POINT_QUEUE* point_queue)
{
	RECTANGLE **new_elements;
	point_queue->max_size *= 2;

	if(point_queue->elements == point_queue->elements_embedded)
	{
		new_elements = (RECTANGLE**)MEM_ALLOC_FUNC(point_queue->max_size * sizeof(RECTANGLE*));
		if(UNLIKELY(new_elements == NULL))
		{
			return FALSE;
		}

		(void)memcpy(new_elements, point_queue->elements_embedded, sizeof(point_queue->elements_embedded));
	}
	else
	{
		new_elements = (RECTANGLE**)MEM_REALLOC_FUNC(point_queue->elements, sizeof(RECTANGLE*) * point_queue->max_size);
		if(UNLIKELY(new_elements == NULL))
		{
			return FALSE;
		}
	}

	point_queue->elements = new_elements;
	return TRUE;
}

static INLINE void PointQueuePush(SWEEP_LINE* sweep, RECTANGLE* rectangle)
{
	RECTANGLE **elements;
	int i, parent;

	if(UNLIKELY(sweep->point_queue.size + 1 == sweep->point_queue.max_size))
	{
		if(UNLIKELY(FALSE == PointQueueGrow(&sweep->point_queue)))
		{
			longjmp(sweep->unwind, GRAPHICS_INTEGER_STATUS_NO_MEMORY);
		}
	}

	elements = sweep->point_queue.elements;
	for(i = ++sweep->point_queue.size;
		i != POINT_QUEUE_FIRST_ENTRY && RectangleCompareStop(rectangle, elements[parent = POINT_QUEUE_PARENT_INDEX(i)]) < 0;
		i = parent
	)
	{
		elements[i] = elements[parent];
	}

	elements[i] = rectangle;
}

static INLINE void PointQueuePop(POINT_QUEUE* point_queue)
{
	RECTANGLE **elements = point_queue->elements;
	RECTANGLE *tail;
	int child, i;

	tail = elements[point_queue->size--];
	if(point_queue->size == 0)
	{
		elements[POINT_QUEUE_FIRST_ENTRY] = NULL;
		return;
	}

	for(i=POINT_QUEUE_FIRST_ENTRY; (child = POINT_QUEUE_LEFT_CHILD_INDEX(i)) <= point_queue->size; i = child)
	{
		if(child != point_queue->size && RectangleCompareStop(elements[child+1], elements[child]) < 0)
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

static INLINE void PointQueueFinish(POINT_QUEUE* point_queue)
{
	if(point_queue->elements != point_queue->elements_embedded)
	{
		MEM_FREE_FUNC(point_queue->elements);
	}
}

static void SortRectangles(RECTANGLE** rectangles, unsigned int num)
{
	unsigned int gap = num;
	unsigned int i, j;
	int swapped;
	do
	{
		gap = GraphicsCombsortNewGap(gap);
		swapped = gap > 1;
		for(i=0; i<num-gap; i++)
		{
			j = i + gap;
			if(RectangleCompareStart(rectangles[i], rectangles[j]) > 0)
			{
				RECTANGLE *temp;
				temp = rectangles[i];
				rectangles[i] = rectangles[j];
				rectangles[j] = temp;
				swapped = 1;
			}
		}
	} while(swapped);
}

static void InitializeSweepLine(
	SWEEP_LINE* sweep_line,
	RECTANGLE** rectangles,
	int num_rectangles
)
{
	SortRectangles(rectangles, num_rectangles);
	rectangles[num_rectangles] = NULL;
	sweep_line->rectangles = rectangles;

	sweep_line->head.x = INT32_MIN;
	sweep_line->head.right = NULL;
	sweep_line->head.direction = 0;
	sweep_line->head.next = &sweep_line->tail;
	sweep_line->tail.x = INT32_MAX;
	sweep_line->tail.right = NULL;
	sweep_line->tail.direction = 0;
	sweep_line->tail.previous = &sweep_line->head;

	sweep_line->insert_left = &sweep_line->tail;
	sweep_line->insert_right = &sweep_line->tail;

	sweep_line->current_y = INT32_MIN;
	sweep_line->last_y = INT32_MIN;

	InitializePointQueue(&sweep_line->point_queue);
}

static INLINE void DeleteSweepLineEdge(
	SWEEP_LINE* sweep_line,
	EDGE* edge,
	GRAPHICS_BOXES* out
)
{
	if(edge->right != NULL)
	{
		EDGE *next = edge->next;
		if(next->x == edge->x)
		{
			next->top = edge->top;
			next->right = edge->right;
		}
		else
		{
			EndBox(sweep_line, edge, sweep_line->current_y, out);
		}
	}

	if(sweep_line->insert_left == edge)
	{
		sweep_line->insert_left = edge->next;
	}
	if(sweep_line->insert_right == edge)
	{
		sweep_line->insert_right = edge->next;
	}

	edge->previous->next = edge->next;
	edge->next->previous = edge->previous;
}

static INLINE void InsertEdge(EDGE* edge, EDGE* position)
{
	if(position->x != edge->x)
	{
		if(position->x > edge->x)
		{
			do
			{
				if(position->previous->x <= edge->x)
				{
					break;
				}
				position = position->previous;
			} while(1);
		}
		else
		{
			do
			{
				position = position->next;
				if(position->x >= edge->x)
				{
					break;
				}
			} while(1);
		}
	}

	position->previous->next = edge;
	edge->previous = position->previous;
	edge->next = position;
	position->previous = edge;
}

static INLINE void SweepLineInsert(SWEEP_LINE* sweep, RECTANGLE* rectangle)
{
	EDGE *position;

	position = sweep->insert_right;
	InsertEdge(&rectangle->right, position);
	sweep->insert_right = &rectangle->right;

	position = sweep->insert_left;
	if(position->x > sweep->insert_right->x)
	{
		position = sweep->insert_right->previous;
	}
	InsertEdge(&rectangle->left, position);
	sweep->insert_left = &rectangle->left;

	PointQueuePush(sweep, rectangle);
}

static INLINE void DeleteSweepLine(
	SWEEP_LINE* sweep,
	RECTANGLE* rectangle,
	GRAPHICS_BOXES* out
)
{
	DeleteSweepLineEdge(sweep, &rectangle->left, out);
	DeleteSweepLineEdge(sweep, &rectangle->right, out);

	PointQueuePop(&sweep->point_queue);
}

static INLINE void SweepLineFinish(SWEEP_LINE* sweep_line)
{
	PointQueueFinish(&sweep_line->point_queue);
}

static eGRAPHICS_STATUS Intersect(RECTANGLE** rectangles, int num_rectangles, GRAPHICS_BOXES* out)
{
	SWEEP_LINE sweep_line;
	RECTANGLE *rectangle;
	eGRAPHICS_STATUS status;

	InitializeSweepLine(&sweep_line, rectangles, num_rectangles);
	if((status = setjmp(sweep_line.unwind)))
	{
		goto unwind;
	}

	rectangle = RectanglePopStart(&sweep_line);
	do
	{
		if(rectangle->top != sweep_line.current_y)
		{
			RECTANGLE *stop;

			stop = RectanglePeekStop(&sweep_line);
			while(stop != NULL && stop->bottom < rectangle->top)
			{
				if(stop->bottom != sweep_line.current_y)
				{
					ActiveEdges(&sweep_line, out);
					sweep_line.current_y = stop->bottom;
				}

				DeleteSweepLine(&sweep_line, stop, out);

				stop = RectanglePeekStop(&sweep_line);
			}

			ActiveEdges(&sweep_line, out);
			sweep_line.current_y = rectangle->top;
		}

		SweepLineInsert(&sweep_line, rectangle);
	} while((rectangle = RectanglePopStart(&sweep_line)) != NULL);

	while((rectangle = RectanglePeekStop(&sweep_line)) != NULL)
	{
		if(rectangle->bottom != sweep_line.current_y)
		{
			ActiveEdges(&sweep_line, out);
			sweep_line.current_y = rectangle->bottom;
		}

		DeleteSweepLine(&sweep_line, rectangle, out);
	}
unwind:
	SweepLineFinish(&sweep_line);
	return status;
}

static eGRAPHICS_STATUS GraphicsBoxesIntersectWithBox(const GRAPHICS_BOXES* boxes, const GRAPHICS_BOX* box, GRAPHICS_BOXES* out)
{
	eGRAPHICS_STATUS status;
	int i, j;

	if(out == boxes)
	{
		struct _GRAPHICS_BOXES_CHUNK *chunk;

		out->num_boxes = 0;
		for(chunk=&out->chunks; chunk!=NULL; chunk=chunk->next)
		{
			for(i=j=0; i<chunk->count; i++)
			{
				GRAPHICS_BOX *b = &chunk->base[i];

				b->point1.x = MAXIMUM(b->point1.x, box->point1.x);
				b->point1.y = MAXIMUM(b->point1.y, box->point1.y);
				b->point2.x = MINIMUM(b->point2.x, box->point2.x);
				b->point2.x = MINIMUM(b->point2.x, box->point2.x);
				if(b->point1.x < b->point2.x && b->point1.y < b->point2.y)
				{
					if(i != j)
					{
						chunk->base[j] = *b;
					}
					j++;
				}
			}

			chunk->count = j;
			out->num_boxes += j;
		}
	}
	else
	{
		const struct _GRAPHICS_BOXES_CHUNK *chunk;

		GraphicsBoxesClear(out);
		GraphicsBoxesLimit(out, box, 1);
		for(chunk=&boxes->chunks; chunk!=NULL; chunk=chunk->next)
		{
			for(i=0; i<chunk->count; i++)
			{
				status = GraphicsBoxesAdd(out, GRAPHICS_ANTIALIAS_DEFAULT, &chunk->base[i]);
				if(UNLIKELY(status))
				{
					return status;
				}
			}
		}
	}

	return GRAPHICS_STATUS_SUCCESS;
}

eGRAPHICS_STATUS GraphicsBoxesIntersect(const GRAPHICS_BOXES* a, const GRAPHICS_BOXES* b, GRAPHICS_BOXES* out)
{
	RECTANGLE stack_rectangles[STACK_BUFFER_SIZE/sizeof(RECTANGLE)];
	RECTANGLE *rectangles;
	RECTANGLE *stack_rectangles_pointers[sizeof(stack_rectangles)/sizeof(stack_rectangles[0])+  1];
	RECTANGLE **rectangles_pointers;
	const struct _GRAPHICS_BOXES_CHUNK *chunk;
	eGRAPHICS_STATUS status;
	int i, j, count;

	if(UNLIKELY(a->num_boxes == 0 || b->num_boxes == 0))
	{
		GraphicsBoxesClear(out);
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(a->num_boxes == 1)
	{
		GRAPHICS_BOX box = a->chunks.base[0];
		return GraphicsBoxesIntersectWithBox(b, &box, out);
	}
	if(b->num_boxes == 1)
	{
		GRAPHICS_BOX box = b->chunks.base[0];
		return GraphicsBoxesIntersectWithBox(a, &box, out);
	}

	rectangles = stack_rectangles;
	rectangles_pointers = stack_rectangles_pointers;
	count = a->num_boxes + b->num_boxes;
	if(count > sizeof(stack_rectangles) / sizeof(stack_rectangles[0]))
	{
		rectangles = (RECTANGLE*)MEM_ALLOC_FUNC(
									(sizeof(RECTANGLE) + sizeof(RECTANGLE*)) * count + sizeof(RECTANGLE*));
		if(rectangles == NULL)
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}

		rectangles_pointers = (RECTANGLE**)(rectangles + count);
	}

	j=0;
	for(chunk=&a->chunks; chunk!=NULL; chunk=chunk->next)
	{
		const GRAPHICS_BOX *box = chunk->base;
		for(i=0; i<chunk->count; i++)
		{
			if(box[i].point1.x < box[i].point2.x)
			{
				rectangles[j].left.x = box[i].point1.x;
				rectangles[j].left.direction = 1;

				rectangles[j].right.x = box[i].point2.x;
				rectangles[j].right.direction = -1;
			}
			else
			{
				rectangles[j].right.x = box[i].point1.x;
				rectangles[j].right.direction = 1;

				rectangles[j].left.x = box[i].point2.x;
				rectangles[j].left.direction = -1;
			}

			rectangles[j].left.a_or_b = 0;
			rectangles[j].left.right = NULL;
			rectangles[j].right.a_or_b = 0;
			rectangles[j].right.right = NULL;

			rectangles[j].top = box[i].point1.y;
			rectangles[j].bottom = box[i].point2.y;

			rectangles_pointers[j] = &rectangles[j];
			j++;
		}
	}
	for(chunk=&b->chunks; chunk!=NULL; chunk=chunk->next)
	{
		const GRAPHICS_BOX *box = chunk->base;
		for(i=0; i<chunk->count; i++)
		{
			if(box[i].point1.x < box[i].point2.x)
			{
				rectangles[j].left.x = box[i].point1.x;
				rectangles[j].left.direction = 1;

				rectangles[j].right.x = box[i].point2.x;
				rectangles[j].right.direction = -1;
			}
			else
			{
				rectangles[j].right.x = box[i].point1.x;
				rectangles[j].right.direction = 1;

				rectangles[j].left.x = box[i].point2.x;
				rectangles[j].left.direction = -1;
			}

			rectangles[j].left.a_or_b = 1;
			rectangles[j].left.right  = NULL;
			rectangles[j].right.a_or_b = 1;
			rectangles[j].right.right = NULL;

			rectangles[j].top = box[i].point1.y;
			rectangles[j].bottom = box[i].point2.y;

			rectangles_pointers[j] = &rectangles[j];
			j++;
		}
	}
	ASSERT(j == count);

	GraphicsBoxesClear(out);
	status = Intersect(rectangles_pointers, j, out);
	if(rectangles != stack_rectangles)
	{
		MEM_FREE_FUNC(rectangles);
	}

	return status;
}

GRAPHICS_BOX* GraphicsBoxesToArray(const GRAPHICS_BOXES* boxes, int* num_boxes)
{
	const struct _GRAPHICS_BOXES_CHUNK *chunk;
	GRAPHICS_BOX *box;
	int i, j;

	*num_boxes = boxes->num_boxes;

	box = (GRAPHICS_BOX*)MEM_ALLOC_FUNC(boxes->num_boxes * sizeof(GRAPHICS_BOX));
	if(box == NULL)
	{
		return NULL;
	}

	j = 0;
	for(chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next)
	{
		for(i=0; i<chunk->count; i++)
		{
			box[j++] = chunk->base[i];
		}
	}

	return box;
}

int GraphicsBoxesCopyToClip(const GRAPHICS_BOXES* boxes, struct _GRAPHICS_CLIP* clip)
{
	if(boxes->num_boxes == 1)
	{
		clip->boxes = &clip->embedded_box;
		clip->boxes[0] = boxes->chunks.base[0];
		clip->num_boxes = 1;
		return TRUE;
	}

	clip->boxes = GraphicsBoxesToArray(boxes, &clip->num_boxes);
	if(UNLIKELY(clip->boxes == NULL))
	{
		clip->clip_all = TRUE;
		return FALSE;
	}

	return TRUE;
}

typedef struct _GRAPHICS_BOX_RENDERER
{
	GRAPHICS_SPAN_RENDERER base;
	GRAPHICS_BOXES *boxes;
} GRAPHICS_BOX_RENDERER;

static eGRAPHICS_STATUS SpanToBoxes(
	void* abstract_renderer,
	int y,
	int h,
	const GRAPHICS_HALF_OPEN_SPAN* spans,
	unsigned num_spans
)
{
	GRAPHICS_BOX_RENDERER *r = (GRAPHICS_BOX_RENDERER*)abstract_renderer;
	eGRAPHICS_STATUS status = GRAPHICS_STATUS_SUCCESS;
	GRAPHICS_BOX box;

	if(num_spans == 0)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	box.point1.y = GraphicsFixedFromInteger(y);
	box.point2.y = GraphicsFixedFromInteger(y + h);
	do
	{
		if(spans[0].coverage)
		{
			box.point1.x = GraphicsFixedFromInteger(spans[0].x);
			box.point2.x = GraphicsFixedFromInteger(spans[1].x);
			status = GraphicsBoxesAdd(r->boxes, GRAPHICS_ANTIALIAS_DEFAULT, &box);
		}
		spans++;
	} while(--num_spans > 1 && status == GRAPHICS_STATUS_SUCCESS);

	return status;
}

eGRAPHICS_STATUS GraphicsRasterisePolygonToBoxes(
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	GRAPHICS_BOXES* boxes
)
{
	GRAPHICS_BOX_RENDERER renderer;
	GRAPHICS_MONO_SCAN_CONVERTER converter;
	eGRAPHICS_INTEGER_STATUS status;
	GRAPHICS_RECTANGLE_INT r;

	GraphicsBoxRoundToRectangle(&polygon->extents, &r);
	(void)InitializeGraphicsMonoScanConverter(&converter, r.x, r.y, r.x + r.width,
											r.y + r.height, fill_rule);
	status = GraphicsMonoScanConvereterAddPolygon(&converter, polygon);
	if(UNLIKELY(status))
	{
		goto cleanup_converter;
	}

	renderer.boxes = boxes;
	renderer.base.render_rows = SpanToBoxes;

	status = converter.base.generate(&converter, &renderer.base);
cleanup_converter:
	converter.base.destroy(&converter);
	return status;
}

#ifdef __cplusplus
}
#endif
