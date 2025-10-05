#ifndef _INCLUDED_DRAW_WINDOW_H_
#define _INCLUDED_DRAW_WINDOW_H_

#include "types.h"
#include "lcms/lcms2.h"
#include "layer.h"
#include "graphics/graphics.h"
#include "graphics/graphics_context.h"
#include "graphics/graphics_pattern.h"
#include "selection_area.h"
#include "history.h"
#include "perspective_ruler.h"

typedef enum _eDRAW_WINDOW_FLAGS
{
	DRAW_WINDOW_HAS_SELECTION_AREA = 0x01,
	DRAW_WINDOW_UPDATE_ACTIVE_UNDER = 0x02,
	DRAW_WINDOW_UPDATE_ACTIVE_OVER = 0x04,
	DRAW_WINDOW_DISPLAY_HORIZON_REVERSE = 0x08,
	DRAW_WINDOW_EDIT_SELECTION = 0x10,
	DRAW_WINDOW_UPDATE_PART = 0x20,
	DRAW_WINDOW_DRAWING_STRAIGHT = 0x40,
	DRAW_WINDOW_SECOND_BG = 0x80,
	DRAW_WINDOW_TOOL_CHANGING = 0x100,
	DRAW_WINDOW_EDITTING_3D_MODEL = 0x200,
	DRAW_WINDOW_IS_FOCAL_WINDOW = 0x400,
	DRAW_WINDOW_INITIALIZED = 0x800,
	DRAW_WINDOW_DISCONNECT_3D = 0x1000,
	DRAW_WINDOW_UPDATE_AREA_INITIALIZED = 0x2000,
	DRAW_WINDOW_IN_RASTERIZING_VECTOR_SCRIPT = 0x4000,
	DRAW_WINDOW_FIRST_DRAW = 0x8000,
	DRAW_WINDOW_ACTIVATE_PERSPECTIVE_RULER = 0x10000
} eDRAW_WINDOW_FLAGS;

typedef enum _eDRAW_WINDOW_DIPSLAY_UPDATE_RESULT
{
	DRAW_WINDOW_DIPSLAY_UPDATE_RESULT_NO_UPDATE,
	DRAW_WINDOW_DIPSLAY_UPDATE_RESULT_PART,
	DRAW_WINDOW_DIPSLAY_UPDATE_RESULT_ALL
} eDRAW_WINDOW_DIPSLAY_UPDATE_RESULT;

typedef struct _UPDATE_RECTANGLE
{
	FLOAT_T x, y;
	FLOAT_T width, height;
	GRAPHICS_DEFAULT_CONTEXT context;
	GRAPHICS_IMAGE_SURFACE surface;
	int need_update;
};

typedef struct _CALLBACK_IDS
{
	unsigned int display;
	unsigned int mouse_button_press;
	unsigned int mouse_move;
	unsigned int mouse_button_release;
	unsigned int mouse_wheel;
	unsigned int configure;
	unsigned int enter;
	unsigned int leave;
} CALLBACK_IDS;

typedef struct _DRAW_WINDOW_WIDGETS *DRAW_WINDOW_WIDGETS_PTR;

struct _DRAW_WINDOW
{
	int mouse_key_flags;
	
	uint8 channel;		// チャンネル数
	uint8 color_mode;	// カラーモード(現在はRGBA32のみ)

	int16 degree;		// 回転角(度数法)

	char* file_name;	// ファイル名

	char* file_path;	// ファイルパス

	// 時間で再描画を呼ぶ関数のID
	unsigned int timer_id;
	unsigned int auto_save_id;

	// タブ切り替え前のレイヤービューの位置
	int layer_view_position;

	// 画像のオリジナル幅、高さ
	int original_width, original_height;
	// 描画領域の幅、高さ(4の倍数)
	int width, height;
	// 一行分のバイト数、レイヤー一枚分のバイト数
	int stride, pixel_buf_size;
	// 回転用にマージンをとった描画領域の広さ、一行分のバイト数
	int disp_size, disp_stride;
	// 回転後の座標計算用
	FLOAT_T half_size;
	FLOAT_T angle;		// 回転角(弧度法)
	// 回転処理用の三角関数の値
	FLOAT_T sin_value, cos_value;
	// 回転処理での移動量
	FLOAT_T trans_x, trans_y;
	// カーソル座標
	FLOAT_T cursor_x, cursor_y;
	FLOAT_T last_cursor_drawn_x, last_cursor_drawn_y;
	FLOAT_T before_cursor_x, before_cursor_y;
	// 前回チェック時のカーソル座標
	FLOAT_T before_x, before_y;
	// 最後にクリックまたはドラッグされた座標
	FLOAT_T last_x, last_y;
	// 最後にクリックまたはドラッグされた時の筆圧
	FLOAT_T last_pressure;
	// カーソル座標補正用
	FLOAT_T add_cursor_x, add_cursor_y;
	// 平均法手ブレ補正での座標変換用
	FLOAT_T rev_add_cursor_x, rev_add_cursor_y;
	// 表示用のパターン
	void *rotate;
	// 画面部分更新用
	UPDATE_RECTANGLE update, temp_update;
	// 描画領域スクロールの座標
	int scroll_x, scroll_y;
	// 画面更新時のクリッピング用
	int update_clip_area[4][2];
	// 最終イベント処理時のデータ
	EVENT_STATE state;

