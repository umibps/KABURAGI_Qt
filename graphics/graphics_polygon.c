#include <string.h>
#include <setjmp.h>
#include "graphics_types.h"
#include "graphics_list.h"
#include "graphics_memory_pool.h"
#include "graphics_region.h"
#include "graphics_matrix.h"
#include "graphics_private.h"
#include "graphics_scan_converter_private.h"
#include "../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define POINT_QUEUE_FIRST_ENTRY 1
#define POINT_QUEUE_PARENT_INDEX(i) ((i) >> 1)
#define POINT_QUEUE_LEFT_CHILD_INDEX(i) ((i) << 1)

typedef struct _EDGE
{
	struct _EDGE *next, *previous;
	struct _EDGE *right;
	GRAPHICS_FLOAT_FIXED x, top;
	int direction;
} EDGE;

typedef struct _RECTANGLE
{
	EDGE left, right;
	int32 top, bottom;
} RECTANGLE;

typedef struct _SWEEP_LINE
{
	RECTANGLE **rectangles;
	RECTANGLE **stop;
	EDGE head, tail, *insert, *cursor;
	int32 current_y;
	int32 last_y;
	int stop_size;

	int32 insert_x;
	eGRAPHICS_FILL_RULE fill_rule;

	int do_traps;
	void *container;

	jmp_buf unwind;
} SWEEP_LINE;

static void ActiveEdgesInsert(SWEEP_LINE* sweep);
static void EdgeEndBox(SWEEP_LINE* sweep_line, EDGE* left, int32 bottom);

static INLINE void EdgeStartOrContinueBox(
	SWEEP_LINE* sweep_line,
	EDGE* left,
	EDGE* right,
	int top
)
{
	if(left->right == right)
	{
		return;
	}

	if(left->right != NULL)
	{
		if(left->right->x == right->x)
		{
			left->right = right;
			return;
		}

		EdgeEndBox(sweep_line, left, top);
	}

	if(left->x != right->x)
	{
		left->top = top;
		left->right = right;
	}
}

static void AddEdge(
	GRAPHICS_POLYGON* polygon,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	int top,
	int bottom,
	int direction
)
{
	GRAPHICS_EDGE *edge;

	ASSERT(top < bottom);

	if(UNLIKELY(polygon->num_edges == polygon->edges_size))
	{
		if(FALSE == GraphicsPolygonGrow(polygon))
		{
			return;
		}
	}

	edge = &polygon->edges[polygon->num_edges++];
	edge->line.point1 = *point1;
	edge->line.point2 = *point2;
	edge->top = top;
	edge->bottom = bottom;
	edge->direction = direction;

	if(top < polygon->extents.point1.y)
	{
		polygon->extents.point1.y = top;
	}
	if(bottom > polygon->extents.point2.y)
	{
		polygon->extents.point2.y = bottom;
	}

	if(point1->x < polygon->extents.point1.x || point1->x > polygon->extents.point2.x)
	{
		GRAPHICS_FLOAT_FIXED x = point1->x;
		if(top != point1->y)
		{
			x = GraphicsEdgeComputeIntersectionXforY(point1, point2, top);
		}
		if(x < polygon->extents.point1.x)
		{
			polygon->extents.point1.x = x;
		}
		if(x > polygon->extents.point2.x)
		{
			polygon->extents.point2.x = x;
		}
	}

	if(point2->x < polygon->extents.point1.x || point2->x > polygon->extents.point2.x)
	{
		GRAPHICS_FLOAT_FIXED x = point2->x;
		if(bottom != point2->y)
		{
			x = GraphicsEdgeComputeIntersectionXforY(point1, point2, bottom);
		}
		if(x < polygon->extents.point1.x)
		{
			polygon->extents.point1.x = x;
		}
		if(x > polygon->extents.point2.x)
		{
			polygon->extents.point2.x = x;
		}
	}
}

static void AddClippedEdge(
	GRAPHICS_POLYGON* polygon,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	const int top,
	const int bottom,
	const int direction
)
{
	GRAPHICS_POINT bottom_left, top_right;
	GRAPHICS_FLOAT_FIXED top_y, bottom_y;
	int n;

	for(n = 0; n < polygon->num_limits; n++)
	{
		const GRAPHICS_BOX *limits = &polygon->limits[n];
		GRAPHICS_FLOAT_FIXED p_left, p_right;

		if(top >= limits->point2.y)
		{
			continue;
		}

		if(bottom <= limits->point1.y)
		{
			continue;
		}

		bottom_left.x = limits->point1.x;
		bottom_left.y = limits->point2.y;

		top_right.x = limits->point2.x;
		top_right.y = limits->point1.y;

		top_y = MAXIMUM(top, limits->point1.y);
		bottom_y = MINIMUM(bottom, limits->point2.y);

		p_left = MINIMUM(point1->x, point2->x);
		p_right = MAXIMUM(point1->x, point2->x);

		if(limits->point1.x <= p_left && p_right <= limits->point2.x)
		{
			AddEdge(polygon, point1, point2, top_y, bottom_y, direction);
		}
		else if(p_right <= limits->point1.x)
		{
			AddEdge(polygon, &limits->point1, &bottom_left, top_y, bottom_y, direction);
		}
		else if(limits->point2.x <= p_left)
		{
			AddEdge(polygon, &top_right, &limits->point2, top_y, bottom_y, direction);
		}
		else
		{
			GRAPHICS_FLOAT_FIXED left_y, right_y;
			int top_left_to_bottom_right;

			top_left_to_bottom_right = (point1->x <= point2->x) == (point1->y <= point2->y);
			if(top_left_to_bottom_right)
			{
				if(p_left >= limits->point1.x)
				{
					left_y = top_y;
				}
				else
				{
					left_y = GraphicsEdgeComputeIntersectionYforX(point1, point2, limits->point1.x);
					if(GraphicsEdgeComputeIntersectionXforY(point1, point2, left_y) < limits->point1.x)
					{
						left_y++;
					}
				}

				left_y = MINIMUM(left_y, bottom_y);
				if(top_y < left_y)
				{
					AddEdge(polygon, &limits->point1, &bottom_left,
						top_y, left_y, direction);
					top_y = left_y;
				}

				if(p_right <= limits->point2.x)
				{
					right_y = bottom_y;
				}
				else
				{
					right_y = GraphicsEdgeComputeIntersectionYforX(point1, point2, limits->point2.x);

					if(GraphicsEdgeComputeIntersectionXforY(point1, point2, right_y) > limits->point2.x)
					{
						right_y--;
					}
				}

				right_y = MAXIMUM(right_y, top_y);
				if(bottom_y > right_y)
				{
					AddEdge(polygon, &top_right, &limits->point2, right_y, bottom_y, direction);
					bottom_y = right_y;
				}
			}
			else
			{
				if(p_right <= limits->point2.x)
				{
					right_y = top_y;
				}
				else
				{
					right_y = GraphicsEdgeComputeIntersectionYforX(
						point1, point2, limits->point2.x);
					if(GraphicsEdgeComputeIntersectionXforY(point1, point2, right_y) > limits->point2.x)
					{
						right_y++;
					}
				}

				right_y = MINIMUM(right_y, bottom_y);
				if(top_y < right_y)
				{
					AddEdge(polygon, &top_right, &limits->point2, top_y, right_y, direction);
					top_y = right_y;
				}

				if(p_left >= limits->point1.x)
				{
					left_y = bottom_y;
				}
				else
				{
					left_y = GraphicsEdgeComputeIntersectionYforX(point1, point2, limits->point1.x);
					if(GraphicsEdgeComputeIntersectionXforY(point1, point2, left_y) < limits->point1.x)
					{
						left_y--;
					}
				}

				left_y = MAXIMUM(left_y, top_y);
				if(bottom_y > left_y)
				{
					AddEdge(polygon, &limits->point1, &bottom_left, left_y, bottom_y, direction);
					bottom_y = left_y;
				}
			}

			if(top_y != bottom_y)
			{
				AddEdge(polygon, point1, point2, top_y, bottom_y, direction);
			}
		}
	}
}


void ContourAddPoint(GRAPHICS_STROKER* stroker, GRAPHICS_CONTOUR* contour, const GRAPHICS_POINT* point)
{
	GraphicsContourAddPoint(contour, point);
}

static void AddFan(
	GRAPHICS_STROKER* stroker,
	const GRAPHICS_SLOPE* in_vector,
	const GRAPHICS_SLOPE* out_vector,
	const GRAPHICS_POINT* mid_point,
	int clockwise,
	GRAPHICS_CONTOUR* contour
)
{
	GRAPHICS_PEN *pen = &stroker->pen;
	int start, stop;

	if(stroker->has_bounds && FALSE == GraphicsBoxContainsPoint(&stroker->bounds, mid_point))
	{
		return;
	}

	ASSERT(stroker->pen.num_vertices);

	if(clockwise)
	{
		GraphicsPenFindActiveCounterClockwiseVertices(pen, in_vector, out_vector, &start, &stop);
		while(start != stop)
		{
			GRAPHICS_POINT p = *mid_point;
			TranslatePoint(&p, &pen->vertices[start].point);
			ContourAddPoint(stroker, contour, &p);

			if(++start == pen->num_vertices)
			{
				start = 0;
			}
		}
	}
	else
	{
		GraphicsPenFindActiveCounterClockwiseVertices(pen, in_vector, out_vector, &start, &stop);
		while(start != stop)
		{
			GRAPHICS_POINT p = *mid_point;
			TranslatePoint(&p, &pen->vertices[start].point);
			ContourAddPoint(stroker, contour, &p);

			if(start-- == 0)
			{
				start += pen->num_vertices;
			}
		}
	}
}

static void AddCap(GRAPHICS_STROKER* stroker, const GRAPHICS_STROKE_FACE* face, GRAPHICS_CONTOUR* contour)
{
	switch(stroker->style.line_cap)
	{
	case GRAPHICS_LINE_CAP_ROUND:
		{
			GRAPHICS_SLOPE slope;
			slope.dx = - face->device_vector.dx;
			slope.dy = - face->device_vector.dy;

			AddFan(stroker, &face->device_vector, &slope, &face->point, FALSE, contour);
		}
		break;
	case GRAPHICS_LINE_CAP_SQUARE:
		{
			GRAPHICS_SLOPE face_vector;
			GRAPHICS_POINT p;
			FLOAT_T dx, dy;

			dx = face->user_vector.x;
			dy = face->user_vector.y;
			dx *= stroker->half_line_width;
			dy *= stroker->half_line_width;
			GraphicsMatrixTransformDistance(stroker->matrix, &dx, &dy);
			face_vector.dx = GraphicsFixedFromFloat(dx);
			face_vector.dy = GraphicsFixedFromFloat(dy);

			p.x = face->counter_clock_wise.x + face_vector.dx;
			p.y = face->counter_clock_wise.y + face_vector.dy;
			ContourAddPoint(stroker, contour, &p);

			p.x = face->clock_wise.x + face_vector.dx;
			p.y = face->clock_wise.y + face_vector.dy;
			ContourAddPoint(stroker, contour, &p);
		}
		break;
	case GRAPHICS_LINE_CAP_BUTT:
	default:
		break;
	}
	ContourAddPoint(stroker, contour, &face->clock_wise);
}

