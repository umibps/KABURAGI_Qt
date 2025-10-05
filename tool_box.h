#ifndef _INCLUDED_TOOL_BOX_H_
#define _INCLUDED_TOOL_BOX_H_

#include "types.h"
#include "color.h"
#include "brush_core.h"
#include "common_tools.h"
#include "vector_brushes.h"
#include "smoother.h"

#define BRUSH_TABLE_WIDTH (4)
#define BRUSH_TABLE_HEIGHT (16)

#define MAX_BRUSH_SIZE 500
#define MAX_BRUSH_STRIDE ((MAX_BRUSH_SIZE) * 4)

#define VECTOR_BRUSH_TABLE_WIDTH (4)
#define VECTOR_BRUSH_TABLE_HEIGHT (8)

#define COMMON_TOOL_TABLE_WIDTH 5
#define COMMON_TOOL_TABLE_HEIGHT 2

#define PALLETE_WIDTH 16
#define PALLETE_HEIGHT 16

typedef enum _eUTILITY_PLACE
{
	UTILITY_PLACE_WINDOW,
	UTILITY_PLACE_LEFT,
	UTILITY_PLACE_RIGHT,
	UTILITY_PLACE_POPUP_LEFT,
	UTILITY_PLACE_POPUP_RIGHT
} eUTILITY_PLACE;

typedef enum _eTOOL_BOX_FLAGS
{
	TOOL_USING_BRUSH = 0x01,
	TOOL_DOCKED = 0x02,
	TOOL_PLACE_RIGHT = 0x04,
	TOOL_SHOW_COLOR_CIRCLE = 0x08,
	TOOL_SHOW_COLOR_PALLETE = 0x10,
	TOOL_POP_UP = 0x20,
	TOOL_CHANGING_BRUSH = 0x40,
	TOOL_BUTTON_STOPPED = 0x80
} eTOOL_BOX_FLAGS;

struct _TOOL_BOX
{
	// 現在表示しているツールの種類 (アクティブレイヤー依存)
	eLAYER_TYPE current_tool_type;
	// 現在表示しているツールの詳細設定
	void *current_detail_ui;
	TOOL_WINDOW_WIDGETS *widgets;
	// 現在使用しているブラシへのポインタ
	BRUSH_CORE *active_brush[2];
	// ブラシ画像のピクセルデータ
	uint8 *brush_pattern_buff, *temp_pattern_buff;
	// コピーするブラシへのポインタ
	void *copy_brush;
	// ブラシファイルの文字コード
	char *brush_code;
	// ウィジェットの配置
	int place;
	// ウィンドウの位置、サイズ
	int window_x, window_y, window_width, window_height;
	// ブラシテーブルと詳細設定を区切るペーンの位置
	int pane_position;
	// ブラシボタンに使用するフォント名
	char *font_file;
	// 色選択用のデータ
    COLOR_CHOOSER color_chooser;
	// ブラシのコアテーブル
	BRUSH_CORE *brushes[BRUSH_TABLE_HEIGHT][BRUSH_TABLE_WIDTH];
	// 共通ツールのブラシテーブル
	COMMON_TOOL_CORE *common_tools[COMMON_TOOL_TABLE_HEIGHT][COMMON_TOOL_TABLE_WIDTH];
	// 現在使用している共通ツールへのポインタ
	COMMON_TOOL_CORE *active_common_tool;
	// 共通ツールファイルの文字コード
	char *common_tool_code;
	// ベクトルブラシのコアテーブル
	VECTOR_BRUSH_CORE *vector_brushes[VECTOR_BRUSH_TABLE_HEIGHT][VECTOR_BRUSH_TABLE_WIDTH];
	// 現在使用しているベクトルブラシへのポインタ
	VECTOR_BRUSH_CORE *active_vector_brush[2];
	// ベクトルブラシファイルの文字コード
	char *vector_brush_code;
	// 通常レイヤーでControlキーが押されているときはスポイトツールに偏差させるため
		// スポイトツールのデータを保持しておく
	COLOR_PICKER color_picker;
	// ベクトルレイヤーでControl、Shiftキーが押されているときには制御点ツールに変化させるため
		// 制御点ツールのデータを保持しておく
	VECTOR_BRUSH_CORE vector_control_core;
	CONTROL_POINT_TOOL vector_control;
	// 手ブレ補正の情報
	SMOOTHER smoother;
	// ブラシ操作データの待ち行列
	MOTION_QUEUE motion_queue;
	// 簡易ブラシ切り替えのデータ
	BRUSH_CHAIN brush_chain;
	// パレットの情報
	uint8 pallete[(PALLETE_WIDTH*PALLETE_HEIGHT)][3];
	uint8 pallete_use[((PALLETE_WIDTH*PALLETE_HEIGHT)+7)/8];
	// ツールボックス全体で扱うフラグ
	unsigned int flags;
};

extern void WriteToolBoxData(
	APPLICATION* app,
	const char* common_tool_file_path,
	const char* brush_file_path,
	const char* vector_brush_file_path
);

extern void ToolDummyFunction(
    struct _DRAW_WINDOW* canvas,
    void* core,
    EVENT_STATE* state
);

extern void ToolDummyDisplay(
    struct _DRAW_WINDOW* canvas,
    void* core,
    FLOAT_T x,
    FLOAT_T y
);

#endif /* #ifndef _INCLUDED_TOOL_BOX_H_ */