	// コールバック関数のIDを記憶
	CALLBACK_IDS callbacks;

	uint8 *back_ground;		// 背景のピクセルデータ
	uint8 *brush_buffer;	// ブラシ用のバッファ
	uint8 *alpha_lock;		// 不透明保護時の透明度保存領域

	LAYER *layer;			// 一番下のレイヤー
	// 表示用、エフェクト用、ブラシカーソル表示用の一時保存
	LAYER *disp_layer, *effect, *disp_temp, *scaled_mixed;
	// アクティブなレイヤー&レイヤーセットへのポインタ
		// 及び表示レイヤーを合成したレイヤー
	LAYER *active_layer, *active_layer_set, *mixed_layer;
	// 作業用、一時保存用、選択範囲、アクティブレイヤーより下のレイヤー
	LAYER *work_layer, *temp_layer,
		*selection, *under_active;
	// マスクとマスク適用前の一時保存用
	LAYER *mask, *mask_temp;
	// テクスチャ用
	LAYER *texture;
	// アンチエイリアス用
	LAYER *anti_alias;
	// 表示用パターン
	GRAPHICS_SURFACE_PATTERN mixed_pattern;
	// αチャンネルのみのマスク用サーフェース
	GRAPHICS_IMAGE_SURFACE alpha_surface, alpha_temp, gray_mask_temp;
	// αチャンネルのみのイメージ
	GRAPHICS_DEFAULT_CONTEXT alpha_context, alpha_temp_context, gray_mask_context;

	// レイヤー合成用の関数群
	void (**layer_blend_functions)(LAYER* src, LAYER* dst);
	void (**part_layer_blend_functions)(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update);
	void (**blend_selection_functions)(LAYER* work, LAYER* selection);

	uint16 num_layer;		// レイヤーの数
	uint16 zoom;			// 拡大・縮小率
	FLOAT_T zoom_rate;		// 浮動小数点型の拡大・縮小率
	FLOAT_T rev_zoom;		// 拡大・縮小率の逆数

	// 解像度
	int16 resolution;
	
	// 選択範囲表示用データ
	SELECTION_AREA selection_area;
	
	// 操作履歴
	HISTORY history;

	// 表示更新等のフラグ
	unsigned int flags;
	
	// 変形処理用のデータ
	struct _TRANSFORM_DATA *transform;

	// 2つめの背景色
	uint8 second_back_ground[3];

	// 表示フィルターの状態
	uint8 display_filter_mode;

	// ICCプロファイルのデータ
	char *icc_profile_path;
	void *icc_profile_data;
	int32 icc_profile_size;
	// ICCプロファイルの内容
	cmsHPROFILE input_icc;
	// ICCプロファイルによる色変換用
	cmsHTRANSFORM icc_transform;

	// 局所キャンバス
	struct _DRAW_WINDOW *focal_window;
	// ルーペモード前のスクロールバーの位置
	int16 focal_x, focal_y;
	
	// パース定規用データ
	PERSPECTIVE_RULER perspective_ruler;

	// アプリケーション全体管理用構造体へのポインタ
	struct _APPLICATION* app;

	// 3Dモデリング用データ
	void *first_project;

	DRAW_WINDOW_WIDGETS_PTR widgets;
};