static void AddLeadingCap(GRAPHICS_STROKER* stroker, const GRAPHICS_STROKE_FACE* face, GRAPHICS_CONTOUR* contour)
{
	GRAPHICS_STROKE_FACE reversed;
	GRAPHICS_POINT t;

	reversed = *face;

	reversed.user_vector.x = - reversed.user_vector.x;
	reversed.user_vector.y = - reversed.user_vector.y;
	reversed.device_vector.dx = - reversed.device_vector.dx;
	reversed.device_vector.dy = - reversed.device_vector.dy;

	t = reversed.clock_wise;
	reversed.clock_wise = reversed.counter_clock_wise;
	reversed.counter_clock_wise = t;

	AddCap(stroker, &reversed, contour);
}

eGRAPHICS_STATUS GraphicsPolygonAddLine(
	GRAPHICS_POLYGON* polygon,
	const GRAPHICS_LINE* line,
	int top,
	int bottom,
	int direction
)
{
	if(line->point1.y == line->point2.y)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(bottom <= top)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(polygon->num_limits != 0)
	{
		if(line->point2.y <= polygon->limit.point1.y)
		{
			return GRAPHICS_STATUS_SUCCESS;
		}

		if(line->point1.y >= polygon->limit.point2.y)
		{
			return GRAPHICS_STATUS_SUCCESS;
		}

		AddClippedEdge(polygon, &line->point1, &line->point2, top, bottom, direction);
	}
	else
	{
		AddEdge(polygon, &line->point1, &line->point2, top, bottom, direction);
	}

	return polygon->status;
}

void GraphicsPolygonAddEdge(
	GRAPHICS_POLYGON* polygon,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2,
	int direction
)
{
	if(point1->y == point2->y)
	{
		return;
	}

	if(point1->y > point2->y)
	{
		const GRAPHICS_POINT *t;
		t = point1, point1 = point2, point2 = t;
		direction = - direction;
	}

	if(polygon->num_limits != 0)
	{
		if(point2->y <= polygon->limit.point1.y)
		{
			return;
		}

		if(point1->y >= polygon->limit.point2.y)
		{
			return;
		}

		AddClippedEdge(polygon, point1, point2, point1->y, point2->y, direction);
	}
	else
	{
		AddEdge(polygon, point1, point2, point1->y, point2->y, direction);
	}
}

eGRAPHICS_STATUS GraphicsPolygonAddExternalEdge(
	GRAPHICS_POLYGON* polygon,
	const GRAPHICS_POINT* point1,
	const GRAPHICS_POINT* point2
)
{
	GraphicsPolygonAddEdge(polygon, point1, point2, 1);
	return polygon->status;
}

eGRAPHICS_STATUS GraphicsPolygonAddContour(GRAPHICS_POLYGON* polygon, const GRAPHICS_CONTOUR* contour)
{
	const GRAPHICS_CONTOUR_CHAIN *chain;
	const GRAPHICS_POINT *previous = NULL;
	int i;

	if(contour->chain.num_points <= 1)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	previous = &contour->chain.points[0];
	for(chain = &contour->chain; chain != NULL; chain = chain->next)
	{
		for(i=0; i<chain->num_points; i++)
		{
			GraphicsPolygonAddEdge(polygon, previous, &chain->points[i], contour->direction);
			previous = &chain->points[i];
		}
	}

	return polygon->status;
}

typedef enum _eGRAPHICS_BO_EVENT_TYPE
{
	GRAPHICS_BO_EVENT_TYPE_STOP,
	GRAPHICS_BO_EVENT_TYPE_INTERSECTION,
	GRAPHICS_BO_EVENT_TYPE_START
} eGRAPHICS_BO_EVENT_TYPE;

typedef struct _GRAPHICS_BO_TRAP
{
	struct _GRAPHICS_BO_EDGE *right;
	int32 top;
} GRAPHICS_BO_TRAP;

typedef struct _GRAPHICS_BO_EDGE
{
	GRAPHICS_EDGE edge;
	struct _GRAPHICS_BO_EDGE *previous;
	struct _GRAPHICS_BO_EDGE *next;
	struct _GRAPHICS_BO_EDGE *colinear;
	GRAPHICS_BO_TRAP defferred_trap;
} GRAPHICS_BO_EDGE;

typedef struct _GRAPHICS_BO_EVENT
{
	eGRAPHICS_BO_EVENT_TYPE type;
	GRAPHICS_POINT point;
} GRAPHICS_BO_EVENT;

typedef struct _GRAPHICS_BO_START_EVENT
{
	eGRAPHICS_BO_EVENT_TYPE type;
	GRAPHICS_POINT point;
	GRAPHICS_BO_EDGE edge;
} GRAPHICS_BO_START_EVENT;

typedef struct _GRAPHICS_BO_QUEUE_EVENT
{
	eGRAPHICS_BO_EVENT_TYPE type;
	GRAPHICS_POINT point;
	GRAPHICS_BO_EDGE *edge1;
	GRAPHICS_BO_EDGE *edge2;
} GRAPHICS_BO_QUEUE_EVENT;

typedef struct _GRAPHICS_POINT_QUEUE
{
	int size, max_size;
	GRAPHICS_BO_EVENT **elements;
	GRAPHICS_BO_EVENT *elements_embedded[1024];
} GRAPHICS_POINT_QUEUE;

typedef struct _GRAPHICS_BO_EVENT_QUEUE
{
	GRAPHICS_MEMORY_POOL pool;
	GRAPHICS_POINT_QUEUE point_queue;
	GRAPHICS_BO_EVENT **start_events;
} GRAPHICS_BO_EVENT_QUEUE;

typedef struct _GRAPHICS_BO_SWEEP_LINE
{
	GRAPHICS_BO_EDGE *head;
	GRAPHICS_BO_EDGE *stopped;
	int32 current_y;
	GRAPHICS_BO_EDGE *current_edge;
} GRAPHICS_BO_SWEEP_LINE;

static GRAPHICS_FLOAT_FIXED LineComputeIntersectionXforY(const GRAPHICS_LINE* line, GRAPHICS_FLOAT_FIXED y)
{
	GRAPHICS_FLOAT_FIXED x, dy;

	if(y == line->point1.y)
	{
		return line->point1.x;
	}
	if(y == line->point2.y)
	{
		return  line->point2.x;
	}

	x = line->point1.x;
	dy = line->point2.y - line->point1.y;
	if(dy != 0)
	{
		x += GraphicsFixedMultiDivideFloor(y - line->point1.y,
			line->point2.x - line->point1.x, dy);
	}

	return x;
}

static INLINE int InitializeGraphicsPointQueue(GRAPHICS_POINT_QUEUE* queue)
{
	queue->max_size = sizeof(queue->elements_embedded) / sizeof(queue->elements_embedded[0]);
	queue->size = 0;

	queue->elements = queue->elements_embedded;

	return TRUE;
}

static INLINE void GraphicsPointQueueFinish(GRAPHICS_POINT_QUEUE* queue)
{
	if(queue->elements != queue->elements_embedded)
	{
		MEM_FREE_FUNC(queue->elements);
	}
}

static eGRAPHICS_STATUS GraphicsPointQueueGrow(GRAPHICS_POINT_QUEUE* queue)
{
	GRAPHICS_BO_EVENT **new_elements;
	queue->max_size *= 2;

	if(queue->elements == queue->elements_embedded)
	{
		new_elements = (GRAPHICS_BO_EVENT**)MEM_ALLOC_FUNC(
			sizeof(*new_elements) * queue->max_size);
		if(new_elements == NULL)
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}
		(void)memcpy(new_elements, queue->elements_embedded, sizeof(queue->elements_embedded));
	}
	else
	{
		new_elements = (GRAPHICS_BO_EVENT**)MEM_REALLOC_FUNC(queue->elements,
			sizeof(*new_elements) * queue->max_size);
		if(new_elements == NULL)
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}
	}

	queue->elements = new_elements;
	return GRAPHICS_STATUS_SUCCESS;
}

static INLINE eGRAPHICS_STATUS GraphcisPointQueuePush(GRAPHICS_POINT_QUEUE* queue, GRAPHICS_BO_EVENT* event)
{
	GRAPHICS_BO_EVENT **elements;
	int i, parent;

	if(queue->size + 1 == queue->max_size)
	{
		eGRAPHICS_STATUS

		status = GraphicsPointQueueGrow(queue);
		if(UNLIKELY(status))
		{
			return status;
		}
	}

	elements = queue->elements;

	for(i=++queue->size; i != POINT_QUEUE_FIRST_ENTRY
		&& GraphicsBoEventCompare(event, elements[parent = POINT_QUEUE_PARENT_INDEX(i)]) < 0; i = parent)
	{
		elements[i] = elements[parent];
	}

	elements[i] = event;

	return GRAPHICS_STATUS_SUCCESS;
}

static INLINE void GraphicsPointQueuePop(GRAPHICS_POINT_QUEUE* queue)
{
	GRAPHICS_BO_EVENT **elements = queue->elements;
	GRAPHICS_BO_EVENT *tail;
	int child, i;

	tail = elements[queue->size--];
	if(queue->size == 0)
	{
		elements[POINT_QUEUE_FIRST_ENTRY] = NULL;
		return;
	}

	for(i=POINT_QUEUE_FIRST_ENTRY; (child = POINT_QUEUE_LEFT_CHILD_INDEX(i)) <= queue->size; i = child)
	{
		if(child != queue->size && GraphicsBoEventCompare(elements[child+1], elements[child]) < 0)
		{
			child++;
		}

		if(GraphicsBoEventCompare(elements[child], tail) >= 0)
		{
			break;
		}

		elements[i] = elements[child];
	}
	elements[i] = tail;
}

static INLINE eGRAPHICS_STATUS GraphicsBoEventQueueInsert(
	GRAPHICS_BO_EVENT_QUEUE* queue,
	eGRAPHICS_BO_EVENT_TYPE type,
	GRAPHICS_BO_EDGE* edge1,
	GRAPHICS_BO_EDGE* edge2,
	const GRAPHICS_POINT* point
)
{
	GRAPHICS_BO_QUEUE_EVENT *event;

	event = (GRAPHICS_BO_QUEUE_EVENT*)GraphicsMemoryPoolAllocation(&queue->pool);
	if(UNLIKELY(event == NULL))
	{
		return GRAPHICS_STATUS_NO_MEMORY;
	}

	event->type = type;
	event->edge1 = edge1;
	event->edge2 = edge2;
	event->point = *point;

	return GraphcisPointQueuePush(&queue->point_queue, (GRAPHICS_BO_EVENT*)event);
}

INLINE int GraphicsBoPoint32Compare(const GRAPHICS_POINT* a, const GRAPHICS_POINT* b)
{
	int compare;

	compare = a->y - b->y;
	if(compare != 0)
	{
		return compare;
	}
	return a->x - b->x;
}

INLINE int GraphicsBoEventCompare(const GRAPHICS_BO_EVENT* a, const GRAPHICS_BO_EVENT* b)
{
	int compare;

	compare = GraphicsBoPoint32Compare(&a->point, &b->point);
	if(compare != 0)
	{
		return compare;
	}

	return a - b;
}

