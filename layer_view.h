#ifndef _INCLUDED_LAYER_VIEW_H_
#define _INCLUDED_LAYER_VIEW_H_

#include "layer.h"

typedef enum _eLAYER_VIEW_FLAGS
{
	LAYER_VIEW_FLAG_IN_DRAG_OPERATION = 0x01,
	LAYER_VIEW_FLAG_DROP_UPPER = 0x02,
	LAYER_VIEW_FLAG_IN_CHANGE_NAME = 0x04,
	LAYER_VIEW_FLAG_DOCKED = 0x08,
	LAYER_VIEW_FLAG_PLACE_RIGHT = 0x10,
	LAYER_VIEW_FLAG_POP_UP = 0x20,
	LAYER_VIEW_FLAG_SCROLLBAR_PLACE_LEFT = 0x40
} eLAYER_VIEW_FLAGS;

typedef struct _LAYER_VIEW
{
	int dummy;
} LAYER_VIEW;

#ifdef __cplusplus
extern "C" {
#endif

/*
* ClearLayerView関数
* レイヤービューのウィジェットを全て削除する
*/
EXTERN void ClearLayerView(APPLICATION* app);

/*
 ResetLayerView関数
* レイヤー一覧ウィジェットをリセットする
* 引数
* canvas	: 表示するキャンバス
*/
EXTERN void ResetLayerView(DRAW_WINDOW* canvas);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_LAYER_VIEW_H_
