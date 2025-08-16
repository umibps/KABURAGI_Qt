#include <png.h>
#include "../types.h"
#include "../memory.h"
#include "../memory_stream.h"
#include "png_file.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
* PNG_IO構造体
* pnglibにストリームのアドレスと読み書き関数ポインタ渡す用
*/
typedef struct _PNG_IO
{
	void* data;
	stream_func_t rw_func;
	void (*flush_func)(void* stream);
} PNG_IO;

/*
* PngReadWrite関数
* pnglibの要求に応じて読み書き用の関数を呼び出す
* 引数
* png_p		: pnglibの圧縮・展開管理構造体アドレス
* data		: 読み書き先のアドレス
* length	: 読み書きするバイト数
*/
static void PngReadWrite(
	png_structp png_p,
	png_bytep data,
	png_size_t length
)
{
	PNG_IO *io = (PNG_IO*)png_get_io_ptr(png_p);
	(void)io->rw_func(data, 1, length, io->data);
}

/*
* PngFlush関数
* ストリームに残ったデータをクリア
* 引数
* png_p		: pnglibの圧縮・展開管理構造体アドレス
*/
static void PngFlush(png_structp png_p)
{
	PNG_IO* io = (PNG_IO*)png_get_io_ptr(png_p);
	if(io->flush_func != NULL)
	{
		io->flush_func(io->data);
	}
}

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
uint8* ReadPNGStream(
	void* stream,
	stream_func_t read_func,
	int32* width,
	int32* height,
	int32* stride
)
{
	PNG_IO io;
	png_structp png_p;
	png_infop info_p;
	uint32 local_width, local_height, local_stride;
	int32 bit_depth, color_type, interlace_type;
	uint8 *pixels;
	uint8 **image;
	uint32 i;

	// ストリームのアドレスと関数ポインタをセット
	io.data = stream;
	io.rw_func = read_func;

	// PNG展開用のデータを生成
	png_p = png_create_read_struct(
		PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL
	);

	// 画像情報を格納するデータ領域作成
	info_p = png_create_info_struct(png_p);

	if(setjmp(png_jmpbuf(png_p)) != 0)
	{
		png_destroy_read_struct(&png_p, &info_p, NULL);

		return NULL;
	}

	// 読み込み用のストリーム、関数ポインタをセット
	png_set_read_fn(png_p, (void*)&io, PngReadWrite);

	// 画像情報の読み込み
	png_read_info(png_p, info_p);
	png_get_IHDR(png_p, info_p, &local_width, &local_height,
		&bit_depth, &color_type, &interlace_type, NULL, NULL
	);
	local_stride = (uint32)png_get_rowbytes(png_p, info_p);

	// ピクセルメモリの確保
	pixels = (uint8*)MEM_ALLOC_FUNC(local_stride*local_height);
	// pnglibでは2次元配列にする必要があるので
		// 高さ分のポインタ配列を作成
	image = (uint8**)MEM_ALLOC_FUNC(sizeof(*image)*local_height);

	// 2次元配列になるようアドレスをセット
	for(i=0; i<local_height; i++)
	{
		image[i] = &pixels[local_stride*i];
	}
	// 画像データの読み込み
	png_read_image(png_p, image);

	// メモリの開放
	png_destroy_read_struct(&png_p, &info_p, NULL);
	MEM_FREE_FUNC(image);

	// 画像データを指定アドレスにセット
	*width = (int32)local_width;
	*height = (int32)local_height;
	*stride = (int32)local_stride;

	return pixels;
}

