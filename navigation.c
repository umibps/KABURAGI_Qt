#include "navigation.h"
#include "draw_window.h"
#include "gui/navigation.h"

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
void ChangeNavigationRotate(
	NAVIGATION* navigation,
	DRAW_WINDOW* canvas,
	int canvas_area_width,
	int canvas_area_height,
	int navigation_width,
	int navigation_height
)
{
	FLOAT_T zoom_x, zoom_y;
	FLOAT_T trans_x, trans_y;
	FLOAT_T half_size;
	int half_width, half_height;

	half_size = (canvas->width > canvas->height) ?
		canvas->width * ROOT2 : canvas->height * ROOT2;

	half_width = navigation_width / 2;
	half_height = navigation_height / 2;

	zoom_x = ((FLOAT_T)canvas->width) / navigation_width;
	zoom_y = ((FLOAT_T)canvas->height) / navigation_height;

	if((canvas->flags & DRAW_WINDOW_DISPLAY_HORIZON_REVERSE) == 0)
	{
		trans_x = - half_width * zoom_x +
					(half_width * (1/ROOT2) * zoom_x * canvas->cos_value
					+ half_height * (1/ROOT2) * zoom_x * canvas->sin_value);
		trans_x /= zoom_x;
		trans_y = -half_height * zoom_y - (
					half_width * (1/ROOT2) * zoom_y * canvas->sin_value
					- half_height * (1/ROOT2) * zoom_y * canvas->cos_value);
		trans_y /= zoom_y;

		NavigationSetDrawCanvasMatrix(navigation->widgets, zoom_x*ROOT2, zoom_y*ROOT2,
										canvas->angle, trans_x, trans_y);
	}
	else
	{
		trans_x = -half_width * zoom_x +
			(-half_width * (1 / ROOT2) * zoom_x * canvas->cos_value + half_height * (1 / ROOT2) * zoom_x * canvas->sin_value);
		trans_x /= zoom_x;
		trans_y = -half_height * zoom_y - (
			-half_width * (1 / ROOT2) * zoom_y * canvas->sin_value - half_height * (1 / ROOT2) * zoom_y * canvas->cos_value);
		trans_y /= zoom_y;

		NavigationSetDrawCanvasMatrix(navigation->widgets, - zoom_x * ROOT2, zoom_y * ROOT2,
			canvas->angle, trans_x, trans_y);
	}
}

void ChangeNavigationDrawWindow(
	NAVIGATION* navigation,
	DRAW_WINDOW* canvas,
	int canvas_area_width,
	int canvas_area_height,
	int navigation_width,
	int navigation_height
)
{
	NavigationSetCanvasPattern(navigation->widgets, canvas);

	// ナビゲーションの枠の幅
	navigation->point.width = (FLOAT_T)canvas_area_width / ((FLOAT_T)canvas->disp_size);
	if(navigation->point.width > 1.0)
	{
		navigation->point.width = 1.0;
	}
	// ナビゲーションの枠の高さ
	navigation->point.height = (FLOAT_T)canvas_area_height / ((FLOAT_T)canvas->disp_size);
	if(navigation->point.height > 1.0)
	{
		navigation->point.height = 1.0;
	}

	navigation->draw_width = canvas_area_width;
	navigation->draw_height = canvas_area_height;

	// 枠表示座標補正用の値
	navigation->point.ratio_x =
		(1.0 / canvas->disp_size) * navigation_width;
	navigation->point.ratio_y =
		(1.0 / canvas->disp_size) * navigation_height;

	// キャンバスの回転角と拡大縮小率を記憶
	navigation->point.angle = canvas->angle;
	navigation->point.zoom = canvas->zoom;

	// ナビゲーションにキャンバスを拡大縮小&回転表示するための行列を更新
	ChangeNavigationRotate(navigation, canvas, canvas_area_width, canvas_area_height,
							navigation_width, navigation_height);
}

void DiplayNavigation(
	NAVIGATION* navigation,
	DRAW_WINDOW* canvas,
	int canvas_area_width,
	int canvas_area_height,
	int canvas_scroll_x,
	int canvas_scroll_y,
	int navigation_width,
	int navigation_height,
	void* draw_context
)
{
	int stride;

	stride = navigation_width * 4;
	// ナビゲーションのサイズが変更されてたらピクセルデータのメモリを確保
	if(navigation->draw_width != navigation_width
		|| navigation->draw_height != navigation_height)
	{
		navigation->pixels = (uint8*)MEM_REALLOC_FUNC(navigation->pixels,
										navigation_height * stride);
		navigation->reverse_buffer = (uint8*)MEM_REALLOC_FUNC(
										navigation->reverse_buffer, navigation_height * stride);

		ChangeNavigationDrawWindow(navigation, canvas,
						canvas_area_width, canvas_area_height, navigation_width, navigation_height);
	}
	// 拡大縮小率またはウィンドウサイズまたは回転角が変更されていたら
		// ナビゲーションにキャンバスを拡大縮小&回転表示するための行列を更新
	else if(navigation->point.zoom != canvas->zoom
			|| navigation->point.angle != canvas->angle
			|| navigation->draw_width != canvas_area_height
			|| navigation->draw_height != canvas_area_height
	)
	{
		ChangeNavigationDrawWindow(navigation, canvas,
			canvas_area_width, canvas_area_height, navigation_width, navigation_height);
	}

	// キャンバスを縮小してナビゲーションに描画
	NavigationDrawCanvas(navigation->widgets, draw_context);

	// 左右反転表示時のピクセルデータ操作はもしかして不要？
	// if((canvas->flags & DRAW_WINDOW_DISPLAY_HORIZON_REVERSE) != 0)

	// キャンバスがスクロールエリアの範囲に収まらない場合には
		// 現在の表示位置を枠で表示する
	if(canvas_area_width <= canvas->disp_size || canvas_area_height <= canvas->disp_size)
	{
		FLOAT_T x, y, width, height;

		x = canvas_scroll_x;
		navigation->point.x = x;
		x *= navigation->point.ratio_x;
		width = navigation->point.width * navigation_width;
		if(x + width > navigation_width)
		{
			width = navigation_width - x;
		}

		y = canvas_scroll_y;
		navigation->point.y = y;
		y *= navigation->point.ratio_y;
		height = navigation->point.height * navigation_height;
		if(y + height > navigation_height)
		{
			height = navigation_height - y;
		}

		NavigationDrawScrollFrame(navigation->widgets, x, y, width, height, draw_context);
	}
}
