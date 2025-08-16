#include "brush_core.h"
#include "draw_window.h"
#include "application.h"
#include "tool_box.h"
#include "text_layer.h"
#include "transform.h"
#include "configure.h"
#include "gui/draw_window.h"
#include "gui/tool_box.h"

void MouseButtonPressEvent(
	DRAW_WINDOW* canvas,
	EVENT_STATE* event_state
)
{
	APPLICATION *app = canvas->app;
	brush_update_function update_func = DefaultToolUpdate;
	void *update_data = NULL;
	EVENT_STATE state;
	FLOAT_T rev_zoom = canvas->rev_zoom;
	FLOAT_T x, y;

	state = *event_state;
	
	if((canvas->mouse_key_flags & (MOUSE_KEY_FLAG_LEFT | MOUSE_KEY_FLAG_RIGHT | MOUSE_KEY_FLAG_CENTER))
			== (MOUSE_KEY_FLAG_LEFT | MOUSE_KEY_FLAG_RIGHT | MOUSE_KEY_FLAG_CENTER))
	{
		return;
	}
	
	// 画面更新用のデータを取得
	if(canvas->transform == NULL)
	{
		if((app->tool_box.flags & TOOL_USING_BRUSH) != 0)
		{
			if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER)
			{
				update_func = app->tool_box.active_brush[app->input]->button_update;
				update_data = app->tool_box.active_brush[app->input];
			}
			else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
			{
				update_func = app->tool_box.active_vector_brush[app->input]->button_update;
				update_data = app->tool_box.active_vector_brush[app->input];
			}
		}
		else
		{
			update_func = app->tool_box.active_common_tool->button_update;
			update_data = app->tool_box.active_common_tool;
		}
	}
	
	if(state.input_device == CURSOR_INPUT_DEVICE_MOUSE)
	{
		state.pressure = (canvas->active_layer->layer_type == TYPE_NORMAL_LAYER) ? 1.0 : 0.5;
	}
	
	if(state.pressure < MINIMUM_PRESSURE)
	{
		state.pressure = MINIMUM_PRESSURE;
	}
	
	canvas->cursor_x = (state.cursor_x - canvas->half_size) * canvas->cos_value
		- (state.cursor_y - canvas->half_size) * canvas->sin_value + canvas->add_cursor_x;
	canvas->cursor_y = (state.cursor_x - canvas->half_size) * canvas->sin_value
		+ (state.cursor_y - canvas->half_size) * canvas->cos_value + canvas->add_cursor_y;
		
	x = rev_zoom * canvas->cursor_x;
	y = rev_zoom * canvas->cursor_y;

	// 左右反転表示中ならばX座標を修正
	if((canvas->flags & DRAW_WINDOW_DISPLAY_HORIZON_REVERSE) != 0)
	{
		x = canvas->width - x;
		canvas->cursor_x = canvas->disp_layer->width - canvas->cursor_x;
	}
	
	state.cursor_x = x, state.cursor_y = y;
	
	// 手ブレ補正実行
	if((state.mouse_key_flag & MOUSE_KEY_FLAG_LEFT) != 0)
	{
		// 現在の座標を記憶しておく
		app->tool_box.motion_queue.last_queued_x = x;
		app->tool_box.motion_queue.last_queued_y = y;

#ifndef _WIN32
		canvas->motion_history_last_time = event->time;
#endif

		// クリックされたボタンを記憶
		canvas->mouse_key_flags =state.mouse_key_flag;

		if(app->tool_box.smoother.num_use != 0)
		{
			if(app->tool_box.smoother.mode == SMOOTH_GAUSSIAN)
			{
				Smooth(&app->tool_box.smoother,
							&x, &y, state.event_time, canvas->rev_zoom);
			}
			else
			{
				app->tool_box.smoother.last_draw_point.x = event_state->cursor_x;
				app->tool_box.smoother.last_draw_point.y = event_state->cursor_y;
				(void)AddAverageSmoothPoint(&app->tool_box.smoother,
					&state, canvas->rev_zoom);
			}
		}

		// パース定規処理
		if((canvas->flags & DRAW_WINDOW_ACTIVATE_PERSPECTIVE_RULER) != FALSE)
		{
			SetPerspectiveRulerClickPoint(&canvas->perspective_ruler, x, y);
		}

		state.cursor_x = x, state.cursor_y = y;
	}

	if(canvas->transform != NULL)
	{
		TransformButtonPress(canvas->transform, x, y);
		return;
	}

	if((app->tool_box.flags & TOOL_USING_BRUSH) == 0)
	{
		app->tool_box.active_common_tool->press_function(
			canvas, app->tool_box.active_common_tool, &state);
	}
	else
	{
		if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER
			|| (canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
		{
			if((state.mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) == 0
					&& (state.mouse_key_flag & MOUSE_KEY_FLAG_LEFT) != 0)
			{
				if((state.mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) == 0)
				{
					app->tool_box.active_brush[app->input]->press_function(
						canvas, app->tool_box.active_brush[app->input], &state
					);
					canvas->state.mouse_key_flag |= MOUSE_KEY_FLAG_LEFT;
				}
				else if((state.mouse_key_flag & MOUSE_KEY_FLAG_LEFT) != 0)
				{
					// シフトキーが押されているので直線で処理
					FLOAT_T d;
					FLOAT_T dx = x - canvas->last_x;
					FLOAT_T dy = y - canvas->last_y;
					d = sqrt(dx*dx + dy*dy) * 2;
					dx = dx / d,	dy = dy / d;

					canvas->last_x += dx;
					canvas->last_y += dy;
					canvas->state.pressure = canvas->last_pressure;
					state = canvas->state;
					d -= 1;

					if((canvas->flags & DRAW_WINDOW_DRAWING_STRAIGHT) == 0)
					{
						state.cursor_x = canvas->last_x;
						state.cursor_y = canvas->last_y;
						app->tool_box.active_brush[app->input]->press_function(
							canvas, app->tool_box.active_brush[app->input], &state);
						canvas->flags |= DRAW_WINDOW_DRAWING_STRAIGHT;
					}

					while(d >= 1)
					{
						canvas->last_x += dx;
						canvas->last_y += dy;

						canvas->state.cursor_x = canvas->last_x;
						canvas->state.cursor_y = canvas->last_y;
						app->tool_box.active_brush[app->input]->motion_function(
							canvas,	app->tool_box.active_brush[app->input], &canvas->state);

						d -= 1;
					}
					state.cursor_x = x;
					state.cursor_y = y;
					app->tool_box.active_brush[app->input]->motion_function(
						canvas, app->tool_box.active_brush[app->input], &state);

					canvas->state.mouse_key_flag &= ~(MOUSE_KEY_FLAG_LEFT);
					canvas->last_x = x,	canvas->last_y = y;
					canvas->last_pressure = event_state->pressure;

					UpdateCanvasWidget(canvas->widgets);
				}
			}
			else
			{
				const int compare_flag = (MOUSE_KEY_FLAG_CONTROL | MOUSE_KEY_FLAG_SHIFT | MOUSE_KEY_FLAG_RIGHT);
				if((state.mouse_key_flag & compare_flag) == compare_flag)
				{
					LAYER *find;

					find = SearchLayerByCursorColor(canvas, (int)x, (int)y);
					if(find != NULL && find != canvas->active_layer)
					{
						ChangeActiveLayer(canvas, find);
						LayerViewSetActiveLayer(app->layer_view->layer_view, find);
						LayerViewMoveActiveLayer(app->layer_view->layer_view, find);
					}
				}
				else
				{
					app->tool_box.color_picker.core.press_function(
						canvas, &app->tool_box.color_picker.core, &state
					);
				}
				canvas->state.mouse_key_flag &= ~(MOUSE_KEY_FLAG_LEFT);
			}
		}
		else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
		{
			if((state.mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
			{
				app->tool_box.vector_control.mode = CONTROL_POINT_DELETE;
				app->tool_box.vector_control_core.press_func(
					canvas, &app->tool_box.vector_control_core, &state
				);
			}
			else if((state.mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0)
			{
				app->tool_box.vector_control.mode = CONTROL_POINT_MOVE;
				app->tool_box.vector_control_core.press_func(
					canvas, &app->tool_box.vector_control_core, &state
				);
			}
			else
			{
				app->tool_box.active_vector_brush[app->input]->press_func(
					canvas, app->tool_box.active_vector_brush[app->input], &state
				);
			}
		}
		else
		{
			TextLayerButtonPressCallBack(canvas, &state);
		}
	}

	// 再描画
	canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_OVER;
	update_func(canvas, state.cursor_x, state.cursor_y, update_data);

	canvas->before_cursor_x = state.cursor_x;
	canvas->before_cursor_y = state.cursor_y;
}

/*
* MouseMotionNotifyEvent関数
* マウスオーバーの処理
* 引数
* canvas	: 描画領域
* event		: マウスの情報
* 返り値
*	常にTRUE
*/
int MouseMotionNotifyEvent(
	DRAW_WINDOW* canvas,
	EVENT_STATE* event_state
)
{
#define QUEUE_APPEND_DISTANCE 0.5
	APPLICATION *app = canvas->app;
	EVENT_STATE state = *event_state;
	brush_update_function update_function = DefaultToolUpdate;
	void *update_data = NULL;
	double pressure;
	double x, y, x0, y0;
	double rev_zoom = canvas->rev_zoom;
	int update_window = 0;
	
	x0 = event_state->cursor_x, y0 = event_state->cursor_y;

	// 回転分を計算
	canvas->cursor_x = (x0 - canvas->half_size) * canvas->cos_value
		- (y0 - canvas->half_size) * canvas->sin_value + canvas->add_cursor_x;
	canvas->cursor_y = (x0 - canvas->half_size) * canvas->sin_value
		+ (y0 - canvas->half_size) * canvas->cos_value + canvas->add_cursor_y;
	x = rev_zoom * canvas->cursor_x;
	y = rev_zoom * canvas->cursor_y;

	// 左右反転表示中ならばX座標を修正
	if((canvas->flags & DRAW_WINDOW_DISPLAY_HORIZON_REVERSE) != 0)
	{
		x = canvas->width - x;
		canvas->cursor_x = canvas->disp_layer->width - canvas->cursor_x;
	}
	
	state.cursor_x = x, state.cursor_y = y;

	// 入力デバイスの設定
	if(event_state->input_device == CURSOR_INPUT_DEVICE_ERASER)
	{
		if(app->input == CURSOR_INPUT_DEVICE_PEN && (app->tool_box.flags & TOOL_USING_BRUSH) != 0)
		{
			void *target_tool = NULL;
			app->input = INPUT_ERASER;
			switch(canvas->active_layer->layer_type)
			{
			case TYPE_NORMAL_LAYER:
				target_tool = app->tool_box.active_brush[app->input];
				break;
			case TYPE_VECTOR_LAYER:
				target_tool = app->tool_box.active_vector_brush[app->input];
				break;
			}
			
			if(target_tool != NULL)
			{
				ChangeDetailUI(canvas->active_layer->layer_type, target_tool, FALSE, app);
			}
		}
	}
	else
	{
		if(app->input != INPUT_PEN && (app->tool_box.flags & TOOL_USING_BRUSH) != 0)
		{
			void *target_tool = NULL;
			app->input = INPUT_PEN;
			switch(canvas->active_layer->layer_type)
			{
			case TYPE_NORMAL_LAYER:
				target_tool = app->tool_box.active_brush[app->input];
				break;
			case TYPE_VECTOR_LAYER:
				target_tool = app->tool_box.active_vector_brush[app->input];
				break;
			}
			
			if(target_tool != NULL)
			{
				ChangeDetailUI(canvas->active_layer->layer_type, target_tool, FALSE, app);
			}
		}
	}

	// 画面更新用のデータを取得
	if(canvas->transform == NULL)
	{
		if((app->tool_box.flags & TOOL_USING_BRUSH) != 0)
		{
			if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER)
			{
				update_function = app->tool_box.active_brush[app->input]->motion_update;
				update_data = (void*)app->tool_box.active_brush[app->input];
			}
			else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
			{
				update_function = app->tool_box.active_vector_brush[app->input]->motion_update;
				update_data = (void*)app->tool_box.active_vector_brush[app->input];
			}
		}
		else
		{
			update_function = app->tool_box.active_common_tool->motion_update;
			update_data = (void*)app->tool_box.active_common_tool;
		}
	}

	// マウス以外なら筆圧を取得
	if(event_state->input_device != CURSOR_INPUT_DEVICE_MOUSE)
	{
		pressure = event_state->pressure;
#if defined(PRIOR_PRESSURE_TO_PRESS_EVENT) && PRIOR_PRESSURE_TO_PRESS_EVENT != 0
		if((canvas->state.mouse_key_flag & MOUSE_KEY_FLAG_LEFT) == 0)
		{
			if(pressure > MINIMUM_PRESSURE && (canvas->mouse_key_flags & MOUSE_KEY_FLAG_LEFT) == 0)
			{
				EVENT_STATE press_state = state;
				press_state.mouse_key_flag |= MOUSE_KEY_FLAG_LEFT;

				canvas->mouse_key_flags |= MOUSE_KEY_FLAG_LEFT;
				canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_OVER;

#ifndef _WIN32
				canvas->motion_history_last_time = event->time;
#endif

				// 手ブレ補正実行
				if(app->tool_box.smoother.num_use != 0
					&& (app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) == 0)
				{
					if(app->tool_box.smoother.mode == SMOOTH_GAUSSIAN)
					{
						Smooth(&app->tool_box.smoother, &x, &y, event_state->event_time, canvas->rev_zoom);
					}
					else
					{
						app->tool_box.smoother.last_draw_point.x = x0;
						app->tool_box.smoother.last_draw_point.y = y0;
						(void)AddAverageSmoothPoint(&app->tool_box.smoother,
							&state, canvas->rev_zoom);
					}
				}

				// 現在の座標を記憶しておく
				app->tool_box.motion_queue.last_queued_x = x;
				app->tool_box.motion_queue.last_queued_y = y;

				// パース定規処理
				if((canvas->flags & DRAW_WINDOW_ACTIVATE_PERSPECTIVE_RULER) != FALSE)
				{
					SetPerspectiveRulerClickPoint(&canvas->perspective_ruler, x, y);
				}

				if(canvas->transform != NULL)
				{
					TransformButtonPress(canvas->transform, x, y);
					goto func_end;
				}

				if((app->tool_box.flags & TOOL_USING_BRUSH) == 0)
				{
					state.cursor_x = x, state.cursor_y = y;
					state.pressure = pressure;
					app->tool_box.active_common_tool->press_function(
						canvas, app->tool_box.active_common_tool, &state);
				}
				else
				{
					if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER
						|| (canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
					{
						if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) == 0)
						{
							if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) == 0)
							{
								state.cursor_x = x, state.cursor_y = y;
								state.pressure = pressure;
								app->tool_box.active_brush[app->input]->press_function(
									canvas, app->tool_box.active_brush[app->input], &state
								);
							}
							else
							{
								// シフトキーが押されているので直線で処理
								FLOAT_T d;
								FLOAT_T dx = x - canvas->last_x;
								FLOAT_T dy = y - canvas->last_y;
								d = sqrt(dx*dx + dy*dy);
								dx = dx / d,	dy = dy / d;

								canvas->last_x += dx;
								canvas->last_y += dy;
								d -= 1;

								if((canvas->flags & DRAW_WINDOW_DRAWING_STRAIGHT) == 0)
								{
									state.cursor_x = canvas->last_x,	state.cursor_y = canvas->last_y;
									state.pressure = pressure;
									app->tool_box.active_brush[app->input]->press_function(
										canvas, app->tool_box.active_brush[app->input], (void*)&state);
									canvas->flags |= DRAW_WINDOW_DRAWING_STRAIGHT;
								}

								while(d >= 1)
								{
									canvas->last_x += dx;
									canvas->last_y += dy;

									state.cursor_x = canvas->last_x,	state.cursor_y = canvas->last_y;
									state.pressure = pressure;
									app->tool_box.active_brush[app->input]->motion_function(
										canvas, app->tool_box.active_brush[app->input], &state);

									d -= 1;
								}
								state.cursor_x = x, state.cursor_y = y;
								state.pressure = pressure;
								app->tool_box.active_brush[app->input]->motion_function(
									canvas, app->tool_box.active_brush[app->input], &state);

								canvas->state.mouse_key_flag &= ~(MOUSE_KEY_FLAG_LEFT);
								canvas->last_x = x,	canvas->last_y = y;
								canvas->last_pressure = pressure;

								UpdateCanvasWidget(canvas->widgets);
							}
						}
						else
						{
							state.cursor_x = x, state.cursor_y = y;
							state.pressure = pressure;
							app->tool_box.color_picker.core.press_function(
								canvas, &app->tool_box.color_picker.core, &state
							);
						}
					}
					else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
					{
						state.cursor_x = x, state.cursor_y = y;
						state.pressure = pressure;
						if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
						{
							app->tool_box.vector_control.mode = CONTROL_POINT_DELETE;
							app->tool_box.vector_control_core.press_func(
								canvas, &app->tool_box.vector_control_core, &state
							);
						}
						else if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0)
						{
							app->tool_box.vector_control.mode = CONTROL_POINT_MOVE;
							app->tool_box.vector_control_core.press_func(
								canvas, &app->tool_box.vector_control_core, &state
							);
						}
						else
						{
							app->tool_box.active_vector_brush[app->input]->press_func(
								canvas, app->tool_box.active_vector_brush[app->input], &state
							);
						}
					}
					else
					{
						state.cursor_x = x, state.cursor_y = y;
						state.pressure = pressure;
						TextLayerButtonPressCallBack(canvas, &state);
					}
				}

				goto func_end;
			}
		}
		else
		{
			if(pressure <= RELEASE_PRESSURE && canvas->mouse_key_flags & MOUSE_KEY_FLAG_LEFT)
			{
				EVENT_STATE release_state = state;
				state.mouse_key_flag |=  MOUSE_KEY_FLAG_LEFT;

				canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_OVER;

				ClearMotionQueue(canvas);

				canvas->mouse_key_flags &= ~(MOUSE_KEY_FLAG_LEFT);

				if(app->tool_box.smoother.num_use > 0 &&
					app->tool_box.smoother.mode != SMOOTH_GAUSSIAN)
				{
					FLOAT_T motion_x, motion_y, motion_pressure;
					FLOAT_T update_x, update_y;
					int finish;

					if(AddAverageSmoothPoint(&app->tool_box.smoother, &state, 0) != FALSE)
					{
						state = canvas->state;
						state.cursor_x = x, state.cursor_y = y;
						state.pressure = pressure;
						if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER
							|| (canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
						{
							app->tool_box.active_brush[app->input]->motion_function(
								canvas,	app->tool_box.active_brush[app->input], &state
							);
						}
						else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
						{
							if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
							{
								/*
								window->app->tool_window.vector_control.mode = CONTROL_POINT_DELETE;
								window->app->tool_window.vector_control_core.motion_func(
									window, x, y, pressure, &window->app->tool_window.vector_control_core, (void*)(&window->state)
								);*/
							}
							else if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0 ||
								(app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) != 0)
							{
								/*
								window->app->tool_window.vector_control.mode = CONTROL_POINT_MOVE;
								window->app->tool_window.vector_control_core.motion_func(
									window, x, y, motion_pressure,
										&window->app->tool_window.vector_control_core, (void*)(&window->state)
								);*/
							}
							else
							{
								app->tool_box.active_vector_brush[app->input]->motion_func(
									canvas, app->tool_box.active_vector_brush[app->input], &state
								);
							}
						}
						else
						{
							TextLayerMotionCallBack(canvas, &state);
						}

						canvas->before_cursor_x = app->tool_box.smoother.last_draw_point.x;
						canvas->before_cursor_y = app->tool_box.smoother.last_draw_point.y;
						update_x = ((x-canvas->width/2)*canvas->cos_value + (y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
							+ canvas->rev_add_cursor_x;
						update_y = (- (x-canvas->width/2)*canvas->sin_value + (y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
							+ canvas->rev_add_cursor_y;
						update_function(canvas, update_x, update_y, update_data);
						app->tool_box.smoother.last_draw_point.x = update_x;
						app->tool_box.smoother.last_draw_point.y = update_y;
					}

					do
					{
						finish = AverageSmoothFlush(&app->tool_box.smoother, &state);

						if((app->tool_box.flags & TOOL_USING_BRUSH) == 0)
						{
							state.cursor_x = motion_x, state.cursor_y = motion_y;
							state.pressure = motion_pressure;
							app->tool_box.active_common_tool->motion_function(
								canvas, app->tool_box.active_common_tool,
									&state);
						}
						else
						{
							if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER
								|| (canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
							{
								FLOAT_T dx = app->tool_box.motion_queue.last_queued_x - x;
								FLOAT_T dy = app->tool_box.motion_queue.last_queued_y - y;

								if(dx * dx + dy * dy >= QUEUE_APPEND_DISTANCE * QUEUE_APPEND_DISTANCE)
								{
									MotionQueueAppendItem(&app->tool_box.motion_queue, &state);
								}
								// window->app->tool_window.active_brush[window->app->input]->motion_func(
								//	window, motion_x, motion_y, motion_pressure,
								//		window->app->tool_window.active_brush[window->app->input], (void*)(&window->state)
								// );
							}
							else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
							{
								if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
								{
									/*
									window->app->tool_window.vector_control.mode = CONTROL_POINT_DELETE;
									window->app->tool_window.vector_control_core.motion_func(
										window, motion_x, motion_y, motion_pressure, &window->app->tool_window.vector_control_core, (void*)(&window->state)
									);*/
								}
								else if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0 ||
									(app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) != 0)
								{
									/*
									window->app->tool_window.vector_control.mode = CONTROL_POINT_MOVE;
									window->app->tool_window.vector_control_core.motion_func(
										window, motion_x, motion_y, motion_pressure,
											&window->app->tool_window.vector_control_core, (void*)(&window->state)
									);*/
								}
								else
								{
									state.cursor_x = motion_x,  state.cursor_y = motion_y;
									state.pressure = motion_pressure;
									app->tool_box.active_vector_brush[app->input]->motion_func(
										canvas, app->tool_box.active_vector_brush[app->input], &state
									);
								}
							}
							else
							{
								state.cursor_x = motion_x,  state.cursor_y = motion_y;
								state.pressure = motion_pressure;
								TextLayerMotionCallBack(canvas, &state);
							}
						}

						canvas->before_cursor_x = app->tool_box.smoother.last_draw_point.x;
						canvas->before_cursor_y = app->tool_box.smoother.last_draw_point.y;
						update_x = ((motion_x-canvas->width/2)*canvas->cos_value + (motion_y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
							+ canvas->rev_add_cursor_x;
						update_y = (- (motion_x-canvas->width/2)*canvas->sin_value + (motion_y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
							+ canvas->rev_add_cursor_y;
						update_function(canvas, update_x, update_y, update_data);
						app->tool_box.smoother.last_draw_point.x = update_x;
						app->tool_box.smoother.last_draw_point.y = update_y;
					} while(finish == FALSE);
				}

				INIT_SMOOTHER(app->tool_box.smoother);

				// ボタン状態もリセット
				canvas->state.mouse_key_flag &= ~(MOUSE_KEY_FLAG_LEFT);

				if(canvas->transform != NULL)
				{
					canvas->transform->trans_point = TRANSFORM_POINT_NONE;
					goto func_end;
				}

				if((app->tool_box.flags & TOOL_USING_BRUSH) == 0)
				{
					state.cursor_x = x, state.cursor_y = y;
					state.pressure = pressure;
					app->tool_box.active_common_tool->release_function(
						canvas, app->tool_box.active_common_tool, &state
					);
				}
				else
				{
					state.cursor_x = x, state.cursor_y = y;
					state.pressure = pressure;
					if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER)
					{
						app->tool_box.active_brush[app->input]->release_function(
							canvas, app->tool_box.active_brush[app->input], &state
						);
					}
					else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
					{
						if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
						{
							app->tool_box.vector_control.mode = CONTROL_POINT_DELETE;
							app->tool_box.vector_control_core.release_func(
									canvas, &app->tool_box.vector_control_core, &state
							);
						}
						else if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0 ||
							(app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) != 0)
						{
							app->tool_box.vector_control.mode = CONTROL_POINT_MOVE;
							app->tool_box.vector_control_core.release_func(
								canvas, &app->tool_box.vector_control_core, &state
						);
						}
						else
						{
							app->tool_box.active_vector_brush[app->input]->release_func(
								canvas, app->tool_box.active_vector_brush[app->input], &state
							);
						}
					}
					else if(canvas->active_layer->layer_type == TYPE_TEXT_LAYER)
					{
						state.cursor_x = x, state.cursor_y = y;
						state.pressure = pressure;
						TextLayerButtonReleaseCallBack(canvas, &state);
					}
				}
			}
		}
#endif
	}
	else
	{
		pressure = 1.0;

#ifdef _WIN32
		if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_LEFT) == 0 && (canvas->state.mouse_key_flag & MOUSE_KEY_FLAG_LEFT) != 0)
#else
		if(0)
#endif
		{
			EVENT_STATE press_state = *event_state;
			state.mouse_key_flag |= MOUSE_KEY_FLAG_LEFT;

			canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_OVER;

			if(app->tool_box.smoother.num_use > 0 &&
				app->tool_box.smoother.mode != SMOOTH_GAUSSIAN)
			{
				FLOAT_T motion_x, motion_y, motion_pressure;
				FLOAT_T update_x, update_y;
				int finish;

				ClearMotionQueue(canvas);

				if(AddAverageSmoothPoint(&app->tool_box.smoother, &press_state, 0) != FALSE)
				{
					if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER
						|| (canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
					{
						FLOAT_T dx = app->tool_box.motion_queue.last_queued_x - x;
						FLOAT_T dy = app->tool_box.motion_queue.last_queued_y - y;

						if(dx * dx + dy * dy >= QUEUE_APPEND_DISTANCE * QUEUE_APPEND_DISTANCE)
						{
							MotionQueueAppendItem(&app->tool_box.motion_queue, &press_state);
						}
						// window->app->tool_window.active_brush[window->app->input]->motion_func(
						//	window, x, y, pressure,
						//		window->app->tool_window.active_brush[window->app->input], (void*)(&window->state)
						// );
					}
					else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
					{
						if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
						{
							/*
							window->app->tool_window.vector_control.mode = CONTROL_POINT_DELETE;
							window->app->tool_window.vector_control_core.motion_func(
								window, x, y, pressure, &window->app->tool_window.vector_control_core, (void*)(&window->state)
							);*/
						}
						else if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0 ||
							(app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) != 0)
						{
							/*
							window->app->tool_window.vector_control.mode = CONTROL_POINT_MOVE;
							window->app->tool_window.vector_control_core.motion_func(
								window, x, y, motion_pressure,
									&window->app->tool_window.vector_control_core, (void*)(&window->state)
							);*/
						}
						else
						{
							state.cursor_x = x, state.cursor_y = y;
							state.pressure = pressure;
							app->tool_box.active_vector_brush[app->input]->motion_func(
								canvas, app->tool_box.active_vector_brush[app->input], &state
							);
						}
					}
					else
					{
						TextLayerMotionCallBack(canvas, &state);
					}

					canvas->before_cursor_x = app->tool_box.smoother.last_draw_point.x;
					canvas->before_cursor_y = app->tool_box.smoother.last_draw_point.y;
					update_x = ((x-canvas->width/2)*canvas->cos_value + (y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
						+ canvas->rev_add_cursor_x;
					update_y = (- (x-canvas->width/2)*canvas->sin_value + (y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
						+ canvas->rev_add_cursor_y;
					update_function(canvas, update_x, update_y, update_data);
					app->tool_box.smoother.last_draw_point.x = update_x;
					app->tool_box.smoother.last_draw_point.y = update_y;
				}

				do
				{
					finish = AverageSmoothFlush(&app->tool_box.smoother,
						&press_state);

					press_state.cursor_x = motion_x,  press_state.cursor_y = motion_y;
					press_state.pressure = motion_pressure;
					if((app->tool_box.flags & TOOL_USING_BRUSH) == 0)
					{
						app->tool_box.active_common_tool->motion_function(
							canvas, app->tool_box.active_common_tool,
								&press_state);
					}
					else
					{
						if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER
							|| (canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
						{
							app->tool_box.active_brush[app->input]->motion_function(
								canvas,	app->tool_box.active_brush[app->input], &press_state
							);
						}
						else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
						{
							if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
							{
								/*
								window->app->tool_window.vector_control.mode = CONTROL_POINT_DELETE;
								window->app->tool_window.vector_control_core.motion_func(
									window, motion_x, motion_y, motion_pressure, &window->app->tool_window.vector_control_core, (void*)(&window->state)
								);
								*/
							}
							else if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0 ||
								(app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) != 0)
							{
								/*
								window->app->tool_window.vector_control.mode = CONTROL_POINT_MOVE;
								window->app->tool_window.vector_control_core.motion_func(
									window, motion_x, motion_y, motion_pressure,
										&window->app->tool_window.vector_control_core, (void*)(&window->state)
								);
								*/
							}
							else
							{
								FLOAT_T dx = app->tool_box.motion_queue.last_queued_x - x;
								FLOAT_T dy = app->tool_box.motion_queue.last_queued_y - y;

								if(dx * dx + dy * dy >= QUEUE_APPEND_DISTANCE * QUEUE_APPEND_DISTANCE)
								{
									MotionQueueAppendItem(&app->tool_box.motion_queue, &press_state);
								}
								// window->app->tool_window.active_vector_brush[window->app->input]->motion_func(
								//	window, motion_x, motion_y, motion_pressure,
								//		window->app->tool_window.active_vector_brush[window->app->input], (void*)(&window->state)
								// );
							}
						}
						else
						{
							TextLayerMotionCallBack(canvas, &state);
						}
					}

					canvas->before_cursor_x = app->tool_box.smoother.last_draw_point.x;
					canvas->before_cursor_y = app->tool_box.smoother.last_draw_point.y;
					update_x = ((motion_x-canvas->width/2)*canvas->cos_value + (motion_y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
						+ canvas->rev_add_cursor_x;
					update_y = (- (motion_x-canvas->width/2)*canvas->sin_value + (motion_y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
						+ canvas->rev_add_cursor_y;
					update_function(canvas, update_x, update_y, update_data);
					app->tool_box.smoother.last_draw_point.x = update_x;
					app->tool_box.smoother.last_draw_point.y = update_y;
				} while(finish == FALSE);
			}

			INIT_SMOOTHER(app->tool_box.smoother);

			// ボタン状態もリセット
			canvas->state.mouse_key_flag &= ~(MOUSE_KEY_FLAG_LEFT);

			if(canvas->transform != NULL)
			{
				canvas->transform->trans_point = TRANSFORM_POINT_NONE;
				update_function(canvas, x0, y0, update_data);
				goto func_end;
			}

			state.cursor_x = x, state.cursor_y = y;
			state.pressure = pressure;
			if((app->tool_box.flags & TOOL_USING_BRUSH) == 0)
			{
				app->tool_box.active_common_tool->release_function(
					canvas, app->tool_box.active_common_tool, &state
				);
			}
			else
			{
				if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER
					|| (canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
				{
					app->tool_box.active_brush[app->input]->release_function(
						canvas, app->tool_box.active_brush[app->input], &state
					);
				}
				else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
				{
					if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
					{
						app->tool_box.vector_control.mode = CONTROL_POINT_DELETE;
						app->tool_box.vector_control_core.release_func(
							canvas, &app->tool_box.vector_control_core, &state
						);
					}
					else if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0 ||
						(app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) != 0)
					{
						app->tool_box.vector_control.mode = CONTROL_POINT_MOVE;
						app->tool_box.vector_control_core.release_func(
							canvas, &app->tool_box.vector_control_core, &state
						);
					}
					else
					{
						app->tool_box.active_vector_brush[app->input]->release_func(
							canvas, app->tool_box.active_vector_brush[app->input], &state
						);
					}
				}
			}

			update_function(canvas, x, y, update_data);
			canvas->last_x = x,	canvas->last_y = y;

			goto func_end;
		}
	}

	// 筆圧が下限値未満なら修正
	if(pressure < MINIMUM_PRESSURE)
	{
		pressure = MINIMUM_PRESSURE;
	}

	// 手ブレ補正実行
	if((canvas->mouse_key_flags & MOUSE_KEY_FLAG_LEFT) != 0)
	{
#define DISPLAY_UPDATE_DISTANCE 4
		if(fabs(canvas->before_x-x0)+fabs(canvas->before_y-y0) > DISPLAY_UPDATE_DISTANCE)
		{
			update_window++;
			canvas->before_x = x0, canvas->before_y = y0;
		}

		if(canvas->transform != NULL)
		{
			TransformMotionNotifyCallBack(canvas, canvas->transform, x, y, &canvas->state);
			update_function(canvas, x0, y0, update_data);
			goto func_end;
		}
		else
		{
			canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_OVER;
			if(app->tool_box.smoother.num_use != 0
				&& (app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) == 0)
			{
				if(app->tool_box.smoother.mode == SMOOTH_GAUSSIAN)
				{
					Smooth(&app->tool_box.smoother, &x, &y, event_state->event_time, canvas->rev_zoom);
				}
				else
				{
					if(AddAverageSmoothPoint(&app->tool_box.smoother, &state, canvas->rev_zoom) == FALSE)
					{
						canvas->flags &= ~(DRAW_WINDOW_UPDATE_ACTIVE_OVER);
						goto func_end;
					}

					canvas->before_cursor_x = app->tool_box.smoother.last_draw_point.x;
					canvas->before_cursor_y = app->tool_box.smoother.last_draw_point.y;
					x0 = ((x-canvas->width/2)*canvas->cos_value + (y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
						+ canvas->rev_add_cursor_x;
					y0 = (- (x-canvas->width/2)*canvas->sin_value + (y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
						+ canvas->rev_add_cursor_y;
					app->tool_box.smoother.last_draw_point.x = x0;
					app->tool_box.smoother.last_draw_point.y = y0;
				}
			}

			// パース定規処理
			if((canvas->flags & DRAW_WINDOW_ACTIVATE_PERSPECTIVE_RULER) != FALSE)
			{
				GetPerspectiveRulerPoint(&canvas->perspective_ruler, x, y, &x, &y);
			}
		}
	}
	else if(canvas->transform != NULL)
	{
		goto func_end;
	}

	if((app->tool_box.flags & TOOL_USING_BRUSH) == 0
		|| canvas->active_layer->layer_type == TYPE_NORMAL_LAYER
		&& (canvas->state.mouse_key_flag & MOUSE_KEY_FLAG_LEFT) != 0
		|| canvas->active_layer->layer_type != TYPE_NORMAL_LAYER
	)
	{
		FLOAT_T dx = app->tool_box.motion_queue.last_queued_x - x;
		FLOAT_T dy = app->tool_box.motion_queue.last_queued_y - y;

		if(dx * dx + dy * dy >= QUEUE_APPEND_DISTANCE * QUEUE_APPEND_DISTANCE)
		{
			MotionQueueAppendItem(&app->tool_box.motion_queue, &state);
		}
	}
	else
	{
		update_function(canvas, x0, y0, update_data);
	}
	if((app->tool_box.flags & TOOL_USING_BRUSH) != 0)
	{
		if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
		{
			if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
			{
				update_function = app->tool_box.vector_control_core.motion_update;
			}
			else if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0 ||
				(app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) != 0)
			{
				update_function = app->tool_box.vector_control_core.motion_update;
			}
		}
	}
	/*
	if((window->app->tool_window.flags & TOOL_USING_BRUSH) == 0)
	{
		window->app->tool_window.active_common_tool->motion_func(
			window, x, y, window->app->tool_window.active_common_tool,
			(void*)(&window->state) );
	}
	else
	{
		if(window->active_layer->layer_type == TYPE_NORMAL_LAYER
			|| (window->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
		{
			window->app->tool_window.active_brush[window->app->input]->motion_func(
				window, x, y, pressure, window->app->tool_window.active_brush[window->app->input], (void*)(&window->state)
			);
		}
		else if(window->active_layer->layer_type == TYPE_VECTOR_LAYER)
		{
			if((state & GDK_SHIFT_MASK) != 0)
			{
				window->app->tool_window.vector_control.mode = CONTROL_POINT_DELETE;
				window->app->tool_window.vector_control_core.motion_func(
					window, x, y, pressure, &window->app->tool_window.vector_control_core, (void*)(&window->state)
				);
				update_func = window->app->tool_window.vector_control_core.motion_update;
			}
			else if((state & GDK_CONTROL_MASK) != 0 ||
				(window->app->tool_window.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) != 0)
			{
				window->app->tool_window.vector_control.mode = CONTROL_POINT_MOVE;
				window->app->tool_window.vector_control_core.motion_func(
					window, x, y, pressure, &window->app->tool_window.vector_control_core, (void*)(&window->state)
				);
				update_func = window->app->tool_window.vector_control_core.motion_update;
			}
			else
			{
				window->app->tool_window.active_vector_brush[window->app->input]->motion_func(
					window, x, y, pressure, window->app->tool_window.active_vector_brush[window->app->input], (void*)(&window->state)
				);
			}
		}
		else
		{
			TextLayerMotionCallBack(window, x, y, state);
		}
	}
	*/

	canvas->state.mouse_key_flag = (event_state->mouse_key_flag & (~(MOUSE_KEY_FLAG_LEFT)))
										| (canvas->state.mouse_key_flag & MOUSE_KEY_FLAG_LEFT);

func_end:

	canvas->before_cursor_x = x0;
	canvas->before_cursor_y = y0;

	return FALSE;
}

void MouseButtonReleaseEvent(
	DRAW_WINDOW* canvas,
	EVENT_STATE* event_state
)
{
	APPLICATION *app = canvas->app;
	brush_update_function update_function = (brush_update_function)DefaultToolUpdate;
	void *update_data = NULL;
	FLOAT_T pressure = event_state->pressure;
	FLOAT_T x, y;

	// 画面更新用のデータを取得
	if(canvas->transform == NULL)
	{
		if((app->tool_box.flags & TOOL_USING_BRUSH) != 0)
		{
			if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER)
			{
				update_function = app->tool_box.active_brush[app->input]->motion_update;
				update_data = app->tool_box.active_brush[app->input];
			}
			else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
			{
				update_function = app->tool_box.active_vector_brush[app->input]->motion_update;
				update_data = app->tool_box.active_vector_brush[app->input];
			}
		}
		else
		{
			update_function = app->tool_box.active_common_tool->motion_update;
			update_data = app->tool_box.active_common_tool;
		}
	}

	if((canvas->mouse_key_flags & MOUSE_KEY_FLAG_LEFT) == 0)
	{
		return;
	}

	canvas->cursor_x = (event_state->cursor_x - canvas->half_size) * canvas->cos_value
		- (event_state->cursor_y - canvas->half_size) * canvas->sin_value + canvas->add_cursor_x;
	canvas->cursor_y = (event_state->cursor_x - canvas->half_size) * canvas->sin_value
		+ (event_state->cursor_y - canvas->half_size) * canvas->cos_value + canvas->add_cursor_y;
	x = canvas->rev_zoom * canvas->cursor_x;
	y = canvas->rev_zoom * canvas->cursor_y;

	// 左右反転表示中ならばX座標を修正
	if((canvas->flags & DRAW_WINDOW_DISPLAY_HORIZON_REVERSE) != 0)
	{
		x = canvas->width - x;
		canvas->cursor_x = canvas->disp_layer->width - canvas->cursor_x;
	}

	// マウス以外なら筆圧を取得
	if(event_state->input_device != CURSOR_INPUT_DEVICE_MOUSE)
	{
	
	}
	else
	{	// マウスなら筆圧は最大に
		event_state->pressure = 1.0;
	}

	// 筆圧が下限値未満なら修正
	if(pressure < MINIMUM_PRESSURE)
	{
		pressure = MINIMUM_PRESSURE;
	}

	// 手ブレ補正データの初期化
	if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_LEFT) != 0
			&& canvas->mouse_key_flags & MOUSE_KEY_FLAG_LEFT)
	{
		ClearMotionQueue(canvas);

		if(app->tool_box.smoother.num_use > 0 &&
			app->tool_box.smoother.mode != SMOOTH_GAUSSIAN)
		{
			EVENT_STATE state = *event_state;
			FLOAT_T motion_x, motion_y, motion_pressure;
			FLOAT_T update_x, update_y;
			int finish;

			if(AddAverageSmoothPoint(&app->tool_box.smoother, &state, 0) != FALSE)
			{
				state.cursor_x = x, state.cursor_y = y;
				state.pressure = pressure;
				if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER
					|| (canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
				{
					app->tool_box.active_brush[app->input]->motion_function(
						canvas, app->tool_box.active_brush[app->input], &state
					);
				}
				else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
				{
					if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
					{
						/*
						window->app->tool_window.vector_control.mode = CONTROL_POINT_DELETE;
						window->app->tool_window.vector_control_core.motion_func(
							window, x, y, pressure, &window->app->tool_window.vector_control_core, (void*)(&window->state)
						);*/
					}
					else if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0
						&& (app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) != 0)
					{
						/*
						window->app->tool_window.vector_control.mode = CONTROL_POINT_MOVE;
						window->app->tool_window.vector_control_core.motion_func(
							window, x, y, motion_pressure,
								&window->app->tool_window.vector_control_core, (void*)(&window->state)
						);*/
					}
					else
					{
						app->tool_box.active_vector_brush[app->input]->motion_func(
							canvas, app->tool_box.active_vector_brush[app->input], &state
						);
					}
				}
				else
				{
					TextLayerMotionCallBack(canvas, &state);
				}

				canvas->before_cursor_x = app->tool_box.smoother.last_draw_point.x;
				canvas->before_cursor_y = app->tool_box.smoother.last_draw_point.y;
				update_x = ((x-canvas->width/2)*canvas->cos_value + (y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_x;
				update_y = (- (x-canvas->width/2)*canvas->sin_value + (y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_y;
				update_function(canvas, update_x, update_y, update_data);
				app->tool_box.smoother.last_draw_point.x = update_x;
				app->tool_box.smoother.last_draw_point.y = update_y;
			}

			do
			{
				finish = AverageSmoothFlush(&app->tool_box.smoother, &state);

				if((app->tool_box.flags & TOOL_USING_BRUSH) == 0)
				{
					app->tool_box.active_common_tool->motion_function(
						canvas, app->tool_box.active_common_tool,
							&state);
				}
				else
				{
					if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER
						|| (canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
					{
						app->tool_box.active_brush[app->input]->motion_function(
							canvas, app->tool_box.active_brush[app->input], &state
						);
					}
					else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
					{
						if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
						{
							/*
							window->app->tool_window.vector_control.mode = CONTROL_POINT_DELETE;
							window->app->tool_window.vector_control_core.motion_func(
								window, motion_x, motion_y, motion_pressure, &window->app->tool_window.vector_control_core, (void*)(&window->state)
							);
							*/
						}
						else if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0 ||
							(app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) != 0)
						{
							/*
							window->app->tool_window.vector_control.mode = CONTROL_POINT_MOVE;
							window->app->tool_window.vector_control_core.motion_func(
								window, motion_x, motion_y, motion_pressure,
									&window->app->tool_window.vector_control_core, (void*)(&window->state)
							);
							*/
						}
						else
						{
							app->tool_box.active_vector_brush[app->input]->motion_func(
								canvas, app->tool_box.active_vector_brush[app->input], &state
							);
						}
					}
					else
					{
						TextLayerMotionCallBack(canvas, &state);
					}
				}

				canvas->before_cursor_x = app->tool_box.smoother.last_draw_point.x;
				canvas->before_cursor_y = app->tool_box.smoother.last_draw_point.y;
				update_x = ((motion_x-canvas->width/2)*canvas->cos_value + (motion_y-canvas->height/2)*canvas->sin_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_x;
				update_y = (- (motion_x-canvas->width/2)*canvas->sin_value + (motion_y-canvas->height/2)*canvas->cos_value) * canvas->zoom_rate
					+ canvas->rev_add_cursor_y;
				update_function(canvas, update_x, update_y, update_data);
				app->tool_box.smoother.last_draw_point.x = update_x;
				app->tool_box.smoother.last_draw_point.y = update_y;
			} while(finish == FALSE);
		}

		INIT_SMOOTHER(app->tool_box.smoother);

		// ボタン状態をリセット
		canvas->state.mouse_key_flag &= ~(MOUSE_KEY_FLAG_LEFT);

		// 座標を記憶
		canvas->last_x = x,	canvas->last_y = y;
	}

	if(canvas->transform != NULL)
	{
		canvas->transform->trans_point = TRANSFORM_POINT_NONE;
		update_function(canvas, event_state->cursor_x, event_state->cursor_y, update_data);
		goto func_end;
	}

	if((app->tool_box.flags & TOOL_USING_BRUSH) == 0)
	{
		EVENT_STATE state = *event_state;
		state.cursor_x = x, state.cursor_y = y;
		state.pressure = pressure;
		app->tool_box.active_common_tool->release_function(
			canvas, app->tool_box.active_common_tool, &state
		);
	}
	else
	{
		EVENT_STATE state = *event_state;
		state.cursor_x = x, state.cursor_y = y;
		state.pressure = pressure;
		if(canvas->active_layer->layer_type == TYPE_NORMAL_LAYER
			|| (canvas->flags & DRAW_WINDOW_EDIT_SELECTION) != 0)
		{
			app->tool_box.active_brush[app->input]->release_function(
				canvas,	app->tool_box.active_brush[app->input], &state
			);
		}
		else if(canvas->active_layer->layer_type == TYPE_VECTOR_LAYER)
		{
			if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_SHIFT) != 0)
			{
				app->tool_box.vector_control.mode = CONTROL_POINT_DELETE;
				app->tool_box.vector_control_core.release_func(
					canvas, &app->tool_box.vector_control_core, &state
				);
			}
			else if((event_state->mouse_key_flag & MOUSE_KEY_FLAG_CONTROL) != 0 ||
				(app->tool_box.vector_control.flags & CONTROL_POINT_TOOL_HAS_POINT) != 0)
			{
				app->tool_box.vector_control.mode = CONTROL_POINT_MOVE;
				app->tool_box.vector_control_core.release_func(
					canvas, &app->tool_box.vector_control_core, &state
				);
			}
			else
			{
				app->tool_box.active_vector_brush[app->input]->release_func(
					canvas, app->tool_box.active_vector_brush[app->input], &state
				);
			}
		}
		else if(canvas->active_layer->layer_type == TYPE_TEXT_LAYER)
		{
			TextLayerButtonReleaseCallBack(canvas, &state);
		}
	}

	if((canvas->mouse_key_flags & MOUSE_KEY_FLAG_LEFT) != 0
		&& (event_state->mouse_key_flag & MOUSE_KEY_FLAG_LEFT) == 0)
	{
		canvas->mouse_key_flags &= ~(MOUSE_KEY_FLAG_LEFT);
	}

	update_function(canvas, event_state->cursor_x, event_state->cursor_y, update_data);

func_end:
	canvas->before_cursor_x = event_state->cursor_x;
	canvas->before_cursor_y = event_state->cursor_y;

	canvas->state.mouse_key_flag = event_state->mouse_key_flag & ~(MOUSE_KEY_FLAG_LEFT);
}
