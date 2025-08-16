#ifndef _INCLUDED_NAVIGATION_H_
#define _INCLUDED_NAVIGATION_H_

#include "types.h"

typedef struct _NAVIGATION
{
	NAVIGATION_WIDGETS *widgets;
	uint8 *pixels;
	uint8 *reverse_buffer;
	int width;			// ナビゲーションウィジェットの幅
	int height;			// ナビゲーションウィジェットの高さ
	int draw_width;		// 画像表示領域の幅
	int draw_height;	// 画像表示領域の高さ

	struct	// 画像表示用の座標や拡大率データ
	{
		FLOAT_T x, y;				// ナビゲーションに表示する現表示領域の枠の座標
		FLOAT_T ratio_x, ratio_y;	// 画像描画時の横/縦拡大率
		FLOAT_T width, height;		// ナビゲーションに表示する枠のサイズ
		FLOAT_T angle;				// キャンバスの回転角
		uint16 zoom;				// ナビゲーションの拡大縮小率
	} point;
} NAVIGATION;

#ifdef __cplusplus
extern "C" {
#endif

EXTERN NAVIGATION_WIDGETS* CreateNavigationWidgets(APPLICATION* app);

/*
* ChangeNavigationRotate関数
* ナビゲーションの回転角度変更時に呼び出す関数
* 引数
* navigation			: ナビゲーションウィンドウの情報
* canvas				: 表示するキャンバス
* canvas_area_width		: キャンバスを表示しているスクロールエリアの幅
* canvas_area_height	: キャンバスを表示しているスクロールエリアの高さ
* navigation_width		: ナビゲーションのキャンバス表示エリアの幅
* navigation_height		: ナビゲーションのキャンバス表示エリアの高さ
*/
EXTERN void ChangeNavigationRotate(
	NAVIGATION* navigation,
	DRAW_WINDOW* canvas,
	int canvas_area_width,
	int canvas_area_height,
	int navigation_width,
	int navigation_height
);

EXTERN void ChangeNavigationDrawWindow(
	NAVIGATION* navigation,
	DRAW_WINDOW* canvas,
	int canvas_area_width,
	int canvas_area_height,
	int navigation_width,
	int navigation_height
);

EXTERN void DiplayNavigation(
	NAVIGATION* navigation,
	DRAW_WINDOW* canvas,
	int canvas_area_width,
	int canvas_area_height,
	int canvas_scroll_x,
	int canvas_scroll_y,
	int navigation_width,
	int navigation_height,
	void* draw_context
);

EXTERN void NavigationDrawCanvas(NAVIGATION_WIDGETS* widgets, void* draw_context);

EXTERN void NavigationDrawScrollFrame(
	NAVIGATION_WIDGETS* widgets,
	FLOAT_T x,
	FLOAT_T y,
	FLOAT_T width,
	FLOAT_T height,
	void* display_context
);

EXTERN void NavigationUpdate(NAVIGATION_WIDGETS* widgets);

#ifdef __cplusplus
}
#endif

#endif	/* _INCLUDED_NAVIGATION_H_ */
