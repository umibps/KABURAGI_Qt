/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/*
 * Copyright (c) 2011  Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include <stdlib.h>
#include <string.h>
#include <limits.h>
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
#include "graphics_scan_converter_private.h"
#include "graphics_inline.h"
#include "types.h"

#define I(x) GraphicsFixedIntegerRoundDown(x)

/* Compute the floored division a/b. Assumes / and % perform symmetric
 * division. */
INLINE static MONO_SCAN_QUOREM FlooredDivrem(int a, int b)
{
	MONO_SCAN_QUOREM qr;
	qr.quo = a/b;
	qr.rem = a%b;
	if((a^b)<0 && qr.rem)
	{
		qr.quo -= 1;
		qr.rem += b;
	}
	return qr;
}

/* Compute the floored division (x*a)/b. Assumes / and % perform symmetric
 * division. */
static MONO_SCAN_QUOREM FlooredMulDivrem(int x, int a, int b)
{
	MONO_SCAN_QUOREM qr;
	long long xa = (long long)x*a;
	qr.quo = xa/b;
	qr.rem = xa%b;
	if((xa>=0) != (b>=0) && qr.rem)
	{
		qr.quo -= 1;
		qr.rem += b;
	}
	return qr;
}

static eGRAPHICS_STATUS InitializePolygon(MONO_SCAN_POLYGON* polygon, int ymin, int ymax)
{
	unsigned h = ymax - ymin + 1;

	polygon->y_buckets = polygon->y_buckets_embedded;
	if(h > sizeof(polygon->y_buckets_embedded) / sizeof(polygon->y_buckets[0]))
	{
		polygon->y_buckets = (MONO_SCAN_EDGE**)MEM_ALLOC_FUNC(
								sizeof(*polygon->y_buckets) * h);
		if(UNLIKELY(NULL == polygon->y_buckets))
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}
	}
	(void)memset(polygon->y_buckets, 0, h * sizeof(MONO_SCAN_EDGE*));
	polygon->y_buckets[h-1] = (void *)-1;

	polygon->ymin = ymin;
	polygon->ymax = ymax;
	return GRAPHICS_STATUS_SUCCESS;
}

static void PolygonFinish(MONO_SCAN_POLYGON* polygon)
{
	if(polygon->y_buckets != polygon->y_buckets_embedded)
	{
		MEM_FREE_FUNC(polygon->y_buckets);
	}

	if(polygon->edges != polygon->edges_embedded)
	{
		MEM_FREE_FUNC(polygon->edges);
	}
}

static void PolygonInsertEdgeIntoItsY_Bucket(
	MONO_SCAN_POLYGON* polygon,
	MONO_SCAN_EDGE* e,
	int y
)
{
	MONO_SCAN_EDGE **ptail = &polygon->y_buckets[y - polygon->ymin];
	if(*ptail)
	{
		(*ptail)->prev = e;
	}
	e->next = *ptail;
	e->prev = NULL;
	*ptail = e;
}

INLINE static void PolygonAddEdge(
	MONO_SCAN_POLYGON* polygon,
	const GRAPHICS_EDGE* edge
)
{
	MONO_SCAN_EDGE *e;
	GRAPHICS_FLOAT_FIXED dx;
	GRAPHICS_FLOAT_FIXED dy;
	int y, ytop, ybot;
	int ymin = polygon->ymin;
	int ymax = polygon->ymax;

	y = I(edge->top);
	ytop = MAXIMUM(y, ymin);

	y = I(edge->bottom);
	ybot = MINIMUM(y, ymax);

	if(ybot <= ytop)
	{
		return;
	}

	e = polygon->edges + polygon->num_edges++;
	e->height_left = ybot - ytop;
	e->direction = edge->direction;

	dx = edge->line.point2.x - edge->line.point1.x;
	dy = edge->line.point2.y - edge->line.point1.y;

	if(dx == 0)
	{
		e->vertical = TRUE;
		e->x.quo = edge->line.point1.x;
		e->x.rem = 0;
		e->dxdy.quo = 0;
		e->dxdy.rem = 0;
		e->dy = 0;
	}
	else
	{
		e->vertical = FALSE;
		e->dxdy = FlooredMulDivrem(dx, GRAPHICS_FIXED_ONE, dy);
		e->dy = dy;

		e->x = FlooredMulDivrem(ytop * GRAPHICS_FIXED_ONE + GRAPHICS_FIXED_FRAC_MASK/2 - edge->line.point1.y,
									dx, dy);
		e->x.quo += edge->line.point1.x;
	}
	e->x.rem -= dy;

	PolygonInsertEdgeIntoItsY_Bucket (polygon, e, ytop);
}

