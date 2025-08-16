#include <string.h>
#include "layer.h"
#include "layer_window.h"
#include "draw_window.h"
#include "application.h"
#include "layer_view.h"
#include "memory.h"
#include "image_file/original_format.h"
#include "gui/draw_window.h"

#ifdef __cplusplus
extern "C" {
#endif

LAYER* CreateLayer(
	int32 x,
	int32 y,
	int32 width,
	int32 height,
	int8 channel,
	eLAYER_TYPE layer_type,
	LAYER* prev_layer,
	LAYER* next_layer,
	const char* name,
	struct _DRAW_WINDOW* window
)
{
	LAYER *ret = (LAYER*)MEM_CALLOC_FUNC(1, sizeof(*ret));

	ret->name = (name == NULL) ? NULL : MEM_STRDUP_FUNC(name);
	ret->layer_type = (uint16)layer_type;
	ret->x = x, ret->y = y;
	ret->width = width, ret->height = height;
	if(channel <= 4)	/* Temporary Layerでは作業用に5channel分メモリ確保する
							ただし、SurfaceのStrideは4channel分で設定する必要あり	*/
	{
		ret->channel = channel;
		ret->stride = width * ret->channel;
	}
	else
	{
		ret->channel = 4;
		ret->stride = width * 4;
	}
	ret->pixels = (uint8*)MEM_CALLOC_FUNC(sizeof(uint8)*width*height*channel, 1);
	InitializeGraphicsImageSurfaceForData(&ret->surface, ret->pixels, GRAPHICS_FORMAT_ARGB32,
											width, height, ret->stride, &window->app->graphics);
	InitializeGraphicsDefaultContext(&ret->context, &ret->surface, &window->app->graphics);
	ret->alpha = 100;
	ret->window = window;

	InitializeLayerContext(ret);

	if(prev_layer != NULL)
	{
		ret->prev = prev_layer;
		ret->layer_set = prev_layer->layer_set;
		prev_layer->next = ret;
	}
	if(next_layer != NULL)
	{
		ret->next = next_layer;
		next_layer->prev = ret;
	}

	return ret;
}

/*
* CreateDispTempLayer関数
* 表示用一時保存レイヤーを作成
* 引数
* x				: レイヤー左上のX座標
* y				: レイヤー左上のY座標
* width			: レイヤーの幅
* height		: レイヤーの高さ
* channel		: レイヤーのチャンネル数
* layer_type	: レイヤーのタイプ
* window		: 追加するレイヤーを管理する描画領域
* 返り値
*	初期化された構造体のアドレス
*/
LAYER* CreateDispTempLayer(
	int32 x,
	int32 y,
	int32 width,
	int32 height,
	int8 channel,
	eLAYER_TYPE layer_type,
	DRAW_WINDOW* window
)
{
	// 返り値
	LAYER* ret = (LAYER*)MEM_CALLOC_FUNC(1, sizeof(*ret));

	// 表示用の描画領域サイズを計算
	window->disp_size = (int)(2 * sqrt((width/2)*(width/2)+(height/2)*(height/2)) + 1);
	window->disp_stride = window->disp_size * 4;
	window->half_size = window->disp_size * 0.5;

	// 0初期化
	(void)memset(ret, 0, sizeof(*ret));

	// 値のセット
	ret->channel = (channel <= 4) ? channel : 4;
	ret->layer_type = (uint16)layer_type;
	ret->x = x, ret->y = y;
	ret->width = width, ret->height = height, ret->stride = width * ret->channel;
	ret->pixels = (uint8*)MEM_CALLOC_FUNC(sizeof(uint8)*window->disp_stride*window->disp_size, 1);
	ret->alpha = 100;
	ret->window = window;

	// ピクセルを初期化
	(void)memset(ret->pixels, 0x0, sizeof(uint8)*window->disp_stride*window->disp_size);

	InitializeGraphicsImageSurfaceForData(&ret->surface, ret->pixels,
										GRAPHICS_FORMAT_ARGB32, ret->width, ret->height, ret->stride, &window->app->graphics);
	InitializeGraphicsDefaultContext(&ret->context, &ret->surface, &window->app->graphics);
	// InitializeLayerContext(ret);

	return ret;
}

/*
 DeleteLayer関数
 レイヤーを削除する
 ウィジェットがある場合、そちらも削除
 引数
 layer	: 削除対象のレイヤー1
*/
void DeleteLayer(LAYER** layer)
{
	DRAW_WINDOW *canvas;

	if(layer == NULL || *layer == NULL)
	{
		return;
	}

	canvas = (*layer)->window;

	if((*layer)->layer_type == TYPE_VECTOR_LAYER)
	{
		DeleteVectorLayer(&(*layer)->layer_data.vector_layer);
	}
	else if((*layer)->layer_type == TYPE_TEXT_LAYER)
	{
		DeleteTextLayer(&(*layer)->layer_data.text_layer);
	}
	else if((*layer)->layer_type == TYPE_LAYER_SET)
	{
		DeleteLayerSet(&(*layer)->layer_data.layer_set);
	}
	else if((*layer)->layer_type == TYPE_ADJUSTMENT_LAYER)
	{
		DeleteAdjustmentLayer(&(*layer)->layer_data.adjustment_layer);
	}

	if((*layer)->next != NULL)
	{
		(*layer)->next->prev = (*layer)->prev;
	}
	if((*layer)->prev != NULL)
	{
		(*layer)->prev->next = (*layer)->next;
	}

	(void)memset(canvas->work_layer->pixels, 0, canvas->pixel_buf_size);
	if((*layer)->widget != NULL)
	{
		canvas->num_layer--;
		DeleteLayerWidget(*layer);
		canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
	}

	MEM_FREE_FUNC((*layer)->last_write_data);

	MEM_FREE_FUNC((*layer)->name);

	MEM_FREE_FUNC((*layer)->pixels);
	MEM_FREE_FUNC(*layer);

	*layer = NULL;
}

LAYER* SearchLayer(LAYER* bottom_layer, const char* name)
{
	LAYER* layer = bottom_layer;

	while(layer != NULL)
	{
		if(strcmp(layer->name, name) == 0)
		{
			return layer;
		}
		layer = layer->next;
	}

	return NULL;
}

int CorrectLayerName(const LAYER* bottom_layer, const char* name)
{
	const LAYER *layer = bottom_layer;

	do
	{
		if(strcmp(layer->name, name) == 0)
		{
			return FALSE;
		}
		layer = layer->next;
	} while(layer != NULL);

	return TRUE;
}

void ChangeLayerOrder(LAYER* change_layer, LAYER* new_prev, LAYER** bottom)
{
	if(change_layer->prev != NULL)
	{
		change_layer->prev->next = change_layer->next;
	}
	if(change_layer->next != NULL)
	{
		change_layer->next->prev = change_layer->prev;
	}

	/*if(change_layer->layer_type == TYPE_ADJUSTMENT_LAYER)
	{
		change_layer->layer_data.adjustment_layer_p->target_layer = new_prev;
		change_layer->layer_data.adjustment_layer_p->release(change_layer->layer_data.adjustment_layer_p);
		change_layer->layer_data.adjustment_layer_p->update(change_layer->layer_data.adjustment_layer_p,
			new_prev, change_layer->window->mixed_layer, 0, 0, change_layer->width, change_layer->height);
	}*/

	if(new_prev != NULL)
	{
		if(new_prev->next != NULL)
		{
			new_prev->next->prev = change_layer;
		}
		change_layer->next = new_prev->next;
		new_prev->next = change_layer;

		if(change_layer == *bottom)
		{
			*bottom = new_prev;
		}
	}
	else
	{
		(*bottom)->prev = change_layer;
		change_layer->next = *bottom;
		*bottom = change_layer;
	}

	change_layer->prev = new_prev;

	// 局所キャンバスモードなら親キャンバスの順序も変更
	if((change_layer->window->flags & DRAW_WINDOW_IS_FOCAL_WINDOW) != 0)
	{
		LAYER *parent_new_prev = NULL;
		if(new_prev != NULL)
		{
			parent_new_prev = SearchLayer(change_layer->window->focal_window->layer, new_prev->name);
		}
		ChangeLayerOrder(SearchLayer(change_layer->window->focal_window->layer, change_layer->name),
			parent_new_prev, &change_layer->window->focal_window->layer);
	}
}

/*
* GetLayerChain関数
* ピン留めされたレイヤー配列を取得する
* 引数
* window	: 描画領域の情報
* num_layer	: レイヤー数を格納する変数のアドレス
* 返り値
*	レイヤー配列
*/
LAYER** GetLayerChain(DRAW_WINDOW* window, uint16* num_layer)
{
	// 返り値
	LAYER** ret =
		(LAYER**)MEM_ALLOC_FUNC(sizeof(*ret)*LAYER_CHAIN_BUFFER_SIZE);
	// バッファサイズ
	unsigned int buffer_size = LAYER_CHAIN_BUFFER_SIZE;
	// チェックするレイヤー
	LAYER* layer = window->layer;
	// レイヤーの数
	unsigned int num = 0;

	// ピン留めされたレイヤーを探す
	while(layer != NULL)
	{	// アクティブなレイヤーかピン留めされたレイヤーなら
		if(layer == window->active_layer || (layer->flags & LAYER_CHAINED) != 0)
		{	// 配列に追加
			ret[num] = layer;
			num++;
		}

		// バッファが不足していたら再確保
		if(num >= buffer_size)
		{
			buffer_size += LAYER_CHAIN_BUFFER_SIZE;
			ret = MEM_REALLOC_FUNC(ret, buffer_size * sizeof(*ret));
		}

		layer = layer->next;
	}

	*num_layer = (uint16)num;

	return ret;
}

/*
* DeleteSelectAreaPixels関数
* 選択範囲のピクセルデータを削除する
* 引数
* window	: 描画領域の情報
* target	: ピクセルデータを削除するレイヤー
* selection	: 選択範囲を管理するレイヤー
*/
void DeleteSelectAreaPixels(
	DRAW_WINDOW* window,
	LAYER* target,
	LAYER* selection
)
{
	// for文用のカウンタ
	int i;

	// 一時保存のレイヤーに選択範囲を写す
	(void)memset(window->temp_layer->pixels, 0, window->pixel_buf_size);
	for(i=0; i<selection->width*selection->height; i++)
	{
		window->temp_layer->pixels[i*4+3] = selection->pixels[i];
	}

	// レイヤー合成でピクセルデータの削除を実行
	window->layer_blend_functions[LAYER_BLEND_ALPHA_MINUS](window->temp_layer, target);
}

void ChangeActiveLayer(DRAW_WINDOW* canvas, LAYER* layer)
{
	APPLICATION *app = canvas->app;

	if(layer->layer_set != NULL)
	{
		canvas->active_layer_set = layer->layer_set;
	}

	if((app->tool_box.current_tool_type != layer->layer_type || layer->layer_type == TYPE_TEXT_LAYER)
			&& canvas->transform == NULL)
	{
		int enable_menu = TRUE;
		int i;

		app->tool_box.current_tool_type = layer->layer_type;
	}

	canvas->active_layer = layer;
}

void SetLayerLockOpacity(APPLICATION* app, int lock)
{
	DRAW_WINDOW *canvas = GetActiveDrawWindow(app);
	DRAW_WINDOW *focal_canvas;
	LAYER *active_layer;

	if(canvas == NULL)
	{
		return;
	}

	active_layer = canvas->active_layer;
	if(active_layer == NULL)
	{
		return;
	}

	if(lock)
	{
		active_layer->flags |= LAYER_LOCK_OPACITY;
	}
	else
	{
		active_layer->flags &= ~(LAYER_LOCK_OPACITY);
	}
	canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;

	if(canvas->flags & DRAW_WINDOW_IS_FOCAL_WINDOW)
	{
		LAYER *layer = SearchLayer(canvas->focal_window->layer, active_layer->name);
		if(layer == NULL)
		{
			return;
		}

		if(lock)
		{
			layer->flags |= LAYER_LOCK_OPACITY;
		}
		else
		{
			layer->flags &= ~(LAYER_LOCK_OPACITY);
		}
	}

	ForceUpdateCanvasWidget(canvas->widgets);
}

void SetLayerMaskUnder(APPLICATION* app, int mask)
{
	DRAW_WINDOW* canvas = GetActiveDrawWindow(app);
	DRAW_WINDOW* focal_canvas;
	LAYER* active_layer;

	if(canvas == NULL)
	{
		return;
	}

	active_layer = canvas->active_layer;
	if(active_layer == NULL)
	{
		return;
	}

	if(mask)
	{
		active_layer->flags |= LAYER_MASKING_WITH_UNDER_LAYER;
	}
	else
	{
		active_layer->flags &= ~(LAYER_MASKING_WITH_UNDER_LAYER);
	}
	canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;

	if(canvas->flags & DRAW_WINDOW_IS_FOCAL_WINDOW)
	{
		LAYER* layer = SearchLayer(canvas->focal_window->layer, active_layer->name);
		if(layer == NULL)
		{
			return;
		}

		if(mask)
		{
			layer->flags |= LAYER_MASKING_WITH_UNDER_LAYER;
		}
		else
		{
			layer->flags &= ~(LAYER_MASKING_WITH_UNDER_LAYER);
		}
	}

	ForceUpdateCanvasWidget(canvas->widgets);
}

typedef struct _ADD_NEW_COLOR_LAYER_DATA
{
	uint16 layer_type;
	uint16 name_length;
	int32 x, y;
	int32 width, height;
	uint8 channel;
	uint16 previous_layer_name_length;
	char *layer_name, *previous_layer_name;
} ADD_NEW_COLOR_LAYER_DATA;

static void AddNewColorLayerUndo(DRAW_WINDOW* canvas, void* history_data)
{
	ADD_NEW_COLOR_LAYER_DATA data;
	uint8 *buffer = (uint8*)history_data;
	LAYER *delete_layer;

	(void)memcpy(&data, buffer, offsetof(ADD_NEW_COLOR_LAYER_DATA, layer_name));
	buffer += offsetof(ADD_NEW_COLOR_LAYER_DATA, layer_name);
	data.layer_name = (char*)buffer;

	delete_layer = SearchLayer(canvas->layer, data.layer_name);
	if(delete_layer == canvas->active_layer)
	{
		LAYER *new_active = (delete_layer->prev == NULL)
			? delete_layer->next : delete_layer->prev;
		ChangeActiveLayer(canvas, new_active);
		LayerViewSetActiveLayer(canvas->app->layer_view->layer_view, new_active);
	}

	DeleteLayer(&delete_layer);

	canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
}

static void AddNewColorLayerRedo(DRAW_WINDOW* canvas, void* history_data)
{
	ADD_NEW_COLOR_LAYER_DATA data;
	uint8 *buffer = (uint8*)history_data;
	LAYER *previous, *next, *new_layer;

	(void)memcpy(&data, buffer, offsetof(ADD_NEW_COLOR_LAYER_DATA, layer_name));
	buffer += offsetof(ADD_NEW_COLOR_LAYER_DATA, layer_name);
	data.layer_name = (char*)buffer;
	buffer += data.name_length;
	data.previous_layer_name = (char*)buffer;

	if(*data.previous_layer_name == '\0')
	{
		previous = NULL;
		next = canvas->layer;
	}
	else
	{
		previous = SearchLayer(canvas->layer, data.previous_layer_name);
		if(previous == NULL)
		{
			return;
		}
		next = previous->next;
	}

	new_layer = CreateLayer(data.x, data.y, data.width, data.height, data.channel,
					data.layer_type, previous, next, data.layer_name, canvas);

	canvas->num_layer++;
	LayerViewAddLayer(new_layer, canvas->layer, canvas->app->layer_view, canvas->num_layer);

	canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
}

void AddNewColorLayerHistory(const LAYER* new_layer, uint16 layer_type)
{
	ADD_NEW_COLOR_LAYER_DATA data;
	DRAW_WINDOW *canvas;
	APPLICATION *app;
	char *history_name;
	uint8 *buffer, *write;
	size_t data_size;

	if(new_layer == NULL)
	{
		return;
	}

	canvas = new_layer->window;
	app = canvas->app;

	data.layer_type = layer_type;
	data.x = new_layer->x, data.y = new_layer->y;
	data.width = new_layer->width, data.height = new_layer->height;
	data.channel = new_layer->channel;
	data.name_length = (uint16)strlen(new_layer->name) + 1;
	if(new_layer->prev == NULL)
	{
		data.previous_layer_name_length = 1;
	}
	else
	{
		data.previous_layer_name_length = (uint16)strlen(new_layer->prev->name) + 1;
	}
	data_size = offsetof(ADD_NEW_COLOR_LAYER_DATA, layer_name)
					+ data.name_length + data.previous_layer_name_length;

	buffer = write = (uint8*)MEM_ALLOC_FUNC(data_size);
	(void)memcpy(write, &data, offsetof(ADD_NEW_COLOR_LAYER_DATA, layer_name));
	write += offsetof(ADD_NEW_COLOR_LAYER_DATA, layer_name);
	(void)memcpy(write, new_layer->name, data.name_length);
	write += data.name_length;
	if(new_layer->prev == NULL)
	{
		*write = '\0';
	}
	else
	{
		(void)memcpy(write, new_layer->prev->name, data.previous_layer_name_length);
	}

	switch(layer_type)
	{
	case TYPE_NORMAL_LAYER:
		history_name = app->labels->layer_window.add_normal;
		break;
	case TYPE_VECTOR_LAYER:
		history_name = app->labels->layer_window.add_vector;
		break;
	case TYPE_3D_LAYER:
		history_name = app->labels->layer_window.add_3d_modeling;
		break;
	case TYPE_ADJUSTMENT_LAYER:
		history_name = app->labels->layer_window.add_adjustment_layer;
		break;
	case TYPE_LAYER_SET:
		history_name = app->labels->layer_window.add_layer_set;
		break;
	default:
		goto release_memory;
	}

	AddHistory(&canvas->history, history_name, buffer, (uint32)data_size,
				AddNewColorLayerUndo, AddNewColorLayerRedo);

release_memory:
	MEM_FREE_FUNC(buffer);
}

/*
 DELETE_LAYER_HISTORY構造体
*/
typedef struct _DELETE_LAYER_HISTORY
{
	uint16 layer_type;
	uint16 layer_mode;
	int32 x, y;
	int32 width,  height, stride;
	uint32 flags;
	int8 channel;
	int8 alpha;
	uint16 layer_name_length;
	char *layer_name;
	uint16 previous_layer_name_length;
	char *previous_layer_name;
	uint32 image_data_size;
	uint8 *image_data;
} DELETE_LAYER_HISTORY;

static void DeleteLayerUndo(DRAW_WINDOW* canvas, void* history_data)
{
	DELETE_LAYER_HISTORY history = {0};
	MEMORY_STREAM stream;
	uint8 *pixels;
	int32 width, height, stride;
	uint32 data_size;
	LAYER *layer;
	LAYER *previous;
	LAYER *focal_layer;
	int32 has_focal_canvas;

	(void)memcpy(&data_size, history_data, sizeof(data_size));


	// 履歴データは最初の4バイトがデータバイト数
		// 読み取り用ストリームの位置を4バイトに設定しておく
		// データサイズはmemcpyで取得済
	stream.block_size = 1;
	stream.data_size = data_size;
	stream.buff_ptr = (uint8*)history_data;
	stream.data_point = sizeof(data_size);

	// 局所キャンバス有無を読み込む
	(void)MemRead(&has_focal_canvas, sizeof(has_focal_canvas), 1, &stream);

	// レイヤー情報を履歴用構造体のデータ構造で読み込む
	(void)MemRead(&history, 1, offsetof(DELETE_LAYER_HISTORY, layer_name_length), &stream);
	history.layer_name = MEM_ALLOC_FUNC(history.layer_name_length);
	(void)MemRead(history.layer_name_length, 1, history.layer_name_length, &stream);
	(void)MemRead(&history.previous_layer_name_length, sizeof(history.previous_layer_name_length),
					1, &stream);
	// レイヤー削除実行時点の削除対象レイヤーの下のレイヤーが
		// レイヤー復活位置を決めるために必要
		// 名前で探す
	if(history.previous_layer_name_length != 0)
	{
		history.previous_layer_name = (char*)MEM_ALLOC_FUNC(history.previous_layer_name_length);
		(void)MemRead(history.previous_layer_name, 1, history.previous_layer_name_length, &stream);
		previous = SearchLayer(canvas->layer, history.previous_layer_name);
	}
	else
	{
		previous = NULL;
	}

	// レイヤー復活	
	layer = CreateLayer(history.x, history.y, history.width, history.height, history.channel,
				history.layer_type, previous, (previous == NULL) ? canvas->layer : previous->next, history.layer_name, canvas);

	ReadLayerData();

	// 局所キャンバスモードの場合は元キャンバスのレイヤーも復活させる
	if(has_focal_canvas)
	{
		// レイヤー情報を履歴用構造体のデータ構造で読み込む
		(void)MemRead(&history, 1, offsetof(DELETE_LAYER_HISTORY, layer_name_length), &stream);
		history.layer_name = MEM_ALLOC_FUNC(history.layer_name_length);
		(void)MemRead(history.layer_name_length, 1, history.layer_name_length, &stream);
		(void)MemRead(&history.previous_layer_name_length, sizeof(history.previous_layer_name_length),
			1, &stream);
		// レイヤー削除実行時点の削除対象レイヤーの下のレイヤーが
			// レイヤー復活位置を決めるために必要
			// 名前で探す
		if(history.previous_layer_name_length != 0)
		{
			history.previous_layer_name = (char*)MEM_ALLOC_FUNC(history.previous_layer_name_length);
			(void)MemRead(history.previous_layer_name, 1, history.previous_layer_name_length, &stream);
			previous = SearchLayer(canvas->layer, history.previous_layer_name);
		}
		else
		{
			previous = NULL;
		}

		focal_layer = CreateLayer(history.x, history.y, history.width, history.height, history.channel,
			history.layer_type, previous, (previous == NULL) ? canvas->focal_window->layer : previous->next, history.layer_name, canvas->focal_window);
		if(focal_layer != NULL)
		{
			canvas->focal_window->num_layer++;
		}
	}

	MEM_FREE_FUNC(history.layer_name);
	MEM_FREE_FUNC(history.previous_layer_name);

	// 画面を更新
	canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;

	canvas->num_layer++;
	LayerViewAddLayer(layer, canvas->layer, canvas->app->layer_view, canvas->num_layer);
}

static void DeleteLayerRedo(DRAW_WINDOW* canvas, void* history_data)
{
	LAYER *delete_layer;
	MEMORY_STREAM stream;
	char *delete_layer_name;
	uint16 name_length;
	uint32 data_size;
	int32 has_focal_canvas;

	stream.block_size = 1;
	(void)memcpy(&data_size, history_data, sizeof(data_size));
	stream.data_size = data_size;
	stream.data_point = offsetof(DELETE_LAYER_HISTORY, layer_name_length) + sizeof(data_size);
	stream.buff_ptr = (uint8*)history_data;

	(void)MemRead(&has_focal_canvas, sizeof(has_focal_canvas), 1, &stream);

	(void)MemRead(&name_length, sizeof(name_length), 1, &stream);
	delete_layer_name = (char*)MEM_ALLOC_FUNC(name_length);
	(void)MemRead(delete_layer_name, 1, name_length, &stream);

	delete_layer = SearchLayer(canvas->layer, delete_layer_name);
	if(delete_layer != NULL)
	{
		DeleteLayer(&delete_layer);
	}

	if(has_focal_canvas && canvas->focal_window != NULL)
	{
		delete_layer = SearchLayer(canvas->focal_window->layer, delete_layer_name);
		if(delete_layer != NULL)
		{
			DeleteLayer(&delete_layer);
		}
	}

	MEM_FREE_FUNC(delete_layer_name);
}

#define DATA_COMPRESSION_LEVEL 8
void StoreDeleteLayerHistory(DRAW_WINDOW* canvas, LAYER* target, MEMORY_STREAM_PTR history_data)
{
	DELETE_LAYER_HISTORY history;
	uint16 layer_name_length;
	uint32 data_size;

	// レイヤーのデータを履歴にセット
	history.layer_type = target->layer_type;
	history.layer_mode = target->layer_mode;
	history.x = target->x;
	history.y = target->y;
	history.width = target->width;
	history.height = target->height;
	history.stride = target->stride;
	history.flags = target->flags;
	history.channel = target->channel;
	history.alpha = target->alpha;
	// ストリームへ書き込む
	(void)MemWrite(&history, 1,
		offsetof(DELETE_LAYER_HISTORY, layer_name_length), history_data);

	// レイヤーの名前を書き込む
	layer_name_length = (uint16)strlen(target->name) + 1;
	(void)MemWrite(&layer_name_length, sizeof(layer_name_length), 1, history_data);
	(void)MemWrite(target->name, 1, layer_name_length, history_data);

	// 前のレイヤーの名前を書き込む
	if(target->prev == NULL)
	{
		layer_name_length = 0;
		(void)MemWrite(&layer_name_length, sizeof(layer_name_length), 1, history_data);
	}
	else
	{
		layer_name_length = (uint16)strlen(target->name) + 1;
		(void)MemWrite(&layer_name_length, sizeof(layer_name_length), 1, history_data);
		(void)MemWrite(target->prev->name, 1, layer_name_length, history_data);
	}

	StoreLayerData(target, history_data, DATA_COMPRESSION_LEVEL, FALSE);
}

void AddDeleteLayerHistory(DRAW_WINDOW* canvas, LAYER* target)
{
	MEMORY_STREAM_PTR stream = NULL;
	MEMORY_STREAM_PTR history_data = CreateMemoryStream(target->stride * target->height*2);
	uint32 data_size;
	int32 has_focal_canvas = canvas->focal_window != NULL;

	(void)MemSeek(history_data, sizeof(data_size), SEEK_SET);
	(void)MemWrite(&has_focal_canvas, sizeof(has_focal_canvas), 1, history_data);

	StoreDeleteLayerHistory(canvas, target, history_data);

	if(has_focal_canvas)
	{
		LAYER *focal_target = SearchLayer(canvas->focal_window->layer, target->name);
		if(focal_target != NULL)
		{
			StoreDeleteLayerHistory(canvas->focal_window, focal_target, history_data);
		}
	}

	data_size = (uint32)history_data->data_point;
	(void)MemSeek(history_data, 0, SEEK_SET);
	(void)MemWrite(&data_size, sizeof(data_size), 1, history_data);

	AddHistory(&canvas->history, canvas->app->labels->menu.delete_layer, history_data->buff_ptr,
				data_size, DeleteLayerUndo, DeleteLayerRedo);

	if(stream != NULL)
	{
		DeleteMemoryStream(stream);
	}
	DeleteMemoryStream(history_data);
}

#ifdef __cplusplus
}
#endif
