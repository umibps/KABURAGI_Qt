#include "../common_tools.h"
#include "../gui/draw_window.h"
#include "../application.h"

#ifdef __cplusplus
extern "C" {
#endif

void ColorPickerButtonPressCallback(
	DRAW_WINDOW* canvas,
	EVENT_STATE* state,
	COMMON_TOOL_CORE* core
)
{
	int x = (int)state->cursor_x, y = (int)state->cursor_y;
	
	if((state->mouse_key_flag & MOUSE_KEY_FLAG_RIGHT)
		&& (state->mouse_key_flag & MOUSE_KEY_FLAG_CONTROL))
	{
		ExcecuteCanvasWidgetColorHistoryPopupMenu(canvas->widgets);
		return;
	}
	
	if(state->cursor_x < 0 || x >= canvas->width || y < 0 || y >= canvas->height)
	{
		return;
	}
	
	if((state->mouse_key_flag & MOUSE_KEY_FLAG_LEFT) || (state->mouse_key_flag & MOUSE_KEY_FLAG_RIGHT))
	{
		COLOR_PICKER *picker = (COLOR_PICKER*)core->tool_data;
		uint8 color[4];
		
		switch(picker->mode)
		{
		case COLOR_PICKER_SOURCE_ACTIVE_LAYER:
			color[0] = canvas->active_layer->pixels[(int)y*canvas->active_layer->stride+x*4+0];
			color[1] = canvas->active_layer->pixels[(int)y*canvas->active_layer->stride+x*4+1];
			color[2] = canvas->active_layer->pixels[(int)y*canvas->active_layer->stride+x*4+2];
			color[3] = canvas->active_layer->pixels[(int)y*canvas->active_layer->stride+x*4+3];
			if(color[3] == 0)
			{
				return;
			}
			break;
		case COLOR_PICKER_SOURCE_CANVAS:
			color[0] = canvas->mixed_layer->pixels[(int)y*canvas->mixed_layer->stride+x*4+0];
			color[1] = canvas->mixed_layer->pixels[(int)y*canvas->mixed_layer->stride+x*4+1];
			color[2] = canvas->mixed_layer->pixels[(int)y*canvas->mixed_layer->stride+x*4+2];
			color[3] = canvas->mixed_layer->pixels[(int)y*canvas->mixed_layer->stride+x*4+3];
			if(color[3] == 0)
			{
				color[0] = color[1] = color[2] = 0xFF;
			}
			break;
		}
		
		canvas->app->tool_box.color_chooser.rgb[0] = color[0];
		canvas->app->tool_box.color_chooser.rgb[1] = color[1];
		canvas->app->tool_box.color_chooser.rgb[2] = color[2];
		
#if defined(USE_BGR_COLOR_SPACE) && USE_BGR_COLOR_SPACE != 0
		{
			uint8 r = canvas->app->tool_box.color_chooser.rgb[2];
			canvas->app->tool_box.color_chooser.rgb[2] =
				canvas->app->tool_box.color_chooser.rgb[0];
			canvas->app->tool_box.color_chooser.rgb[0] = r;
			
			r = color[2];
			color[2] = color[0];
			color[0] = r;
		}
#endif
		RGB2HSV_Pixel(color, &canvas->app->tool_box.color_chooser.hsv);
	}
}

#ifdef __cplusplus
}
#endif