static MONO_SCAN_EDGE* MergeSortedEdges(MONO_SCAN_EDGE* head_a, MONO_SCAN_EDGE* head_b)
{
	MONO_SCAN_EDGE *head, **next, *prev;
	int32 x;

	prev = head_a->prev;
	next = &head;
	if(head_a->x.quo <= head_b->x.quo)
	{
		head = head_a;
	}
	else
	{
		head = head_b;
		head_b->prev = prev;
		goto start_with_b;
	}

	do
	{
		x = head_b->x.quo;
		while(head_a != NULL && head_a->x.quo <= x)
		{
			prev = head_a;
			next = &head_a->next;
			head_a = head_a->next;
		}

		head_b->prev = prev;
		*next = head_b;
		if(head_a == NULL)
		{
			return head;
		}
start_with_b:
		x = head_a->x.quo;
		while(head_b != NULL && head_b->x.quo <= x)
		{
			prev = head_b;
			next = &head_b->next;
			head_b = head_b->next;
		}

		head_a->prev = prev;
		*next = head_a;
		if(head_b == NULL)
		{
			return head;
		}
	} while(1);
}

static MONO_SCAN_EDGE* SortEdges(
	MONO_SCAN_EDGE* list,
	unsigned int level,
	MONO_SCAN_EDGE** head_out
)
{
	MONO_SCAN_EDGE *head_other, *remaining;
	unsigned int i;

	head_other = list->next;

	if(head_other == NULL)
	{
		*head_out = list;
		return NULL;
	}

	remaining = head_other->next;
	if(list->x.quo <= head_other->x.quo)
	{
		*head_out = list;
		head_other->next = NULL;
	}
	else
	{
		*head_out = head_other;
		head_other->prev = list->prev;
		head_other->next = list;
		list->prev = head_other;
		list->next = NULL;
	}

	for(i = 0; i < level && remaining; i++)
	{
		remaining = SortEdges(remaining, i, &head_other);
		*head_out = MergeSortedEdges(*head_out, head_other);
	}

	return remaining;
}

static MONO_SCAN_EDGE* MergeUnsortedEdges(MONO_SCAN_EDGE* head, MONO_SCAN_EDGE* unsorted)
{
	SortEdges(unsorted, UINT_MAX, &unsorted);
	return MergeSortedEdges(head, unsorted);
}

INLINE static void ActiveListMergeEdges(MONO_SCAN_CONVERTER* c, MONO_SCAN_EDGE* edges)
{
	MONO_SCAN_EDGE *e;

	for(e = edges; c->is_vertical && e; e = e->next)
	{
		c->is_vertical = e->vertical;
	}

	c->head.next = MergeUnsortedEdges(c->head.next, edges);
}

INLINE static void AddSpan(MONO_SCAN_CONVERTER* c, int x1, int x2)
{
	int n;

	if(x1 < c->xmin)
	{
		x1 = c->xmin;
	}
	if(x2 > c->xmax)
	{
		x2 = c->xmax;
	}
	if(x2 <= x1)
	{
		return;
	}

	n = c->num_spans++;
	c->spans[n].x = x1;
	c->spans[n].coverage = 255;

	n = c->num_spans++;
	c->spans[n].x = x2;
	c->spans[n].coverage = 0;
}

INLINE static void Row(MONO_SCAN_CONVERTER* c, unsigned int mask)
{
	MONO_SCAN_EDGE *edge = c->head.next;
	int xstart = INT_MIN, prev_x = INT_MIN;
	int winding = 0;

	c->num_spans = 0;
	while(&c->tail != edge)
	{
		MONO_SCAN_EDGE *next = edge->next;
		int xend = I(edge->x.quo);

		if(--edge->height_left)
		{
			if(!edge->vertical)
			{
				edge->x.quo += edge->dxdy.quo;
				edge->x.rem += edge->dxdy.rem;
				if(edge->x.rem >= 0)
				{
					++edge->x.quo;
					edge->x.rem -= edge->dy;
				}
			}

			if(edge->x.quo < prev_x)
			{
				MONO_SCAN_EDGE *pos = edge->prev;
				pos->next = next;
				next->prev = pos;
				do
				{
					pos = pos->prev;
				} while(edge->x.quo < pos->x.quo);
				pos->next->prev = edge;
				edge->next = pos->next;
				edge->prev = pos;
				pos->next = edge;
			}
			else
			{
				prev_x = edge->x.quo;
			}
		}
		else
		{
			edge->prev->next = next;
			next->prev = edge->prev;
		}

		winding += edge->direction;
		if((winding & mask) == 0)
		{
			if(I(next->x.quo) > xend + 1)
			{
				AddSpan(c, xstart, xend);
				xstart = INT_MIN;
			}
		}
		else if(xstart == INT_MIN)
		{
			xstart = xend;
		}

		edge = next;
	}
}

