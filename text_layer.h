#ifndef _INCLUDED_TEXT_LAYER_H_
#define _INCLUDED_TEXT_LAYER_H_

#include "types.h"
#include "gui/gui.h"
#include "gui/layer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEXT_LAYER_SELECT_POINT_DISTANCE 25	// テキスト描画範囲を変更する際の受付範囲

typedef enum _eTEXT_STYLE
{
	TEXT_STYLE_NORMAL,
	TEXT_STYLE_ITALIC,
	TEXT_STYLE_OBLIQUE
} eTEXT_STYLE;

/*********************************
* eTEXT_LAYER_FLAGS列挙体		*
* テキストレイヤーの表示用フラグ *
*********************************/
typedef enum _eTEXT_LAYER_FLAGS
{
	TEXT_LAYER_VERTICAL = 0x01,					// 縦書き
	TEXT_LAYER_BOLD = 0x02,						// 太字
	TEXT_LAYER_ITALIC = 0x04,					// イタリック体
	TEXT_LAYER_OBLIQUE = 0x08,					// 斜体
	TEXT_LAYER_BALLOON_HAS_EDGE = 0x10,			// 吹き出しに角があるか
	TEXT_LAYER_CENTERING_HORIZONTALLY = 0x20,	// 水平方向を中央揃え
	TEXT_LAYER_CENTERING_VERTICALLY = 0x40,		// 垂直方向を中央揃え
	TEXT_LAYER_ADJUST_RANGE_TO_TEXT = 0x80		// テキストに合わせて描画範囲を調整
} eTEXT_LAYER_FLAGS;

/*********************************
* eTEXT_LAYER_BALLOON_TYPE列挙体 *
* 吹き出しのタイプ			   *
*********************************/
typedef enum _eTEXT_LAYER_BALLOON_TYPE
{
	TEXT_LAYER_BALLOON_TYPE_SQUARE,
	TEXT_LAYER_BALLOON_TYPE_ECLIPSE,
	TEXT_LAYER_BALLOON_TYPE_ECLIPSE_THINKING,
	TEXT_LAYER_BALLOON_TYPE_CRASH,
	TEXT_LAYER_BALLOON_TYPE_SMOKE,
	TEXT_LAYER_BALLOON_TYPE_SMOKE_THINKING1,
	TEXT_LAYER_BALLOON_TYPE_SMOKE_THINKING2,
	TEXT_LAYER_BALLOON_TYPE_BURST,
	NUM_TEXT_LAYER_BALLOON_TYPE
} eTEXT_LAYER_BALLOON_TYPE;

/********************************
* TEXT_LAYER_BALLOON_DATA構造体 *
* 吹き出しの形状を決めるデータ  *
********************************/
typedef struct _TEXT_LAYER_BALLOON_DATA
{
	uint16 num_edge;				// 尖りの数
	uint16 num_children;			// 引き出し線の小さい吹き出しの数
	FLOAT_T edge_size;				// 尖りのサイズ
	uint32 random_seed;				// 乱数の初期値
	FLOAT_T edge_random_size;		// 尖りのサイズのランダム変化量
	FLOAT_T edge_random_distance;	// 尖りの間隔のランダム変化量
	FLOAT_T start_child_size;		// 引き出し線の小さい吹き出しの開始サイズ
	FLOAT_T end_child_size;			// 引き出し線の小さい吹き出しの終了サイズ
} TEXT_LAYER_BALLOON_DATA;

/*********************************************
* TEXT_LAYER_POINTS構造体					*
* テキストレイヤー上でのドラッグ処理のデータ *
*********************************************/
typedef struct _TEXT_LAYER_POINTS
{
	int point_index;
	FLOAT_T points[9][2];
} TEXT_LAYER_POINTS;

