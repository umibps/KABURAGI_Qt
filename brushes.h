#ifndef _INCLUDED_BRUSHES_H_
#define _INCLUDED_BRUSHES_H_

#include "brush_core.h"
#include "ini_file.h"

#define DEFAULT_BRUSH_NUM_BASE_SIZE 3
#define DEFAULT_BRUSH_SIZE_MINIMUM 0.1
#define DEFAULT_BRUSH_SIZE_MAXIMUM 500
#define DEFAULT_BRUSH_FLOW_MINIMUM 1
#define DEFAULT_BRUSH_FLOW_MAXIMUM 100

#define BRUSH_POINT_BUFFER_SIZE 256

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _PENCIL
{
	BRUSH_CORE core;

	int8 channel;
	uint8 shape;
	uint8 brush_mode;
} PENCIL;

extern void LoadBrushDetailData(
	BRUSH_CORE** core,
	INI_FILE_PTR file,
	const char* section_name,
	const char* brush_type,
	APPLICATION* app
);

/*
* PencilPressCallBack関数
* 鉛筆ツール使用時のマウスクリックに対するコールバック関数
* 引数
* canvas	: 描画領域の情報
* state		: マウスの状態
*/
extern void PencilPressCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
);

extern void PencilMotionCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
);

extern void PencilReleaseCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
);

#define PencilDrawCursor DefaultBrushDrawCursor

#define PencilButtonUpdate DefaultBrushButtonUpdate

#define PencilMotionUpdate DefaultBrushMotionUpdate

#define PencilChangeZoom DefaultBrushChangeZoom

#define PcncilChangeColor DefaultBrushChangeColor


typedef struct _ERASER
{
	BRUSH_CORE core;

	int8 channel;
	uint8 shape;
	uint8 brush_mode;
} ERASER;

/*
* EraserPressCallBack関数
* 消しゴムツール使用時のマウスクリックに対するコールバック関数
* 引数
* canvas	: 描画領域の情報
* state		: マウスの状態
*/
extern void EraserPressCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
);

extern void EraserMotionCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
);

extern void EraserReleaseCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
);

#define EraserDrawCursor DefaultBrushDrawCursor

#define EraserButtonUpdate DefaultBrushButtonUpdate

#define EraserMotionUpdate DefaultBrushMotionUpdate

#define EraserChangeZoom DefaultBrushChangeZoom

typedef enum _eBLEND_BRUSH_TARGET
{
	BLEND_BRUSH_TARGET_UNDER_LAYER,
	BLEND_BRUSH_TARGET_CANVAS
} eBLEND_BRUSH_TARGET;

typedef struct _BLEND_BRUSH
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
} BLEND_BRUSH;

/*
* BlendBrushPressCallBack関数
* 合成ブラシ使用時のマウスクリックに対するコールバック関数
* 引数
* canvas	: 描画領域の情報
* state		: マウスの状態
*/
extern void BlendBrushPressCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
);

extern void BlendBrushMotionCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
);

extern void BlendBrushReleaseCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
);

#define BlendBrushDrawCursor DefaultBrushDrawCursor

#define BlendBrushButtonUpdate DefaultBrushButtonUpdate

#define BlendBrushMotionUpdate DefaultBrushMotionUpdate

#define BlendBrushChangeZoom DefaultBrushChangeZoom

#define BlendBrushChangeColor DefaultBrushChangeColor

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _INCLUDED_BRUSHES_H_ */