INLINE static void Decrement(MONO_SCAN_EDGE* e, int h)
{
	e->height_left -= h;
	if(e->height_left == 0)
	{
		e->prev->next = e->next;
		e->next->prev = e->prev;
	}
}

static eGRAPHICS_STATUS InitializeMonoScanConverter(
	MONO_SCAN_CONVERTER* c,
	int xmin, int ymin,
	int xmax, int ymax
)
{
	eGRAPHICS_STATUS status;
	int max_num_spans;

	status = InitializePolygon(c->polygon, ymin, ymax);
	if(UNLIKELY(status))
	{
		return status;
	}

	max_num_spans = xmax - xmin + 1;
	if(max_num_spans > sizeof(c->spans_embedded) / sizeof(c->spans_embedded[0]))
	{
		c->spans = MEM_ALLOC_FUNC(
			max_num_spans * sizeof(GRAPHICS_HALF_OPEN_SPAN));
		if(UNLIKELY(c->spans == NULL))
		{
			PolygonFinish(c->polygon);
			return GRAPHICS_STATUS_NO_MEMORY;
		}
	}
	else
	{
		c->spans = c->spans_embedded;
	}

	c->xmin = xmin;
	c->xmax = xmax;
	c->ymin = ymin;
	c->ymax = ymax;

	c->head.vertical = 1;
	c->head.height_left = INT_MAX;
	c->head.x.quo = GraphicsFixedFromInteger(GraphicsFixedIntegerPart(INT_MIN));
	c->head.prev = NULL;
	c->head.next = &c->tail;
	c->tail.prev = &c->head;
	c->tail.next = NULL;
	c->tail.x.quo = GraphicsFixedFromInteger(GraphicsFixedIntegerPart(INT_MIN));
	c->tail.height_left = INT_MAX;
	c->tail.vertical = 1;

	c->is_vertical = 1;
	return GRAPHICS_STATUS_SUCCESS;
}

static void MonoScanConverterFinish(MONO_SCAN_CONVERTER* self)
{
	if(self->spans != self->spans_embedded)
	{
		MEM_FREE_FUNC(self->spans);
	}

	PolygonFinish(self->polygon);
}

