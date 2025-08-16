#include <string.h>
#include <limits.h>
#include "pixel_manipulate.h"
#include "pixel_manipulate_region.h"
#include "pixel_manipulate_private.h"
#include "memory.h"

#define FREE_DATA(REGION) if((REGION)->data != NULL && (REGION)->data->size != 0) MEM_FREE_FUNC((REGION)->data)
#define EXTENT_CHECK(r1, r2) (!(((r1)->x2 <= (r2)->x1) || \
								((r1)->x1 >= (r2)->x2) || \
								((r1)->y2 <= (r2)->y1) || \
								((r1)->y1 >= (r2)->y2)))
#define SUB_SUMES(r1, r2) (((r1)->x1 <= (r2)->x1) && \
							((r1)->x2 >= (r2)->x2) && \
							((r1)->y1 <= (r2)->y1) && \
							((r1)->y2 >= (r2)->y2))
#define NAR(region) ((region)->data->size == 0 && (region)->data->num_rects == 0)
#define RECTS(region) ((region)->data != NULL ? (PIXEL_MANIPULATE_BOX32*)((region)->data + 1) : &(region)->extents)
#define BOX(region, i) (BOX_POINTER(&(region)[i]))
#define END(reg) BOX(reg, (reg)->data->num_rects - 1)
#define TOP(region) BOX(region, (region)->data->num_rects)
#define FIND_BAND(r, r_band_end, r_end, r_y1) \
	do  \
	{   \
		r_y1 = (r)->y1; \
		r_band_end = (r) + 1; \
		while((r_band_end != r_end) && (r_band_end->y1 == r_y1))   \
		{   \
			r_band_end++;   \
		}   \
	} while(0)
#define RECTANGLE_ALLOCATE(region, n)   \
	do  \
	{   \
		if(!(region)->data != NULL || (((region)->data->num_rects + (n)) > (region)->data->size != 0))  \
		{   \
			if(FALSE == PixelManipulateRectangleAllocate(region, n)) \
			{   \
				return FALSE;   \
			}   \
		}   \
	} while(0)
#define RECTANGLE_ALLOCATE_BAIL(region, n, bail)	\
	do  \
	{   \
		if((region)->data == NULL || (((region)->data->num_rects + (n)) > (region)->data->size))	\
		{   \
			if(FALSE == PixelManipulateRectangleAllocate(region, n))	\
			{   \
				goto bail;  \
			}   \
		}   \
	} while(0)
#define ADD_RECTANGLE(next_rect, nx1, ny1, nx2, ny2)	\
	do  \
	{   \
		(next_rect)->x1 = nx1;  \
		(next_rect)->y1 = ny1;  \
		(next_rect)->x2 = nx2;  \
		(next_rect)->y2 = ny2;  \
		(next_rect)++;  \
	} while(0)
#define NEW_RECTANGLE(region, next_rectangle, nx1, ny1, nx2, ny2)   \
	do  \
	{   \
		if((region)->data == NULL || ((region)->data->num_rects == (region)->data->size))   \
		{   \
			if(FALSE == PixelManipulateRectangleAllocate(region, 1))	\
			{   \
				return FALSE;   \
			}   \
			next_rectangle = TOP(region);   \
		}   \
		ADD_RECTANGLE(next_rectangle, nx1, ny1, nx2, ny2);  \
		region->data->num_rects++;  \
	} while(0)
#define MERGE_RECTANGLE(r)							\
	do									\
	{									\
		if (r->x1 <= x2)						\
	{								\
			/* Merge with current rectangle */				\
			if (x2 < r->x2)						\
		x2 = r->x2;						\
	}								\
	else								\
	{								\
			/* Add current rectangle, start new one */			\
			NEW_RECTANGLE(region, next_rect, x1, y1, x2, y2);		\
			x1 = r->x1;							\
			x2 = r->x2;							\
	}								\
		r++;								\
	} while (0)
#define COALESCE(new_region, previous_band, current_band)   \
	do  \
	{   \
		if(current_band - previous_band == new_region->data->num_rects - current_band)  \
		{   \
			previous_band = PixelManipulateCoalesce(new_region, previous_band, current_band);   \
		}   \
		else	\
		{   \
			previous_band = current_band;   \
		}   \
	} while(0)
#define APPEND_REGIONS(new_region, r, r_end)	\
	do  \
	{   \
		int new_rectangles; \
		if((new_rectangles = r_end - r) != 0)   \
		{   \
			RECTANGLE_ALLOCATE_BAIL(new_region, new_rectangles, bail);  \
			(void)memmove(TOP(new_region), r, new_rectangles * sizeof(PIXEL_MANIPULATE_BOX32));  \
			(new_region)->data->num_rects += new_rectangles; \
		}   \
	} while(0)
#define DOWN_SIZE(region, num_rects)	\
	do  \
	{   \
		if(((num_rects) < ((region)->data->size >> 1)) && ((region)->data->size > 50))  \
		{   \
			PIXEL_MANIPULATE_REGION32_DATA *new_data;	\
			size_t data_size = (num_rects * sizeof(PIXEL_MANIPULATE_BOX32)) + sizeof(PIXEL_MANIPULATE_REGION32);	\
			\
			if(data_size == 0)  \
			{   \
				new_data = NULL;	\
			}   \
			else	\
			{   \
				new_data = (PIXEL_MANIPULATE_REGION32*)MEM_REALLOC_FUNC((region)->data, data_size); \
			}   \
			\
			if(new_data != NULL)	\
			{   \
				new_data->size = (num_rects);   \
				(region)->data = new_data;  \
			}   \
		}   \
	} while(0)

