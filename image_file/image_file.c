#include <string.h>
#include "original_format.h"
#include "png_file.h"
#include "../application.h"
#include "../draw_window.h"
#include "../utils.h"
#include "../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

void RGBpixels_to_RGBApixels(uint8* original, uint8* result, int width, int height, int stride)
{
	uint8 *src, *dst;
	int dst_stride = width * 4;
	int x, y;

	for(y=0; y<height; y++)
	{
		for(x = 0, src = &original[y * stride], dst = &result[y * dst_stride]; x < width; src += 3, dst += 4)
		{
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = 0xFF;
		}
	}
}

void GrapyPixels_to_RGBApixels(uint8* original, uint8* result, int width, int height, int stride)
{
	uint8 *src, *dst;
	int dst_stride = width * 4;
	int x, y;

	for(y = 0; y < height; y++)
	{
		for(x = 0, src = &original[y * stride], dst = &result[y * dst_stride]; x < width; src ++, dst += 4)
		{
			dst[0] = src[0];
			dst[1] = src[0];
			dst[2] = src[0];
			dst[3] = 0xFF;
		}
	}
}

/*
* WriteCanvasToImageFiel関数
* キャンバスを拡張子に応じてファイルに書き込みを行う
* 引数
* app				: 書き込み進捗を表示する為、アプリケーション管理用データを渡す
* canvas			: 書き込むキャンバス
* file_path			: 書き込むファイルのパス
* compression_level	: ZIP圧縮レベル
*/
void WriteCanvasToImageFile(
	APPLICATION* app,
	DRAW_WINDOW* canvas,
	char* file_path,
	int compression_level
)
{
	char *file_extention;

	file_extention = GetFileExtention(file_path);
	if(StringCompareIgnoreCase(file_extention, ".kab") == 0)
	{
		FILE* fp = fopen(file_path, "wb");
		if(fp != NULL)
		{
			StoreCanvas((void*)fp, (stream_func_t)fwrite, canvas, TRUE, compression_level);
			(void)fclose(fp);
			MEM_FREE_FUNC(canvas->file_path);
			canvas->file_path = MEM_STRDUP_FUNC(file_path);
		}
	}
	else if(StringCompareIgnoreCase(file_extention, ".png") == 0)
	{
		FILE* fp = fopen(file_path, "wb");
		if(fp != NULL)
		{
			WritePNGStream((void*)fp, (stream_func_t)fwrite, (void (*)(void*))fflush,
				canvas->mixed_layer->pixels, canvas->width, canvas->height, canvas->stride, 4,
				FALSE, compression_level);
			(void)fclose(fp);
			MEM_FREE_FUNC(canvas->file_path);
			canvas->file_path = MEM_STRDUP_FUNC(file_path);
		}
	}
}

/*
* ReadImageFile関数
* 画像ファイルを読み込み、キャンバスを作成してファイル内容を反映する
* 引数
* app		: 進捗を表示する為、アプリケーション管理データを渡す
* file_path	: 読み込むファイルパス
* 返り値
*	作成&ファイル内容を反映したキャンバス。失敗時はNULL
*/
DRAW_WINDOW* ReadImageFile(APPLICATION* app, char* file_path)
{
	DRAW_WINDOW *canvas = NULL;
	FILE *fp;
	char *file_extention;

	if(file_path == NULL)
	{
		return NULL;
	}

	fp = fopen(file_path, "rb");

	file_extention = GetFileExtention(file_path);
	if(StringCompareIgnoreCase(file_extention, ".kab") == 0)
	{
		char *file_name = GetFileName(file_path);
		size_t file_size;
		(void)fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		rewind(fp);
		canvas = ReadOriginalFormat((void*)fp, (stream_func_t)fread, file_size, app, file_name);
	}
	else if(StringCompareIgnoreCase(file_extention, ".png") == 0)
	{
		int32 width, height, stride;
		uint8 *pixels;
		int channel;

		pixels = ReadPNGStream((void*)fp, (stream_func_t)fread, &width, &height, &stride);
		channel = stride / width;
		if(channel == 1)
		{
			uint8 *old_pixels = pixels;
			pixels = (uint8*)MEM_ALLOC_FUNC(width * 4 * height);
			GrapyPixels_to_RGBApixels(old_pixels, pixels, width, height, stride);
			MEM_FREE_FUNC(old_pixels);
		}
		else if(channel == 3)
		{
			uint8* old_pixels = pixels;
			pixels = (uint8*)MEM_ALLOC_FUNC(width * 4 * height);
			RGBpixels_to_RGBApixels(old_pixels, pixels, width, height, stride);
			MEM_FREE_FUNC(old_pixels);
		}
		canvas = CreateDrawWindow(width, height, 4, file_path, app->widgets, app->window_num, app);
		(void)memcpy(canvas->layer->pixels, pixels, canvas->pixel_buf_size);
		MEM_FREE_FUNC(pixels);
	}

	(void)fclose(fp);

	return canvas;
}

#ifdef __cplusplus
}
#endif