static INLINE void GraphicsBoEventQueueSort(GRAPHICS_BO_EVENT** base, unsigned int nmemb)
{
	unsigned int gap = nmemb;
	unsigned int i, j;
	int swapped;
	do
	{
		gap = GraphicsCombsortNewGap(gap);
		swapped = gap > 1;
		for(i=0; i < nmemb - gap; i++)
		{
			j = i + gap;
			if(GraphicsBoEventCompare(base[i], base[j]) > 0)
			{
				GRAPHICS_BO_EVENT *temp;
				temp = base[i];
				base[i] = base[j];
				base[j] = temp;
				swapped = 1;
			}
		}
	} while(swapped != 0);
}

static INLINE int SlopeCompare(const GRAPHICS_BO_EDGE* a, const GRAPHICS_BO_EDGE* b)
{
	int32 adx = a->edge.line.point2.x - a->edge.line.point1.x;
	int32 bdx = b->edge.line.point2.x - b->edge.line.point1.x;

	if(adx == 0)
	{
		return -bdx;
	}
	if(bdx == 0)
	{
		return adx;
	}

	if((adx ^ bdx) < 0)
	{
		return adx;
	}

	{
		int32 ady = a->edge.line.point2.y - a->edge.line.point1.y;
		int32 bdy = b->edge.line.point2.y - b->edge.line.point1.y;
		int64 adx_bdy = INTEGER32x32_64_MULTI(adx, bdy);
		int64 bdx_ady = INTEGER32x32_64_MULTI(bdx, ady);

		return (adx_bdy == bdx_ady) ? 0 : (adx_bdy < bdx_ady) ? -1 : 1;
	}
}

static void InitializeGraphicsBoEventQueue(
	GRAPHICS_BO_EVENT_QUEUE* event_queue,
	GRAPHICS_BO_EVENT** start_events,
	int num_events
)
{
	InitializeGraphicsMemoryPool(&event_queue->pool, sizeof(*event_queue));
	event_queue->start_events = start_events;

	InitializeGraphicsPointQueue(&event_queue->point_queue);
	event_queue->point_queue.elements[POINT_QUEUE_FIRST_ENTRY] = NULL;
}

static void InitializeGraphicsBoSweepLine(GRAPHICS_BO_SWEEP_LINE* sweep_line)
{
	sweep_line->head = NULL;
	sweep_line->stopped = NULL;
	sweep_line->current_y = INT32_MIN;
	sweep_line->current_edge = NULL;
}

static GRAPHICS_BO_EVENT* GraphicsBoEventDequeue(GRAPHICS_BO_EVENT_QUEUE* event_queue)
{
	GRAPHICS_BO_EVENT *event, *compare;

	event = event_queue->point_queue.elements[POINT_QUEUE_FIRST_ENTRY];
	compare = *event_queue->start_events;
	if(event == NULL || (compare != NULL && GraphicsBoEventCompare(compare, event) < 0))
	{
		event = compare;
		event_queue->start_events++;
	}
	else
	{
		GraphicsPointQueuePop(&event_queue->point_queue);
	}

	return event;
}

static int EdgeCompareForYagainstX(const GRAPHICS_BO_EDGE* a, int32 y, int32 x)
{
	int32 adx, ady;
	int32 dx, dy;
	int64 L, R;

	if(x < a->edge.line.point1.x  && x < a->edge.line.point2.x)
	{
		return 1;
	}
	if(x > a->edge.line.point1.x && x > a->edge.line.point2.x)
	{
		return -1;
	}

	adx = y - a->edge.line.point1.y;
	dx = x - a->edge.line.point1.x;

	if(adx == 0)
	{
		return -dx;
	}
	if(dx == 0 || (adx ^ dx) < 0)
	{
		return adx;
	}

	dy = y - a->edge.line.point1.y;
	ady = a->edge.line.point2.y - a->edge.line.point1.y;

	L = GRAPHICS_INT32x32_64_MULTI(dy, adx);
	R = GRAPHICS_INT32x32_64_MULTI(dx, ady);

	return (L == R) ? 0 : (L < R) ? -1 : 1;
}

static INLINE int EdgesColinear(GRAPHICS_BO_EDGE* a, GRAPHICS_BO_EDGE* b)
{
#ifdef _MSC_VER
# ifdef _WIN64
#  define COMPILE64BIT 1
# else
#  define COMPILE64BIT 0
# endif
#else
# if SIZE_MAX > 0xFFFFFFFF
#  define COMPILE64BIT 1
# else
#  define COMPILE64BIT 0
# endif
#endif

#if COMPILE64BIT != 0
# define HAS_COLINEAR(a, b) ((GRAPHICS_BO_EDGE *)(((uint64)(a))&~1) == (b))
# define IS_COLINEAR(e) (((uint64)(e))&1)
# define MARK_COLINEAR(e, v) ((GRAPHICS_BO_EDGE *)(((uint64)(e))|(v)))
#else
# define HAS_COLINEAR(a, b) ((GRAPHICS_BO_EDGE *)(((uint32)(a))&~1) == (b))
# define IS_COLINEAR(e) (((uint32)(e))&1)
# define MARK_COLINEAR(e, v) ((GRAPHICS_BO_EDGE *)(((uint32)(e))|(v)))
#endif
	unsigned int p;

	if(HAS_COLINEAR(a->colinear, b))
	{
		return IS_COLINEAR(a->colinear);
	}

	if(HAS_COLINEAR(b->colinear, a))
	{
		p = IS_COLINEAR(b->colinear);
		a->colinear = MARK_COLINEAR(a, p);
		return p;
	}

	p = 0;
	p |= (a->edge.line.point1.x == b->edge.line.point1.x) << 0;
	p |= (a->edge.line.point1.y == b->edge.line.point1.y) << 1;
	p |= (a->edge.line.point2.x == b->edge.line.point2.x) << 3;
	p |= (a->edge.line.point2.y == b->edge.line.point2.y) << 4;
	if(p == ((1 << 0) | (1 << 1) | (1 << 3) | (1 << 4)))
	{
		a->colinear = MARK_COLINEAR(b, 1);
		return TRUE;
	}

	if(SlopeCompare(a, b) != 0)
	{
		a->colinear = MARK_COLINEAR(b, 0);
		return FALSE;
	}

	if(p != 0)
	{
		p = (((p >> 1) & p) & 5) != 0;
	}
	else if(a->edge.line.point1.y < b->edge.line.point1.y)
	{
		p = EdgeCompareForYagainstX(b, a->edge.line.point1.y,
			a->edge.line.point1.x) == 0;
	}
	else
	{
		p = EdgeCompareForYagainstX(a, b->edge.line.point1.y,
			b->edge.line.point1.x) == 0;
	}

	a->colinear = MARK_COLINEAR(b, p);
	return p;
}

static GraphicsBoEdgeEndTrap(GRAPHICS_BO_EDGE* left, int32 bottom, GRAPHICS_TRAPS* traps)
{
	GRAPHICS_BO_TRAP *trap = &left->defferred_trap;

	if(trap->top < bottom)
	{
		GraphicsTrapsAddTrap(traps, trap->top, bottom,
			&left->edge.line, &trap->right->edge.line);
	}
	trap->right = NULL;
}

static INLINE void GraphicsBoEdgeStartOrContinueTrap(
	GRAPHICS_BO_EDGE* left,
	GRAPHICS_BO_EDGE* right,
	int top,
	GRAPHICS_TRAPS* traps
)
{
	if(left->defferred_trap.right == right)
	{
		return;
	}

	ASSERT(right != NULL);
	if(left->defferred_trap.right != NULL)
	{
		if(EdgesColinear(left->defferred_trap.right, right) != 0)
		{
			left->defferred_trap.right = right;
			return;
		}
		GraphicsBoEdgeEndTrap(left, top, traps);
	}

	if(FALSE == EdgesColinear(left, right))
	{
		left->defferred_trap.top = top;
		left->defferred_trap.right = right;
	}
}

static void EdgeEndBox(SWEEP_LINE* sweep_line, EDGE* left, int32 bottom)
{
	eGRAPHICS_STATUS status = GRAPHICS_STATUS_SUCCESS;

	if(LIKELY(left->top < bottom))
	{
		if(sweep_line->do_traps)
		{
			GRAPHICS_LINE _left = {
				{left->x, left->top}, {left->x, bottom}};
			GRAPHICS_LINE _right = {
				{left->right->x, left->top}, {left->right->x, bottom}};
			GraphicsTrapsAddTrap(sweep_line->container, left->top, bottom, &_left, &_right);
			status = ((GRAPHICS_TRAPS*)sweep_line->container)->status;
		}
		else
		{
			GRAPHICS_BOX box;

			box.point1.x = left->x;
			box.point1.y = left->top;
			box.point2.x = left->right->x;
			box.point2.y = bottom;

			status = GraphicsBoxesAdd(sweep_line->container, GRAPHICS_ANTIALIAS_DEFAULT, &box);
		}
	}
	if(UNLIKELY(status))
	{
		longjmp(sweep_line->unwind, status);
	}

	left->right = NULL;
}

static INLINE void OttmannRectangularActiveEdgesToTraps(SWEEP_LINE* sweep)
{
	int top = sweep->current_y;
	EDGE *position;

	if(sweep->last_y == sweep->current_y)
	{
		return;
	}

	if(sweep->insert != FALSE)
	{
		ActiveEdgesInsert(sweep);
	}

	position = sweep->head.next;
	if(position == &sweep->tail)
	{
		return;
	}

	if(sweep->fill_rule == GRAPHICS_FILL_RULE_WINDING)
	{
		do
		{
			EDGE *left, *right;
			int winding;

			left = position;
			winding = left->direction;

			right = left->next;

			while(right->x == left->x)
			{
				if(right->right != NULL)
				{
					ASSERT(left->right == NULL);
					left->top = right->top;
					left->right = right->right;
					right->right = NULL;
				}
				winding += right->direction;
				right = right->next;
			}

			if(winding == 0)
			{
				if(left->right != NULL)
				{
					EdgeEndBox(sweep, left, top);
				}
				position = right;
				continue;
			}

			do
			{
				if(UNLIKELY(right->right != NULL))
				{
					EdgeEndBox(sweep, right, top);
				}

				winding += right->direction;
				if(winding == 0 && right->x != right->next->x)
				{
					break;
				}

				right = right->next;
			} while(1);

			EdgeStartOrContinueBox(sweep, left, right, top);

			position = right->next;
		} while(position != &sweep->tail);
	}
	else
	{
		do
		{
			EDGE *right = position->next;
			int count = 0;

			do
			{
				if(UNLIKELY(right->right != NULL))
				{
					EdgeEndBox(sweep, right, top);
				}

				if(++count & 1 && right->x != right->next->x)
				{
					break;
				}

				right = right->next;
			} while(1);

			EdgeStartOrContinueBox(sweep, position, right, top);

			position = right->next;
		} while(position != &sweep->tail);
	}

	sweep->last_y = sweep->current_y;
}