static eGRAPHICS_STATUS MonoScanConverterAllocateEdges(
	MONO_SCAN_CONVERTER* c,
	int num_edges
)
{
	c->polygon->num_edges = 0;
	c->polygon->edges = c->polygon->edges_embedded;
	if(num_edges > sizeof(c->polygon->edges_embedded) / sizeof(c->polygon->edges_embedded[0]))
	{
		c->polygon->edges = MEM_ALLOC_FUNC(num_edges * sizeof(MONO_SCAN_EDGE));
		if(UNLIKELY(c->polygon->edges == NULL))
		{
			return GRAPHICS_STATUS_NO_MEMORY;
		}
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static void MonoScanConverterAddEdge(
	MONO_SCAN_CONVERTER* c,
	const GRAPHICS_EDGE* edge
)
{
	PolygonAddEdge(c->polygon, edge);
}

static void StepEdges(MONO_SCAN_CONVERTER* c, int count)
{
	MONO_SCAN_EDGE *edge;

	for(edge = c->head.next; edge != &c->tail; edge = edge->next)
	{
		edge->height_left -= count;
		if(! edge->height_left)
		{
			edge->prev->next = edge->next;
			edge->next->prev = edge->prev;
		}
	}
}

static eGRAPHICS_STATUS MonoScanConverterRender(
	MONO_SCAN_CONVERTER* c,
	unsigned int winding_mask,
	GRAPHICS_SPAN_RENDERER* renderer
)
{
	MONO_SCAN_POLYGON *polygon = c->polygon;
	int i, j, h = c->ymax - c->ymin;
	eGRAPHICS_STATUS status;

	for (i = 0; i < h; i = j)
	{
		j = i + 1;

		if(polygon->y_buckets[i])
		{
			ActiveListMergeEdges (c, polygon->y_buckets[i]);
		}

		if(c->is_vertical)
		{
			int min_height;
			MONO_SCAN_EDGE *e;

			e = c->head.next;
			min_height = e->height_left;
			while(e != &c->tail)
			{
				if(e->height_left < min_height)
				{
					min_height = e->height_left;
				}
				e = e->next;
			}

			while(--min_height >= 1 && polygon->y_buckets[j] == NULL)
			{
				j++;
			}
			if(j != i + 1)
			{
				StepEdges (c, j - (i + 1));
			}
		}

		Row(c, winding_mask);
		if(c->num_spans)
		{
			status = renderer->render_rows(renderer, c->ymin+i, j-i,
											c->spans, c->num_spans);
			if(UNLIKELY(status))
			{
				return status;
			}
		}

		/* XXX recompute after dropping edges? */
		if(c->head.next == &c->tail)
		{
			c->is_vertical = 1;
		}
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static void GraphicsMonoScanConverterDestroy(void* converter)
{
	GRAPHICS_MONO_SCAN_CONVERTER *self = (GRAPHICS_MONO_SCAN_CONVERTER*)converter;
	MonoScanConverterFinish(self->converter);
	MEM_FREE_FUNC(self);
}

eGRAPHICS_STATUS GraphicsMonoScanConvereterAddPolygon(
	void* converter,
	const GRAPHICS_POLYGON* polygon
)
{
	GRAPHICS_MONO_SCAN_CONVERTER *self = (GRAPHICS_MONO_SCAN_CONVERTER*)converter;
	eGRAPHICS_STATUS status;
	int i;

#if 0
	FILE *file = fopen ("polygon.txt", "w");
	_cairo_debug_print_polygon (file, polygon);
	fclose (file);
#endif

	status = MonoScanConverterAllocateEdges(self->converter,
											polygon->num_edges);
	if(UNLIKELY(status))
	{
		return status;
	}

	for(i = 0; i < polygon->num_edges; i++)
	{
		MonoScanConverterAddEdge (self->converter, &polygon->edges[i]);
	}

	return GRAPHICS_STATUS_SUCCESS;
}

static eGRAPHICS_STATUS GraphicsMonoScanConverterGenerate(
	void* converter,
	GRAPHICS_SPAN_RENDERER* renderer
)
{
	GRAPHICS_MONO_SCAN_CONVERTER *self = (GRAPHICS_MONO_SCAN_CONVERTER*)converter;

	return MonoScanConverterRender(self->converter,
							self->fill_rule == GRAPHICS_FILL_RULE_WINDING ? ~0 : 1,
																		renderer);
}

eGRAPHICS_STATUS InitializeGraphicsMonoScanConverter(
	GRAPHICS_MONO_SCAN_CONVERTER* converter,
	int			xmin,
	int			ymin,
	int			xmax,
	int			ymax,
	eGRAPHICS_FILL_RULE fill_rule
)
{
	eGRAPHICS_STATUS status;

	converter->base.destroy = GraphicsMonoScanConverterDestroy;
	converter->base.generate = GraphicsMonoScanConverterGenerate;

	status = InitializeMonoScanConverter(converter->converter, xmin, ymin, xmax, ymax);
	if(UNLIKELY(status))
	{
		goto bail;
	}

	converter->fill_rule = fill_rule;

	return status;

bail:
	converter->base.destroy(&converter->base);
	return status;
}

GRAPHICS_SCAN_CONVERTER* GraphicsMonoScanConverterCreate(
	int xmin,
	int ymin,
	int xmax,
	int ymax,
	eGRAPHICS_FILL_RULE fill_rule
)
{
	GRAPHICS_MONO_SCAN_CONVERTER *converter;
	eGRAPHICS_STATUS status;

	converter = (GRAPHICS_MONO_SCAN_CONVERTER*)MEM_ALLOC_FUNC(sizeof(GRAPHICS_MONO_SCAN_CONVERTER));
	if(UNLIKELY(converter == NULL))
	{
		status = GRAPHICS_STATUS_NO_MEMORY;
		goto bail_nomem;
	}

	status = InitializeMonoScanConverter(converter->converter,
								xmin, ymin, xmax, ymax);
	if(UNLIKELY(status))
	{
		goto bail;
	}

	converter->fill_rule = fill_rule;

	return &converter->base;

bail:
	converter->base.destroy(&converter->base);
bail_nomem:
	return NULL;
}
