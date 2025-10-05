#ifndef _INCLUDED_BRUSH_CORE_H_
#define _INCLUDED_BRUSH_CORE_H_

#include <stdio.h>
#include "types.h"
#include "layer.h"
#include "draw_window.h"
#include "utils.h"

INLINE static LAYER* SwitchBrushTemporaryLayer(DRAW_WINDOW* canvas, LAYER* current_temp)
{
	return (current_temp == canvas->temp_layer) ? canvas->mask_temp : canvas->temp_layer;
}

#ifdef __cplusplus
extern "C" {
#endif

#define BRUSH_STEP (FLOAT_T)(0.0750)
#define MIN_BRUSH_STEP (FLOAT_T)(BRUSH_STEP * MINIMUM_PRESSURE)
#define MINIMUM_BRUSH_DISTANCE 0.25
#define BRUSH_UPDATE_MARGIN 7
#define BRUSH_MAXIMUM_CIRCLE_SIZE (500)
#define BRUSH_POINT_BUFFER_SIZE 256

typedef enum _eBRUSH_SHAPE
{
	BRUSH_SHAPE_CIRCLE,
	BRUSH_SHAPE_IMAGE
} eBRUSH_SHAPE;

typedef enum _eBRUSH_TYPE
{
	BRUSH_TYPE_PENCIL,
	BRUSH_TYPE_HARD_PEN,
	BRUSH_TYPE_AIR_BRUSH,
	BRUSH_TYPE_BLEND_BRUSH,
	BRUSH_TYPE_OLD_AIR_BRUSH,
	BRUSH_TYPE_WATER_COLOR_BRUSH,
	BRUSH_TYPE_PICKER_BRUSH,
	BRUSH_TYPE_ERASER,
	BRUSH_TYPE_BUCKET,
	BRUSH_TYPE_PATTERN_FILL,
	BRUSH_TYPE_BLUR,
	BRUSH_TYPE_SMUDGE,
	BRUSH_TYPE_MIX_BRUSH,
	BRUSH_TYPE_GRADATION,
	BRUSH_TYPE_TEXT,
	BRUSH_TYPE_STAMP_TOOL,
	BRUSH_TYPE_IMAGE_BRUSH,
	BRUSH_TYPE_BLEND_IMAGE_BRUSH,
	BRUSH_TYPE_PICKER_IMAGE_BRUSH,
	BRUSH_TYPE_SCRIPT_BRUSH,
	BRUSH_TYPE_CUSTOM_BRUSH,
	BRUSH_TYPE_PLUG_IN,
	NUM_BRUSH_TYPE
} eBRUSH_TYPE;

typedef enum _eBRUSH_FLAGS
{
	BRUSH_FLAG_SIZE = 0x01,
	BRUSH_FLAG_FLOW = 0x02,
	BRUSH_FLAG_ENTER = 0x04,
	BRUSH_FLAG_OUT = 0x08,
	BRUSH_FLAG_ROTATE = 0x10,
	BRUSH_FLAG_ANTI_ALIAS = 0x20,
	BRUSH_FLAG_USE_OLD_ANTI_ALIAS = 0x40,
	BRUSH_FLAG_DRAW_STARTED = 0x80,
	BRUSH_FLAG_OVERWRITE_DRAW = 0x100
} eBRUSH_FLAGS;

typedef struct _BRUSH_UPDATE_INFO
{
	// 更新を行う範囲
	FLOAT_T min_x, min_y, max_x, max_y;
	// 更新範囲の左上の座標
	int start_x, start_y;
	// 更新範囲の幅、高さ
	FLOAT_T width, height;
	// 描画を実行するか否か
	int update;
} BRUSH_UPDATE_INFO;

typedef void (*brush_core_function)(DRAW_WINDOW* canvas, struct _BRUSH_CORE* core, EVENT_STATE* state);

typedef void (*brush_update_function)(DRAW_WINDOW* canvas, FLOAT_T x, FLOAT_T y, BRUSH_CORE* core);

struct _BRUSH_CORE
{
	APPLICATION *app;
	int base_scale;
	FLOAT_T max_x, max_y, min_x, min_y;
	FLOAT_T before_x, before_y;
	FLOAT_T radius;
	FLOAT_T opacity;
	FLOAT_T outline_hardness;
	FLOAT_T blur;
	FLOAT_T minimum_pressure;
	eLAYER_BLEND_MODE blend_mode;
	uint32 flags;
	size_t detail_data_size;

	GRAPHICS_IMAGE_SURFACE brush_surface;
	GRAPHICS_IMAGE_SURFACE temp_surface;
	GRAPHICS_PATTERN_UNION brush_pattern;
	GRAPHICS_PATTERN_UNION temp_pattern;
	GRAPHICS_DEFAULT_CONTEXT temp_context;
	uint8 *brush_pattern_buffer, *temp_pattern_buffer;
	size_t brush_pattern_buffer_size;
	int stride;

	BRUSH_UPDATE_INFO cursor_update_info;

	uint8 *brush_cursor_buffer;
	size_t brush_cursor_buffer_size;
	GRAPHICS_IMAGE_SURFACE brush_cursor_surface;
	eLAYER_BLEND_MODE cursor_blend_mode;

	char *name;
	char *image_file_path;
	uint8 (*color)[3];
	uint8 (*back_color)[3];
	eBRUSH_TYPE brush_type;
	char hot_key;

	void *brush_data;
	brush_core_function press_function, motion_function, release_function;
	void (*key_press_function)(DRAW_WINDOW* canvas, void* key, BRUSH_CORE* core);
	void (*draw_cursor)(DRAW_WINDOW* canvas, FLOAT_T x, FLOAT_T y, BRUSH_CORE* core);
	brush_update_function button_update, motion_update;
	void (*create_detail_ui)(APPLICATION *app, BRUSH_CORE* core);
	void (*change_color)(const uint8 color[3], void* data);
	void (*change_zoom)(FLOAT_T zoom, BRUSH_CORE* core);
	void (*change_editting_selection)(void* data, int is_editting);
	void *button;
};

typedef struct _GENERAL_BRUSH
{
	BRUSH_CORE core;

	uint16 blend_mode;
	int target;

	FLOAT_T points[BRUSH_POINT_BUFFER_SIZE][4];
	FLOAT_T sum_distance, travel;
	FLOAT_T finish_length;
	FLOAT_T draw_start;
	FLOAT_T enter_length;
	FLOAT_T enter_size;
	FLOAT_T enter, out;
	int num_point;
	int draw_finished;
	int reference_point;
	int8 channel;
	uint8 shape;
	uint8 brush_mode;
} GENERAL_BRUSH;

typedef struct _BRUSH_UPDATE_AREA
{
	// 更新を行う範囲
	FLOAT_T min_x, min_y, max_x, max_y;
	// 初期化済みフラグ
	int initialized;
} BRUSH_UPDATE_AREA;

typedef struct _BRUSH_CHAIN_ITEM
{
	POINTER_ARRAY *names;
} BRUSH_CHAIN_ITEM;

typedef struct _BRUSH_CHAIN
{
	int key;
	int current;
	int change_key;
	unsigned int timer_id;
	int bursh_button_stop;
	POINTER_ARRAY *chains;
} BRUSH_CHAIN;

EXTERN void InitializeBrushCore(BRUSH_CORE* core, APPLICATION* app);

EXTERN int ReadBrushInitializeFile(APPLICATION* app, const char* file_path);

/*
* DefaultToolUpdate関数
* デフォルトのツールアップデートの関数
* 引数
* canvas	: アクティブな描画領域
* x			: マウスカーソルのX座標
* y			: マウスカーソルのY座標
* dummy		: ダミーポインタ
*/
EXTERN void DefaultToolUpdate(DRAW_WINDOW* canvas, FLOAT_T x, FLOAT_T y, void* dummy);

/*
* BrushCoreSetCirclePattern関数
* ブラシの円形画像パターンを作成
* 引数
* core				: ブラシの基本情報
* r					: 半径
* outline_hardness	: 輪郭の硬さ
* blur				: ボケ足
* alpha				: 不透明度
* color				: 色
*/
EXTERN void BrushCoreSetCirclePattern(
	struct _BRUSH_CORE* core,
	FLOAT_T r,
	FLOAT_T outline_hardness,
	FLOAT_T blur,
	FLOAT_T alpha,
	const uint8 color[3]
);

/*
* BrushCoreSetGrayCirclePattern関数
* ブラシのグレースケール円形画像パターンを作成
* 引数
* core				: ブラシの基本情報
* r					: 半径
* outline_hardness	: 輪郭の硬さ
* blur				: ボケ足
* alpha				: 不透明度
*/
EXTERN void BrushCoreSetGrayCirclePattern(
	struct _BRUSH_CORE* core,
	FLOAT_T r,
	FLOAT_T outline_hardness,
	FLOAT_T blur,
	FLOAT_T alpha
);

EXTERN void DummyMouseCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
);

