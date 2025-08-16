#ifndef _INCLUDED_SMOOTHER_H_
#define _INCLUDED_SMOOTHER_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// 手ブレ補正に使用するデータ点の最大数
#define SMOOTHER_POINT_BUFFER_SIZE 30
// 手ブレ補正の適用割合の最大値
#define SMOOTHER_RATE_MAX 100
// 手ブレ補正の方式
typedef enum _eSMOOTH_METHOD
{
	SMOOTH_GAUSSIAN,
	SMOOTH_AVERAGE
} eSMOOTH_METHOD;

/***********************
* SMOOTHER_POINT構造体 *
* 手ブレ補正用の座標   *
***********************/
typedef struct _SMOOTHER_POINT
{
	FLOAT_T x, y;
} SMOOTHER_POINT;

/*******************
* SMOOTHER構造体   *
* 手ブレ補正の情報 *
*******************/
typedef struct _SMOOTHER
{
	// 手ブレ補正に使用する座標データ
	SMOOTHER_POINT buffer[SMOOTHER_POINT_BUFFER_SIZE];
	// 前回手ブレ補正したときの座標データ
	SMOOTHER_POINT before_point;
	// 最後に描画した座標
	SMOOTHER_POINT last_draw_point;
	// 平均値計算用の合計値
	SMOOTHER_POINT sum;
	// 距離データ
	FLOAT_T velocity[SMOOTHER_POINT_BUFFER_SIZE];
	// 補正をかける割合
	FLOAT_T rate;
	// 配列のインデックス
	int index;
	// データ点数
	int num_data;
	// 使用するデータ点数
	int num_use;
	// 前回手ブレ補正したときの時間
	unsigned int last_time;
	// 手ブレ補正の方式
	int mode;
} SMOOTHER;

// マウスカーソルの座標 待ち行列の最小バッファサイズ
#define MINIMUM_MOTION_QUEUE_BUFFER_SIZE 128
// マウスカーソルの座標 待ち行列の最大バッファサイズ
#define MAXIMUM_MOTION_QUEUE_BUFFER_SIZE 8192

/************************************
* MOTION_QUEUE_ITEM構造体		   *
* マウスカーソル移動の1回分のデータ *
************************************/
typedef struct _MOTION_QUEUE_ITEM
{
	EVENT_STATE state;
} MOTION_QUEUE_ITEM;

/***************************
* MOTION_QUEUE構造体	   *
* マウスカーソル移動の情報 *
***************************/
typedef struct _MOTION_QUEUE
{
	// 最後に追加したマウスカーソルの座標
	FLOAT_T last_queued_x, last_queued_y;
	// 保有アイテム数
	int num_items;
	// インデックス探索位置
	int start_index;
	// 保有アイテム最大数
	int max_items;
	// アイテムバッファー
	MOTION_QUEUE_ITEM *queue;
} MOTION_QUEUE;

// 関数のプロトタイプ宣言
#define INIT_SMOOTHER(SMOOTHER) (SMOOTHER).index = 0, (SMOOTHER).num_data=0

/*
* Smooth関数
* 手ブレ補正を行う
* 引数
* smoother	: 手ブレ補正のデータ
* x			: 現在のx座標。補正後の値が入る
* y			: 現在のy座標。補正後の値が入る
* this_time	: 手ブレ補正処理開始時間
* zoom_rate	: 描画領域の拡大縮小率
*/
extern void Smooth(
	SMOOTHER* smoother,
	FLOAT_T* x,
	FLOAT_T* y,
	unsigned int this_time,
	FLOAT_T zoom_rate
);

/*
* AddAverageSmoothPoint関数
* 座標平均化による手ブレ補正にデータ点を追加
* 引数
* smoother	: 手ブレ補正のデータ
* state     : 座標、筆圧等のデータ
* zoom_rate	: 描画領域の拡大縮小率
* 返り値
*	ブラシの描画を行う:TRUE	行わない:FALSE
*/
EXTERN int AddAverageSmoothPoint(
	SMOOTHER* smoother,
	EVENT_STATE* state,
	FLOAT_T zoom_rate
);

/*
* AverageSmoothFlush関数
* 平均化による手ブレ補正の残りバッファを1つ取り出す
* 引数
* smoother	: 手ブレ補正の情報
* state     : 座標, 筆圧等の情報
* 返り値
*	バッファの残り無し:TRUE	残り有り:FALSE
*/
EXTERN int AverageSmoothFlush(
	SMOOTHER* smoother,
	EVENT_STATE* state
);

/*
* MotionQueueAppendItem関数
* マウスカーソルの座標を追加する
* 引数
* queue     : 座標管理のデータ
* state     : 座標, 筆圧などのデータ
*/
EXTERN void MotionQueueAppendItem(
	MOTION_QUEUE* queue,
	EVENT_STATE* state
);

#ifdef __cplusplus
}
#endif

#endif	// #ifndef _INCLUDED_SMOOTHER_H_