static INLINE void ActiveEdgesToTraps(
	GRAPHICS_BO_EDGE* position,
	int32 top,
	unsigned int mask,
	GRAPHICS_TRAPS* traps
)
{
	GRAPHICS_BO_EDGE *left;
	int in_out;

	in_out = 0;
	left = position;
	while(position != NULL)
	{
		if(position != left && position->defferred_trap.right != NULL)
		{
			if(left->defferred_trap.right == NULL && EdgesColinear(left, position) != 0)
			{
				left->defferred_trap = position->defferred_trap;
				position->defferred_trap.right = NULL;
			}
			else
			{
				GraphicsBoEdgeEndTrap(position, top, traps);
			}
		}

		in_out += position->edge.direction;
		if((in_out & mask) == 0)
		{
			if(position->next == NULL || FALSE == EdgesColinear(position, position->next))
			{
				GraphicsBoEdgeStartOrContinueTrap(left, position, top, traps);
				left = position->next;
			}
		}

		position = position->next;
	}
}

static int EdgesCompareXforY_General(
	const GRAPHICS_BO_EDGE* a,
	const GRAPHICS_BO_EDGE* b,
	int32 y
)
{
	int32 dx;
	int32 adx, ady;
	int32 bdx, bdy;

	enum
	{
		HAVE_NONE = 0x00,
		HAVE_DX = 0x01,
		HAVE_ADX = 0x02,
		HAVE_DX_ADX = HAVE_DX | HAVE_ADX,
		HAVE_BDX = 0x04,
		HAVE_DX_BDX = HAVE_DX | HAVE_BDX,
		HAVE_ADX_BDX = HAVE_ADX | HAVE_BDX,
		HAVE_ALL = HAVE_DX | HAVE_ADX | HAVE_BDX
	} have_dx_adx_bdx = HAVE_ALL;

	{
		int32 a_min, a_max;
		int32 b_min, b_max;
		if(a->edge.line.point1.x < a->edge.line.point2.x)
		{
			a_min = a->edge.line.point1.x;
			a_max = a->edge.line.point2.x;
		}
		else
		{
			a_min = a->edge.line.point2.x;
			a_max = a->edge.line.point1.x;
		}
		if(b->edge.line.point1.x < b->edge.line.point2.x)
		{
			b_min = b->edge.line.point1.x;
			b_max = b->edge.line.point2.x;
		}
		else
		{
			b_min = b->edge.line.point2.x;
			b_max = b->edge.line.point1.x;
		}
		if(a_max < b_min)
		{
			return -1;
		}
		if(a_min > b_max)
		{
			return +1;
		}
	}

	ady = a->edge.line.point2.y - a->edge.line.point1.y;
	adx = a->edge.line.point2.x - a->edge.line.point1.x;
	if(adx == 0)
	{
		have_dx_adx_bdx &= ~HAVE_ADX;
	}

	bdy = b->edge.line.point2.y - b->edge.line.point1.y;
	bdx = b->edge.line.point2.x - b->edge.line.point1.x;
	if(bdx == 0)
	{
		have_dx_adx_bdx &= ~HAVE_BDX;
	}

	dx = a->edge.line.point1.x - b->edge.line.point1.x;
	if(dx == 0)
	{
		have_dx_adx_bdx &= ~HAVE_DX;
	}

#define L GRAPHICS_INT64x32_128_MULTI(GRAPHICS_INT32x32_64_MULTI(ady, bdy), dx)
#define A GRAPHICS_INT64x32_128_MULTI(GRAPHICS_INT32x32_64_MULTI(adx, bdy), y - a->edge.line.point1.y)
#define B GRAPHICS_INT64x32_128_MULTI(GRAPHICS_INT32x32_64_MULTI(bdx, ady), y -  b->edge.line.point1.y)
	switch(have_dx_adx_bdx)
	{
	default:
	case HAVE_NONE:
		return 0;
	case HAVE_DX:
		return dx;
	case HAVE_ADX:
		return adx;
	case HAVE_BDX:
		return -bdx;
	case HAVE_ADX_BDX:
		if((adx ^ bdx) < 0)
		{
			return adx;
		}
		else if(a->edge.line.point1.y == b->edge.line.point1.y)
		{
			int64 adx_bdy, bdx_ady;

			adx_bdy = GRAPHICS_INT32x32_64_MULTI(adx, bdy);
			bdx_ady = GRAPHICS_INT32x32_64_MULTI(bdx, ady);

			return (adx_bdy == bdx_ady ? 0 : (adx_bdy < bdx_ady) ? -1 : 1);
		}
		else
		{
			return Integer128Compare(A, B);
		}
	case HAVE_DX_ADX:
		if((-adx ^ dx) < 0)
		{
			return dx;
		}
		else
		{
			int64 ady_dx, dy_adx;

			ady_dx = GRAPHICS_INT32x32_64_MULTI(ady, dx);
			dy_adx = GRAPHICS_INT32x32_64_MULTI(a->edge.line.point1.y - y, adx);

			return (ady_dx == dy_adx ? 0 : ady_dx < dy_adx ? -1 : 1);
		}
	case HAVE_DX_BDX:
		if((bdx ^ dx) < 0)
		{
			return dx;
		}
		else
		{
			int64 bdy_dx, dy_bdx;

			bdy_dx = GRAPHICS_INT32x32_64_MULTI(bdy, dx);
			dy_bdx = GRAPHICS_INT32x32_64_MULTI(y - b->edge.line.point1.y, bdx);

			return (bdy_dx == dy_bdx ? 0 : bdy_dx < dy_bdx ? -1 : 1);
		}
	case HAVE_ALL:
		return Integer128Compare(L, INTEGER128_SUB(B, A));
	}
#undef B
#undef A
#undef L
}

static INLINE int LineEqual(const GRAPHICS_LINE* a, const GRAPHICS_LINE* b)
{
	return (a->point1.x == b->point1.x && a->point1.y == b->point1.y
				&& a->point2.x == b->point2.x && a->point2.y == b->point2.y);
}

static int EdgesCompareXforY(
	const GRAPHICS_BO_EDGE* a,
	const GRAPHICS_BO_EDGE* b,
	int32 y
)
{
	enum
	{
		HAVE_NEITHER = 0x00,
		HAVE_AX = 0x01,
		HAVE_BX = 0x02,
		HAVE_BOTH = HAVE_AX | HAVE_BX
	} have_ax_bx = HAVE_BOTH;
	int32 ax = 0, bx = 0;

	if(y == a->edge.line.point1.y)
	{
		ax = a->edge.line.point1.x;
	}
	else if(y == a->edge.line.point2.y)
	{
		ax = a->edge.line.point2.x;
	}
	else
	{
		have_ax_bx = HAVE_BX;
	}

	if(y == b->edge.line.point1.y)
	{
		bx = b->edge.line.point1.x;
	}
	else if(y == b->edge.line.point2.y)
	{
		bx = b->edge.line.point2.x;
	}
	else
	{
		have_ax_bx &= ~HAVE_BX;
	}

	switch(have_ax_bx)
	{
	default:
	case HAVE_NEITHER:
		return EdgesCompareXforY_General(a, b, y);
	case HAVE_AX:
		return - EdgeCompareForYagainstX(b, y, ax);
	case HAVE_BX:
		return EdgeCompareForYagainstX(a, y, bx);
	case HAVE_BOTH:
		return ax - bx;
	}
}

typedef struct _GRAPHICS_BO_INTERSECT_ORDINATE
{
	int32 ordinate;
	enum {EXACT, INEXACT} exactness;
} GRAPHICS_BO_INTERSECT_ORDINATE;

typedef struct _GRAPHICS_BO_INTERSECT_POINT
{
	GRAPHICS_BO_INTERSECT_ORDINATE x, y;
} GRAPHICS_BO_INTERSECT_POINT;

static INLINE int64 Determin32_64(int32 a, int32 b, int32 c, int32 d)
{
	return GRAPHICS_INT32x32_64_MULTI(a, d) - GRAPHICS_INT32x32_64_MULTI(b, c);
}

static INLINE int128 Determin64x32_128(int64 a, int32 b, int64 c, int32 d)
{
	return UnsignedInteger128Sub(GRAPHICS_INT64x32_128_MULTI(a, d), GRAPHICS_UINT64x32_128_MULTI(c, b));
}

static int IntersectLines(GRAPHICS_BO_EDGE* a, GRAPHICS_BO_EDGE* b, GRAPHICS_BO_INTERSECT_POINT* intersection)
{
	int64 a_determin, b_determin;

	int32 dx1 = a->edge.line.point1.x - a->edge.line.point2.x;
	int32 dy1 = a->edge.line.point1.y - a->edge.line.point2.y;

	int32 dx2 = b->edge.line.point1.x - b->edge.line.point2.x;
	int32 dy2 = b->edge.line.point1.y - b->edge.line.point2.y;

	int64 den_determin;
	int64 R;
	GRAPHICS_QUOREM64 qr;

	den_determin = Determin32_64(dx1, dy1, dx2, dy2);

	R = Determin32_64(dx2, dy2, b->edge.line.point1.x - a->edge.line.point1.x,
					  b->edge.line.point1.y - a->edge.line.point1.y);
	if(den_determin < 0)
	{
		if(den_determin >= R)
		{
			return FALSE;
		}
	}
	else
	{
		if(den_determin <= R)
		{
			return FALSE;
		}
	}

	R = Determin32_64(dy1, dx1, a->edge.line.point1.y - b->edge.line.point1.y,
					  a->edge.line.point1.x - b->edge.line.point1.x);
	if(den_determin < 0)
	{
		if(den_determin >= R)
		{
			return FALSE;
		}
	}
	else
	{
		if(den_determin <= R)
		{
			return FALSE;
		}
	}

	a_determin = Determin32_64(a->edge.line.point1.x, a->edge.line.point1.y,
		a->edge.line.point2.x, a->edge.line.point2.y);
	b_determin = Determin32_64(b->edge.line.point1.x, b->edge.line.point1.y,
		b->edge.line.point2.x, b->edge.line.point2.y);

	qr = GraphicsInteger96by64_32x64_DivideRemain(
		Determin64x32_128(a_determin, dx1, b_determin, dx2), den_determin);
	if(qr.rem == den_determin)
	{
		return FALSE;
	}

	intersection->x.exactness = EXACT;
	if(qr.rem == 0)
	{
		if(den_determin < 0 && qr.rem < 0)
		{
			qr.rem = ~ qr.rem;
		}
		qr.rem = qr.rem * 2;
		if(qr.rem >= den_determin)
		{
			qr.quo = qr.quo + (qr.quo < 0 ? -1 : 1);
		}
		else
		{
			intersection->x.exactness = INEXACT;
		}
	}

	intersection->x.ordinate = (int32)qr.quo;

	qr = GraphicsInteger96by64_32x64_DivideRemain(Determin64x32_128(a_determin, dy1, b_determin, dy2), den_determin);

	if(qr.rem == den_determin)
	{
		return FALSE;
	}
	intersection->y.exactness = EXACT;
	if(qr.rem != 0)
	{
		if(den_determin < 0 && qr.rem < 0)
		{
			qr.rem = ~ qr.rem;
		}
		qr.rem = qr.rem * 2;
		if(qr.rem >= den_determin)
		{
			qr.quo = qr.quo + (qr.quo < 0 ? -1 : 1);
		}
		else
		{
			intersection->y.exactness = INEXACT;
		}
	}
	intersection->y.ordinate = (int32)qr.quo;

	return TRUE;
}

