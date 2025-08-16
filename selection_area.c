#include <stdio.h>
#include <string.h>
#include "selection_area.h"
#include "draw_window.h"
#include "history.h"
#include "memory.h"
#include "application.h"
#include "brushes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SELECTION_AREA_BUFF_SIZE 1024
#define SELECTION_AREA_DISP_THRESHOLD 12
#define SELECTION_AREA_CHECKED 0x01

typedef enum _eSELECTION_CHECK_PATTERN
{
	SELECTION_CHECK_DOWN,
	SELECTION_CHECK_RIGHT_DOWN,
	SELECTION_CHECK_RIGHT,
	SELECTION_CHECK_RIGHT_UP,
	SELECTION_CHECK_UP,
	SELECTION_CHECK_LEFT_UP,
	SELECTION_CHECK_LEFT,
	SELECTION_CHECK_LEFT_DOWN,
	SELECTION_CHECK_OVER
} eSELECTION_CHECK_PATTERN;

static void AddSelectionArea(
	uint8* selection,
	int32 x,
	int32 y,
	SELECTION_SEGMENT_POINT** points,
	int32 *num_points,
	size_t *buff_size,
	LAYER* temp,
	int32 threshold
)
{
	int direction = SELECTION_CHECK_DOWN;
	int next_direction;
	int decision_next = 0;
	int i;

	while((temp->pixels[y*temp->width+x] & SELECTION_AREA_CHECKED) == 0)
	{
		temp->pixels[y*temp->width+x] |= SELECTION_AREA_CHECKED;

		(*points)[*num_points].x = x;
		(*points)[*num_points].y = y;
		(*num_points)++;

		if(*num_points == *buff_size)
		{
			size_t before_size = *buff_size;
			*buff_size += SELECTION_AREA_BUFF_SIZE;
			*points = (SELECTION_SEGMENT_POINT*)MEM_REALLOC_FUNC(*points, (*buff_size)*sizeof(SELECTION_SEGMENT_POINT));
			//(void)memset(&(*points)[before_size], 0,
			//	SELECTION_AREA_BUFF_SIZE * sizeof(SELECTION_SEGMENT_POINT));
		}

		switch(direction)
		{
		case SELECTION_CHECK_DOWN:
		case SELECTION_CHECK_RIGHT_DOWN:
			next_direction = SELECTION_CHECK_LEFT_DOWN;
			break;
		case SELECTION_CHECK_RIGHT:
		case SELECTION_CHECK_RIGHT_UP:
			next_direction = SELECTION_CHECK_RIGHT_DOWN;
			break;
		case SELECTION_CHECK_UP:
		case SELECTION_CHECK_LEFT_UP:
			next_direction = SELECTION_CHECK_RIGHT_UP;
			break;
		default:
			next_direction = SELECTION_CHECK_LEFT_UP;
		}

		for(i=0, decision_next = 0; i<SELECTION_CHECK_OVER
			&& decision_next == 0; i++, next_direction++)
		{
			if(next_direction == SELECTION_CHECK_OVER)
			{
				next_direction = SELECTION_CHECK_DOWN;
			}

			switch(next_direction)
			{
			case SELECTION_CHECK_DOWN:
				if(y < temp->height-1
					&& selection[(y+1)*temp->width+x] > threshold)
				{
					decision_next++;
					y++;
				}
				break;
			case SELECTION_CHECK_RIGHT_DOWN:
				if(x < temp->width-1 && y < temp->height-1
					&& selection[(y+1)*temp->width+x+1] > threshold)
				{
					decision_next++;
					x++; y++;
				}
				break;
			case SELECTION_CHECK_RIGHT:
				if(x < temp->width-1
					&& selection[y*temp->width+x+1] > threshold)
				{
					decision_next++;
					x++;
				}
				break;
			case SELECTION_CHECK_RIGHT_UP:
				if(x < temp->width-1 && y > 0
					&& selection[(y-1)*temp->width+x+1] > threshold)
				{
					decision_next++;
					x++, y--;
				}
				break;
			case SELECTION_CHECK_UP:
				if(y > 0
					&& selection[(y-1)*temp->width+x] > threshold)
				{
					decision_next++;
					y--;
				}
				break;
			case SELECTION_CHECK_LEFT_UP:
				if(x > 0 && y > 0
					&& selection[(y-1)*temp->width+x-1] > threshold)
				{
					decision_next++;
					x--, y--;
				}
				break;
			case SELECTION_CHECK_LEFT:
				if(x > 0
					&& selection[y*temp->width+x-1] > threshold)
				{
					decision_next++;
					x--;
				}
				break;
			default:
				if(x > 0 && y < temp->height-1
					&& selection[(y+1)*temp->width+x-1] > SELECTION_AREA_DISP_THRESHOLD)
				{
					decision_next++;
					x--, y++;
				}
			}

			direction = next_direction;
		}
	}
}