EXTERN void DummyBrushDrawCursor(
	DRAW_WINDOW* canvas,
	FLOAT_T x,
	FLOAT_T y,
	BRUSH_CORE* core
);

EXTERN void DummyBrushButton_or_Move_Update(DRAW_WINDOW* canvas, FLOAT_T x, FLOAT_T y, BRUSH_CORE* core);

EXTERN void DummyBrushChangeZoom(FLOAT_T zoom, BRUSH_CORE* core);

EXTERN void DefaultBrushDrawCursor(
	DRAW_WINDOW* canvas,
	FLOAT_T x,
	FLOAT_T y,
	BRUSH_CORE* core
);

EXTERN void DefaultBrushButtonUpdate(DRAW_WINDOW* canvas, FLOAT_T x, FLOAT_T y, BRUSH_CORE* core);

EXTERN void DefaultBrushMotionUpdate(DRAW_WINDOW* canvas, FLOAT_T x, FLOAT_T y, BRUSH_CORE* core);

EXTERN void DefaultBrushChangeZoom(FLOAT_T zoom, BRUSH_CORE* core);

EXTERN void DefaultBrushChangeColor(const uint8* color, void* data);

EXTERN void WithoutShapeBrushDrawCursor(
	DRAW_WINDOW* canvas,
	FLOAT_T x,
	FLOAT_T y,
	BRUSH_CORE* core
);

EXTERN void WithoutShapeBrushButtonUpdate(DRAW_WINDOW* canvas, FLOAT_T x, FLOAT_T y, BRUSH_CORE* core);

EXTERN void WithoutShapeBrushMotionUpdate(DRAW_WINDOW* canvas, FLOAT_T x, FLOAT_T y, BRUSH_CORE* core);

EXTERN void WithoutShapeBrushChangeZoom(FLOAT_T zoom, BRUSH_CORE* core);

EXTERN void EditSelectionUndoRedo(DRAW_WINDOW* canvas, void* p);

EXTERN void AddBrushHistory(
	BRUSH_CORE* core,
	LAYER* active
);

EXTERN void AddSelectionEditHistory(BRUSH_CORE* core, LAYER* selection);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_BRUSH_CORE_H_