/*
* ReadPNGHeader関数
* PNGイメージのヘッダ情報を読み込む
* 引数
* stream	: データストリーム
* read_func	: 読み込み用の関数ポインタ
* width		: イメージの幅を格納するアドレス
* height	: イメージの高さを格納するアドレス
* stride	: イメージの一行分のバイト数を格納するアドレス
* dpi		: イメージの解像度(DPI)を格納するアドレス
* icc_profile_name	: ICCプロファイルの名前を受けるポインタ
* icc_profile_data	: ICCプロファイルのデータを受けるポインタ
* icc_profile_size	: ICCプロファイルのデータのバイト数を格納するアドレス
*/
void ReadPNGHeader(
	void* stream,
	stream_func_t read_func,
	int32* width,
	int32* height,
	int32* stride,
	int32* dpi,
	char** icc_profile_name,
	uint8** icc_profile_data,
	uint32* icc_profile_size
)
{
	PNG_IO io;
	png_structp png_p;
	png_infop info_p;
	uint32 local_width, local_height, local_stride;
	int32 bit_depth, color_type, interlace_type;

	// ストリームのアドレスと関数ポインタをセット
	io.data = stream;
	io.rw_func = read_func;

	// PNG展開用のデータを生成
	png_p = png_create_read_struct(
		PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL
	);

	// 画像情報を格納するデータ領域作成
	info_p = png_create_info_struct(png_p);

	if(setjmp(png_jmpbuf(png_p)) != 0)
	{
		png_destroy_read_struct(&png_p, &info_p, NULL);

		return;
	}

	// 読み込み用のストリーム、関数ポインタをセット
	png_set_read_fn(png_p, (void*)&io, PngReadWrite);

	// 画像情報の読み込み
	png_read_info(png_p, info_p);
	png_get_IHDR(png_p, info_p, &local_width, &local_height,
		&bit_depth, &color_type, &interlace_type, NULL, NULL
	);
	local_stride = (uint32)png_get_rowbytes(png_p, info_p);

	// 解像度の取得
	if(dpi != NULL)
	{
		png_uint_32 res_x, res_y;
		int unit_type;

		if(png_get_pHYs(png_p, info_p, &res_x, &res_y, &unit_type) == PNG_INFO_pHYs)
		{
			if(unit_type == PNG_RESOLUTION_METER)
			{
				if(res_x > res_y)
				{
					*dpi = (int32)(res_x * 0.0254 + 0.5);
				}
				else
				{
					*dpi = (int32)(res_y * 0.0254 + 0.5);
				}
			}
		}
	}

	// ICCプロファイルの取得
	if(icc_profile_data != NULL && icc_profile_size != NULL)
	{
		png_charp name;
		png_charp profile;
		png_uint_32 profile_size;
		int compression_type;

		*icc_profile_data = NULL;
		*icc_profile_size = 0;

		if(png_get_iCCP(png_p, info_p, &name, &compression_type,
			&profile, &profile_size) == PNG_INFO_iCCP)
		{
			if(name != NULL && icc_profile_name != NULL)
			{
				*icc_profile_name = MEM_STRDUP_FUNC(name);
			}

			*icc_profile_data = (uint8*)MEM_ALLOC_FUNC(profile_size);
			(void)memcpy(*icc_profile_data, profile, profile_size);
			*icc_profile_size = profile_size;
		}
	}

	// メモリの開放
	png_destroy_read_struct(&png_p, &info_p, NULL);

	// 画像データを指定アドレスにセット
	if(width != NULL)
	{
		*width = (int32)local_width;
	}
	if(height != NULL)
	{
		*height = (int32)local_height;
	}
	if(stride != NULL)
	{
		*stride = (int32)local_stride;
	}
}