int UpdateSelectionArea(
	SELECTION_AREA* area,
	LAYER* selection,
	LAYER* disp_temp
)
{
	DRAW_WINDOW *canvas = selection->window;
	int32 num_area = 0;
	int result = FALSE;
	int update;
	int start_index = selection->width * selection->height;
	int i, j;
#ifdef OLD_SELECTION_AREA
	SELECTION_SEGMENT* area_data;
	size_t buff_size, segment_buff_size = SELECTION_AREA_BUFF_SIZE;

	// 以前のデータを開放
	for(i=0; i<area->num_area; i++)
	{
		MEM_FREE_FUNC(area->area_data[i].points);
	}
	MEM_FREE_FUNC(area->area_data);

	(void)memset(temp->pixels, 0, temp->stride*temp->height);
#else
	GRAPHICS_DEFAULT_CONTEXT context;
	GRAPHICS_IMAGE_SURFACE surface;
	GRAPHICS_SURFACE_PATTERN pattern;
	int stride = GraphicsFormatStrideForWidth(GRAPHICS_FORMAT_A8, canvas->disp_layer->width);
	if(stride != area->stride)
	{
		area->stride = GraphicsFormatStrideForWidth(GRAPHICS_FORMAT_A8, canvas->disp_layer->width);
		area->pixels = (uint8*)MEM_REALLOC_FUNC(area->pixels, area->stride * canvas->disp_layer->height);
		DestroyGraphicsSurface(&area->surface.base);
		InitializeGraphicsImageSurfaceForData(&area->surface, area->pixels, GRAPHICS_FORMAT_A8,
			canvas->disp_layer->width, canvas->disp_layer->height, area->stride, &canvas->app->graphics);
	}
	(void)memset(area->pixels, 0, area->stride * canvas->disp_layer->height);

	InitializeGraphicsImageSurfaceForData(&surface, disp_temp->pixels, GRAPHICS_FORMAT_A8,
		selection->window->disp_layer->width, selection->window->disp_layer->height, stride, &canvas->app->graphics);
	InitializeGraphicsDefaultContext(&context, &surface, &canvas->app->graphics);
	(void)memset(disp_temp->pixels, 0, area->stride * canvas->selection->height);

	InitializeGraphicsPatternForSurface(&pattern, &selection->surface.base);
	GraphicsPatternSetFilter(&pattern.base, GRAPHICS_FILTER_FAST);
	GraphicsScale(&context.base, selection->window->zoom_rate, selection->window->zoom_rate);
	GraphicsSetSource(&context.base, &pattern.base);
	GraphicsPaint(&context.base);

	DestroyGraphicsPattern(&pattern.base);
	DestroyGraphicsContext(&context.base);
	DestroyGraphicsSurface(&surface.base);
#endif

	// 最大・最小を初期化
	area->min_x = selection->width + 1, area->min_y = selection->height + 1;
	area->max_x = -1, area->max_y = -1;

#ifdef OLD_SELECTION_AREA
	// 選択範囲のエッジ検出
	LaplacianFilter(selection->pixels, selection->width, selection->height,
		selection->width, &temp->pixels[start_index]);

	area_data = (SELECTION_SEGMENT*)MEM_ALLOC_FUNC(
		sizeof(*area_data)*segment_buff_size);
	for(i=0; i<selection->height; i++)
	{
		for(j=0; j<selection->width; j++)
		{
			update = 0;

			if(temp->pixels[start_index+temp->width*i+j]
				> SELECTION_AREA_DISP_THRESHOLD
				&& temp->pixels[selection->stride*i+j*selection->channel] == 0)
			{
				area_data[num_area].num_points = 0;
				buff_size = SELECTION_AREA_BUFF_SIZE;
				area_data[num_area].points = (SELECTION_SEGMENT_POINT*)MEM_ALLOC_FUNC(
					buff_size  * sizeof(SELECTION_SEGMENT_POINT));
				(void)memset(area_data[num_area].points, 0, buff_size  * sizeof(SELECTION_SEGMENT_POINT));

				AddSelectionArea(&temp->pixels[selection->width*selection->height], j, i, &area_data[num_area].points,
					&area_data[num_area].num_points, &buff_size, temp, SELECTION_AREA_DISP_THRESHOLD);

				if(area_data[num_area].num_points == 0)
				{
					area_data[num_area].points->x = j;
					area_data[num_area].points->y = i;

					area_data[num_area].num_points++;
				}

				if(num_area == segment_buff_size - 1)
				{
					size_t before_size = segment_buff_size;

					segment_buff_size += SELECTION_AREA_BUFF_SIZE;
					area_data = (SELECTION_SEGMENT*)MEM_REALLOC_FUNC(
						area_data, sizeof(*area_data)*segment_buff_size);

					(void)memset(&area_data[before_size], 0, sizeof(*area_data)*SELECTION_AREA_BUFF_SIZE);
				}

				num_area++;
				update++;
				result = TRUE;
			}
			else if(selection->pixels[selection->stride*i+j*selection->channel] != 0)
			{
				update++;
				result = TRUE;
			}

			if(update != 0)
			{
				if(area->min_x > j) area->min_x = j;
				if(area->min_y > i) area->min_y = i;
				if(area->max_x < j) area->max_x = j;
				area->max_y = i;
			}
		}
	}

	area->area_data = (SELECTION_SEGMENT*)MEM_ALLOC_FUNC(sizeof(*area->area_data)*num_area);
	area->num_area = num_area;
	for(i=0; i<num_area; i++)
	{
		area->area_data[i].points =
			(SELECTION_SEGMENT_POINT*)MEM_ALLOC_FUNC(sizeof(*area->area_data)*area_data[i].num_points);
		(void)memcpy(area->area_data[i].points, area_data[i].points, sizeof(*area_data->points)*area_data[i].num_points);
		MEM_FREE_FUNC(area_data[i].points);
		area->area_data[i].num_points = area_data[i].num_points;
	}

	MEM_FREE_FUNC(area_data);
#else
	LaplacianFilter(disp_temp->pixels, selection->window->disp_layer->width, selection->window->disp_layer->height,
		stride, area->pixels);

	for(i=0; i<selection->height; i++)
	{
		for(j=0; j<selection->width; j++)
		{
			update = 0;

			if(selection->pixels[selection->stride*i+j*selection->channel] != 0)
			{
				update++;
				result = TRUE;

				if(area->min_x > j) area->min_x = j;
				if(area->min_y > i) area->min_y = i;
				if(area->max_x < j) area->max_x = j;
				area->max_y = i;
			}
		}
	}
#endif

	if(result == FALSE)
	{
		for(i=0; i<selection->window->app->menus.num_disable_if_no_select; i++)
		{
			WidgetSetSensitive(selection->window->app->menus.disable_if_no_select[i], FALSE);
		}
	}
	else
	{
		for(i=0; i<selection->window->app->menus.num_disable_if_no_select; i++)
		{
			WidgetSetSensitive(selection->window->app->menus.disable_if_no_select[i], TRUE);
		}
	}

	return result;
}

