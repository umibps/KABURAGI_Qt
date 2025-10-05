#ifndef _INCLUDED_APPLICATION_H_
#define _INCLUDED_APPLICATION_H_

#include "labels.h"
#include "fractal_label.h"
#include "draw_window.h"
#include "graphics/graphics.h"
#include "selection_area.h"
#include "display_filter.h"
#include "tool_box.h"
#include "navigation.h"
#include "texture.h"

#define MAX_CANVAS (128)

#define FILE_VERSION 6
#define THUMBNAIL_SIZE 128

#define APPLICATION_INITIALIZE_FILE_PATH "./application.ini"
#define COMMON_TOOL_INITIALIZE_FILE_PATH "./common_tools.ini"
#define BRUSH_INITIALIZE_FILE_PATH "./brushes.ini"
#define VECTOR_BRUSH_INITIALIZE_FILE_PATH "./vector_brushes.ini"

/*************************************************
* eAPPLICATION_FLAGS列挙体					   *
* アプリケーションのファイル操作中等で立つフラグ *
*************************************************/
typedef enum _eAPPLICATION_FLAGS
{
	APPLICATION_IN_OPEN_OPERATION = 0x01,				// ファイルオープン
	APPLICATION_IN_MAKE_NEW_DRAW_AREA = 0x02,			// 描画領域の新規作成
	APPLICATION_IN_REVERSE_OPERATION = 0x04,			// 左右反転処理中
	APPLICATION_IN_EDIT_SELECTION = 0x08,				// 選択範囲編集切り替え中
	APPLICATION_INITIALIZED = 0x10,						// 初期化済み
	APPLICATION_IN_DELETE_EVENT = 0x20,					// 削除イベント内
	APPLICATION_FULL_SCREEN = 0x40,						// フルスクリーンモード
	APPLICATION_WINDOW_MAXIMIZE = 0x80,					// ウィンドウの最大化
	APPLICATION_DISPLAY_GRAY_SCALE = 0x100,				// グレースケールでの表示
	APPLICATION_DISPLAY_SOFT_PROOF = 0x200,				// ICCプロファイルでソフトプルーフ表示
	APPLICATION_REMOVE_ACCELARATOR = 0x400,				// ショートカットキーを無効化
	APPLICATION_DRAW_WITH_TOUCH = 0x800,				// タッチイベントで描画する
	APPLICATION_SET_BACK_GROUND_COLOR = 0x1000,			// キャンバスの背景を設定する
	APPLICATION_SHOW_PREVIEW_ON_TASK_BAR = 0x2000,		// プレビューウィンドウをタスクバーに表示する
	APPLICATION_IN_SWITCH_DRAW_WINDOW = 0x4000,			// 描画領域の切替中
	APPLICATION_WRITE_PROGRAM_DATA_DIRECTORY = 0x8000,	// ファイルの書出しをProgram Dataフェオルダにする
	APPLICATION_HAS_3D_LAYER = 0x10000,					// 3Dモデリングの使用可否
	APPLICATION_KEEP_LAYER_SAVE_DATA = 0x20000			// レイヤーの保存データをメモリ維持 (書き出し高速化用)
} eAPPLICATION_FLAGS;

typedef enum _eINPUT_DEVICE
{
	INPUT_PEN,
	INPUT_ERASER
} eINPUT_DEVICE;

typedef struct _MAIN_WINDOW_WIDGETS* MAIN_WINDOW_WIDGETS_PTR;

typedef struct _APPLICATION_MENUS
{
#define DISABLE_BUFFER_SIZE 80
#define SELECTION_AREA_DEPENDS_BUFFER_SIZE 32
#define MULTI_LAYER_DEPENDS_BUFFER_SIZE 8
#define DISABLE_AT_NORMAL_LAYER_BUFFER_SIZE 8
	// 描画領域が無いときに無効
	void *disable_if_no_open[DISABLE_BUFFER_SIZE];
	// 選択範囲が無いときに無効
	void *disable_if_no_select[SELECTION_AREA_DEPENDS_BUFFER_SIZE];
	// レイヤーが一枚しかないときに無効
	void *disable_if_single_layer[MULTI_LAYER_DEPENDS_BUFFER_SIZE];
	// 通常のレイヤーのときに無効
	void *disable_if_normal_layer[DISABLE_AT_NORMAL_LAYER_BUFFER_SIZE];
	uint8 num_disable_if_no_open;
	uint8 menu_start_disable_if_no_open;
	uint8 num_disable_if_no_select;
	uint8 menu_start_disable_if_no_select;
	uint8 num_disable_if_single_layer;
	uint8 menu_start_disable_if_single_layer;
	uint8 num_disable_if_normal_layer;
	uint8 menu_start_disable_if_normal_layer;
	// 下のレイヤーと結合メニュー
	void *merge_down_menu;
	// 背景色切り替えメニュー
	void *change_back_ground_menu;
	// 表示フィルター切り替えメニュー
	void *display_filter_menus[NUM_DISPLAY_FUNCION_TYPE];
#undef DISABLE_BUFFER_SIZE
#undef SELECTION_AREA_DEPENDS_BUFFER_SIZE
#undef MULTI_LAYER_DEPENDS_BUFFER_SIZE
#undef DISABLE_AT_NORMAL_LAYER_BUFFER_SIZE
} APPLICATION_MENUS;

