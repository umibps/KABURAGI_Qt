#ifndef _INCLUDED_LAYER_H_
#define _INCLUDED_LAYER_H_

#include <stdio.h>
#include "types.h"
#include "graphics/graphics.h"
#include "vector_layer.h"
#include "text_layer.h"
#include "layer_set.h"
#include "adjustment_layer.h"

#define LAYER_CHAIN_BUFFER_SIZE 1024
#define MAX_LAYER_EXTRA_DATA_NUM 8
#define MAX_LAYER_NAME_LENGTH 256

typedef struct _LAYER_WIDGET LAYER_WIDGET;

typedef enum _eLAYER_FLAGS
{
	LAYER_FLAG_INVISIBLE = 0x01,
	LAYER_MASKING_WITH_UNDER_LAYER = 0x02,
	LAYER_LOCK_OPACITY = 0x04,
	LAYER_CHAINED = 0x08,
	LAYER_SET_CLOSE = 0x10,
	LAYER_MODIFIED = 0x20
} eLAYER_FLAGS;

typedef enum _eLAYER_TYPE
{
	TYPE_NORMAL_LAYER,
	TYPE_VECTOR_LAYER,
	TYPE_TEXT_LAYER,
	TYPE_LAYER_SET,
	TYPE_3D_LAYER,
	TYPE_ADJUSTMENT_LAYER,
	NUM_LAYER_TYPE
} eLAYER_TYPE;

typedef enum _eLAYER_BLEND_MODE
{
	LAYER_BLEND_NORMAL,
	LAYER_BLEND_ADD,
	LAYER_BLEND_MULTIPLY,
	LAYER_BLEND_SCREEN,
	LAYER_BLEND_OVERLAY,
	LAYER_BLEND_LIGHTEN,
	LAYER_BLEND_DARKEN,
	LAYER_BLEND_DODGE,
	LAYER_BLEND_BURN,
	LAYER_BLEND_HARD_LIGHT,
	LAYER_BLEND_SOFT_LIGHT,
	LAYER_BLEND_DIFFERENCE,
	LAYER_BLEND_EXCLUSION,
	LAYER_BLEND_HSL_HUE,
	LAYER_BLEND_HSL_SATURATION,
	LAYER_BLEND_HSL_COLOR,
	LAYER_BLEND_HSL_LUMINOSITY,
	LAYER_BLEND_BINALIZE,
	LAYER_BLEND_SLELECTABLE_NUM,
	LAYER_BLEND_COLOR_REVERSE = LAYER_BLEND_SLELECTABLE_NUM,
	LAYER_BLEND_GREATER,
	LAYER_BLEND_ALPHA_MINUS,
	LAYER_BLEND_SOURCE,
	LAYER_BLEND_ATOP,
	LAYER_BLEND_SOURCE_OVER,
	LAYER_BLEND_OVER,
	NUM_LAYER_BLEND_FUNCTIONS,
	INVALID_LAYER_BLEND_MODE = -1
} eLAYER_BLEND_MODE;

/***************************
* EXTRA_LAYER_DATA構造体   *
* レイヤーの追加情報を格納 *
***************************/
typedef struct _EXTRA_LAYER_DATA
{
	char *name;
	size_t data_size;
	void *data;
} EXTRA_LAYER_DATA;