void DrawSelectionArea(
	SELECTION_AREA* area,
	DRAW_WINDOW* window
)
{
#ifdef OLD_SELECTION_AREA
#define SELECTION_DRAW_DISTANCE 8
	uint32 index;
	gboolean black_flag;
	int i, j;

	for(i=0; i<area->num_area; i++)
	{
		index = area->index;
		black_flag = TRUE;
		cairo_set_source_rgb(window->effect->cairo_p, 0, 0, 0);

		for(j=0; j<area->area_data[i].num_points-1; j++, index++)
		{
			cairo_move_to(
				window->effect->cairo_p,
				area->area_data[i].points[j].x*window->zoom_rate,
				area->area_data[i].points[j].y*window->zoom_rate
			);
			cairo_line_to(
				window->effect->cairo_p,
				area->area_data[i].points[j+1].x*window->zoom_rate,
				area->area_data[i].points[j+1].y*window->zoom_rate
			);
			cairo_stroke(window->effect->cairo_p);

			if(index % SELECTION_DRAW_DISTANCE == 0)
			{
				black_flag = !black_flag;

				if(black_flag == FALSE)
				{
					cairo_set_source_rgb(window->effect->cairo_p, 1, 1, 1);
				}
				else
				{
					cairo_set_source_rgb(window->effect->cairo_p, 0, 0, 0);
				}
			}
		}
	}
#else
#define SELECTION_DRAW_DISTANCE 2
#define SELECTION_DRAW_STEP 6
	GRAPHICS_LINEAR_PATTERN pattern;

	InitializeGraphicsPatternLinear(
			&pattern, 0, 0, SELECTION_DRAW_STEP, SELECTION_DRAW_STEP, &window->app->graphics);
	switch(area->index)
	{
	case 0:
		GraphicsPatternAddColorStopRGB(&pattern.base.base, 0, 0, 0, 0);
		GraphicsPatternAddColorStopRGB(&pattern.base.base, 0.5, 1, 1, 1);
		break;
	case 1:
		GraphicsPatternAddColorStopRGB(&pattern.base.base, 0, 1, 1, 1);
		GraphicsPatternAddColorStopRGB(&pattern.base.base, 0.5, 0, 0, 0);
		break;
	}
	GraphicsPatternSetExtend(&pattern.base.base, GRAPHICS_PATTERN_NOTIFY_EXTEND);

	GraphicsSave(&window->effect->context.base);
	/*cairo_rectangle(window->effect->cairo_p, area->min_x * window->zoom_rate - 1, area->min_y * window->zoom_rate - 1,
		(area->max_x - area->min_x) * window->zoom_rate + 4, (area->max_y - area->min_y) * window->zoom_rate + 4);
	cairo_clip(window->effect->cairo_p);*/
	GraphicsSetSource(&window->effect->context.base, &pattern.base.base);
	GraphicsMaskSurface(&window->effect->context.base, &area->surface.base, 0, 0);
	GraphicsRestore(&window->effect->context.base);

	DestroyGraphicsPattern(&pattern.base.base);
#endif
	area->index++;
	if(area->index == SELECTION_DRAW_DISTANCE)
	{
		area->index = 0;
	}
}

void DisplayEditSelection(DRAW_WINDOW* window)
{
	FLOAT_T zoom = window->zoom * 0.01;
	FLOAT_T rev_zoom = 1 / zoom;
	GRAPHICS_IMAGE_SURFACE surface;
	InitializeGraphicsImageSurfaceForData(&surface, window->temp_layer->pixels,
			GRAPHICS_FORMAT_A8, window->width, window->height, window->width, &window->app->graphics);

	(void)memcpy(window->temp_layer->pixels, window->selection->pixels,
		window->selection->stride * window->selection->height);
	window->blend_selection_functions[window->selection->layer_mode](window->work_layer, window->temp_layer);

	GraphicsScale(&window->disp_layer->context.base, zoom, zoom);
	GraphicsSetSourceRGBA(&window->disp_layer->context.base, 1.0, 0.5, 0.5, 0.5);
	GraphicsRectangle(&window->disp_layer->context.base, 0, 0,
		window->disp_layer->width, window->disp_layer->height);
	GraphicsMaskSurface(&window->disp_layer->context.base, &surface.base, 0, 0);
	GraphicsScale(&window->disp_layer->context.base, rev_zoom, rev_zoom);

	DestroyGraphicsSurface(&surface.base);
}

typedef struct _SELECTION_AREA_HISTROY_DATA
{
	int32 x, y;
	int32 width, height;
	uint8 *pixels;
} SELECTION_AREA_HISTORY_DATA;

static void SelectionAreaChangeUndoRedo(
	DRAW_WINDOW* window,
	void* p
)
{
	SELECTION_AREA_HISTORY_DATA data;
	uint8* buff = (uint8*)p;
	uint8* before_data;
	int i;

	(void)memcpy(&data, buff, offsetof(SELECTION_AREA_HISTORY_DATA, pixels));
	buff += offsetof(SELECTION_AREA_HISTORY_DATA, pixels);
	data.pixels = buff;

	before_data = (uint8*)MEM_ALLOC_FUNC(data.width*data.height);
	for(i=0; i<data.height; i++)
	{
		(void)memcpy(&before_data[i*data.width],
			&window->selection->pixels[(data.y+i)*window->width+data.x], data.width);
	}

	for(i=0; i<data.height; i++)
	{
		(void)memcpy(&window->selection->pixels[(data.y+i)*window->width+data.x],
			&data.pixels[i*data.width], data.width);
	}

	(void)memcpy(data.pixels, before_data, data.width*data.height);

#ifdef OLD_SELECTION_AREA
	if(UpdateSelectionArea(&window->selection_area, window->selection, window->temp_layer) == FALSE)
#else
	if(UpdateSelectionArea(&window->selection_area, window->selection, window->disp_temp) == FALSE)
#endif
	{
		window->flags &= ~(DRAW_WINDOW_HAS_SELECTION_AREA);
	}
	else
	{
		window->flags |= DRAW_WINDOW_HAS_SELECTION_AREA;
	}
	
	MEM_FREE_FUNC(before_data);
}

void AddSelectionAreaChangeHistory(
	DRAW_WINDOW* window,
	const char* tool_name,
	int32 min_x,
	int32 min_y,
	int32 max_x,
	int32 max_y
)
{
	SELECTION_AREA_HISTORY_DATA data;
	uint8 *buff, *write;
	size_t data_size;
	int i;

	if(min_x > 0)
	{
		min_x--;
	}
	else if(min_x < 0)
	{
		min_x = 0;
	}
	if(min_y > 0)
	{
		min_y--;
	}
	else if(min_y < 0)
	{
		min_y = 0;
	}
	if(max_x < window->width)
	{
		max_x++;
	}
	else if(max_x > window->width)
	{
		max_x = window->width;
	}
	if(max_y < window->height)
	{
		max_y++;
	}
	else if(max_y > window->height)
	{
		max_y = window->height;
	}

	data.x = min_x, data.y = min_y;
	data.width = max_x - min_x;
	data.height = max_y - min_y;

	data_size = offsetof(SELECTION_AREA_HISTORY_DATA, pixels)+data.width*data.height;
	buff = write = (uint8*)MEM_ALLOC_FUNC(data_size);
	(void)memcpy(write, &data, offsetof(SELECTION_AREA_HISTORY_DATA, pixels));
	write += offsetof(SELECTION_AREA_HISTORY_DATA, pixels);

	for(i=0; i<data.height; i++)
	{
		(void)memcpy(write,
			&window->temp_layer->pixels[(data.y+i)*window->width+data.x], data.width);
		write += data.width;
	}

	AddHistory(
		&window->history,
		tool_name,
		buff,
		(uint32)data_size,
		(history_func)SelectionAreaChangeUndoRedo,
		(history_func)SelectionAreaChangeUndoRedo
	);

	MEM_FREE_FUNC(buff);
}

