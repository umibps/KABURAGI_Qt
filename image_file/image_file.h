#ifndef _INCLUDED_IMAGE_FILE_H_
#define _INCLUDED_IMAGE_FILE_H_

#include "../types.h"

/*
* ReadImageFile関数
* 画像ファイルを読み込み、キャンバスを作成してファイル内容を反映する
* 引数
* app		: 進捗を表示する為、アプリケーション管理データを渡す
* file_path	: 読み込むファイルパス
* 返り値
*	作成&ファイル内容を反映したキャンバス。失敗時はNULL
*/
EXTERN DRAW_WINDOW* ReadImageFile(APPLICATION* app, char* file_path);

/*
* WriteCanvasToImageFiel関数
* キャンバスを拡張子に応じてファイルに書き込みを行う
* 引数
* app				: 書き込み進捗を表示する為、アプリケーション管理用データを渡す
* canvas			: 書き込むキャンバス
* file_path			: 書き込むファイルのパス
* compression_level	: ZIP圧縮レベル
*/
EXTERN void WriteCanvasToImageFile(
	APPLICATION* app,
	DRAW_WINDOW* canvas,
	char* file_path,
	int compression_level
);

EXTERN void RGBpixels_to_RGBApixels(uint8* original, uint8* result, int width, int height, int stride);
EXTERN void GrapyPixels_to_RGBApixels(uint8* original, uint8* result, int width, int height, int stride);

#endif	/* #ifndef _INCLUDED_IMAGE_FILE_H_ */