#ifdef __cplusplus
extern "C" {
#endif

extern DRAW_WINDOW_WIDGETS_PTR CreateDrawWindowWidgets(void* app, DRAW_WINDOW* canvas);

extern void ReleaseDrawWindowWidget(struct _DRAW_WINDOW_WIDGETS* widgets);

extern void ResizeDrawWindowWidgets(DRAW_WINDOW* canvas);

EXTERN LAYER* CreateLayer(
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
);

/*****************************************************
* CreateDispTempLayer関数							*
* 表示用一時保存レイヤーを作成					   *
* 引数											   *
* x				: レイヤー左上のX座標				*
* y				: レイヤー左上のY座標				*
* width			: レイヤーの幅					   *
* height		: レイヤーの高さ					 *
* channel		: レイヤーのチャンネル数			 *
* layer_type	: レイヤーのタイプ				   *
* window		: 追加するレイヤーを管理する描画領域 *
* 返り値											 *
*	初期化された構造体のアドレス					 *
*****************************************************/
EXTERN LAYER* CreateDispTempLayer(
	int32 x,
	int32 y,
	int32 width,
	int32 height,
	int8 channel,
	eLAYER_TYPE layer_type,
	struct _DRAW_WINDOW* window
);

EXTERN void InitializeCanvasContext(DRAW_WINDOW* canvas);

EXTERN void ReleaseCanvasContext(DRAW_WINDOW* canvas);

EXTERN void MouseButtonPressEvent(
    DRAW_WINDOW* canvas,
    EVENT_STATE* event_state
);

/*
* MouseMotionNotifyEvent関数
* マウスオーバーの処理
* 引数
* canvas	: 描画領域
* event		: マウスの情報
* 返り値
*	常にTRUE
*/
EXTERN int MouseMotionNotifyEvent(
    DRAW_WINDOW* canvas,
    EVENT_STATE* event_state
);

EXTERN void MouseButtonReleaseEvent(
    DRAW_WINDOW* canvas,
    EVENT_STATE* event_state
);

/*
* ExecuteMotionQueue関数
* 待ち行列に溜まったマウスの処理を行う
*  一定時間を超えたら処理を中断する
* 引数
* canvas	: 対応する描画領域
*/
EXTERN void ExecuteMotionQueue(DRAW_WINDOW* canvas);

/*
* ClearMotionQueue関数
* 待ち行列に溜まったデータを全て処理する
* 引数
* canvas	: 対応する描画領域
*/
EXTERN void ClearMotionQueue(DRAW_WINDOW* canvas);

EXTERN void DisplayEditSelection(DRAW_WINDOW* window);

/*
* DrawWindowChangeZoom関数
* キャンバスの表示拡大縮小率を変更する
* 引数
* canvas	: 拡大縮小率を変更するキャンバス
* zoom		: 変更後の拡大縮小率
*/
EXTERN void DrawWindowChangeZoom(DRAW_WINDOW* canvas, int16 zoom);

/*
* DrawWindowChangeRotate関数
* キャンバスの表示回転角を変更する
* 引数
* canvas	: 回転角を変更するキャンバス
* angle		: 新しい表示回転角(°)
*/
EXTERN void DrawWindowChangeRotate(DRAW_WINDOW* canvas, int angle);

/*
* ResizeCanvasDispTempLayer関数
* 表示用の一時保存レイヤーの幅、高さを変更
* canvas		: サイズを変更するキャンバス
* new_width		: キャンバスの新しい幅
* new_height	: キャンバスの新しい高さ
*/
EXTERN void ResizeCanvasDispTempLayer(
	DRAW_WINDOW* canvas,
	int32 new_width,
	int32 new_height
);

EXTERN void RenderTextLayer(DRAW_WINDOW* window, LAYER* target, TEXT_LAYER* layer);

EXTERN void DisplayTextLayerRange(DRAW_WINDOW* window, TEXT_LAYER* layer);

/*
* MixLayerSet関数
* レイヤーセット内を合成
* 引数
* bottom	: レイヤーセットの一番下のレイヤー
* next		: 合成後に次に合成するレイヤー
* canvas	: 描画領域を管理する構造体のアドレス
*/
EXTERN void MixLayerSet(LAYER* bottom, LAYER** next, DRAW_WINDOW* canvas);

/*
* MixLayerSetActiveOver関数
* レイヤーセット中のアクティブレイヤー以上のレイヤーを合成する
* 引数
* start		: アクティブレイヤー
* next		: 合成後の次に合成するレイヤー
* canvas	: 描画領域を管理する構造体のアドレス
*/
EXTERN void MixLayerSetActiveOver(LAYER* start, LAYER** next, DRAW_WINDOW* canvas);

/*
* GetLayerChain関数
* ピン留めされたレイヤー配列を取得する
* 引数
* window	: 描画領域の情報
* num_layer	: レイヤー数を格納する変数のアドレス
* 返り値
*	レイヤー配列
*/
EXTERN LAYER** GetLayerChain(DRAW_WINDOW* window, uint16* num_layer);

/*
* DisplayPerspectiveRulerMarker関数
* パース定規のマーカーを表示する
* 引数
* canvas	: マーカーを表示するキャンバス
* target	: 表示結果を描画するレイヤー
*/
EXTERN void DisplayPerspectiveRulerMarker(DRAW_WINDOW* canvas, LAYER* target);

/*
* DeleteSelectAreaPixels関数
* 選択範囲のピクセルデータを削除する
* 引数
* window	: 描画領域の情報
* target	: ピクセルデータを削除するレイヤー
* selection	: 選択範囲を管理するレイヤー
*/
EXTERN void DeleteSelectAreaPixels(
	DRAW_WINDOW* window,
	LAYER* target,
	LAYER* selection
);

/*
* SearchLayerByCursorColor関数
* カーソル位置にある色でレイヤーを探す
* 引数
* canvdas	: キャンバス
* x			: カーソルのX座標
* y			: カーソルのY座標
* 返り値
*	該当するレイヤー。見つからなければNULL
*/
EXTERN LAYER* SearchLayerByCursorColor(DRAW_WINDOW* canvas, int x, int y);

#ifdef __cplusplus
}
#endif

#endif // DRAW_WINDOW_H