/*
* ReadPNGDetailData関数
* PNGイメージデータを詳細情報付きで読み込む
* 引数
* stream	: データストリーム
* read_func	: 読み込み用の関数ポインタ
* width		: イメージの幅を格納するアドレス
* height	: イメージの高さを格納するアドレス
* stride	: イメージの一行分のバイト数を格納するアドレス
* dpi		: イメージの解像度(DPI)を格納するアドレス
* icc_profile_name	: ICCプロファイルの名前を受けるポインタ
* icc_profile_data	: ICCプロファイルのデータを受けるポインタ
* icc_profile_size	: ICCプロファイルのデータのバイト数を格納するアドレス
* 返り値
*	ピクセルデータ
*/
uint8* ReadPNGDetailData(
	void* stream,
	stream_func_t read_func,
	int32* width,
	int32* height,
	int32* stride,
	int32* dpi,
	char** icc_profile_name,
	uint8** icc_profile_data,
	uint32* icc_profile_size
)
{
	PNG_IO io;
	png_structp png_p;
	png_infop info_p;
	uint32 local_width, local_height, local_stride;
	int32 bit_depth, color_type, interlace_type;
	uint8* pixels;
	uint8** image;
	uint32 i;

	// ストリームのアドレスと関数ポインタをセット
	io.data = stream;
	io.rw_func = read_func;

	// PNG展開用のデータを生成
	png_p = png_create_read_struct(
		PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL
	);

	// 画像情報を格納するデータ領域作成
	info_p = png_create_info_struct(png_p);

	if(setjmp(png_jmpbuf(png_p)) != 0)
	{
		png_destroy_read_struct(&png_p, &info_p, NULL);

		return NULL;
	}

	// 読み込み用のストリーム、関数ポインタをセット
	png_set_read_fn(png_p, (void*)&io, PngReadWrite);

	// 画像情報の読み込み
	png_read_info(png_p, info_p);
	png_get_IHDR(png_p, info_p, &local_width, &local_height,
		&bit_depth, &color_type, &interlace_type, NULL, NULL
	);
	local_stride = (uint32)png_get_rowbytes(png_p, info_p);

	// 解像度の取得
	if(dpi != NULL)
	{
		png_uint_32 res_x, res_y;
		int unit_type;

		if(png_get_pHYs(png_p, info_p, &res_x, &res_y, &unit_type) == PNG_INFO_pHYs)
		{
			if(unit_type == PNG_RESOLUTION_METER)
			{
				if(res_x > res_y)
				{
					*dpi = (int32)(res_x * 0.0254 + 0.5);
				}
				else
				{
					*dpi = (int32)(res_y * 0.0254 + 0.5);
				}
			}
		}
	}

	// ICCプロファイルの取得
	if(icc_profile_data != NULL && icc_profile_size != NULL)
	{
		png_charp name;
		png_charp profile;
		png_uint_32 profile_size;
		int compression_type;

		if(png_get_iCCP(png_p, info_p, &name, &compression_type,
			&profile, &profile_size) == PNG_INFO_iCCP)
		{
			if(name != NULL)
			{
				*icc_profile_name = MEM_STRDUP_FUNC(name);
			}

			*icc_profile_data = (uint8*)MEM_ALLOC_FUNC(profile_size);
			(void)memcpy(*icc_profile_data, profile, profile_size);
			*icc_profile_size = profile_size;
		}
	}

	// ピクセルメモリの確保
	pixels = (uint8*)MEM_ALLOC_FUNC(local_stride*local_height);
	// pnglibでは2次元配列にする必要があるので
		// 高さ分のポインタ配列を作成
	image = (uint8**)MEM_ALLOC_FUNC(sizeof(*image)*local_height);

	// 2次元配列になるようアドレスをセット
	for(i=0; i<local_height; i++)
	{
		image[i] = &pixels[local_stride*i];
	}
	// 画像データの読み込み
	png_read_image(png_p, image);

	// メモリの開放
	png_destroy_read_struct(&png_p, &info_p, NULL);
	MEM_FREE_FUNC(image);

	// 画像データを指定アドレスにセット
	*width = (int32)local_width;
	*height = (int32)local_height;
	*stride = (int32)local_stride;

	return pixels;
}

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
void WritePNGStream(
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
)
{
	PNG_IO io = {stream, write_func, flush_func};
	png_structp png_p;
	png_infop info_p;
	uint8** image;
	int color_type;
	int i;

	// チャンネル数に合わせてカラータイプを設定
	switch(channel)
	{
	case 1:
		color_type = PNG_COLOR_TYPE_GRAY;
		break;
	case 2:
		color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
		break;
	case 3:
		color_type = PNG_COLOR_TYPE_RGB;
		break;
	case 4:
		color_type = PNG_COLOR_TYPE_RGBA;
		break;
	default:
		return;
	}

	// PNG書き込み用のデータを生成
	png_p = png_create_write_struct(
		PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL
	);

	// 画像情報格納用のメモリを確保
	info_p = png_create_info_struct(png_p);

	// 書き込み用のストリームと関数ポインタをセット
	png_set_write_fn(png_p, &io, PngReadWrite, PngFlush);
	// 圧縮には全てのフィルタを使用
	png_set_filter(png_p, 0, PNG_ALL_FILTERS);
	// 圧縮レベルを設定
	png_set_compression_level(png_p, compression);

	// PNGの情報をセット
	png_set_IHDR(png_p, info_p, width, height, 8, color_type,
		interlace, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	// pnglib用に2次元配列を作成
	image = (uint8**)MEM_ALLOC_FUNC(height*sizeof(*image));
	for(i=0; i<height; i++)
	{
		image[i] = &pixels[stride*i];
	}

	// 画像データの書き込み
	png_write_info(png_p, info_p);
	png_write_image(png_p, image);
	png_write_end(png_p, info_p);

	// メモリの開放
	png_destroy_write_struct(&png_p, &info_p);

	MEM_FREE_FUNC(image);
}

#ifdef __cplusplus
}
#endif