static INLINE int GraphicsBoSweepLineCompareEdges(
	const GRAPHICS_BO_SWEEP_LINE* sweep_line,
	const GRAPHICS_BO_EDGE* a,
	const GRAPHICS_BO_EDGE* b
)
{
	int compare;

	if(FALSE == LineEqual(&a->edge.line, &b->edge.line))
	{
		compare = EdgesCompareXforY(a, b, sweep_line->current_y);
		if(compare != 0)
		{
			return compare;
		}

		compare = SlopeCompare(a, b);
		if(compare != 0)
		{
			return compare;
		}
	}

	return b->edge.bottom - a->edge.bottom;
}

static eGRAPHICS_STATUS GraphicsBoSweepLineInsert(GRAPHICS_BO_SWEEP_LINE* sweep_line, GRAPHICS_BO_EDGE* edge)
{
	if(sweep_line->current_edge != NULL)
	{
		GRAPHICS_BO_EDGE *previous, *next;
		int compare;

		compare = GraphicsBoSweepLineCompareEdges(sweep_line,
					sweep_line->current_edge, edge);
		if(compare < 0)
		{
			previous = sweep_line->current_edge;
			next = previous->next;
			while(next != NULL && GraphicsBoSweepLineCompareEdges(sweep_line, next, edge) < 0)
			{
				previous = next, next = previous->next;
			}

			previous->next = edge;
			edge->previous = previous;
			edge->next = next;
			if(next != NULL)
			{
				next->previous = edge;
			}
		}
		else if(compare > 0)
		{
			next = sweep_line->current_edge;
			previous = next->previous;
			while(previous != NULL && GraphicsBoSweepLineCompareEdges(sweep_line, previous, edge) > 0)
			{
				next = previous, previous = next->previous;
			}

			next->previous = edge;
			edge->next = next;
			edge->previous = previous;
			if(previous != NULL)
			{
				previous->next = edge;
			}
			else
			{
				sweep_line->head = edge;
			}
		}
		else
		{
			previous = sweep_line->current_edge;
			edge->previous = previous;
			edge->next = previous->next;
			if(previous->next != NULL)
			{
				previous->next->previous = edge;
				previous->next = edge;
			}
		}
	}
	else
	{
		sweep_line->head = edge;
	}

	sweep_line->current_edge = edge;

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsBoEventQueueInsertStop(
	GRAPHICS_BO_EVENT_QUEUE* event_queue,
	GRAPHICS_BO_EDGE* edge
)
{
	GRAPHICS_POINT point;

	point.y = edge->edge.bottom;
	point.x = LineComputeIntersectionXforY(&edge->edge.line, point.y);
	return GraphicsBoEventQueueInsert(event_queue, GRAPHICS_BO_EVENT_TYPE_STOP, edge, NULL, &point);
}

static int GraphicsBoIntersectOrdinate32Compare(GRAPHICS_BO_INTERSECT_ORDINATE a, int32 b)
{
	if(a.ordinate > b)
	{
		return +1;
	}
	if(a.ordinate < b)
	{
		return -1;
	}

	return INEXACT == a.exactness;
}

static int GraphicsBoEdgeContainsIntersectPoint(GRAPHICS_BO_EDGE* edge, GRAPHICS_BO_INTERSECT_POINT* point)
{
	int compare_top, compare_bottom;

	compare_top = GraphicsBoIntersectOrdinate32Compare(point->y, edge->edge.top);
	compare_bottom = GraphicsBoIntersectOrdinate32Compare(point->y, edge->edge.bottom);

	if(compare_top < 0 || compare_bottom > 0)
	{
		return FALSE;
	}

	if(compare_top > 0 && compare_bottom < 0)
	{
		return TRUE;
	}

	if(compare_top == 0)
	{
		GRAPHICS_FLOAT_FIXED top_x;

		top_x = LineComputeIntersectionXforY(&edge->edge.line, edge->edge.top);
		return GraphicsBoIntersectOrdinate32Compare(point->x, top_x) > 0;
	}
	else
	{
		GRAPHICS_FLOAT_FIXED bottom_x;

		bottom_x = LineComputeIntersectionXforY(&edge->edge.line, edge->edge.bottom);
		return GraphicsBoIntersectOrdinate32Compare(point->x, bottom_x) < 0;
	}
	return FALSE;
}

static int GraphicsBoEdgeIntersect(
	GRAPHICS_BO_EDGE* a,
	GRAPHICS_BO_EDGE* b,
	GRAPHICS_POINT* intersection
)
{
	GRAPHICS_BO_INTERSECT_POINT quorem;

	if(FALSE == IntersectLines(a, b, &quorem))
	{
		return FALSE;
	}

	if(FALSE == GraphicsBoEdgeContainsIntersectPoint(a, &quorem))
	{
		return FALSE;
	}

	if(FALSE == GraphicsBoEdgeContainsIntersectPoint(b, &quorem))
	{
		return FALSE;
	}

	intersection->x = quorem.x.ordinate;
	intersection->y = quorem.y.ordinate;

	return TRUE;
}

static INLINE eGRAPHICS_STATUS GraphicsBoEventQueueInsertIfIntersectBelowCurrentY(
	GRAPHICS_BO_EVENT_QUEUE* event_queue,
	GRAPHICS_BO_EDGE* left,
	GRAPHICS_BO_EDGE* right
)
{
	GRAPHICS_POINT intersection;

	if(LineEqual(&left->edge.line, &right->edge.line))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(GraphicsLinesEqual(&left->edge.line, &right->edge.line))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(SlopeCompare(left, right) <= 0)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(FALSE == GraphicsBoEdgeIntersect(left, right, &intersection))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	return GraphicsBoEventQueueInsert(event_queue, GRAPHICS_BO_EVENT_TYPE_INTERSECTION,
				left, right, &intersection);
}

static void GraphicsBoEventQueueDelete(GRAPHICS_BO_EVENT_QUEUE* queue, GRAPHICS_BO_EVENT* event)
{
	GraphicsMemoryPoolFree(&queue->pool, event);
}

/*
static void GraphicsBoSweepLineInsert(GRAPHICS_BO_SWEEP_LINE* sweep_line, GRAPHICS_BO_EDGE* edge)
{
	if(sweep_line->current_edge != NULL)
	{
		GRAPHICS_BO_EDGE *previous, *next;
		int compare;

		compare = GraphicsBoSweepLineCompareEdges(sweep_line,
				sweep_line->current_edge, edge);
		if(compare < 0)
		{
			previous = sweep_line->current_edge;
			next = previous->next;
			while(next != NULL
				  && GraphicsBoSweepLineCompareEdges(sweep_line, next, edge) < 0)
			{
				previous = next,	next = previous->next;
			}

			previous->next = edge;
			edge->previous = previous;
			edge->next = next;
			if(next != NULL)
			{
				next->previous = edge;
			}
		}
		else if(compare > 0)
		{
			next = sweep_line->current_edge;
			previous = next->previous;
			while(previous != NULL
				  && GraphicsBoSweepLineCompareEdges(sweep_line, previous, edge) > 0)
			{
				next = previous, previous = next->previous;
			}
		}
		else
		{
			previous = sweep_line->current_edge;
			edge->previous = previous;
			edge->next = previous->next;
			if(previous->next != NULL)
			{
				previous->next->previous = edge;
				previous->next = edge;
			}
		}
	}
	else
	{
		sweep_line->head = edge;
		edge->next = NULL;
	}

	sweep_line->current_edge = edge;
}
*/

static void GraphicsBoSweepLineDelete(GRAPHICS_BO_SWEEP_LINE* sweep_line, GRAPHICS_BO_EDGE* edge)
{
	if(edge->previous != NULL)
	{
		edge->previous->next = edge->next;
	}
	else
	{
		sweep_line->head = edge->next;
	}

	if(edge->next != NULL)
	{
		edge->next->previous = edge->previous;
	}

	if(sweep_line->current_edge == edge)
	{
		sweep_line->current_edge = (edge->previous != NULL) ? edge->previous : edge->next;
	}
}

static void GraphicsBoSweepLineSwap(
	GRAPHICS_BO_SWEEP_LINE* sweep_line,
	GRAPHICS_BO_EDGE* left,
	GRAPHICS_BO_EDGE* right
)
{
	if(left->previous != NULL)
	{
		left->previous->next = right;
	}
	else
	{
		sweep_line->head = right;
	}

	if(right->next != NULL)
	{
		right->next->previous = left;
	}

	left->next = right->next;
	right->next = left;

	right->previous = left->previous;
	left->previous = right;
}

static void GraphicsBoEventQueueFinish(GRAPHICS_BO_EVENT_QUEUE* event_queue)
{
	GraphicsPointQueueFinish(&event_queue->point_queue);
	GraphicsMemoryPoolFinish(&event_queue->pool);
}

eGRAPHICS_STATUS GraphicsBentleyOttmannTessellateBoEdges(
	GRAPHICS_BO_EVENT** start_events,
	int num_events,
	eGRAPHICS_FILL_RULE fill_rule,
	GRAPHICS_TRAPS* traps,
	int* num_intersections
)
{
	eGRAPHICS_STATUS status;
	int intersection_count = 0;
	GRAPHICS_BO_EVENT_QUEUE event_queue;
	GRAPHICS_BO_SWEEP_LINE sweep_line;
	GRAPHICS_BO_EVENT *event;
	int rule;
	GRAPHICS_BO_EDGE *left, *right;
	GRAPHICS_BO_EDGE *edge1, *edge2;

	if(fill_rule == GRAPHICS_FILL_RULE_WINDING)
	{
		rule = -1;
	}
	else
	{
		rule = 1;
	}

	InitializeGraphicsBoEventQueue(&event_queue, start_events, num_events);
	InitializeGraphicsBoSweepLine(&sweep_line);

	while((event = GraphicsBoEventDequeue(&event_queue)))
	{
		if(event->point.y != sweep_line.current_y)
		{
			for(edge1 = sweep_line.stopped; edge1 != NULL; edge1 = edge1->next)
			{
				if(edge1->defferred_trap.right != NULL)
				{
					GraphicsBoEdgeEndTrap(edge1, edge1->edge.bottom, traps);
				}
			}
			sweep_line.stopped = NULL;

			ActiveEdgesToTraps(sweep_line.head, sweep_line.current_y, fill_rule, traps);
			sweep_line.current_y = event->point.y;
		}

		switch(event->type)
		{
		case GRAPHICS_BO_EVENT_TYPE_START:
			edge1 = &((GRAPHICS_BO_START_EVENT*)event)->edge;

			GraphicsBoSweepLineInsert(&sweep_line, edge1);
			status = GraphicsBoEventQueueInsertStop(&event_queue, edge1);
			if(UNLIKELY(status))
			{
				goto UNWIND;
			}

			for(left = sweep_line.stopped; left != NULL; left = left->next)
			{
				if(edge1->edge.top <= left->edge.bottom && EdgesColinear(edge1, left))
				{
					edge1->defferred_trap = left->defferred_trap;
					if(left->previous != NULL)
					{
						left->previous = left->next;
					}
					else
					{
						sweep_line.stopped = left->next;
					}
					if(left->next != NULL)
					{
						left->next->previous = left->previous;
					}
					break;
				}
			}

			left = edge1->previous;
			right = edge1->next;

			if(left != NULL)
			{
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY(&event_queue, left, edge1);
				if(UNLIKELY(status))
				{
					goto UNWIND;
				}
			}

			if(right != NULL)
			{
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY(&event_queue, edge1, right);
				if(UNLIKELY(status))
				{
					goto UNWIND;
				}
			}

			break;

		case GRAPHICS_BO_EVENT_TYPE_STOP:
			edge1 = ((GRAPHICS_BO_QUEUE_EVENT*)event)->edge1;
			GraphicsBoEventQueueDelete(&event_queue, event);

			left = edge1->previous;
			right = edge1->next;

			GraphicsBoSweepLineDelete(&sweep_line, edge1);

			if(edge1->defferred_trap.right != NULL)
			{
				edge1->next = sweep_line.stopped;
				if(sweep_line.stopped != NULL)
				{
					sweep_line.stopped->previous = edge1;
				}
				sweep_line.stopped = edge1;
				edge1->previous = NULL;
			}

			if(left != NULL && right != NULL)
			{
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY(&event_queue, left, right);
				if(UNLIKELY(status))
				{
					goto UNWIND;
				}
			}

			break;

		case GRAPHICS_BO_EVENT_TYPE_INTERSECTION:
			edge1 = ((GRAPHICS_BO_QUEUE_EVENT*)event)->edge1;
			edge2 = ((GRAPHICS_BO_QUEUE_EVENT*)event)->edge2;
			GraphicsBoEventQueueDelete(&event_queue, event);

			if(edge2 != edge1->next)
			{
				break;
			}

			intersection_count++;

			left = edge1->previous;
			right = edge2->next;

			GraphicsBoSweepLineSwap(&sweep_line, edge1, edge2);

			if(left != NULL)
			{
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY(&event_queue, left, edge2);
				if(UNLIKELY(status))
				{
					goto UNWIND;
				}
			}

			if(right != NULL)
			{
				status = GraphicsBoEventQueueInsertIfIntersectBelowCurrentY(&event_queue, edge1, right);
				if(UNLIKELY(status))
				{
					goto UNWIND;
				}
			}

			break;
		}
	}

	*num_intersections = intersection_count;
	for(edge1 = sweep_line.stopped; edge1 != NULL; edge1 = edge1->next)
	{
		if(edge1->defferred_trap.right != NULL)
		{
			GraphicsBoEdgeEndTrap(edge1, edge1->edge.bottom, traps);
		}
	}
	status = traps->status;
UNWIND:
	GraphicsBoEventQueueFinish(&event_queue);

	return status;
}

static eGRAPHICS_STATUS GraphicsBentleyOttmannTessellatePolygon(
	GRAPHICS_TRAPS* traps,
	const GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule
)
{
#define BUFFER_SIZE 512
	int intersections;
	GRAPHICS_BO_START_EVENT stack_events[BUFFER_SIZE];
	GRAPHICS_BO_START_EVENT *events;
	GRAPHICS_BO_EVENT *stack_event_pointers[BUFFER_SIZE + 1];
	GRAPHICS_BO_EVENT **events_pointers;
	GRAPHICS_BO_START_EVENT *stack_event_y[64];
	GRAPHICS_BO_START_EVENT **event_y = NULL;
	int i, num_events, y, y_min, y_max;
	eGRAPHICS_STATUS status;

	num_events = polygon->num_edges;
	if(UNLIKELY(0 == num_events))
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(polygon->num_limits != 0)
	{
		y_min = GRAPHICS_FIXED_INTEGER_FLOOR(polygon->limit.point1.y);
		y_max = GRAPHICS_FIXED_INTEGER_CEIL(polygon->limit.point2.y) - y_min;

		if(y_max > 64)
		{
			event_y = (GRAPHICS_BO_EVENT**)MEM_ALLOC_FUNC(sizeof(*event_y) * y_max);
			if(event_y == NULL)
			{
				return GRAPHICS_STATUS_NO_MEMORY;
			}
		}
		else
		{
			event_y = stack_event_y;
		}
		(void)memset(event_y, 0, y_max * sizeof(event_y));
	}

	events = stack_events;
	events_pointers = stack_event_pointers;
	if(num_events > sizeof(stack_events) / sizeof(stack_events[0]))
	{
		events = (GRAPHICS_BO_START_EVENT*)MEM_ALLOC_FUNC(
			(sizeof(GRAPHICS_BO_START_EVENT) + sizeof(GRAPHICS_BO_EVENT*)) * num_events + sizeof(GRAPHICS_BO_EVENT*));
		if(events == NULL)
		{
			if(event_y != stack_event_y)
			{
				MEM_FREE_FUNC(event_y);
			}
			return GRAPHICS_STATUS_NO_MEMORY;
		}
		events_pointers = (GRAPHICS_BO_EVENT**)(events + num_events);
	}

	for(i=0; i<num_events; i++)
	{
		events[i].type = GRAPHICS_BO_EVENT_TYPE_START;
		events[i].point.y = polygon->edges[i].top;
		events[i].point.x = LineComputeIntersectionXforY(&polygon->edges[i].line, events[i].point.y);

		events[i].edge.edge = polygon->edges[i];
		events[i].edge.defferred_trap.right = NULL;
		events[i].edge.previous = NULL;
		events[i].edge.next = NULL;
		events[i].edge.colinear = NULL;

		if(event_y != NULL)
		{
			y = GRAPHICS_FIXED_INTEGER_FLOOR(events[i].point.y) - y_min;
			events[i].edge.next = (GRAPHICS_BO_EDGE*)event_y[y];
			event_y[y] = (GRAPHICS_BO_START_EVENT*)&events[i];
		}
		else
		{
			events_pointers[i] = (GRAPHICS_BO_EVENT*)&events[i];
		}
	}

	if(event_y != NULL)
	{
		for(y = i = 0; y < y_max && i < num_events; y++)
		{
			GRAPHICS_BO_START_EVENT *e;
			int j = i;
			for(e = event_y[y]; e != NULL; e = (GRAPHICS_BO_START_EVENT*)e->edge.next)
			{
				events_pointers[i++] = (GRAPHICS_BO_EVENT*)e;
			}
			if(i > j + 1)
			{
				GraphicsBoEventQueueSort(events_pointers + j, i - j);
			}
		}
		if(event_y != stack_event_y)
		{
			MEM_FREE_FUNC(event_y);
		}
	}
	else
	{
		GraphicsBoEventQueueSort(events_pointers, i);
	}
	events_pointers[i] = NULL;

	status = GraphicsBentleyOttmannTessellateBoEdges(events_pointers, num_events,
				fill_rule, traps, &intersections);

	if(events != stack_events)
	{
		MEM_FREE_FUNC(events);
	}

	return status;
}

#define STACK_BUFFER_SIZE (512 * sizeof(int))

static INLINE int RectangleCompareStart(const RECTANGLE* a, const RECTANGLE* b)
{
	return a->top - b->top;
}

static INLINE int RectangleCompareStop(const RECTANGLE* a, const RECTANGLE* b)
{
	return a->bottom - b->bottom;
}

static INLINE unsigned int RectangleSortNewGap(unsigned int gap)
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

static void RectangleSort(RECTANGLE** base, unsigned int nmemb)
{
	unsigned int gap = nmemb;
	unsigned int i, j;
	int swapped;
	do
	{
		gap = RectangleSortNewGap(gap);
		swapped = gap > 1;
		for(i=0; i<nmemb-gap; i++)
		{
			j = i + gap;
			if(RectangleCompareStart(base[i], base[j]) > 0)
			{
				RECTANGLE *temp;
				temp = base[i];
				base[i] = base[j];
				base[j] = temp;
				swapped = 1;
			}
		}
	} while(swapped != 0);
}

static void InitializeSweepLine(
	SWEEP_LINE* sweep_line,
	RECTANGLE** rectangles,
	int num_rectangles,
	eGRAPHICS_FILL_RULE fill_rule,
	int do_traps,
	void* container
)
{
	rectangles[-2] = NULL;
	rectangles[-1] = NULL;
	rectangles[num_rectangles] = NULL;
	sweep_line->rectangles = rectangles;
	sweep_line->stop = rectangles - 2;
	sweep_line->stop_size = 0;

	sweep_line->insert = NULL;
	sweep_line->insert_x = INT_MAX;
	sweep_line->cursor = &sweep_line->tail;

	sweep_line->head.direction = 0;
	sweep_line->head.x = INT32_MIN;
	sweep_line->head.right = NULL;
	sweep_line->head.previous = NULL;
	sweep_line->head.previous = NULL;
	sweep_line->head.next = &sweep_line->tail;
	sweep_line->tail.previous = &sweep_line->head;
	sweep_line->tail.right = NULL;
	sweep_line->tail.x = INT32_MAX;
	sweep_line->tail.direction = 0;

	sweep_line->current_y = INT32_MIN;
	sweep_line->last_y = INT32_MIN;

	sweep_line->fill_rule = fill_rule;
	sweep_line->container = container;
	sweep_line->do_traps = do_traps;
}

/*
static void EdgeEndBox(SWEEP_LINE* sweep_line, EDGE* left, int32 bottom)
{
	eGRAPHICS_STATUS status = GRAPHICS_STATUS_SUCCESS;

	if(left->top < bottom)
	{
		if(sweep_line->do_traps)
		{
			GRAPHICS_LINE _left = {{left->x, left->top}, {left->x, bottom}},
					_right = {{left->right->x, left->top}, {left->right->x, bottom}};
			GraphicsTrapsAddTrap((GRAPHICS_TRAPS*)sweep_line->container, left->top,
									bottom, &_left, &_right);
			status = ((GRAPHICS_TRAPS*)sweep_line->container)->status;
		}
		else
		{
			GRAPHICS_BOX box;

			box.point1.x = left->x;
			box.point1.y = left->top;
			box.point2.x = left->right->x;
			box.point2.y = bottom;

			status = GraphicsBoxesAdd((GRAPHICS_BOXES*)sweep_line->container,
										  GRAPHICS_ANTIALIAS_DEFAULT, &box);
		}
	}
	if(UNLIKELY(status))
	{
		longjmp(sweep_line->unwind, status);
	}

	left->right = NULL;
}
*/

static EDGE* MergeSortedEdges(EDGE* head_a, EDGE* head_b)
{
	EDGE *head, *previous;
	int32 x;

	previous = head_a->previous;
	if(head_a->x <= head_b->x)
	{
		head = head_a;
	}
	else
	{
		head_b->previous = previous;
		head = head_b;
		goto START_WITH_B;
	}

	do
	{
		x = head_b->x;
		while(head_a != NULL && head_a->x <= x)
		{
			previous = head_a;
			head_a = head_a->next;
		}

		head_b->previous = previous;
		previous->next = head_b;
		if(head_a == NULL)
		{
			return head;
		}

START_WITH_B:
		x = head_a->x;
		while(head_b != NULL && head_b->x <= x)
		{
			previous = head_b;
			head_b = head_b->next;
		}

		head_a->previous = previous;
		previous->next = head_a;
		if(head_b == NULL)
		{
			return head;
		}
	} while(1);
}

static EDGE* SortEdges(EDGE* list, unsigned int level, EDGE** head_out)
{
	EDGE *head_other, *remaining;
	unsigned int i;

	head_other = list->next;

	if(head_other == NULL)
	{
		*head_out = list;
		return NULL;
	}

	remaining = head_other->next;
	if(list->x <= head_other->x)
	{
		*head_out = list;
		head_other->next = NULL;
	}
	else
	{
		*head_out = head_other;
		head_other->previous = list->previous;
		head_other->next = list;
		list->previous = head_other;
		list->next = NULL;
	}

	for(i=0; i < level && remaining != NULL; i++)
	{
		remaining = SortEdges(remaining, i, &head_other);
		*head_out = MergeSortedEdges(*head_out, head_other);
	}

	return remaining;
}

static EDGE* MergeUnsortedEdges(EDGE* head, EDGE* unsorted)
{
	SortEdges(unsorted, UINT_MAX, &unsorted);
	return MergeSortedEdges(head, unsorted);
}

static void ActiveEdgesInsert(SWEEP_LINE* sweep)
{
	EDGE *previous;
	int x;

	x = sweep->insert_x;
	previous = sweep->cursor;
	if(previous->x > x)
	{
		do
		{
			previous = previous->previous;
		} while(previous->x < x);
	}
	else
	{
		while(previous->next->x < x)
		{
			previous = previous->next;
		}
	}

	previous->next = MergeUnsortedEdges(previous->next, sweep->insert);
	sweep->cursor = sweep->insert;
	sweep->insert = NULL;
	sweep->insert_x = INT_MAX;
}

static INLINE void ActiveSweepEdgesToTraps(SWEEP_LINE* sweep)
{
	int top = sweep->current_y;
	EDGE *position;

	if(sweep->last_y == sweep->current_y)
	{
		return;
	}

	if(sweep->insert != NULL)
	{
		ActiveEdgesInsert(sweep);
	}

	position = sweep->head.next;
	if(position == &sweep->tail)
	{
		return;
	}

	if(sweep->fill_rule == GRAPHICS_FILL_RULE_WINDING)
	{
		do
		{
			EDGE *left, *right;
			int winding;

			left = position;
			winding = left->direction;

			right = left->next;

			while(right->x == left->x)
			{
				if(right->right != NULL)
				{
					ASSERT(left->right == NULL);

					left->top = right->top;
					left->right = right->right;
					right->right = NULL;
				}
				winding += right->direction;
				right = right->next;
			}

			if(winding == 0)
			{
				if(left->right != NULL)
				{
					EdgeEndBox(sweep, left, top);
				}
				position = right;
				continue;
			}

			do
			{
				if(UNLIKELY(right->right != NULL))
				{
					EdgeEndBox(sweep, right, top);
				}

				winding += right->direction;
				if(winding == 0 && right->x != right->next->x)
				{
					break;
				}

				right = right->next;
			} while(1);

			EdgeStartOrContinueBox(sweep, left, right, top);

			position = right->next;
		} while(position != &sweep->tail);
	}
	else
	{
		do
		{
			EDGE *right = position->next;
			int count = 0;

			do
			{
				if(UNLIKELY(right->right != NULL))
				{
					EdgeEndBox(sweep, right, top);
				}

				if(++count & 1  && right->x != right->next->x)
				{
					break;
				}

				right = right->next;
			} while(1);

			EdgeStartOrContinueBox(sweep, position, right, top);

			position = right->next;
		} while(position != &sweep->tail);
	}

	sweep->last_y = sweep->current_y;
}

static INLINE void RectanglePopStop(SWEEP_LINE* sweep)
{
	RECTANGLE **elements = sweep->stop;
	RECTANGLE *tail;
	int child, i;

	tail = elements[sweep->stop_size--];
	if(sweep->stop_size == 0)
	{
		elements[POINT_QUEUE_FIRST_ENTRY] = NULL;
		return;
	}

	for(i=POINT_QUEUE_FIRST_ENTRY; (child = POINT_QUEUE_LEFT_CHILD_INDEX(i)) <= sweep->stop_size; i=child)
	{
		if(child != sweep->stop_size && RectangleCompareStop(elements[child+1], elements[child]) < 0)
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

static INLINE RECTANGLE* RectanglePopStart(SWEEP_LINE* sweep_line)
{
	return *sweep_line->rectangles++;
}

static INLINE RECTANGLE* RectanglePeekStop(SWEEP_LINE* sweep_line)
{
	return sweep_line->stop[POINT_QUEUE_FIRST_ENTRY];
}

static INLINE int SweepLineDeleteEdge(SWEEP_LINE* sweep, EDGE* edge)
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
			EdgeEndBox(sweep, edge, sweep->current_y);
		}
	}

	if(sweep->cursor == edge)
	{
		sweep->cursor = edge->previous;
	}

	edge->previous->next = edge->next;
	edge->next->previous = edge->previous;

	return TRUE;
}

