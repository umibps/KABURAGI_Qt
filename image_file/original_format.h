#ifndef _INCLUDED_ORIGINAL_FORMAT_H_
#define _INCLUDED_ORIGINAL_FORMAT_H_

#include "../types.h"
#include "../vector.h"
#include "../memory_stream.h"
#include "../layer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
* ReadOriginalFormat関数
* 独自形式のデータを読み込む
* 引数
* stream		: 読み込み元データストリーム
* read_function	: 読み込み用関数
* stream_size	: データの総バイト数
* app			: 読み込み進捗表示&キャンバス作成の為、アプリケーション管理データを渡す
* data_name		: ファイル名
* 返り値
*	読み込んだデータを適用したキャンバス。失敗したらNULL
*/
EXTERN DRAW_WINDOW* ReadOriginalFormat(
	void* stream,
	stream_func_t read_function,
	size_t stream_size,
	APPLICATION* app,
	const char* data_name
);

EXTERN void StoreCanvas(
	void* stream,
	stream_func_t write_function,
	DRAW_WINDOW* canvas,
	int add_thumbnail,
	int compress_level
);

EXTERN void StoreLayerData(
	LAYER* layer,
	MEMORY_STREAM_PTR stream,
	int32 compress_level,
	int save_layer_data
);

EXTERN void StoreVectorLayerdata(
	LAYER* layer,
	void* out,
	stream_func_t write_function,
	MEMORY_STREAM_PTR stream,
	int32 compress_level
);

EXTERN void StoreVectorShapeData(
	VECTOR_DATA* data,
	MEMORY_STREAM_PTR stream
);

EXTERN void StoreVectorScriptData(VECTOR_SCRIPT* script, MEMORY_STREAM_PTR stream);

EXTERN void StoreAdjustmentLayerData(
	void* stream,
	stream_func_t write_function,
	ADJUSTMENT_LAYER* layer
);

#ifdef __cplusplus
}
#endif

#endif	/* _INCLUDED_ORIGINAL_FORMAT_H_ */