struct _APPLICATION
{
	unsigned int flags;
	
	MAIN_WINDOW_WIDGETS_PTR widgets;
	LAYER_VIEW_WIDGETS *layer_view;
	// ウィンドウの位置、サイズ
	int window_x, window_y, window_width, window_height;
	// 左右ペーンの位置
	int left_pane_position, right_pane_position;
	// ブラシなどの管理データ
	TOOL_BOX tool_box;
	// ナビゲーションウィンドウ
	NAVIGATION navigation;
	// ブラシテクスチャ
	TEXTURES textures;
	// GUIの拡大率
	FLOAT_T gui_scale;
	// 入力デバイス
	eINPUT_DEVICE input;
	DRAW_WINDOW *draw_window[MAX_CANVAS];
	int window_num;
	int active_window;

	APPLICATION_MENUS menus;
	// 表示時に適用するフィルターのデータ
	DISPLAY_FILTER display_filter;
	
	GRAPHICS graphics;

	// UIに表示する文字列
	APPLICATION_LABELS *labels;
	FRACTAL_LABEL *fractal_labels;

	// レイヤー合成用の関数ポインタ配列
	void (*layer_blend_functions[NUM_LAYER_BLEND_FUNCTIONS])(LAYER* src, LAYER* dst);
	void (*part_layer_blend_functions[NUM_LAYER_BLEND_FUNCTIONS])(LAYER* src, LAYER* dst, UPDATE_RECTANGLE* update);
	void (*blend_selection_functions[NUM_SELECTION_BLEND_MODE])(LAYER* work, LAYER* selection);
	
	// 吹き出しを描画する関数ポインタ配列
	void (*draw_balloon_functions[NUM_TEXT_LAYER_BALLOON_TYPE])(TEXT_LAYER*, LAYER*, DRAW_WINDOW*);
	
	// 初期化用ファイルパス
	char *common_tool_file_path;
	char *brush_file_path;
};

#ifdef __cplusplus
extern "C" {
#endif

/*
* InitializeApplication関数
* アプリケーションの初期化
* 引数
* app				: アプリケーション全体を管理する構造体のアドレス
* argv				: main関数の第一引数
* argc				: main関数の第二引数
* init_file_name	: 初期化ファイルの名前
*/
EXTERN void InitializeApplication(APPLICATION* app, char** argv, int argc, char* init_file_name);

/*
* ReadInitializeFile関数
* 初期化ファイルを読み込む
* 引数
* app		: アプリケーションを管理する構造体のアドレス
* file_path	: 初期化ファイルのパス
* 返り値
*	正常終了:0	失敗:負の値
*/
EXTERN int ReadInitializeFile(APPLICATION* app, const char* file_path);

EXTERN DRAW_WINDOW* CreateDrawWindow(
	int32 width,
	int32 height,
	uint8 channel,
	const char* name,
	void* tab,
	uint16 window_id,
	struct _APPLICATION* app
);

/*********************************************************
* GetActiveDrawWindow関数								*
* アクティブな描画領域を取得する						 *
* 引数												   *
* app		: アプリケーションを管理する構造体のアドレス *
* 返り値												 *
*	アクティブな描画領域								 *
*********************************************************/
EXTERN DRAW_WINDOW* GetActiveDrawWindow(APPLICATION* app);

EXTERN void ExecuteOpen(APPLICATION* app);
EXTERN void ExecuteSave(APPLICATION* app);
EXTERN void ExecuteSaveAs(APPLICATION* app);
EXTERN void ExecuteMakeColorLayer(APPLICATION* app);
EXTERN void ExecuteDeleteActiveLayer(APPLICATION* app);
EXTERN void ExecuteUndo(APPLICATION* app);
EXTERN void ExecuteRedo(APPLICATION* app);

EXTERN MAIN_WINDOW_WIDGETS_PTR CreateMainWindowWidgets(APPLICATION* app);

EXTERN void AddDrawWindow2MainWindow(MAIN_WINDOW_WIDGETS_PTR main_window, DRAW_WINDOW* canvas);

EXTERN void InitializeBlendSelectionFunctions(void (*functions[])(LAYER* work, LAYER* selection));

/*
* WriteApplicationSettingFiles関数
* 設定ファイルを書き込む
* 引数
* app	: アプリケーションを管理する構造体のアドレス
*/
EXTERN void WriteApplicationSettingFiles(APPLICATION* app);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _INCLUDED_APPLICATION_H_ */