static INLINE int SweepLineDelete(SWEEP_LINE* sweep, RECTANGLE* rectangle)
{
	int update;

	update = TRUE;
	if(sweep->fill_rule == GRAPHICS_FILL_RULE_WINDING
			&& rectangle->left.previous->direction == rectangle->left.direction)
	{
		update = rectangle->left.next != &rectangle->right;
	}

	SweepLineDeleteEdge(sweep, &rectangle->left);
	SweepLineDeleteEdge(sweep, &rectangle->right);

	RectanglePopStop(sweep);
	return update;
}

static INLINE void PointQueuePush(SWEEP_LINE* sweep, RECTANGLE* rectangle)
{
	RECTANGLE **elements;
	int i, parent;

	elements = sweep->stop;
	for(i=++sweep->stop_size; i!=POINT_QUEUE_FIRST_ENTRY && RectangleCompareStop(rectangle, elements[parent = POINT_QUEUE_PARENT_INDEX(i)]) < 0; i=parent)
	{
		elements[i] = elements[parent];
	}

	elements[i] = rectangle;
}

static INLINE void SweepLineInsert(SWEEP_LINE* sweep, RECTANGLE* rectangle)
{
	if(sweep->insert != NULL)
	{
		sweep->insert->previous = &rectangle->right;
	}
	rectangle->right.next = sweep->insert;
	rectangle->right.previous = sweep->insert;
	rectangle->left.next = &rectangle->right;
	rectangle->left.previous = NULL;
	sweep->insert = &rectangle->left;
	if(rectangle->left.x < sweep->insert_x)
	{
		sweep->insert_x = rectangle->left.x;
	}

	PointQueuePush(sweep, rectangle);
}