#define GOOD_RECT(rect) ((rect)->x1 < (rect)->x2 && (rect)->y1 < (rect)->y2)
#define BAD_RECT(rect) ((rect)->x1 > (rect)->x2 || (rect)->y1 > (rect)->y2)

#define REGION_RECTANGLES(reg) ((reg)->data != NULL ? (PIXEL_MANIPULATE_BOX32*)((reg)->data + 1) : &(reg)->extents)

#ifdef __cplusplus
extern "C" {
#endif

void PixelManipulateRegion32Finish(PIXEL_MANIPULATE_REGION32* region)
{
	// FREE_DATA(region);
	if(region->data != &region->data_embedded && region->data != NULL
		&& region->data != PIXEL_MANIPULATE_EMPTY_DATA)
	{
		MEM_FREE_FUNC(region->data);
	}
}

INLINE void PixelManipulateResetClipRegion(PIXEL_MANIPULATE_IMAGE* image)
{
	image->common.have_clip_region = FALSE;
}

static INLINE int PixelManipulateCoalesce(PIXEL_MANIPULATE_REGION32* region, int previous_start, int current_start)
{
	PIXEL_MANIPULATE_BOX32 *previous_box;
	PIXEL_MANIPULATE_BOX32 *current_box;
	int num_rects;
	int y2;

	num_rects = current_start - previous_start;

	if(num_rects == 0)
	{
		return current_start;
	}

	previous_box = ((BOX(region, previous_start)));
	current_box = ((BOX(region, current_start)));
	if(previous_box->y2 != current_box->y1)
	{
		return current_start;
	}

	y2 = current_box->y2;
	do
	{
		if((previous_box->x1 != current_box->x1) || (previous_box->x2 != current_box->x2))
		{
			return (current_start);
		}
		previous_box++;
		current_box++;
		num_rects--;
	} while(num_rects != 0);

	region->data->num_rects -= num_rects;

	do
	{
		previous_box--;
		previous_box->y2 = y2;
		num_rects--;
	} while(num_rects != 0);

	return previous_start;
}

static INLINE PIXEL_MANIPULATE_REGION32* AllocationData(size_t n)
{
	return (PIXEL_MANIPULATE_REGION32*)MEM_ALLOC_FUNC(sizeof(PIXEL_MANIPULATE_BOX32)*(n+1)+sizeof(PIXEL_MANIPULATE_REGION32));
}

static int PixelManipulateBreak(PIXEL_MANIPULATE_REGION32* region)
{
	const PIXEL_MANIPULATE_BOX32 clear_extents = {0};
	FREE_DATA(region);

	region->extents = clear_extents;
	region->data = PIXEL_MANIPULATE_BROKEN_DATA;

	return FALSE;
}

int PixelManipulateRegion32Copy(PIXEL_MANIPULATE_REGION32* dest, PIXEL_MANIPULATE_REGION32* src)
{
	if(dest == src)
	{
		return TRUE;
	}

	dest->extents = src->extents;

	if(src->data == NULL || src->data->size == 0)
	{
		FREE_DATA(dest);
		dest->data = src->data;
		return TRUE;
	}

	if(dest->data == NULL || (dest->data->size < src->data->num_rects))
	{
		FREE_DATA(dest);
		dest->data = AllocationData(src->data->num_rects);

		if(dest->data == NULL)
		{
			return FALSE;
		}
		dest->data->size = src->data->num_rects;
	}

	dest->data->num_rects = src->data->num_rects;

	(void)memmove(BOX_POINTER(dest), BOX_POINTER(src),
		dest->data->num_rects * sizeof(PIXEL_MANIPULATE_BOX32));

	return TRUE;
}

void PixelManipulateRegion32Translate(PIXEL_MANIPULATE_REGION32* region, int x, int y)
{
	int64 x1, x2, y1, y2;
	int num_box;
	PIXEL_MANIPULATE_BOX32 *pbox;

	region->extents.x1 = x1 = region->extents.x1 + x;
	region->extents.y1 = y1 = region->extents.y1 + y;
	region->extents.x2 = x2 = region->extents.x2 + x;
	region->extents.y2 = y2 = region->extents.y2 + y;

	if(((x1 - INT_MIN) | (y1 - INT_MIN) | (INT_MAX - x2) | (INT_MAX - y2)) >= 0)
	{
		if(region->data != NULL && (num_box = region->data->num_rects) != 0)
		{
			for(pbox = BOX_POINTER(region); num_box--; pbox++)
			{
				pbox->x1 += x;
				pbox->y1 += y;
				pbox->x2 += x;
				pbox->y2 += y;
			}
		}
		return;
	}

	if(((x2 - INT_MIN) | (y2 - INT_MIN) | (INT_MAX - x1) | (INT_MAX - y1)) <= 0)
	{
		region->extents.x2 = region->extents.x1;
		region->extents.y2 = region->extents.y1;
		FREE_DATA(region);
		region->data = NULL;
		return;
	}

	if(x1 < INT_MIN)
	{
		region->extents.x1 = INT_MIN;
	}
	else if(x2 > INT_MAX)
	{
		region->extents.x2 = INT_MAX;
	}

	if(y1 < INT_MIN)
	{
		region->extents.y1 = INT_MIN;
	}
	else if(y2 > INT_MAX)
	{
		region->extents.y2 = INT_MAX;
	}

	if(region->data != NULL && (num_box = region->data->num_rects) != 0)
	{
		PIXEL_MANIPULATE_BOX32 *pbox_out;

		for(pbox_out = pbox = (PIXEL_MANIPULATE_BOX32*)(region->data+1); num_box--; pbox++)
		{
			pbox_out->x1 = x1 = pbox->x1 + x;
			pbox_out->y1 = y1 = pbox->y1 + y;
			pbox_out->x2 = x2 = pbox->x2 + x;
			pbox_out->y2 = y2 = pbox->y2 + y;

			if(((x2 - INT_MIN) | (y2 - INT_MIN) | (INT_MAX - x1) | (INT_MAX - y1)) <= 0)
			{
				region->data->num_rects--;
				continue;
			}

			if(x1 < INT_MIN)
			{
				pbox->x1 = INT_MIN;
			}
			else if(x2 > INT_MAX)
			{
				pbox->x2 = INT_MAX;
			}

			if(y1 < INT_MIN)
			{
				pbox->y1 = INT_MIN;
			}
			else if(y2 > INT_MAX)
			{
				pbox->y2 = INT_MAX;
			}

			pbox_out++;
		}

		if(pbox_out != pbox)
		{
			if(region->data->num_rects == 1)
			{
				region->extents = (*(PIXEL_MANIPULATE_BOX32*)(region->data+1));
				FREE_DATA(region);
				region->data = NULL;
			}
			else
			{
				PixelManipulateSetExtents(region);
			}
		}
	}
}

static int PixelManipulateRectangleAllocate(PIXEL_MANIPULATE_REGION32* region, int n)
{
	PIXEL_MANIPULATE_REGION32 *data;

	if(region->data == NULL)
	{
		n++;
		region->data = AllocationData(n);

		if(region->data == NULL)
		{
			return FALSE;
		}

		region->data->num_rects = 1;
		*BOX_POINTER(region) = region->extents;
	}
	else if(region->data->size == 0)
	{
		region->data = AllocationData(n);

		if(region->data == NULL)
		{
			return FALSE;
		}
		region->data->num_rects = 0;
	}
	else
	{
		size_t data_size;

		if(n == 1)
		{
			n = region->data->num_rects;
			if(n > 500)
			{
				n = 250;
			}
		}

		n += region->data->num_rects;
		data_size = sizeof(PIXEL_MANIPULATE_BOX32) * n + sizeof(PIXEL_MANIPULATE_REGION32);

		if(data_size == 0)
		{
			data = NULL;
		}
		else
		{
			if(region->data != &region->data_embedded)
			{
				data = (PIXEL_MANIPULATE_REGION32*)MEM_REALLOC_FUNC(region->data, data_size);
				if(data == NULL)
				{
					return FALSE;
				}
			}
			else
			{
				data = (PIXEL_MANIPULATE_REGION32*)MEM_ALLOC_FUNC(data_size);
				if(data == NULL)
				{
					return FALSE;
				}
				(void)memcpy(data, region->data, region->data->size * sizeof(PIXEL_MANIPULATE_REGION32_DATA));
			}
		}

		region->data = data;
	}

	region->data->size = n;

	return TRUE;
}

static INLINE int PixelManipulateRegionAppendNonZero(
	PIXEL_MANIPULATE_REGION32* region,
	PIXEL_MANIPULATE_BOX32* r,
	PIXEL_MANIPULATE_BOX32* r_end,
	int y1,
	int y2
)
{
	PIXEL_MANIPULATE_BOX32 *next_rectangle;
	int new_rectangles;

	new_rectangles = r_end - r;

	RECTANGLE_ALLOCATE(region, new_rectangles);
	next_rectangle = TOP(region);
	region->data->num_rects += new_rectangles;

	do
	{
		ADD_RECTANGLE(next_rectangle, r->x1, y1, r->x2, y2);
		r++;
	} while(r != r_end);

	return TRUE;
}

typedef int (*overlap_process)(
	PIXEL_MANIPULATE_REGION32* region,
	PIXEL_MANIPULATE_BOX32* r1,
	PIXEL_MANIPULATE_BOX32* r1_end,
	PIXEL_MANIPULATE_BOX32* r2,
	PIXEL_MANIPULATE_BOX32* r2_end,
	int y1,
	int y2
);

static int PixelManipulateOperate(
	PIXEL_MANIPULATE_REGION32* new_region,
	PIXEL_MANIPULATE_REGION32* region1,
	PIXEL_MANIPULATE_REGION32* region2,
	overlap_process overlap_function,
	int append_non1,
	int append_non2
)
{
	PIXEL_MANIPULATE_BOX32 *r1;
	PIXEL_MANIPULATE_BOX32 *r2;
	PIXEL_MANIPULATE_BOX32 *r1_end;
	PIXEL_MANIPULATE_BOX32 *r2_end;
	int y_bottom;
	int y_top;
	PIXEL_MANIPULATE_REGION32 *old_data;
	int previous_band;
	int current_band;
	PIXEL_MANIPULATE_BOX32 *r1_band_end;
	PIXEL_MANIPULATE_BOX32 *r2_band_end;
	int top;
	int bottom;
	int r1_y1;
	int r2_y1;
	int new_size;
	int num_rects;

	if(NAR(region1) || NAR(region2))
	{
		return PixelManipulateBreak(new_region);
	}

	r1 = RECTS(region1);
	new_size = NUM_RECTS(region1);
	r1_end = r1 + new_size;

	num_rects = NUM_RECTS(region2);
	r2 = RECTS(region2);
	r2_end = r2 + num_rects;

	old_data = NULL;

	if(((new_region == region1) && (new_size > 1))
		|| ((new_region == region2) && (num_rects > 1)))
	{
		old_data = new_region->data;
		new_region->data->size = 0;
		new_region->data->num_rects = 0;
	}

	if(num_rects > new_size)
	{
		new_size = num_rects;
	}

	new_size <<= 1;

	if(new_region->data == NULL)
	{
		// Nothing to do
	}
	else if(new_region->data->size != 0)
	{
		new_region->data->num_rects = 0;
	}

	if(new_region->data == NULL || new_size > new_region->data->size != 0)
	{
		if(FALSE == PixelManipulateRectangleAllocate(new_region, new_size))
		{
			return FALSE;
		}
	}

	y_bottom = MINIMUM(r1->y1, r2->y1);

	previous_band = 0;

	do
	{
		FIND_BAND(r1, r1_band_end, r1_end, r1_y1);
		FIND_BAND(r1, r2_band_end, r2_end, r2_y1);

		if(r1_y1 < r2_y1)
		{
			if(append_non1)
			{
				top = MAXIMUM(r1_y1, y_bottom);
				bottom = MINIMUM(r1->y2, r2_y1);
				if(top != bottom)
				{
					current_band = new_region->data->num_rects;
					if(FALSE == PixelManipulateRegionAppendNonZero(new_region, r1, r1_band_end, top, bottom))
					{
						goto bail;
					}
					COALESCE(new_region, previous_band, current_band);
				}
			}
			y_top = r2_y1;
		}
		else if(r2_y1 < r1_y1)
		{
			if(append_non2)
			{
				top = MAXIMUM(r2_y1, y_bottom);
				bottom = MINIMUM(r2->y2, r1_y1);

				if(top != bottom)
				{
					current_band = new_region->data->num_rects;

					if(FALSE == PixelManipulateRegionAppendNonZero(new_region, r2, r2_band_end, top, bottom))
					{
						goto bail;
					}
					COALESCE(new_region, previous_band, current_band);
				}
			}

			y_top = r1_y1;
		}
		else
		{
			y_top = r1_y1;
		}

		y_bottom = MINIMUM(r1->y2, r2->y2);
		if(y_bottom > y_top)
		{
			current_band = new_region->data->num_rects;

			if(FALSE == (*overlap_function)(new_region,
				r1, r1_band_end,
				r2, r2_band_end,
				y_top, y_bottom)
			)
			{
				goto bail;
			}
			COALESCE(new_region, previous_band, current_band);
		}

		if(r1->y2 == y_bottom)
		{
			r1 = r1_band_end;
		}

		if(r2->y2 == y_bottom)
		{
			r2 = r2_band_end;
		}
	} while(r1 != r1_end && r2 != r2_end);

	if((r1 != r1_end) && append_non1)
	{
		FIND_BAND(r1, r1_band_end, r1_end, r1_y1);

		current_band = new_region->data->num_rects;

		if(FALSE == PixelManipulateRegionAppendNonZero(new_region,
			r1, r1_band_end, MAXIMUM(r1_y1, y_bottom), r1->y2))
		{
			goto bail;
		}
		COALESCE(new_region, previous_band, current_band);

		APPEND_REGIONS(new_region, r1_band_end, r1_end);
	}
	else if((r2 != r2_end) && append_non2)
	{
		FIND_BAND(r2, r2_band_end, r2_end, r2_y1);

		current_band = new_region->data->num_rects;

		if(FALSE == PixelManipulateRegionAppendNonZero(new_region,
			r2, r2_band_end, MAXIMUM(r2_y1, y_bottom), r2->y2))
		{
			goto bail;
		}

		COALESCE(new_region, previous_band, current_band);

		APPEND_REGIONS(new_region, r2_band_end, r2_end);
	}

	MEM_FREE_FUNC(old_data);

	if(0 == (num_rects = new_region->data->num_rects))
	{
		FREE_DATA(new_region);
		new_region->data->num_rects = 0;
		new_region->data->size = 0;
	}
	else if(num_rects == 1)
	{
		new_region->extents = (*BOX_POINTER(new_region));
		FREE_DATA(new_region);
		new_region->data = NULL;
	}
	else
	{
		DOWN_SIZE(new_region, num_rects);
	}

	return TRUE;

bail:
	MEM_FREE_FUNC(old_data);

	return FALSE;
}

static int PixelManipulateRegionIntersectO(
	PIXEL_MANIPULATE_REGION32* region,
	PIXEL_MANIPULATE_BOX32* r1,
	PIXEL_MANIPULATE_BOX32* r1_end,
	PIXEL_MANIPULATE_BOX32* r2,
	PIXEL_MANIPULATE_BOX32* r2_end,
	int y1,
	int y2
)
{
	int x1;
	int x2;
	PIXEL_MANIPULATE_BOX32 *next_rectangle;

	next_rectangle = TOP(region);

	do
	{
		x1 = MAXIMUM(r1->x1, r2->x1);
		x2 = MINIMUM(r1->x2, r2->x2);

		if(x1 < x2)
		{
			NEW_RECTANGLE(region, next_rectangle, x1, y1, x2, y2);
		}

		if(r1->x2 == x2)
		{
			r1++;
		}

		if(r2->x2 == x2)
		{
			r2++;
		}
	} while((r1 != r1_end) && (r2 != r2_end));

	return TRUE;
}

int PixelManipulateRegion32Intersect(
	PIXEL_MANIPULATE_REGION32* new_region,
	PIXEL_MANIPULATE_REGION32* region1,
	PIXEL_MANIPULATE_REGION32* region2
)
{
	if(region1->data == NULL || region2->data == NULL
		|| !EXTENT_CHECK(&region1->extents, &region2->extents))
	{
		FREE_DATA(new_region);
		new_region->extents.x2 = new_region->extents.x1;
		new_region->extents.y2 = new_region->extents.y1;
		if(NAR(region1) || NAR(region2))
		{
			new_region->data->size = 0;
			new_region->data->num_rects = 0;
			new_region->data->broken = 1;
			return FALSE;
		}
		else
		{
			new_region->data->size = 0;
			new_region->data->num_rects = 0;
			new_region->data->broken = 0;
		}
	}
	else if(region1->data == NULL && region2->data == NULL)
	{
		new_region->extents.x1 = MAXIMUM(region1->extents.x1, region2->extents.x1);
		new_region->extents.x1 = MAXIMUM(region1->extents.y1, region2->extents.y1);
		new_region->extents.x1 = MINIMUM(region1->extents.x2, region2->extents.x2);
		new_region->extents.x1 = MINIMUM(region1->extents.y2, region2->extents.y2);

		FREE_DATA(new_region);
		new_region->data = NULL;
	}
	else if(region2->data == NULL && SUB_SUMES(&region2->extents, &region1->extents))
	{
		return PixelManipulateRegion32Copy(new_region, region1);
	}
	else if(region1->data == NULL && SUB_SUMES(&region1->extents, &region2->extents))
	{
		return PixelManipulateRegion32Copy(new_region, region2);
	}
	else if(region1 == region2)
	{
		return PixelManipulateRegion32Copy(new_region, region1);
	}
	else
	{
		if(FALSE == PixelManipulateOperate(new_region, region1, region2,
			PixelManipulateRegionIntersectO, FALSE, FALSE))
		{
			return FALSE;
		}
		PixelManipulateSetExtents(new_region);
	}

	return TRUE;
}

int PixelManipulateRegion32IntersectRectangle(
	PIXEL_MANIPULATE_REGION32* dest,
	PIXEL_MANIPULATE_REGION32* source,
	int x,
	int y,
	unsigned int width,
	unsigned int height
)
{
	PIXEL_MANIPULATE_REGION32 region;

	region.data = NULL;
	region.extents.x1 = x;
	region.extents.y1 = y;
	region.extents.x2 = x + width;
	region.extents.y2 = y + height;

	return PixelManipulateRegion32Intersect(dest, source, &region);
}

void PixelManipulateSetExtents(PIXEL_MANIPULATE_REGION32* region)
{
	PIXEL_MANIPULATE_BOX32 *box, *box_end;

	if(region->data == NULL)
	{
		return;
	}

	if(region->data->size == 0)
	{
		region->extents.x2 = region->extents.x1;
		region->extents.y2 = region->extents.y1;
		return;
	}

	box = (PIXEL_MANIPULATE_BOX32*)(region->data+1);
	box_end = (PIXEL_MANIPULATE_BOX32*)(region->data+region->data->num_rects);

	region->extents.x1 = box->x1;
	region->extents.y1 = box->y1;
	region->extents.x2 = box_end->x2;
	region->extents.y2 = box_end->y2;

	if(region->extents.y1 < region->extents.y2)
	{
		return;
	}

	while(box <= box_end)
	{
		if(box->x1 < region->extents.x1)
		{
			region->extents.x1 = box->x1;
		}
		if(box->x2 > region->extents.x2)
		{
			region->extents.x2 = box->x2;
		}
		box++;
	}
}

#define EXCHANGE_RECTS(a, b)	\
	{						   \
		PIXEL_MANIPULATE_BOX32 t;		\
		t = rects[a];		   \
		rects[a] = rects[b];	\
		rects[b] = t;		   \
	}

static void quick_sort_rects(
	PIXEL_MANIPULATE_BOX32 rects[],
	int		numRects
)
{
	int y1;
	int x1;
	int i, j;
	PIXEL_MANIPULATE_BOX32 *r;

	/* Always called with numRects > 1 */

	do
	{
		if(numRects == 2)
		{
			if(rects[0].y1 > rects[1].y1 ||
				(rects[0].y1 == rects[1].y1 && rects[0].x1 > rects[1].x1))
			{
				EXCHANGE_RECTS (0, 1);
			}

			return;
		}

		/* Choose partition element, stick in location 0 */
		EXCHANGE_RECTS (0, numRects >> 1);
		y1 = rects[0].y1;
		x1 = rects[0].x1;

		/* Partition array */
		i = 0;
		j = numRects;

		do
		{
			r = &(rects[i]);
			do
			{
				r++;
				i++;
			} while (i != numRects && (r->y1 < y1 || (r->y1 == y1 && r->x1 < x1)));

			r = &(rects[j]);
			do
			{
				r--;
				j--;
			} while (y1 < r->y1 || (y1 == r->y1 && x1 < r->x1));
		
			if(i < j)
			{
				EXCHANGE_RECTS (i, j);
			}
		} while (i < j);

		/* Move partition element back to middle */
		EXCHANGE_RECTS (0, j);

		/* Recurse */
		if(numRects - j - 1 > 1)
		{
			quick_sort_rects (&rects[j + 1], numRects - j - 1);
		}
		numRects = j;
	}while(numRects > 1);
}

#define RECTALLOC_BAIL(region, n, bail)					\
	do									\
	{									\
		if (!(region)->data ||						\
			(((region)->data->num_rects + (n)) > (region)->data->size))	\
		{								\
			if (FALSE == PixelManipulateRectangleAllocate(region, n))				\
				goto bail;						\
		}								\
	} while (0)

static int PixelManipulateRegionUnionO(
	PIXEL_MANIPULATE_REGION32* region,
	PIXEL_MANIPULATE_BOX32* r1,
	PIXEL_MANIPULATE_BOX32* r1_end,
	PIXEL_MANIPULATE_BOX32* r2,
	PIXEL_MANIPULATE_BOX32* r2_end,
	int y1,
	int y2
)
{
	PIXEL_MANIPULATE_BOX32 *next_rect;
	int x1;			/* left and right side of current union */
	int x2;

	// critical_if_fail (y1 < y2);
	// critical_if_fail (r1 != r1_end && r2 != r2_end);

	next_rect = TOP(region);

	/* Start off current rectangle */
	if (r1->x1 < r2->x1)
	{
		x1 = r1->x1;
		x2 = r1->x2;
		r1++;
	}
	else
	{
		x1 = r2->x1;
		x2 = r2->x2;
		r2++;
	}
	while (r1 != r1_end && r2 != r2_end)
	{
		if (r1->x1 < r2->x1)
			MERGE_RECTANGLE(r1);
		else
			MERGE_RECTANGLE(r2);
	}

	/* Finish off whoever (if any) is left */
	if (r1 != r1_end)
	{
		do
		{
			MERGE_RECTANGLE(r1);
		}
		while (r1 != r1_end);
	}
	else if (r2 != r2_end)
	{
		do
		{
			MERGE_RECTANGLE(r2);
		}
		while (r2 != r2_end);
	}

	/* Add current rectangle */
	NEW_RECTANGLE(region, next_rect, x1, y1, x2, y2);

	return TRUE;
}

static int validate(PIXEL_MANIPULATE_REGION32* badreg)
{
	/* Descriptor for regions under construction  in Step 2. */
	typedef struct
	{
		PIXEL_MANIPULATE_REGION32 reg;
		int prev_band;
		int cur_band;
	} region_info_t;

	PIXEL_MANIPULATE_BOX32 empty = {0};

	region_info_t stack_regions[64];

	int numRects;				   /* Original numRects for badreg		*/
	region_info_t *ri;			  /* Array of current regions			*/
	int num_ri;					 /* Number of entries used in ri		*/
	int size_ri;					/* Number of entries available in ri	*/
	int i;						  /* Index into rects				*/
	int j;						  /* Index into ri				*/
	region_info_t *rit;			 /* &ri[j]					*/
	PIXEL_MANIPULATE_REGION32 *reg;			 /* ri[j].reg				*/
	PIXEL_MANIPULATE_BOX32 *box;				/* Current box in rects			*/
	PIXEL_MANIPULATE_BOX32 *ri_box;			 /* Last box in ri[j].reg			*/
	PIXEL_MANIPULATE_REGION32 *hreg;			/* ri[j_half].reg				*/
	int ret = TRUE;

	if(FALSE == badreg->data)
	{
		// GOOD(badreg);
		return TRUE;
	}
	
	numRects = badreg->data->num_rects;
	if (0 == numRects)
	{
		if(PIXEL_MANIPULATE_REGION_NAR(badreg))
		{
			return FALSE;
		}
		// GOOD(badreg);
		return TRUE;
	}
	
	if (badreg->extents.x1 < badreg->extents.x2)
	{
		if ((numRects) == 1)
		{
			FREE_DATA(badreg);
			badreg->data = (PIXEL_MANIPULATE_REGION32*) NULL;
		}
		else
		{
			DOWN_SIZE(badreg, numRects);
		}

		//GOOD(badreg);

		return TRUE;
	}

	/* Step 1: Sort the rects array into ascending (y1, x1) order */
	quick_sort_rects(BOX_POINTER(badreg), numRects);

	/* Step 2: Scatter the sorted array into the minimum number of regions */

	/* Set up the first region to be the first rectangle in badreg */
	/* Note that step 2 code will never overflow the ri[0].reg rects array */
	ri = stack_regions;
	size_ri = sizeof (stack_regions) / sizeof (stack_regions[0]);
	num_ri = 1;
	ri[0].prev_band = 0;
	ri[0].cur_band = 0;
	ri[0].reg = *badreg;
	box = BOX_POINTER(&ri[0].reg);
	ri[0].reg.extents = *box;
	ri[0].reg.data->num_rects = 1;
	badreg->extents = empty;
	badreg->data = PIXEL_MANIPULATE_EMPTY_DATA;

	/* Now scatter rectangles into the minimum set of valid regions.  If the
	 * next rectangle to be added to a region would force an existing rectangle
	 * in the region to be split up in order to maintain y-x banding, just
	 * forget it.  Try the next region.  If it doesn't fit cleanly into any
	 * region, make a new one.
	 */

	for(i = numRects; --i > 0;)
	{
		box++;
		/* Look for a region to append box to */
		for(j = num_ri, rit = ri; --j >= 0; rit++)
		{
			reg = &rit->reg;
			ri_box = END(reg);

			if(box->y1 == ri_box->y1 && box->y2 == ri_box->y2)
			{
				/* box is in same band as ri_box.  Merge or append it */
				if(box->x1 <= ri_box->x2)
				{
					/* Merge it with ri_box */
					if(box->x2 > ri_box->x2)
					{
						ri_box->x2 = box->x2;
					}
				}
				else
				{
					RECTALLOC_BAIL(reg, 1, bail);
					*TOP (reg) = *box;
					reg->data->num_rects++;
				}
		
				goto next_rect;   /* So sue me */
			}
			else if(box->y1 >= ri_box->y2)
			{
				/* Put box into new band */
				if (reg->extents.x2 < ri_box->x2)
				{
					reg->extents.x2 = ri_box->x2;
				}
				if (reg->extents.x1 > box->x1)
				{
					reg->extents.x1 = box->x1;
				}
				COALESCE(reg, rit->prev_band, rit->cur_band);
				rit->cur_band = reg->data->num_rects;
				RECTALLOC_BAIL(reg, 1, bail);
				*TOP (reg) = *box;
				reg->data->num_rects++;

				goto next_rect;
			}
			/* Well, this region was inappropriate.  Try the next one. */
		} /* for j */

		/* Uh-oh.  No regions were appropriate.  Create a new one. */
		if (size_ri == num_ri)
		{
			size_t data_size;

			/* Oops, allocate space for new region information */
			size_ri <<= 1;

			data_size = size_ri * sizeof(region_info_t);
			if (data_size / size_ri != sizeof(region_info_t))
				goto bail;

			if(ri == stack_regions)
			{
				rit = malloc (data_size);
				if (!rit)
					goto bail;
				(void)memcpy (rit, ri, num_ri * sizeof (region_info_t));
			}
			else
			{
				rit = (region_info_t *) realloc (ri, data_size);
				if (!rit)
					goto bail;
			}
			ri = rit;
			rit = &ri[num_ri];
		}
		num_ri++;
		rit->prev_band = 0;
		rit->cur_band = 0;
		rit->reg.extents = *box;
		rit->reg.data = (PIXEL_MANIPULATE_REGION32*)NULL;

		/* MUST force allocation */
		if (!PixelManipulateRectangleAllocate(&rit->reg, (i + num_ri) / num_ri))
			goto bail;
	
		next_rect: ;
	} /* for i */

	/* Make a final pass over each region in order to COALESCE and set
	 * extents.x2 and extents.y2
	 */
	for(j = num_ri, rit = ri; --j >= 0; rit++)
	{
		reg = &rit->reg;
		ri_box =  REGION_END(reg);
		reg->extents.y2 = ri_box->y2;

		if(reg->extents.x2 < ri_box->x2)
			reg->extents.x2 = ri_box->x2;
	
		COALESCE(reg, rit->prev_band, rit->cur_band);

		if(reg->data->num_rects == 1) /* keep unions happy below */
		{
			FREE_DATA(reg);
			reg->data = (PIXEL_MANIPULATE_REGION32*)NULL;
		}
	}

	/* Step 3: Union all regions into a single region */
	while(num_ri > 1)
	{
		int half = num_ri / 2;
		for (j = num_ri & 1; j < (half + (num_ri & 1)); j++)
		{
			reg = &ri[j].reg;
			hreg = &ri[j + half].reg;

			if(FALSE == PixelManipulateOperate(reg, reg, hreg,  PixelManipulateRegionUnionO, TRUE, TRUE))
				ret = FALSE;

			if(hreg->extents.x1 < reg->extents.x1)
				reg->extents.x1 = hreg->extents.x1;

			if(hreg->extents.y1 < reg->extents.y1)
				reg->extents.y1 = hreg->extents.y1;

			if(hreg->extents.x2 > reg->extents.x2)
				reg->extents.x2 = hreg->extents.x2;

			if(hreg->extents.y2 > reg->extents.y2)
				reg->extents.y2 = hreg->extents.y2;

			FREE_DATA(hreg);
		}

		num_ri -= half;

		if (!ret)
			goto bail;
	}

	*badreg = ri[0].reg;

	if(ri != stack_regions)
		free(ri);

	// GOOD (badreg);
	return ret;

bail:
	for(i = 0; i < num_ri; i++)
		FREE_DATA (&ri[i].reg);

	if(ri != stack_regions)
		free (ri);

	return PixelManipulateBreak(badreg);
}

void InitializePixelManipulateRegion32Rectangle(
	PIXEL_MANIPULATE_REGION32* region,
	int x,
	int y,
	unsigned int width,
	unsigned int height
)
{
	region->extents.x1 = x;
	region->extents.y1 = y;
	region->extents.x2 = x + width;
	region->extents.y2 = y + height;

	if(!GOOD_RECT(&region->extents))
	{
		InitializePixelManipulateRegion32(region);
		return;
	}

	region->data = NULL;
}

int InitializePixelManipulateRegion32Rectangles(
	PIXEL_MANIPULATE_REGION32* region,
	const PIXEL_MANIPULATE_BOX32* boxes,
	int count
)
{
	PIXEL_MANIPULATE_BOX32 *rectangles;
	int displacement;
	int i;

	if(count == 1)
	{
		InitializePixelManipulateRegion32Rectangle(region,
			boxes[0].x1, boxes[0].y1, boxes[0].x2 - boxes[0].x1, boxes[0].y2 - boxes[0].y1);
		return TRUE;
	}

	InitializePixelManipulateRegion32(region);

	if(count == 0)
	{
		return TRUE;
	}

	if(FALSE == PixelManipulateRectangleAllocate(region, count))
	{
		return FALSE;
	}

	rectangles = REGION_RECTANGLES(region);

	(void)memcpy(rectangles, boxes, sizeof(*boxes) * count);
	region->data->num_rects = count;

	displacement = 0;

	for(i=0; i<count; ++i)
	{
		PIXEL_MANIPULATE_BOX32 *box = &rectangles[i];

		if(box->x1 >= box->x2 || box->y1 >= box->y2)
		{
			displacement++;
		}
		else if(displacement != 0)
		{
			rectangles[i - displacement] = rectangles[i];
		}
	}

	region->data->num_rects -= displacement;

	if(region->data->num_rects == 0)
	{
		FREE_DATA(region);
		InitializePixelManipulateRegion32(region);

		return TRUE;
	}

	if(region->data->num_rects == 1)
	{
		region->extents = rectangles[0];

		FREE_DATA(region);
		region->data = NULL;

		return TRUE;
	}

	region->extents.x1 = region->extents.x2 = 0;

	return validate(region);
}

static PIXEL_MANIPULATE_BOX32* find_box_for_y(
	PIXEL_MANIPULATE_BOX32* begin,
	PIXEL_MANIPULATE_BOX32* end,
	int y
)
{
	PIXEL_MANIPULATE_BOX32 *mid;

	if(end == begin)
	{
		return end;
	}

	if(end - begin == 1)
	{
		if(begin->y2 > y)
		{
			return begin;
		}
		else
		{
			return end;
		}
	}

	mid = begin + (end - begin) / 2;
	if(mid->y2 > y)
	{
		return find_box_for_y(begin, mid, y);
	}
	return find_box_for_y(mid, end, y);
}

ePIXEL_MANIPULATE_REGION_OVERLAP PixelManipulateRegion32ContainsRectangle(
	PIXEL_MANIPULATE_REGION32* region,
	PIXEL_MANIPULATE_BOX32* prect
)
{
	PIXEL_MANIPULATE_BOX32 *pbox;
	PIXEL_MANIPULATE_BOX32 *pbox_end;
	int part_in, part_out;
	int num_rects;
	int x, y;

	num_rects = NUM_RECTS(region);

	if(num_rects == 0 || FALSE == EXTENT_CHECK(&region->extents, prect))
	{
		return PIXEL_MANIPULATE_REGION_OVERLAP_OUT;
	}

	if(num_rects == 1)
	{
		if(SUB_SUMES(&region->extents, prect))
		{
			return PIXEL_MANIPULATE_REGION_OVERLAP_IN;
		}
		else
		{
			return PIXEL_MANIPULATE_REGION_OVERLAP_PART;
		}
	}

	part_out = FALSE;
	part_in = FALSE;

	x = prect->x1;
	y = prect->y1;

	for(pbox = BOX_POINTER(region), pbox_end = pbox + num_rects; pbox != pbox_end; pbox++)
	{
		if(pbox->y2 <= y)
		{
			if((pbox = find_box_for_y(pbox, pbox_end, y)) == pbox_end)
			{
				break;
			}
		}

		if(pbox->y1 > y)
		{
			part_out = TRUE;
			if(part_in || (pbox->y1 >= prect->y2))
			{
			break;
			}
			y = pbox->y1;
		}

		if(pbox->x2 <= x)
		{
			continue;
		}

		if(pbox->x1 > x)
		{
			part_out = TRUE;
			if(part_in)
			{
				break;
			}
		}

		if(pbox->x1 < prect->x2)
		{
			part_in = TRUE;
			if(part_out)
			{
				break;
			}
		}

		if(pbox->x2 >= prect->x2)
		{
			y = pbox->y2;
			if(y >= prect->y2)
			{
				break;
			}
			x = prect->x1;
		}
		else
		{
			part_out = TRUE;
			break;
		}
	}

	if(part_in)
	{
		if(y < prect->y2)
		{
			return PIXEL_MANIPULATE_REGION_OVERLAP_PART;
		}
		else
		{
			return PIXEL_MANIPULATE_REGION_OVERLAP_IN;
		}
	}
	return PIXEL_MANIPULATE_REGION_OVERLAP_OUT;
}

#ifdef __cplusplus
}
#endif
