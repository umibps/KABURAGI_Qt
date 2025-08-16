#if defined _MSC_VER && _MSC_VER >= 1400
# define _CRT_SECURE_NO_DEPRECATE
#endif

#include <string.h>
#include "layer.h"
#include "layer_window.h"
#include "draw_window.h"
#include "application.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
* MixLayerSet関数
* レイヤーセット内を合成
* 引数
* bottom	: レイヤーセットの一番下のレイヤー
* next		: 合成後に次に合成するレイヤー
* canvas	: 描画領域を管理する構造体のアドレス
*/
void MixLayerSet(LAYER* bottom, LAYER** next, DRAW_WINDOW* canvas)
{
	// レイヤーセットのピクセルデータのバイト数
	size_t pixel_bytes = bottom->layer_set->stride*bottom->layer_set->height;
	// 所属レイヤーセット
	LAYER *layer_set = bottom->layer_set;
	// レイヤー合成用
	LAYER *layer = bottom;
	LAYER *blend_layer;
	int blend_mode;

	// レイヤーセットのピクセルデータをリセット
	(void)memset(layer_set->pixels, 0, pixel_bytes);

	if(layer_set == canvas->active_layer_set)
	{
		(void)memcpy(canvas->under_active->pixels, canvas->mixed_layer->pixels, canvas->pixel_buf_size);
	}

	// 所属レイヤーセットまでループ
	while(1)
	{
		blend_layer = layer;
		blend_mode = layer->layer_mode;

		// アクティブレイヤーなら
		if(blend_layer == canvas->active_layer)
		{
			// 現在の合成結果を記憶
			(void)memcpy(blend_layer->layer_set->layer_data.layer_set->active_under->pixels,
				blend_layer->layer_set->pixels, canvas->pixel_buf_size);

			// 可視判定
			if((blend_layer->flags & LAYER_FLAG_INVISIBLE) == 0)
			{
				if(layer->layer_type == TYPE_NORMAL_LAYER)
				{	// 通常レイヤーは
						// 作業レイヤーとアクティブレイヤーを一度合成してから下のレイヤーと合成
					(void)memcpy(canvas->temp_layer->pixels, layer->pixels, layer->stride*layer->height);
					canvas->layer_blend_functions[canvas->work_layer->layer_mode](canvas->work_layer, canvas->temp_layer);
					blend_layer = canvas->temp_layer;
					blend_layer->alpha = layer->alpha;
					blend_layer->flags = layer->flags;
					blend_layer->prev = layer->prev;
				}
				else if(layer->layer_type == TYPE_VECTOR_LAYER)
				{	// ベクトルレイヤーは
						// レイヤーのラスタライズ処理を行なってから作業レイヤーと下のレイヤーを合成
					RasterizeVectorLayer(canvas, layer, layer->layer_data.vector_layer);
					canvas->layer_blend_functions[canvas->work_layer->layer_mode](canvas->work_layer, layer);
				}
				else if(layer->layer_type == TYPE_TEXT_LAYER)
				{	// テキストレイヤーは
						// テキストの内容をラスタライズ処理してから下のレイヤーと合成
					RenderTextLayer(canvas, layer, layer->layer_data.text_layer);
				}

				while(layer->next != NULL && layer->next->layer_type == TYPE_ADJUSTMENT_LAYER)
				{
					if((layer->next->flags & LAYER_FLAG_INVISIBLE) != 0)
					{
						layer->next->layer_data.adjustment_layer->filter_func(
							layer->layer_data.adjustment_layer, layer->pixels, layer->next->pixels,
								layer->width * layer->height, layer);
					}
					layer->next->layer_data.adjustment_layer->update(
						layer->layer_data.adjustment_layer, layer, canvas->mixed_layer,
							0, 0, layer->width, layer->height);
					blend_layer = layer->next;
					layer = layer->next;
				}

				// 合成する対象と方法が確定したので合成を実行する
				canvas->layer_blend_functions[blend_mode](blend_layer, layer->layer_set);
				// 合成したらデータを元に戻す
				canvas->temp_layer->alpha = 100;
				canvas->temp_layer->flags = 0;
				canvas->temp_layer->prev = NULL;
				GraphicsSetOperator(&canvas->temp_layer->context.base, GRAPHICS_OPERATOR_OVER);
			}	// 可視判定
					// if((blend_layer->flags & LAYER_FLAG_INVISIBLE) == 0)

			// サムネイル更新
			UpdateLayerThumbnail(layer);
		}	// アクティブレイヤーなら
				// if(blend_layer == canvas->active_layer)
		else
		{
			// 可視判定
			if((blend_layer->flags & LAYER_FLAG_INVISIBLE) == 0)
			{
				canvas->layer_blend_functions[blend_mode](blend_layer, layer_set);
			}
		}

		layer = layer->next;

		if(layer->layer_set != layer_set)
		{
			// 所属レイヤーセットに到達したら
			if(layer == layer_set || layer->layer_set == NULL)
			{
				break;
			}

			MixLayerSet(layer, &layer, canvas);
		}
	}	// while(1)
			// 所属レイヤーセットまでループ

	// サムネイル更新
	UpdateLayerThumbnail(layer);

	*next = layer;
}