static eGRAPHICS_STATUS GraphicsBentleyOttmannTessellateRectangular(
	RECTANGLE** rectangles,
	int num_rectangles,
	eGRAPHICS_FILL_RULE fill_rule,
	int do_traps,
	void* container
)
{
	SWEEP_LINE sweep_line;
	RECTANGLE *rectangle;
	eGRAPHICS_STATUS status;
	int update;

	InitializeSweepLine(&sweep_line, rectangles, num_rectangles,
							fill_rule, do_traps, container);
	if((status = setjmp(sweep_line.unwind)))
	{
		return status;
	}

	update = FALSE;

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
					if(update)
					{
						OttmannRectangularActiveEdgesToTraps(&sweep_line);
						update = FALSE;
					}

					sweep_line.current_y = stop->bottom;
				}

				update |= SweepLineDelete(&sweep_line, stop);
				stop = RectanglePeekStop(&sweep_line);
			}

			if(update)
			{
				OttmannRectangularActiveEdgesToTraps(&sweep_line);
				update = FALSE;
			}

			sweep_line.current_y = rectangle->top;
		}

		do
		{
			SweepLineInsert(&sweep_line, rectangle);
		} while((rectangle = RectanglePopStart(&sweep_line)) != NULL && sweep_line.current_y == rectangle->top);
		update = TRUE;
	} while(rectangle != NULL);

	while((rectangle = RectanglePeekStop(&sweep_line)) != NULL)
	{
		if(rectangle->bottom != sweep_line.current_y)
		{
			if(update)
			{
				OttmannRectangularActiveEdgesToTraps(&sweep_line);
				update = FALSE;
			}
			sweep_line.current_y = rectangle->bottom;
		}

		update |= SweepLineDelete(&sweep_line, rectangle);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

eGRAPHICS_STATUS GraphicsBentleyOttmannTessellateBoxes(
	const GRAPHICS_BOXES* in,
	eGRAPHICS_FILL_RULE fill_rule,
	GRAPHICS_BOXES* out
)
{
	RECTANGLE stack_rectangles[STACK_BUFFER_SIZE / sizeof(RECTANGLE)];
	RECTANGLE *stack_rectangles_pointers[sizeof(stack_rectangles) / sizeof(RECTANGLE) + 3];
	RECTANGLE *rectangles, **rectangles_pointers;
	RECTANGLE *stack_rectangles_chain[STACK_BUFFER_SIZE / sizeof(RECTANGLE*)];
	RECTANGLE **rectangles_chain = NULL;
	const struct _GRAPHICS_BOXES_CHUNK *chunk;
	eGRAPHICS_STATUS status;
	int i, j, y_min, y_max;

	if(UNLIKELY(in->num_boxes == 0))
	{
		GraphicsBoxesClear(out);
		return GRAPHICS_STATUS_SUCCESS;
	}

	if(in->num_boxes == 1)
	{
		if(in == out)
		{
			GRAPHICS_BOX *box = &in->chunks.base[0];

			if(box->point1.x > box->point2.x)
			{
				GRAPHICS_FLOAT_FIXED temp = box->point1.x;
				box->point1.x = box->point2.x;
				box->point2.x = temp;
			}
		}
		else
		{
			GRAPHICS_BOX box = in->chunks.base[0];

			if(box.point1.x > box.point2.x)
			{
				GRAPHICS_FLOAT_FIXED temp = box.point1.x;
				box.point1.x = box.point2.x;
				box.point2.x = temp;
			}

			GraphicsBoxesClear(out);
			status = GraphicsBoxesAdd(out, GRAPHICS_ANTIALIAS_DEFAULT, &box);
			ASSERT(status == GRAPHICS_STATUS_SUCCESS);
		}
		return GRAPHICS_STATUS_SUCCESS;
	}

	y_min = INT_MAX; y_max = INT_MIN;
	for(chunk=&in->chunks; chunk!=NULL; chunk=chunk->next)
	{
		const GRAPHICS_BOX *box = chunk->base;
		for(i=0; i<chunk->count; i++)
		{
			if(box[i].point1.y < y_min)
			{
				y_min = box[i].point1.y;
			}
			if(box[i].point1.y > y_max)
			{
				y_max = box[i].point1.y;
			}
		}
	}
	y_min = GraphicsFixedFromInteger(y_min);
	y_max = GraphicsFixedFromInteger(y_max) + 1;
	y_max -= y_min;

	if(y_max < in->num_boxes)
	{
		rectangles_chain = stack_rectangles_chain;
		if(y_max > sizeof(stack_rectangles_chain) / sizeof(*stack_rectangles_chain))
		{
			rectangles_chain = (RECTANGLE**)MEM_ALLOC_FUNC(y_max * sizeof(RECTANGLE*));
			if(rectangles_chain == NULL)
			{
				return GRAPHICS_STATUS_NO_MEMORY;
			}
		}
		(void)memset(rectangles_chain, 0, y_max * sizeof(RECTANGLE*));
	}

	rectangles = stack_rectangles;
	rectangles_pointers = stack_rectangles_pointers;
	if(in->num_boxes > sizeof(stack_rectangles) / sizeof(stack_rectangles[0]))
	{
		rectangles = (RECTANGLE*)MEM_ALLOC_FUNC(
						in->num_boxes * (sizeof(RECTANGLE) + sizeof(RECTANGLE)) + 3 * sizeof(RECTANGLE*));
		if(UNLIKELY(rectangles == NULL))
		{
			if(rectangles_chain != stack_rectangles_chain)
			{
				MEM_FREE_FUNC(rectangles_chain);
			}
			return GRAPHICS_STATUS_NO_MEMORY;
		}

		rectangles_pointers = (RECTANGLE**)(rectangles + in->num_boxes);
	}

	j = 0;
	for(chunk=&in->chunks; chunk!=NULL; chunk=chunk->next)
	{
		const GRAPHICS_BOX *box = chunk->base;
		for(i=0; i<chunk->count; i++)
		{
			int h;

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

			rectangles[j].left.right = NULL;
			rectangles[j].right.right = NULL;

			rectangles[j].top = box[i].point1.y;
			rectangles[j].bottom = box[i].point2.y;

			if(rectangles_chain != NULL)
			{
				h = GraphicsFixedIntegerFloor(box[i].point1.y) - y_min;
				rectangles[j].left.next = (EDGE*)rectangles_chain[h];
				rectangles_chain[h] = &rectangles[j];
			}
			else
			{
				rectangles_pointers[j+2] = &rectangles[j];
			}
			j++;
		}
	}

	if(rectangles_chain)
	{
		j = 2;
		for(y_min=0; y_min<y_max; y_min++)
		{
			RECTANGLE *r;
			int start = j;
			for(r=rectangles_chain[y_min]; r!=NULL; r=(RECTANGLE*)r->left.next)
			{
				rectangles_pointers[j++] = r;
			}
			if(j > start + 1)
			{
					RectangleSort(rectangles_pointers + start, j - start);
			}
		}

		if(rectangles_chain != stack_rectangles_chain)
		{
			MEM_FREE_FUNC(rectangles_chain);
		}

		j -= 2;
	}
	else
	{
		RectangleSort(rectangles_pointers + 2, j);
	}

	GraphicsBoxesClear(out);
	status = GraphicsBentleyOttmannTessellateRectangular(rectangles_pointers+2,
				j, fill_rule, FALSE, out);
	if(rectangles != stack_rectangles)
	{
		MEM_FREE_FUNC(rectangles);
	}

	return status;
}

void GraphicsPolygonLimit(GRAPHICS_POLYGON* polygon, const GRAPHICS_BOX* limits, int num_limits)
{
	int n;

	polygon->limits = limits;
	polygon->num_limits = num_limits;

	if(polygon->num_limits != 0)
	{
		polygon->limit = limits[0];
		for(n=1; n<num_limits; n++)
		{
			if(limits[n].point1.x < polygon->limit.point1.x)
			{
				polygon->limit.point1.x = limits[n].point1.x;
			}

			if(limits[n].point1.y < polygon->limit.point1.y)
			{
				polygon->limit.point1.y = limits[n].point1.y;
			}

			if(limits[n].point2.x > polygon->limit.point2.x)
			{
				polygon->limit.point2.x = limits[n].point2.x;
			}

			if(limits[n].point2.y > polygon->limit.point2.y)
			{
				polygon->limit.point2.y = limits[n].point2.y;
			}
		}
	}
}

void InitializeGraphicsPolygon(
	GRAPHICS_POLYGON* polygon,
	const GRAPHICS_BOX* limits,
	int num_limits
)
{
	const GRAPHICS_POLYGON local_polygon = {0};

	*polygon = local_polygon;

	polygon->edges = polygon->edges_embedded;
	polygon->edges_size = sizeof(polygon->edges_embedded) / sizeof(polygon->edges_embedded[0]);

	polygon->extents.point1.x = polygon->extents.point1.y = 0x7FFFFFFF; // INT32_MAX
	polygon->extents.point2.x = polygon->extents.point2.y = - 0x7FFFFFFF - 1; // INT32_MIN

	GraphicsPolygonLimit(polygon, limits, num_limits);
}

eGRAPHICS_STATUS GraphicsPolygonInitializeBoxes(GRAPHICS_POLYGON* polygon, const GRAPHICS_BOXES*boxes)
{
	const struct _GRAPHICS_BOXES_CHUNK *chunk;
	int i;

	polygon->status = GRAPHICS_STATUS_SUCCESS;

	polygon->num_edges = 0;

	polygon->edges = polygon->edges_embedded;
	polygon->edges_size = sizeof(polygon->edges_embedded) / sizeof(polygon->edges_embedded[0]);
	if(boxes->num_boxes > sizeof(polygon->edges_embedded) / sizeof(polygon->edges_embedded[0]) / 2)
	{
		polygon->edges_size = 2 * boxes->num_boxes;
		polygon->edges = (GRAPHICS_EDGE*)MEM_ALLOC_FUNC((2 * sizeof(GRAPHICS_EDGE)) * polygon->edges_size);
		if(polygon->edges == NULL)
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}
	}

	polygon->extents.point1.x = polygon->extents.point1.y = INT32_MAX;
	polygon->extents.point2.x = polygon->extents.point2.y = INT32_MIN;

	polygon->limits = NULL;
	polygon->num_limits = 0;

	for(chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next)
	{
		for(i=0; i<chunk->count; i++)
		{
			GRAPHICS_POINT point1, point2;

			point1 = chunk->base[i].point1;
			point2.x = point1.x;
			point2.y = chunk->base[i].point2.x;
			GraphicsPolygonAddEdge(polygon, &point1, &point2, 1);

			point1 = chunk->base[i].point2;
			point2.x = point1.x;
			point2.y = chunk->base[i].point1.y;
			GraphicsPolygonAddEdge(polygon, &point1, &point2, 1);
		}
	}

	return polygon->status;
}