/***********************
* LAYER構造体		  *
* レイヤーの情報を格納 *
***********************/
typedef struct _LAYER
{
	uint8 *pixels;			// ピクセルデータ
	char *name;				// レイヤー名
	uint16 layer_type;		// レイヤータイプ(ノーマル、ベクトル、テキスト)
	uint16 layer_mode;		// レイヤーの合成モード
	int32 x, y;				// レイヤー左上隅の座標
	// レイヤーの幅、高さ、一行分のバイト数
	int width, height, stride;
	unsigned int flags;		// レイヤー表示・非表示等のフラグ
	int8 alpha;				// レイヤーの不透明度
	int8 channel;			// レイヤーのチャンネル数
	uint8 num_extra_data;	// 追加情報の数

	GRAPHICS_IMAGE_SURFACE surface;
	GRAPHICS_DEFAULT_CONTEXT context;
	
	union
	{
		VECTOR_LAYER *vector_layer;
		TEXT_LAYER *text_layer;
		LAYER_SET *layer_set;
		ADJUSTMENT_LAYER *adjustment_layer;
	} layer_data;

	// 前後のレイヤーへのポインタ
	struct _LAYER *prev, *next;
	// レイヤーセットへのポインタ
	struct _LAYER *layer_set;
	// レイヤービューのウィジェット
	struct _LAYER_WIDGET *widget;

	// 追加情報
	EXTRA_LAYER_DATA extra_data[MAX_LAYER_EXTRA_DATA_NUM];

	// ファイル読み込み時の3Dモデルデータ
	void *modeling_data;
	size_t modeling_data_size;

	// ファイル書き出しを高速化するために最後に書き出したデータを記憶
	void *last_write_data;
	size_t last_write_data_size;

	// 描画領域へのポインタ
	struct _DRAW_WINDOW *window;
};

typedef struct _LAYER_BASE_DATA
{
	uint16 layer_type;	// レイヤータイプ(ノーマル、ベクトル、テキスト)
	uint16 layer_mode;	// レイヤーの合成モード
	int32 x, y;			// レイヤー左上隅の座標
	// レイヤーの幅、高さ
	int32 width, height;
	uint32 flags;		// レイヤー表示・非表示等のフラグ
	int8 alpha;			// レイヤーの不透明度
	int8 channel;		// レイヤーのチャンネル数
	int8 layer_set;		// レイヤーセット内のレイヤーか否か
} LAYER_BASE_DATA;

static INLINE eGRAPHICS_OPERATOR LayerBlendModeToGraphicsOperator(eLAYER_BLEND_MODE blend_mode)
{
	if(blend_mode == LAYER_BLEND_NORMAL)
	{
		return GRAPHICS_OPERATOR_OVER;
	}
	return (eGRAPHICS_OPERATOR)(GRAPHICS_OPERATOR_COLOR_BLEND_START + blend_mode - 1);
}

#ifdef __cplusplus
extern "C" {
#endif

EXTERN void InitializeLayerContext(LAYER* layer);
EXTERN void ReleaseLayerContext(LAYER* layer);
/*
 DeleteLayer関数
 レイヤーを削除する
 ウィジェットがある場合、そちらも削除
 引数
 layer	: 削除対象のレイヤー1
*/
EXTERN void DeleteLayer(LAYER** layer);

EXTERN void AddDeleteLayerHistory(DRAW_WINDOW* canvas, LAYER* target);
EXTERN int CorrectLayerName(const LAYER* bottom_layer, const char* name);
EXTERN LAYER* SearchLayer(LAYER* bottom_layer, const char* name);
EXTERN void ChangeLayerOrder(LAYER* change_layer, LAYER* new_prev, LAYER** bottom);
EXTERN void ChangeActiveLayer(DRAW_WINDOW* canvas, LAYER* layer);
extern void SetLayerLockOpacity(APPLICATION* app, int lock);
extern void SetLayerMaskUnder(APPLICATION* app, int mask);
EXTERN void SetLayerBlendFunctionsArray(void (*layer_blend_functions[])(LAYER* src, LAYER* dst));
EXTERN void SetPartLayerBlendFunctionsArray(void (*layer_part_blend_functions[])(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update));

/*
* ResizeLayerBuffer関数
* レイヤーの幅、高さの情報を変更する
* 引数
* target		: サイズを変更するレイヤー
* new_width		: 新しいレイヤーの幅
* new_height	: 新しいレイヤーの高さ
*/
EXTERN void ResizeLayerBuffer(
	LAYER* target,
	int32 new_width,
	int32 new_height
);

#ifdef __cplusplus
}
#endif

#endif // LAYER_H
