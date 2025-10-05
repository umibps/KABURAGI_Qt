#ifndef _INCLUDED_DISPLAY_H_
#define _INCLUDED_DISPLAY_H_

#include "draw_window.h"

EXTERN eDRAW_WINDOW_DIPSLAY_UPDATE_RESULT LayerBlendForDisplay(DRAW_WINDOW* canvas);

/*
* MixLayerForSave関数
* 保存/自動選択/バケツツールのために背景ピクセルデータ無しでレイヤーを合成
* 引数
* canvas	: 合成を実施するキャンバス
* 返り値
*	合成されたレイヤー 使用後はDeleteLayer必要
*/
EXTERN LAYER* MixLayerForSave(DRAW_WINDOW* canvas);

#endif // #ifndef _INCLUDED_DISPLAY_H_