int ColorDifference(uint8* color1, uint8* color2, int32 channel)
{
	int d, result = 0;
	int i;

	for(i=0; i<channel; i++)
	{
		d = (int)color1[i] - (int)color2[i];
		result += (d >= 0) ? d : -d;
	}

	return result;
}

typedef struct _DETECT_POINT
{
	int32 x, y;
} DETECT_POINT;

void DetectSameColorArea(
	LAYER* target,
	uint8* buff,
	uint8* temp_buff,
	int32 start_x,
	int32 start_y,
	uint8 *color,
	uint8 channel,
	int16 threshold,
	int32* min_x,
	int32* min_y,
	int32* max_x,
	int32* max_y,
	eSELECT_FUZZY_DIRECTION direction
)
{
#define STACK_BUFF_SIZE 4096
	DETECT_POINT* stack, top;
	int buff_size = sizeof(*stack)*STACK_BUFF_SIZE;
	int32 local_min_x = start_x, local_min_y = start_y;
	int32 local_max_x = start_x, local_max_y = start_y;
	int stack_point = 0;

	stack = (DETECT_POINT*)MEM_ALLOC_FUNC(sizeof(*stack)*buff_size);
	stack->x = start_x;
	stack->y = start_y;

	// 4方向で選択範囲検出
	if(direction == FUZZY_SELECT_DIRECTION_QUAD)
	{
		do
		{
			top = stack[stack_point];
			stack_point--;

			if(temp_buff[top.y*target->width+top.x] != 0)
			{
				continue;
			}

			buff[top.y*target->width+top.x] = 0xff;
			temp_buff[top.y*target->width+top.x] = SELECTION_AREA_CHECKED;

			if(local_min_x > top.x)
			{
				local_min_x = top.x;
			}
			else if(local_max_x < top.x)
			{
				local_max_x = top.x;
			}
			if(local_min_y > top.y)
			{
				local_min_y = top.y;
			}
			else if(local_max_y < top.y)
			{
				local_max_y = top.y;
			}

			if(top.y > 0 && ColorDifference(color,
				&target->pixels[target->stride*(top.y-1)+top.x*target->channel], channel) < threshold
				&& temp_buff[target->width*(top.y-1)+top.x] == 0)
			{
				stack_point++;
				stack[stack_point].x = top.x;
				stack[stack_point].y = top.y - 1;
			}

			if(top.x < target->width - 1 && ColorDifference(color,
				&target->pixels[target->stride*top.y+(top.x+1)*target->channel], channel) < threshold
				&& temp_buff[target->width*top.y+top.x+1] == 0)
				{
				stack_point++;
				stack[stack_point].x = top.x + 1;
				stack[stack_point].y = top.y;
			}

			if(top.y < target->height-1 && ColorDifference(color,
				&target->pixels[target->stride*(top.y+1)+top.x*target->channel], channel) < threshold
				&& temp_buff[target->width*(top.y+1)+top.x] == 0)
			{
				stack_point++;
				stack[stack_point].x = top.x;
				stack[stack_point].y = top.y+1;
			}

			if(top.x > 0 && ColorDifference(color,
				&target->pixels[target->stride*top.y+(top.x-1)*target->channel], channel) < threshold
				&& temp_buff[target->width*top.y+top.x-1] == 0)
			{
				stack_point++;
				stack[stack_point].x = top.x - 1;
				stack[stack_point].y = top.y;
			}

			if(stack_point >= buff_size - 5)
			{
				buff_size += STACK_BUFF_SIZE;
				stack = (DETECT_POINT*)MEM_REALLOC_FUNC(stack, buff_size*sizeof(*stack));
			}
		} while(stack_point >= 0);
	}	// 4方向で選択範囲検出
		// if(direction == FUZZY_SELECT_DIRECTION_QUAD)
	else	// 8方向で選択範囲検出
	{
		do
		{
			top = stack[stack_point];
			stack_point--;

			if(temp_buff[top.y*target->width+top.x] != 0)
			{
				continue;
			}

			buff[top.y*target->width+top.x] = 0xff;
			temp_buff[top.y*target->width+top.x] = SELECTION_AREA_CHECKED;

			if(local_min_x > top.x)
			{
				local_min_x = top.x;
			}
			else if(local_max_x < top.x)
			{
				local_max_x = top.x;
			}
			if(local_min_y > top.y)
			{
				local_min_y = top.y;
			}
			else if(local_max_y < top.y)
			{
				local_max_y = top.y;
			}

			if(top.y > 0 && top.x > 0 &&
				ColorDifference(color,
				&target->pixels[target->stride*(top.y-1)+(top.x-1)*target->channel], channel) < threshold
				&& temp_buff[target->width*(top.y-1)+top.x-1] == 0)
			{
				stack_point++;
				stack[stack_point].x = top.x - 1;
				stack[stack_point].y = top.y - 1;
			}

			if(top.y > 0 && ColorDifference(color,
				&target->pixels[target->stride*(top.y-1)+top.x*target->channel], channel) < threshold
				&& temp_buff[target->width*(top.y-1)+top.x] == 0)
			{
				stack_point++;
				stack[stack_point].x = top.x;
				stack[stack_point].y = top.y - 1;
			}

			if(top.y > 0 && top.x < target->width - 1 &&
				ColorDifference(color,
				&target->pixels[target->stride*(top.y-1)+(top.x+1)*target->channel], channel) < threshold
				&& temp_buff[target->width*(top.y-1)+top.x+1] == 0)
			{
				stack_point++;
				stack[stack_point].x = top.x + 1;
				stack[stack_point].y = top.y - 1;
			}

			if(top.x < target->width - 1 && ColorDifference(color,
				&target->pixels[target->stride*top.y+(top.x+1)*target->channel], channel) < threshold
				&& temp_buff[target->width*top.y+top.x+1] == 0)
				{
				stack_point++;
				stack[stack_point].x = top.x + 1;
				stack[stack_point].y = top.y;
			}

			if(top.y < target->height-1 && ColorDifference(color,
				&target->pixels[target->stride*(top.y+1)+top.x*target->channel], channel) < threshold
				&& temp_buff[target->width*(top.y+1)+top.x] == 0)
			{
				stack_point++;
				stack[stack_point].x = top.x;
				stack[stack_point].y = top.y+1;
			}

			if(top.y < target->height - 1 && top.x > 0 &&
				ColorDifference(color,
				&target->pixels[target->stride*(top.y+1)+(top.x-1)*target->channel], channel) < threshold
				&& temp_buff[target->width*(top.y+1)+top.x-1] == 0)
			{
				stack_point++;
				stack[stack_point].x = top.x - 1;
				stack[stack_point].y = top.y + 1;
			}

			if(top.x > 0 && ColorDifference(color,
				&target->pixels[target->stride*top.y+(top.x-1)*target->channel], channel) < threshold
				&& temp_buff[target->width*top.y+top.x-1] == 0)
			{
				stack_point++;
				stack[stack_point].x = top.x - 1;
				stack[stack_point].y = top.y;
			}

			if(top.y < target->height - 1 && top.x < target->width - 1 &&
				ColorDifference(color,
				&target->pixels[target->stride*(top.y+1)+(top.x+1)*target->channel], channel) < threshold
				&& temp_buff[target->width*(top.y+1)+top.x+1] == 0)
			{
				stack_point++;
				stack[stack_point].x = top.x + 1;
				stack[stack_point].y = top.y + 1;
			}

			if(stack_point >= buff_size - 9)
			{
				buff_size += STACK_BUFF_SIZE;
				stack = (DETECT_POINT*)MEM_REALLOC_FUNC(stack, buff_size*sizeof(*stack));
			}
		} while(stack_point >= 0);
	}

	*min_x = local_min_x, *min_y = local_min_y;
	*max_x = local_max_x, *max_y = local_max_y;

	MEM_FREE_FUNC(stack);
}