/*******************************
* TEXT_LAYER構造体			 *
* テキストレイヤーの情報を格納 *
*******************************/
typedef struct _TEXT_LAYER
{
	TEXT_LAYER_WIDGETS *widgets;						// フォント変更などのウィジェット
	char *text;											// バッファから取り出したテキスト情報
	FLOAT_T x, y;										// テキスト表示領域左上の座標
	FLOAT_T width, height;								// テキスト表示領域の幅、高さ
	uint16 balloon_type;								// 吹き出しのタイプ
	FLOAT_T line_width;									// 線の太さ
	FLOAT_T edge_position[3][2];						// 吹き出しの尖り先
	FLOAT_T arc_start, arc_end;							// 吹き出しの円弧開始と終了角度
	TEXT_LAYER_POINTS *drag_points;						// ドラッグ処理のデータ
	TEXT_LAYER_BALLOON_DATA balloon_data;				// 吹き出し描画用のデータ
	TEXT_LAYER_BALLOON_DATA_WIDGETS balloon_widgets;	// 吹き出しの詳細設定ウィジェット
	int base_size;										// フォントサイズの倍率
	FLOAT_T font_size;									// 表示テキストのサイズ
	const char *font_file_path;							// フォントファイルのパス
	const char *font_name;								// フォントの名前
	uint8 color[3];										// 表示文字色
	uint8 back_color[4];								// 背景色
	uint8 line_color[4];								// 吹き出し枠の色
	uint32 flags;										// 縦書き、太字等のフラグ
} TEXT_LAYER;

/*****************************************
* TEXT_LAYER_BASE_DATA構造体			 *
* テキストレイヤーの基本情報(書き出し用) *
*****************************************/
typedef struct _TEXT_LAYER_BASE_DATA
{
	FLOAT_T x, y;							// テキスト表示領域左上の座標
	FLOAT_T width, height;					// テキスト表示領域の幅、高さ
	uint16 balloon_type;					// 吹き出しのタイプ
	FLOAT_T font_size;						// 表示テキストのサイズ
	uint8 base_size;						// 文字サイズの倍率
	uint8 color[3];							// 表示文字色
	FLOAT_T edge_position[3][2];			// 吹き出しの尖り先
	FLOAT_T arc_start;						// 吹き出しの円弧開始角
	FLOAT_T arc_end;						// 吹き出しの円弧終了角
	uint8 back_color[4];					// 吹き出しの背景色
	uint8 line_color[4];					// 吹き出しの線の色
	FLOAT_T line_width;						// 吹き出しの線の太さ
	TEXT_LAYER_BALLOON_DATA balloon_data;	// 吹き出しの詳細データ
	uint32 flags;							// 縦書き、太字等のフラグ
} TEXT_LAYER_BASE_DATA;

/*
* CreateTextLayer関数
* テキストレイヤーのデータメモリ確保と初期化
* 引数
* window			: キャンバスの情報
* x				 : レイヤーのX座標
* y				 : レイヤーのY座標
* width			 : レイヤーの幅
* height			: レイヤーの高さ
* base_size		 : 文字サイズの倍率
* font_size		 : 文字サイズ
* font_file_path	: TrueTypeフォントファイルパス
* color			 : 文字色
* balloon_type	  : 吹き出しのタイプ
* back_color		: 吹き出しの背景色
* line_color		: 吹き出しの線の色
* line_width		: 吹き出しの線の太さ
* balloon_data  	: 吹き出し形状の詳細データ
* flags			 : テキスト表示のフラグ
* 返り値
*	初期化された構造体のアドレス
*/
EXTERN TEXT_LAYER* CreateTextLayer(
	void* window,
	FLOAT_T x,
	FLOAT_T y,
	FLOAT_T width,
	FLOAT_T height,
	int base_size,
	FLOAT_T font_size,
	const char* font_file_path,
	const uint8 color[3],
	uint16 balloon_type,
	const uint8 back_color[4],
	const uint8 line_color[4],
	FLOAT_T line_width,
	TEXT_LAYER_BALLOON_DATA* balloon_data,
	uint32 flags
);

EXTERN void TextLayerButtonPressCallBack(
	DRAW_WINDOW* canvas,
	const EVENT_STATE* event_info
);

EXTERN void TextLayerMotionCallBack(
	DRAW_WINDOW* canvas,
	EVENT_STATE* event_state
);

EXTERN void TextLayerButtonReleaseCallBack(
	DRAW_WINDOW* canvas,
	EVENT_STATE* event_state
);

/*
* TextLayerGetBalloonArcPoint関数
* 吹き出しの尖り開始座標を取得する
* layer	: テキストレイヤーの情報
* angle	: 吹き出しの中心に対する尖り開始位置の角度
* point	: 座標を取得する配列
*/
EXTERN void TextLayerGetBalloonArcPoint(
	TEXT_LAYER* layer,
	FLOAT_T angle,
	FLOAT_T point[2]
);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _INCLUDED_TEXT_LAYER_H_ */