/*
* MixLayerSetActiveOver関数
* レイヤーセット中のアクティブレイヤー以上のレイヤーを合成する
* 引数
* start		: アクティブレイヤー
* next		: 合成後の次に合成するレイヤー
* canvas	: 描画領域を管理する構造体のアドレス
*/
void MixLayerSetActiveOver(LAYER* start, LAYER** next, DRAW_WINDOW* canvas)
{
	// レイヤーセットのピクセルデータのバイト数
	size_t pixel_bytes = start->layer_set->stride*start->layer_set->height;
	// 所属レイヤーセット
	LAYER *layer_set = start->layer_set;
	// レイヤー合成用
	LAYER *layer = start;
	LAYER *blend_layer;
	int blend_mode;

	// アクティブなレイヤーより下の合成結果をコピー
	(void)memcpy(layer->layer_set->pixels,
		layer->layer_set->layer_data.layer_set->active_under->pixels, pixel_bytes);

	// 所属レイヤーセットまでループ
	while(1)
	{
		blend_layer = layer;
		blend_mode = layer->layer_mode;

		// 可視判定
		if((blend_layer->flags & LAYER_FLAG_INVISIBLE) == 0)
		{	// アクティブレイヤーなら
			if(blend_layer == canvas->active_layer)
			{
				if(layer->layer_type == TYPE_NORMAL_LAYER)
				{	// 通常レイヤーは
						// 作業レイヤーとアクティブレイヤーを一度合成してから下のレイヤーと合成
					(void)memcpy(canvas->temp_layer->pixels, layer->pixels, layer->stride*layer->height);
					canvas->layer_blend_functions[canvas->work_layer->layer_mode](canvas->work_layer, canvas->temp_layer);
					blend_layer = canvas->temp_layer;
					blend_layer->alpha = layer->alpha;
					blend_layer->flags = layer->flags;
					blend_layer->prev = layer->prev;
				}
				else if(layer->layer_type == TYPE_VECTOR_LAYER)
				{	// ベクトルレイヤーは
						// レイヤーのラスタライズ処理を行なってから作業レイヤーと下のレイヤーを合成
					RasterizeVectorLayer(canvas, layer, layer->layer_data.vector_layer);
					canvas->layer_blend_functions[canvas->work_layer->layer_mode](canvas->work_layer, layer);
				}
				else if(layer->layer_type == TYPE_TEXT_LAYER)
				{	// テキストレイヤーは
						// テキストの内容をラスタライズ処理してから下のレイヤーと合成
					RenderTextLayer(canvas, layer, layer->layer_data.text_layer);
				}

				while(layer->next != NULL && layer->next->layer_type == TYPE_ADJUSTMENT_LAYER)
				{
					if((layer->next->flags & LAYER_FLAG_INVISIBLE) != 0)
					{
						layer->next->layer_data.adjustment_layer->filter_func(
							layer->layer_data.adjustment_layer, layer->pixels, layer->next->pixels,
								layer->width * layer->height, layer);
					}
					layer->next->layer_data.adjustment_layer->update(
						layer->layer_data.adjustment_layer, layer, canvas->mixed_layer,
							0, 0, layer->width, layer->height);
					blend_layer = layer->next;
					layer = layer->next;
				}

				// サムネイル更新
				UpdateLayerThumbnail(layer);
			}	// アクティブレイヤーなら
					// if(blend_layer == canvas->active_layer)

			// 合成する対象と方法が確定したので合成を実行する
			canvas->layer_blend_functions[blend_mode](blend_layer, layer_set);
			// 合成したらデータを元に戻す
			canvas->temp_layer->alpha = 100;
			canvas->temp_layer->flags = 0;
			canvas->temp_layer->prev = NULL;
			GraphicsSetOperator(&canvas->temp_layer->context.base, GRAPHICS_OPERATOR_OVER);
		}	// 可視判定
				// if((blend_layer->flags & LAYER_FLAG_INVISIBLE) == 0)

		layer = layer->next;

		if(layer->layer_set != layer_set)
		{
			// 所属レイヤーセットに到達したら
			if(layer == layer_set)
			{
				if(layer_set->layer_set == NULL)
				{
					break;
				}
				layer_set = layer_set->layer_set;
			}
		}
	}	// while(1)
			// 所属レイヤーセットまでループ

	// サムネイル更新
	UpdateLayerThumbnail(layer);

	*next = layer;
}

#ifdef __cplusplus
}
#endif