/*****************************************
* AddSelectionAreaByColor関数			*
* 色域選択を実行						 *
* 引数								   *
* target	: 色比較を行うレイヤー	   *
* buff		: 選択範囲を記憶するバッファ *
* color		: 選択する色				 *
* threshold	: 選択判断の閾値			 *
* min_x		: 選択範囲の最小のX座標	  *
* min_y		: 選択範囲の最小のY座標	  *
* max_x		: 選択範囲の最大のX座標	  *
* max_y		: 選択範囲の最大のY座標	  *
* 返り値								 *
*	選択範囲有り:1 選択範囲無し:0		*
*****************************************/
int AddSelectionAreaByColor(
	LAYER* target,
	uint8* buff,
	uint8* color,
	int channel,
	int32 threshold,
	int32* min_x,
	int32* min_y,
	int32* max_x,
	int32* max_y
)
{
	// 選択範囲の座標の最小、最大値を記憶
	int32 local_min_x, local_min_y, local_max_x, local_max_y;
	// for文用のカウンタ
	int x, y;
	// 返り値
	int result = 0;

	// 最小、最大値を初期化
	local_min_x = target->width, local_min_y = target->height;
	local_max_x = local_max_y = 0;

	// 指定色とピクセルの色を比較して閾値内なら選択範囲に
	for(y=0; y<target->height; y++)
	{	
		for(x=0; x<target->width; x++)
		{
			// 色の差を計算
			if(ColorDifference(&target->pixels[y*target->stride+x*target->channel],
				color, channel) <= threshold)
			{
				if(local_min_x > x)
				{
					local_min_x = x;
				}
				if(local_min_y > y)
				{
					local_min_y = y;
				}
				if(local_max_x < x)
				{
					local_max_x = x;
				}
				if(local_max_y < y)
				{
					local_max_y = y;
				}

				buff[y*target->width+x] = 0xff;

				// 選択範囲が存在
				result = 1;
			}
		}
	}

	*min_x = local_min_x;
	*min_y = local_min_y;
	*max_x = local_max_x;
	*max_y = local_max_y;

	return result;
}

void UnSetSelectionArea(APPLICATION* app)
{
	DRAW_WINDOW* window = GetActiveDrawWindow(app);
	int32 min_x, min_y, max_x, max_y;

	// 選択範囲がなければ終了
	if((window->flags & (DRAW_WINDOW_HAS_SELECTION_AREA
		| DRAW_WINDOW_EDIT_SELECTION)) == 0)
	{
		return;
	}

	// 選択範囲の座標を取得
	min_x = window->selection_area.min_x;
	if(min_x < 0)
	{
		min_x = 0;
	}
	min_y = window->selection_area.min_y;
	if(min_y < 0)
	{
		min_y = 0;
	}
	max_x = window->selection_area.max_x;
	if(max_x > window->width)
	{
		max_x = window->width;
	}
	max_y = window->selection_area.max_y;
	if(max_y > window->height)
	{
		max_y = window->height;
	}

	// 現在の選択範囲を記憶
	(void)memcpy(window->temp_layer->pixels, window->selection->pixels, window->width*window->height);

	// 選択を解除
	(void)memset(window->selection->pixels, 0, window->width*window->selection->height);

	// 履歴データを追加
	AddSelectionAreaChangeHistory(window, app->labels->menu.select_none,
		min_x, min_y, max_x, max_y);
	window->flags &= ~(DRAW_WINDOW_HAS_SELECTION_AREA);
	window->flags |= DRAW_WINDOW_UPDATE_ACTIVE_OVER;
}

void InvertSelectionArea(APPLICATION* app)
{
	DRAW_WINDOW* window = app->draw_window[app->active_window];
	int i;

	for(i=0; i<window->selection->width*window->selection->height; i++)
	{
		window->selection->pixels[i] = 0xff - window->selection->pixels[i];
	}

#ifdef OLD_SELECTION_AREA
	if(UpdateSelectionArea(&window->selection_area, window->selection, window->temp_layer) == FALSE)
#else
	if(UpdateSelectionArea(&window->selection_area, window->selection, window->disp_temp) == FALSE)
#endif
	{
		window->flags &= ~(DRAW_WINDOW_HAS_SELECTION_AREA);
	}
	else
	{
		window->flags |= DRAW_WINDOW_HAS_SELECTION_AREA;
	}
}

