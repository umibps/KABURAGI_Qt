#ifndef _INCLUDED_PNG_FILE_H_
#define _INCLUDED_PNG_FILE_H_

#include "../types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
* ReadPNGStream関数
* PNGイメージデータを読み込む
* 引数
* stream	: データストリーム
* read_func	: 読み込み用の関数ポインタ
* width		: イメージの幅を格納するアドレス
* height	: イメージの高さを格納するアドレス
* stride	: イメージの一行分のバイト数を格納するアドレス
* 返り値
*	ピクセルデータ
*/
EXTERN uint8* ReadPNGStream(
	void* stream,
	stream_func_t read_func,
	int32* width,
	int32* height,
	int32* stride
);

/*
* WritePNGStream関数
* PNGのデータをストリームに書き込む
* 引数
* stream		: データストリーム
* write_func	: 書き込み用の関数ポインタ
* pixels		: ピクセルデータ
* width			: 画像の幅
* height		: 画像の高さ
* stride		: 画像データ一行分のバイト数
* channel		: 画像のチャンネル数
* interlace		: インターレースの有無
* compression	: 圧縮レベル
*/
EXTERN void WritePNGStream(
	void* stream,
	stream_func_t write_func,
	void (*flush_func)(void*),
	uint8* pixels,
	int32 width,
	int32 height,
	int32 stride,
	uint8 channel,
	int32 interlace,
	int32 compression
);

extern uint8* ReadPNGDetailData(
	void* stream,
	stream_func_t read_func,
	int32* width,
	int32* height,
	int32* stride,
	int32* dpi,
	char** icc_profile_name,
	uint8** icc_profile_data,
	uint32* icc_profile_size
);

#ifdef __cplusplus
}
#endif

#endif	/* #ifndef _INCLUDED_PNG_FILE_H_ */
