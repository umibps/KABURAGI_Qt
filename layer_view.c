#include "draw_window.h"
#include "application.h"
#include "layer_window.h"

/*
* ClearLayerView関数
* レイヤービューのウィジェットを全て削除する
*/
void ClearLayerView(APPLICATION* app)
{
	DRAW_WINDOW *active_canvas = GetActiveDrawWindow(app);
	LAYER *layer = active_canvas->layer;

	while(layer != NULL)
	{
		DeleteLayerWidget(layer);
		layer = layer->next;
	}
}

/*
 ResetLayerView関数
* レイヤー一覧ウィジェットをリセットする
* 引数
* canvas	: 表示するキャンバス
*/
void ResetLayerView(DRAW_WINDOW* canvas)
{
	APPLICATION *app = canvas->app;
	LAYER *layer;
	int i;

	ClearLayerView(app);

	// 一番上のレイヤーから順に再追加する
	layer = canvas->layer;
	while(layer->next != NULL)
	{
		layer = layer->next;
	}

	for(i = 0; i < layer != canvas->layer && layer->prev != NULL; i++, layer = layer->prev)
	{
		LayerViewAddLayer(layer, layer->prev, app->layer_view, i+1);
	}
	canvas->num_layer = (uint16)(i+1);
	LayerViewAddLayer(layer, canvas->layer, canvas->num_layer, canvas->num_layer);

	LayerViewSetActiveLayer(app->layer_view->layer_view, canvas->active_layer);
}