/*****************************************
* ExtendSelectionAreaOneStep関数		 *
* 選択範囲を1ピクセル拡大する			*
* 引数								   *
* select	: 選択範囲を管理するレイヤー *
* temp		: 一時保存用のレイヤー	   *
*****************************************/
void ExtendSelectionAreaOneStep(LAYER* select, LAYER* temp)
{
	// 配列のインデックス
	int index;
	// for文用のカウンタ
	int x, y;
	// 周囲のピクセルの最大値
	uint8 max;

	// 自分自身と上下左右のピクセルを見て
		// より大きな値を次の選択範囲の値にする

	// 一行目を処理
	// 左端
	max = select->pixels[0];
	if(max < select->pixels[1])
	{
		max = select->pixels[1];
	}
	if(max < select->pixels[select->width])
	{
		max = select->pixels[select->width];
	}
	temp->pixels[0] = max;

	// 右端手前まで
	for(x = 1; x<select->width - 1; x++)
	{
		max = select->pixels[x];
		if(max < select->pixels[x-1])
		{
			max = select->pixels[x-1];
		}
		if(max < select->pixels[x+1])
		{
			max = select->pixels[x+1];
		}
		if(max < select->pixels[select->width+x])
		{
			max = select->pixels[select->width+x];
		}
		temp->pixels[x] = max;
	}

	// 右端
	max = select->pixels[select->width-1];
	if(max < select->pixels[select->width-2])
	{
		max = select->pixels[select->width-2];
	}
	if(max < select->pixels[select->width*2-1])
	{
		max = select->pixels[select->width*2-1];
	}
	temp->pixels[select->width-1] = max;

	// 2行目から一番下手前まで
	for(y=1; y<select->height-1; y++)
	{
		// 左端
		index = y*select->width;
		max = select->pixels[index];
		if(max < select->pixels[index-select->width])
		{
			max = select->pixels[index-select->width];
		}
		if(max < select->pixels[index+1])
		{
			max = select->pixels[index+1];
		}
		if(max < select->pixels[index+select->width])
		{
			max = select->pixels[index+select->width];
		}
		temp->pixels[index] = max;

		// 右端手前まで
		for(x=1; x<select->width-1; x++)
		{
			index = y*select->width+x;
			max = select->pixels[index];
			if(max < select->pixels[index-select->width])
			{
				max = select->pixels[index-select->width];
			}
			if(max < select->pixels[index-1])
			{
				max = select->pixels[index-1];
			}
			if(max < select->pixels[index+1])
			{
				max = select->pixels[index+1];
			}
			if(max < select->pixels[index+select->width])
			{
				max = select->pixels[index+select->width];
			}
			temp->pixels[index] = max;
		}

		// 右端
		index = y*select->width+select->width-1;
		max = select->pixels[index];
		if(max < select->pixels[index-select->width])
		{
			max = select->pixels[index-select->width];
		}
		if(max < select->pixels[index-1])
		{
			max = select->pixels[index-1];
		}
		if(max < select->pixels[index+select->width])
		{
			max = select->pixels[index+select->width];
		}
		temp->pixels[index] = max;
	}

	// 一番下の行
	// 左端
	index = y*select->width;
	max = select->pixels[index];
	if(max < select->pixels[index-select->width])
	{
		max = select->pixels[index-select->width];
	}
	if(max < select->pixels[index+1])
	{
		max = select->pixels[index+1];
	}
	temp->pixels[index] = max;

	// 右端手前まで
	for(x=1; x<select->width-1; x++)
	{
		index = y*select->width+x;
		max = select->pixels[index];
		if(max < select->pixels[index-select->width])
		{
			max = select->pixels[index-select->width];
		}
		if(max < select->pixels[index-1])
		{
			max = select->pixels[index-1];
		}
		if(max < select->pixels[index+1])
		{
			max = select->pixels[index+1];
		}
		temp->pixels[index] = max;
	}

	// 右端
	index = y*select->width+select->width-1;
	max = select->pixels[index];
	if(max < select->pixels[index-select->width])
	{
		max = select->pixels[index-select->width];
	}
	if(max < select->pixels[index-1])
	{
		max = select->pixels[index-1];
	}
	temp->pixels[index] = max;
}

/*****************************************************
* ExtendSelectionArea関数							 *
* 選択範囲を拡大する									 *
* 引数												*
* canvas	: 選択範囲を拡大するキャンバス				   *
* num_steps : 拡大するピクセル数						  *
*****************************************************/
void ExtendSelectionArea(DRAW_WINDOW* canvas, int num_steps)
{
	int copy_size;
	// for文用のカウンタ
	int i;

	copy_size = canvas->selection->width * canvas->selection->height;
	// 選択範囲の情報を一時保存にコピー
	(void)memcpy(canvas->temp_layer->pixels,
					canvas->selection->pixels, copy_size);
	for(i=0; i<num_steps; i++)
	{
		ExtendSelectionAreaOneStep(canvas->selection, canvas->mask_temp);
		(void)memcpy(canvas->selection->pixels, canvas->mask_temp->pixels, copy_size);
	}

	// 選択範囲更新の履歴データを作成
	AddSelectionAreaChangeHistory(
		canvas, canvas->app->labels->menu.selection_extend, 0, 0, canvas->width, canvas->height);

		// 選択範囲を更新
#ifdef OLD_SELECTION_AREA
	(void)UpdateSelectionArea(&canvas->selection_area, canvas->selection, canvas->temp_layer);
#else
	(void)UpdateSelectionArea(&canvas->selection_area, canvas->selection, canvas->disp_temp);
#endif
}