void GraphicsPolygonFinish(GRAPHICS_POLYGON* polygon)
{
	if(polygon->edges != polygon->edges_embedded)
	{
		MEM_FREE_FUNC(polygon->edges);
	}
}

void GraphicsPolygonTranslate(GRAPHICS_POLYGON* polygon, int dx, int dy)
{
	int n;

	dx = GraphicsFixedFromInteger(dx);
	dy = GraphicsFixedFromInteger(dy);

	polygon->extents.point1.x += dx;
	polygon->extents.point2.x += dx;
	polygon->extents.point1.y += dy;
	polygon->extents.point2.y += dy;

	for(n=0; n<polygon->num_edges; n++)
	{
		GRAPHICS_EDGE *edge = &polygon->edges[n];

		edge->top += dy;
		edge->bottom += dy;

		edge->line.point1.x += dx;
		edge->line.point2.x += dx;
		edge->line.point1.y += dy;
		edge->line.point2.y += dy;
	}
}

eGRAPHICS_STATUS InitializeGraphicsPolygonBoxArray(
	GRAPHICS_POLYGON* polygon,
	GRAPHICS_BOX* boxes,
	int num_boxes
)
{
	int i;

	polygon->status = GRAPHICS_STATUS_SUCCESS;

	polygon->num_edges = 0;

	polygon->edges = polygon->edges_embedded;
	polygon->edges_size = sizeof(polygon->edges_embedded) / sizeof(polygon->edges_embedded[0]);
	if(num_boxes > (sizeof(polygon->edges_embedded) / sizeof(polygon->edges_embedded[0])) / 2)
	{
		polygon->edges_size = num_boxes * 2;
		polygon->edges = (GRAPHICS_EDGE*)MEM_ALLOC_FUNC(sizeof(GRAPHICS_EDGE) * polygon->edges_size);
		if(polygon->edges == NULL)
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}
	}

	polygon->extents.point1.x = polygon->extents.point1.y = INT32_MAX;
	polygon->extents.point2.x = polygon->extents.point2.y = INT32_MIN;

	polygon->limits = NULL;
	polygon->num_limits = 0;

	for(i=0; i<num_boxes; i++)
	{
		GRAPHICS_POINT point1, point2;

		point1 = boxes[i].point1;
		point2.x = point1.x;
		point2.y = boxes[i].point2.y;
		GraphicsPolygonAddEdge(polygon, &point1, &point2, 1);

		point1 = boxes[i].point2;
		point2.x = point1.x;
		point2.y = boxes[i].point1.y;
		GraphicsPolygonAddEdge(polygon, &point1, &point2, 1);
	}

	return polygon->status;
}

void InitializeGraphicsPolygonWithClip(GRAPHICS_POLYGON* polygon, const GRAPHICS_CLIP* clip)
{
	if(clip != NULL)
	{
		InitializeGraphicsPolygon(polygon, clip->boxes, clip->num_boxes);
	}
	else
	{
		InitializeGraphicsPolygon(polygon, 0, 0);
	}
}

typedef struct _GRAPHICS_TRAP_RENDERER
{
	GRAPHICS_SPAN_RENDERER base;
	GRAPHICS_TRAPS *traps;
} GRAPHICS_TRAP_RENDERER;

static eGRAPHICS_STATUS SpanToTraps(
	void* abstract_renderer,
	int y,
	int h,
	const GRAPHICS_HALF_OPEN_SPAN* spans,
	unsigned num_spans
)
{
	GRAPHICS_TRAP_RENDERER *r = (GRAPHICS_TRAP_RENDERER*)abstract_renderer;
	GRAPHICS_FLOAT_FIXED top, bottom;

	if(num_spans == 0)
	{
		return GRAPHICS_STATUS_SUCCESS;
	}

	top = GraphicsFixedFromInteger(y);
	bottom = GraphicsFixedFromInteger(y + h);
	do
	{
		if(spans[0].coverage)
		{
			GRAPHICS_FLOAT_FIXED x0 = GraphicsFixedFromInteger(spans[0].x);
			GRAPHICS_FLOAT_FIXED x1 = GraphicsFixedFromInteger(spans[1].x);
			GRAPHICS_LINE left = {{x0, top}, {x0, bottom}},
							right = {{x1, top}, {x1, bottom}};
			GraphicsTrapsAddTrap(r->traps, top, bottom, &left, &right);
		}
		spans++;
	} while(--num_spans > 1);

	return GRAPHICS_STATUS_SUCCESS;
}

eGRAPHICS_INTEGER_STATUS GraphicsRasterisePolygonToTraps(
	GRAPHICS_POLYGON* polygon,
	eGRAPHICS_FILL_RULE fill_rule,
	eGRAPHICS_ANTIALIAS antialias,
	GRAPHICS_TRAPS* traps
)
{
	GRAPHICS_TRAP_RENDERER renderer;
	GRAPHICS_MONO_SCAN_CONVERTER converter;
	eGRAPHICS_INTEGER_STATUS status;
	GRAPHICS_RECTANGLE_INT r;

	renderer.traps = traps;
	renderer.base.render_rows = SpanToTraps;

	GraphicsBoxRoundToRectangle(&polygon->extents, &r);
	(void)InitializeGraphicsMonoScanConverter(&converter, r.x, r.y,
												r.x + r.width, r.y + r.height, fill_rule);
	status = GraphicsMonoScanConvereterAddPolygon(&converter, polygon);
	if(LIKELY(status == GRAPHICS_INTEGER_STATUS_SUCCESS))
	{
		status = converter.base.generate(&converter, &renderer.base);
	}
	converter.base.destroy(&converter);
	return status;
}

#ifdef __cplusplus
}
#endif
