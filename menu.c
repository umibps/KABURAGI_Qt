#include <string.h>
#include "application.h"
#include "layer.h"
#include "layer_view.h"
#include "utils.h"
#include "image_file/image_file.h"
#include "gui/dialog.h"
#include "gui/draw_window.h"

#ifdef __cplusplus
extern "C" {
#endif

void ExecuteOpen(APPLICATION* app)
{
	DRAW_WINDOW *canvas, **store_canvas;
	char *file_path;

	file_path = GetImageFileOpenPath(app);

	if(file_path == NULL)
	{
		return;
	}

	// CreateDrawWindow内でapp->window_numが更新される関係上
		// アドレスの保存先を記録しておく
	store_canvas = &app->draw_window[app->window_num];
	*store_canvas = canvas = ReadImageFile(app, file_path);
	if(canvas != NULL)
	{
		if(canvas->active_layer == NULL)
		{
			canvas->active_layer = canvas->layer;
		}
		canvas->widgets = CreateDrawWindowWidgets(app, canvas);
		AddDrawWindow2MainWindow(app->widgets, canvas);
		ResetLayerView(canvas);
	}
}

#define DATA_COMPRESSION_LEVEL 5
void ExecuteSave(APPLICATION* app)
{
	DRAW_WINDOW* canvas = GetActiveDrawWindow(app);
	char* file_path;
	char* file_extention;

	if(canvas == NULL)
	{
		return;
	}

	if(canvas->file_path == NULL)
	{
		ExecuteSaveAs(app);
		return;
	}

	WriteCanvasToImageFile(app, canvas, canvas->file_path, DATA_COMPRESSION_LEVEL);
}

void ExecuteSaveAs(APPLICATION* app)
{
	DRAW_WINDOW *canvas = GetActiveDrawWindow(app);
	char *file_path;
	char *file_extention;

	if(canvas == NULL)
	{
		return;
	}

	file_path = GetImageFileSaveaPath(app);
	WriteCanvasToImageFile(app, canvas, file_path, DATA_COMPRESSION_LEVEL);
}

void ExecuteMakeColorLayer(APPLICATION* app)
{
	// アクティブな描画領域
	DRAW_WINDOW *canvas = GetActiveDrawWindow(app);
	// 作成したレイヤーのアドレス
	LAYER* layer;
	// 新規作成するレイヤーの名前
	char layer_name[MAX_LAYER_NAME_LENGTH];
	// レイヤーの名前決定用のカウンタ
	int counter = 1;

	if(canvas == NULL)
	{
		return;
	}

	//AUTO_SAVE(app->draw_window[app->active_window]);

	// 「レイヤー○」の○に入る値を決定
		// (レイヤー名の重複を避ける)
	do
	{
		(void)sprintf(layer_name, "%s %d", app->labels->menu.new_color, counter);
		counter++;
	} while(CorrectLayerName(canvas->layer, layer_name) == 0);

	// 現在のアクティブレイヤーが通常レイヤー以外ならばツール変更
	/*
	if(window->active_layer->layer_type != TYPE_NORMAL_LAYER)
	{
		gtk_widget_destroy(app->tool_window.brush_table);
		CreateBrushTable(app, &app->tool_window, app->tool_window.brushes);
		gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(app->tool_window.brush_scroll),
			app->tool_window.brush_table);
		gtk_widget_show_all(app->tool_window.brush_table);
	}*/

	// レイヤー作成
	layer = CreateLayer(
		0,
		0,
		canvas->width,
		canvas->height,
		canvas->channel,
		TYPE_NORMAL_LAYER,
		canvas->active_layer,
		canvas->active_layer->next,
		layer_name,
		canvas
	);
	// 作業レイヤーと一時保存レイヤーのピクセルデータを初期化
	(void)memset(canvas->work_layer->pixels, 0, canvas->work_layer->stride*layer->height);

	// 局所キャンバスモードなら親キャンバスでもレイヤー作成
	if((canvas->flags & DRAW_WINDOW_IS_FOCAL_WINDOW) != 0)
	{
		DRAW_WINDOW *parent = app->draw_window[app->active_window];
		LAYER *parent_prev = SearchLayer(parent->layer, canvas->active_layer->name);
		LAYER *parent_next = (parent_prev == NULL) ? parent->layer : parent_prev->next;
		LAYER *parent_new = CreateLayer(0, 0, parent->width, parent->height, parent->channel,
			(eLAYER_TYPE)layer->layer_type, parent_prev, parent_next, layer->name, parent);
		parent->num_layer++;
		AddNewColorLayerHistory(parent_new, parent_new->layer_type);
	}

	// レイヤーの数を更新
	canvas->num_layer++;

	// レイヤーウィンドウに追加してアクティブレイヤーに
	LayerViewAddLayer(layer, canvas->layer,
		app->layer_view, canvas->num_layer);
	ChangeActiveLayer(canvas, layer);
	LayerViewSetActiveLayer(app->layer_view->layer_view, layer);

	// 「新規レイヤー」の履歴を登録
	AddNewColorLayerHistory(layer, layer->layer_type);

	// 画面を更新
	canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
}

void ExecuteDeleteActiveLayer(APPLICATION* app)
{
	DRAW_WINDOW *canvas = GetActiveDrawWindow(app);
	LAYER *next_active, *delete_layer = canvas->active_layer;

	// レイヤーが0枚以下にならないようにチェック
	if(canvas->num_layer <= 1)
	{
		return;
	}

	AddDeleteLayerHistory(canvas, delete_layer);

	// 局所キャンバスモードの場合、元キャンバスのレイヤーも削除
	if(canvas->flags & DRAW_WINDOW_IS_FOCAL_WINDOW)
	{
		LAYER *focal_delete = SearchLayer(canvas->focal_window->layer, canvas->active_layer->name);

		if(focal_delete != NULL && canvas->focal_window->num_layer > 1)
		{
			if(focal_delete == canvas->focal_window->active_layer)
			{
				canvas->focal_window->active_layer =
					(focal_delete->prev == NULL) ? focal_delete->next : focal_delete->prev;
			}
			DeleteLayer(&focal_delete);
		}
		canvas->focal_window->num_layer--;
	}

	// 削除対象のレイヤーが一番下の場合はアクティブレイヤーを上側	
		// そうでなければ下側のレイヤーを削除後のアクティブレイヤーにする
	if(canvas->active_layer == canvas->layer)
	{
		next_active = canvas->layer->next;
		ChangeActiveLayer(canvas, next_active);
		DeleteLayer(&delete_layer);
		canvas->layer = next_active;
	}
	else
	{
		next_active = canvas->active_layer->prev;
		ChangeActiveLayer(canvas, next_active);
		DeleteLayer(&delete_layer);
	}
	canvas->num_layer--;

	// GUIのアクティブレイヤーを変更
	ChangeActiveLayer(canvas, next_active);
	LayerViewSetActiveLayer(app->layer_view->layer_view, next_active);

	ResetLayerView(canvas);

	// 画面を更新
	canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
}

void ExecuteUndo(APPLICATION* app)
{
	DRAW_WINDOW *canvas = GetActiveDrawWindow(app);

	if(canvas == NULL)
	{
		return;
	}

	if(canvas->history.rest_undo > 0)
	{
		int execute = (canvas->history.point == 0)
				? HISTORY_BUFFER_SIZE - 1 : canvas->history.point - 1;
		canvas->history.history[execute].undo(canvas, canvas->history.history[execute].data);
		canvas->history.rest_undo--;
		canvas->history.rest_redo++;

		if(canvas->history.point > 0)
		{
			canvas->history.point--;
		}
		else
		{
			canvas->history.point = HISTORY_BUFFER_SIZE - 1;
		}

		canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
		ForceUpdateCanvasWidget(canvas->widgets);
	}
}

void ExecuteRedo(APPLICATION* app)
{
	DRAW_WINDOW* canvas = GetActiveDrawWindow(app);

	if(canvas == NULL)
	{
		return;
	}

	if(canvas->history.rest_redo > 0)
	{
		int execute = canvas->history.point;
		if(execute == HISTORY_BUFFER_SIZE)
		{
			execute = 0;
		}
		canvas->history.history[execute].redo(canvas, canvas->history.history[execute].data);
		canvas->history.point++;
		canvas->history.rest_undo++;
		canvas->history.rest_redo--;

		if(canvas->history.point == HISTORY_BUFFER_SIZE)
		{
			canvas->history.point = 0;
		}

		canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
		ForceUpdateCanvasWidget(canvas->widgets);
	}
}

#ifdef __cplusplus
}
#endif