/*****************************************
* RecuctSelectionAreaOneStep関数		 *
* 選択範囲を1ピクセル縮小する			*
* 引数								   *
* select	: 選択範囲を管理するレイヤー *
* temp		: 一時保存用のレイヤー	   *
*****************************************/
void ReductSelectionAreaOneStep(LAYER* select, LAYER* temp)
{
	// 配列のインデックス
	int index;
	// for文用のカウンタ
	int x, y;
	// 周囲のピクセルの最小値
	uint8 min;

	// 自分自身と上下左右のピクセルを見て
		// より小さな値を次の選択範囲の値にする

	// 一行目を処理
	// 左端
	min = select->pixels[0];
	if(min > select->pixels[1])
	{
		min = select->pixels[1];
	}
	if(min > select->pixels[select->width])
	{
		min = select->pixels[select->width];
	}
	temp->pixels[0] = min;

	// 右端手前まで
	for(x = 0; x<select->width - 1; x++)
	{
		min = select->pixels[x];
		if(min > select->pixels[x-1])
		{
			min = select->pixels[x-1];
		}
		if(min > select->pixels[x+1])
		{
			min = select->pixels[x+1];
		}
		if(min > select->pixels[select->width+x])
		{
			min = select->pixels[select->width+x];
		}
		temp->pixels[x] = min;
	}

	// 右端
	min = select->pixels[select->width-1];
	if(min > select->pixels[select->width-2])
	{
		min = select->pixels[select->width-2];
	}
	if(min > select->pixels[select->width*2-1])
	{
		min = select->pixels[select->width*2-1];
	}
	temp->pixels[select->width-1] = min;

	// 2行目から一番下手前まで
	for(y=1; y<select->height-1; y++)
	{
		// 左端
		index = y*select->width;
		min = select->pixels[index];
		if(min > select->pixels[index-select->width])
		{
			min = select->pixels[index-select->width];
		}
		if(min > select->pixels[index+1])
		{
			min = select->pixels[index+1];
		}
		if(min > select->pixels[index+select->width])
		{
			min = select->pixels[index+select->width];
		}
		temp->pixels[index] = min;

		// 右端手前まで
		for(x=1; x<select->width-1; x++)
		{
			index = y*select->width+x;
			min = select->pixels[index];
			if(min > select->pixels[index-select->width])
			{
				min = select->pixels[index-select->width];
			}
			if(min > select->pixels[index-1])
			{
				min = select->pixels[index-1];
			}
			if(min > select->pixels[index+1])
			{
				min = select->pixels[index+1];
			}
			if(min > select->pixels[index+select->width])
			{
				min = select->pixels[index+select->width];
			}
			temp->pixels[index] = min;
		}

		// 右端
		index = y*select->width+select->width-1;
		min = select->pixels[index];
		if(min > select->pixels[index-select->width])
		{
			min = select->pixels[index-select->width];
		}
		if(min > select->pixels[index-1])
		{
			min = select->pixels[index-1];
		}
		if(min > select->pixels[index+select->width])
		{
			min = select->pixels[index+select->width];
		}
		temp->pixels[index] = min;
	}

	// 一番下の行
	// 左端
	index = y*select->width;
	min = select->pixels[index];
	if(min > select->pixels[index-select->width])
	{
		min = select->pixels[index-select->width];
	}
	if(min > select->pixels[index+1])
	{
		min = select->pixels[index+1];
	}
	temp->pixels[index] = min;

	// 右端手前まで
	for(x=1; x<select->width-1; x++)
	{
		index = y*select->width+x;
		min = select->pixels[index];
		if(min > select->pixels[index-select->width])
		{
			min = select->pixels[index-select->width];
		}
		if(min > select->pixels[index-1])
		{
			min = select->pixels[index-1];
		}
		if(min > select->pixels[index+1])
		{
			min = select->pixels[index+1];
		}
		temp->pixels[index] = min;
	}

	// 右端
	index = y*select->width+select->width-1;
	min = select->pixels[index];
	if(min < select->pixels[index-select->width])
	{
		min = select->pixels[index-select->width];
	}
	if(min > select->pixels[index-1])
	{
		min = select->pixels[index-1];
	}
	temp->pixels[index] = min;
}

/*****************************************************
* ExecuteReductSelectionArea関数					  *
* 選択範囲を縮小する									 *
* 引数											   *
* canvas	: 選択範囲を縮小するキャンバス				   *
* num_steps : 縮小するピクセル数						  *
*****************************************************/
void ReductSelectionArea(DRAW_WINDOW* canvas, int num_steps)
{
#define MAX_REDUCT_PIXEL 100
	int copy_size;
	// for文用のカウンタ
	int i;

	copy_size =	// コピーするバイト数
			canvas->selection->width * canvas->selection->height;
	// 選択範囲の情報を一時保存にコピー
	(void)memcpy(canvas->temp_layer->pixels, canvas->selection->pixels, copy_size);
	for(i=0; i<num_steps; i++)
	{
		ReductSelectionAreaOneStep(canvas->selection, canvas->mask_temp);
		(void)memcpy(canvas->selection->pixels, canvas->mask_temp->pixels, copy_size);
	}

	// 選択範囲更新の履歴データを作成
	AddSelectionAreaChangeHistory(
			canvas, canvas->app->labels->menu.selection_extend, 0, 0, canvas->width, canvas->height);

	// 選択範囲を更新
#ifdef OLD_SELECTION_AREA
	if(UpdateSelectionArea(&canvas->selection_area, canvas->selection, canvas->temp_layer) == FALSE)
#else
	if(UpdateSelectionArea(&canvas->selection_area, canvas->selection, canvas->disp_temp) == FALSE)
#endif
	{	// 選択範囲無し
		canvas->flags &= ~(DRAW_WINDOW_HAS_SELECTION_AREA);
	}
#undef MAX_REDUCT_PIXEL
}

