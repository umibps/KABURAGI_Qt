#ifndef _INCLUDED_BRUSHES_H_
#define _INCLUDED_BRUSHES_H_

#include "brush_core.h"
#include "ini_file.h"

#define DEFAULT_BRUSH_NUM_BASE_SIZE 3
#define DEFAULT_BRUSH_SIZE_MINIMUM 0.1
#define DEFAULT_BRUSH_SIZE_MAXIMUM 500
#define DEFAULT_BRUSH_FLOW_MINIMUM 1
#define DEFAULT_BRUSH_FLOW_MAXIMUM 100

#ifdef __cplusplus
extern "C" {
#endif

EXTERN void LoadDefaultBrushData(
	BRUSH_CORE* core,
	INI_FILE_PTR file,
	const char* section_name,
	APPLICATION* app
);

EXTERN void LoadBrushDetailData(
	BRUSH_CORE** core,
	INI_FILE_PTR file,
	const char* section_name,
	const char* brush_type,
	APPLICATION* app
);

EXTERN void LoadGeneralBrushData(
	GENERAL_BRUSH* brush,
	INI_FILE_PTR file,
	const char* section_name,
	APPLICATION* app
);

EXTERN void WriteDefaultBrushData(
	BRUSH_CORE* core,
	INI_FILE_PTR file,
	const char* section_name,
	const char* brush_type
);

EXTERN void WriteGeneralBrushData(
	GENERAL_BRUSH* brush,
	INI_FILE_PTR file,
	const char* section_name,
	const char* brush_type
);

/*
* WriteBrushDetailData関数
* ブラシの詳細設定を書き出す
* 引数
* tool_box	: ツールボックスウィンドウ
* file_path	: 書き出すファイルのパス
* app		: アプリケーションを管理する構造体のアドレス
* 返り値
*	正常終了:0	失敗:負の値
*/
EXTERN int WriteBrushDetailData(TOOL_BOX* tool_box, const char* file_path, APPLICATION* app);

typedef struct _PENCIL
{
	BRUSH_CORE core;

	int8 channel;
	uint8 shape;
	uint8 brush_mode;
} PENCIL;

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

/*
* LoadPencilData関数
* 鉛筆ツールの設定データを読み取る
* 引数
* file			: 初期化ファイル読み取りデータ
* section_name	: 初期化ファイルに鉛筆が記録されてるセクション名
* app			: ブラシ初期化にはアプリケーション管理用データが必要
* 返り値
*	鉛筆ツールデータ 使用終了後にMEM_FREE_FUNC必要
*/
EXTERN BRUSH_CORE* LoadPencilData(INI_FILE_PTR file, const char* section_name, APPLICATION* app);

/*
* WritePencilData関数
* 鉛筆ツールの設定データを記録する
* 引数
* file			: 書き込みファイル操作用データ
* core			: 記録するツールデータ
* section_name	: 記録するセクション名
*/
EXTERN void WritePencilData(INI_FILE_PTR file, BRUSH_CORE* core, const char* section_name);

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

/*
* LoadEraserData関数
* 消しゴムツールの設定データを読み取る
* 引数
* file			: 初期化ファイル読み取りデータ
* section_name	: 初期化ファイルに消しゴムが記録されてるセクション名
* app			: ブラシ初期化にはアプリケーション管理用データが必要
* 返り値
*	消しゴムデータ 使用終了後にMEM_FREE_FUNC必要
*/
EXTERN BRUSH_CORE* LoadEraserData(INI_FILE_PTR file, const char* section_name, APPLICATION* app);

/*
* WriteEraserData関数
* 消しゴムツールの設定データを記録する
* 引数
* file			: 書き込みファイル操作用データ
* core			: 記録するツールデータ
* section_name	: 記録するセクション名
*/
EXTERN void WriteEraserData(INI_FILE_PTR file, BRUSH_CORE* core, const char* section_name);

typedef enum _eBUCKET_SELECT_MODE
{
	BUCKET_SELECT_MODE_RGB,
	BUCKET_SELECT_MODE_RGBA,
	BUCKET_SELECT_MODE_ALPHA
} eBUCKET_SELECT_MODE;

typedef enum _eBUCKET_TARGET
{
	BUCKET_TARGET_ACTIVE_LAYER,
	BUCKET_TARGET_CANVAS
} eBUCKET_TARGET;

typedef struct _BUCKET
{
	BRUSH_CORE core;

	int threshold;
	int extend;
	eBUCKET_SELECT_MODE select_mode;
	eBUCKET_TARGET target;
	eSELECT_FUZZY_DIRECTION select_direction;
} BUCKET;

typedef enum _eBLEND_BRUSH_TARGET
{
	BLEND_BRUSH_TARGET_UNDER_LAYER,
	BLEND_BRUSH_TARGET_CANVAS
} eBLEND_BRUSH_TARGET;

/*
* BucketPressCallBack関数
* 鉛筆ツール使用時のマウスクリックに対するコールバック関数
* 引数
* canvas	: 描画領域の情報
* state		: マウスの状態
*/
extern void BucketPressCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
);

extern void BucketEditSelectionPressCallBack(
	DRAW_WINDOW* canvas,
	BRUSH_CORE* core,
	EVENT_STATE* state
);

#define BucketMotionCallBack DummyMouseCallBack
#define BucketReleaseCallBack DummyMouseCallBack
#define BucketDrawCursor WithoutShapeBrushDrawCursor
#define BucketButtonUpdate WithoutShapeBrushButtonUpdate
#define BucketMotionUpdate WithoutShapeBrushMotionUpdate
#define BucketChangeZoom WithoutShapeBrushChangeZoom

/*
* LoadBucketData関数
* バケツツールの設定データを読み取る
* 引数
* file			: 初期化ファイル読み取りデータ
* section_name	: 初期化ファイルにバケツが記録されてるセクション名
* app			: ブラシ初期化にはアプリケーション管理用データが必要
* 返り値
*	バケツデータ 使用終了後にMEM_FREE_FUNC必要
*/
EXTERN BRUSH_CORE* LoadBucketData(INI_FILE_PTR file, const char* section_name, APPLICATION* app);

/*
* WriteBucketData関数
* バケツツールの設定データを記録する
* 引数
* file			: 書き込みファイル操作用データ
* core			: 記録するツールデータ
* section_name	: 記録するセクション名
*/
EXTERN void WriteBucketData(INI_FILE_PTR file, BRUSH_CORE* core, const char* section_name);

typedef struct _BLEND_BRUSH
{
	GENERAL_BRUSH base_data;
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

/*
* LoadBlendBrushData関数
* 合成ブラシツールの設定データを読み取る
* 引数
* file			: 初期化ファイル読み取りデータ
* section_name	: 初期化ファイルに合成ブラシが記録されてるセクション名
* app			: ブラシ初期化にはアプリケーション管理用データが必要
* 返り値
*	合成ブラシツールデータ 使用終了後にMEM_FREE_FUNC必要
*/
EXTERN BRUSH_CORE* LoadBlendBrushData(INI_FILE_PTR file, const char* section_name, APPLICATION* app);

/*
* WriteBlendBrushData関数
* 合成ブラシツールの設定データを記録する
* 引数
* file			: 書き込みファイル操作用データ
* core			: 記録するツールデータ
* section_name	: 記録するセクション名
*/
EXTERN void WriteBlendBrushData(INI_FILE_PTR file, BRUSH_CORE* core, const char* section_name);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _INCLUDED_BRUSHES_H_ */