/*********************************************************
* ChangeEditSelectionMode関数							*
* 選択範囲編集モードを切り替える						 *
* 引数												   *
* menu_item	: メニューアイテムウィジェット			   *
* app		: アプリケーションを管理する構造体のアドレス *
*********************************************************/
void ChangeEditSelectionMode(void* menu_item, APPLICATION* app)
{
	DRAW_WINDOW *window = app->draw_window[app->active_window];
	//int state = gtk_check_menu_item_get_active(
	//	GTK_CHECK_MENU_ITEM(menu_item));
	int x, y;

	if((app->flags & APPLICATION_IN_EDIT_SELECTION) == 0)
	{
		app->flags |= APPLICATION_IN_REVERSE_OPERATION;
		//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->widgets->edit_selection), state);
		app->flags &= ~(APPLICATION_IN_EDIT_SELECTION);
	}

	//if(state == FALSE)
	{
		for(y=0; y<BRUSH_TABLE_HEIGHT; y++)
		{
			for(x=0; x<BRUSH_TABLE_WIDTH; x++)
			{
				if(app->tool_box.brushes[y][x]->name != NULL)
				{
					//SetBrushCallBack(&app->tool_box.brushes[y][x]);
				}
			}
		}

		app->draw_window[app->active_window]->flags
			&= ~(DRAW_WINDOW_EDIT_SELECTION);

#ifdef OLD_SELECTION_AREA
		if(UpdateSelectionArea(&window->selection_area, window->selection, window->temp_layer) == FALSE)
#else
		if(UpdateSelectionArea(&window->selection_area, window->selection, window->disp_temp) == FALSE)
#endif
		{
			window->flags &= ~(DRAW_WINDOW_HAS_SELECTION_AREA);
		}
		else
		{
			window->flags |= DRAW_WINDOW_HAS_SELECTION_AREA;
		}

		/*
		if(window->active_layer->layer_type == TYPE_VECTOR_LAYER)
		{
			gtk_widget_destroy(app->tool_window.brush_table);
			CreateVectorBrushTable(app, &app->tool_window, app->tool_window.vector_brushes);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->tool_window.active_vector_brush[app->input]->button), TRUE);

			gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(app->tool_window.brush_scroll),
				app->tool_window.brush_table);
			gtk_widget_show_all(app->tool_window.brush_table);
			gtk_widget_show_all(app->tool_window.detail_ui);
		}
		else if(window->active_layer->layer_type == TYPE_TEXT_LAYER)
		{
			app->tool_window.brush_table = window->active_layer->layer_data.text_layer_p->text_field =
				gtk_text_view_new();
			window->active_layer->layer_data.text_layer_p->buffer =
				gtk_text_view_get_buffer(GTK_TEXT_VIEW(window->active_layer->layer_data.text_layer_p->text_field));
			if(window->active_layer->layer_data.text_layer_p->text != NULL)
			{
				gtk_text_buffer_set_text(window->active_layer->layer_data.text_layer_p->buffer,
					window->active_layer->layer_data.text_layer_p->text, -1);
			}
			(void)g_signal_connect(G_OBJECT(window->active_layer->layer_data.text_layer_p->buffer), "changed",
				G_CALLBACK(OnChangeTextCallBack), window->active_layer);
			gtk_widget_destroy(app->tool_window.detail_ui);
			app->tool_window.detail_ui = CreateTextLayerDetailUI(
				app, window->active_layer, window->active_layer->layer_data.text_layer_p);
			gtk_scrolled_window_add_with_viewport(
				GTK_SCROLLED_WINDOW(app->tool_window.detail_ui_scroll),
				app->tool_window.detail_ui
			);

			gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(app->tool_window.brush_scroll),
				app->tool_window.brush_table);
			gtk_widget_show_all(app->tool_window.brush_table);
			gtk_widget_show_all(app->tool_window.detail_ui);
		}
		*/
	}
	//else
	{
		/*
		for(y=0; y<BRUSH_TABLE_HEIGHT; y++)
		{
			for(x=0; x<BRUSH_TABLE_WIDTH; x++)
			{
				if(&app->tool_box.brushes[y][x].name != NULL)
				{
					SetEditSelectionCallBack(&app->tool_box.brushes[y][x]);
				}
			}
		}

		if(window->active_layer->layer_type != TYPE_NORMAL_LAYER)
		{
			gtk_widget_destroy(app->tool_window.brush_table);
			CreateBrushTable(app, &app->tool_window, app->tool_window.brushes);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->tool_window.active_brush[app->input]->button), TRUE);

			gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(app->tool_window.brush_scroll),
				app->tool_window.brush_table);
			gtk_widget_show_all(app->tool_window.brush_table);
			gtk_widget_show_all(app->tool_window.detail_ui);
		}

		for(x=0; x<app->menus.num_disable_if_no_select; x++)
		{
			gtk_widget_set_sensitive(app->menus.disable_if_no_select[x], TRUE);
		}
		*/

		window->flags |= DRAW_WINDOW_EDIT_SELECTION;
		window->flags &= ~(DRAW_WINDOW_HAS_SELECTION_AREA);
	}

	if(app->tool_box.active_brush[app->input]->change_editting_selection != NULL)
	{
		//app->tool_box.active_brush[app->input]->change_editting_selection(
		//	app->tool_box.active_brush[app->input]->brush_data, state);
	}
}

/***************************************************************
* LaplacianFilter関数										  *
* ラプラシアンフィルタでグレースケール情報からエッジを抽出する *
* 引数														 *
* pixels		: グレースケールのイメージ					 *
* width			: 画像の幅									 *
* height		: 画像の高さ								   *
* stride		: 1行分のバイト数							  *
* write_buff	: エッジデータを書き出すバッファ			   *
***************************************************************/
void LaplacianFilter(
	uint8* pixels,
	int width,
	int height,
	int stride,
	uint8* write_buff
)
{
	int sum;
	int weight;
	int i, j, k, l;

	// 一行目はエッジなし
	(void)memcpy(write_buff, pixels, width);

	for(i=1; i<height-1; i++)
	{
		// 一列目はエッジなし
		write_buff[i*stride] = pixels[i*stride];
		for(j=1; j<width-1; j++)
		{
			sum = 0;
			for(k=-1; k<=1; k++)
			{
				for(l=-1; l<=1; l++)
				{
					if(k == 0 && l == 0)
					{
						weight = -4;
					}
					else if(k == 0 || l == 0)
					{
						weight = 1;
					}
					else
					{
						continue;
					}

					sum += weight * pixels[(i+k)*stride + j + l];
				}
			}

			write_buff[i*stride + j] = (uint8)abs(sum);
		}
		// 一番右の列もエッジなし
		write_buff[i*stride+j] = pixels[i*stride+j];
	}

	// 一番下の行はエッジなし
	(void)memcpy(&write_buff[i*stride], &pixels[i*stride], width);
}

void BlendEditSelectionNormal(LAYER* work, LAYER* selection)
{
	uint8 *src = &work->pixels[3];
	int i;

	for(i=0; i<work->width * work->height; i++, src += 4)
	{
		selection->pixels[i] = (uint8)(((
			(*src)*DIV_PIXEL+(selection->pixels[i]*DIV_PIXEL)*(0xff-(*src))*DIV_PIXEL))*255);
	}
}

void BlendEditSelectionSource(LAYER* work, LAYER* selection)
{
	uint8 *src = &work->pixels[3];
	int i;

	for(i=0; i<work->width*work->height; i++, src += 4)
	{
		selection->pixels[i] = *src;
	}
}

void BlendEditSelectionCopy(LAYER* work, LAYER* selection)
{
	(void)memcpy(selection->pixels, work->pixels, selection->stride*selection->height);
}

void BlendEditSelectionOut(LAYER* work, LAYER* selection)
{
	uint8 *pixels = selection->pixels, *src = &work->pixels[3];
	int i;

	for(i=0; i<selection->width * selection->height; i++, src += 4)
	{
		pixels[i] = (uint8)(((1 + (unsigned int)pixels[i]) * (unsigned int)(0xff - *src)) >> 8);
	}
}

void InitializeBlendSelectionFunctions(void (*functions[])(LAYER* work, LAYER* selection))
{
	functions[SELECTION_BLEND_NORMAL] = BlendEditSelectionNormal;
	functions[SELECTION_BLEND_SOURCE] = BlendEditSelectionSource;
	functions[SELECTION_BLEND_COPY] = BlendEditSelectionCopy;
	functions[SELECTION_BLEND_MINUS] = BlendEditSelectionOut;
}

#ifdef __cplusplus
}
#endif
