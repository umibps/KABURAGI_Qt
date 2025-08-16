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
* WriteCanvasToImageFiel�֐�
* �L�����o�X���g���q�ɉ����ăt�@�C���ɏ������݂��s��
* ����
* app				: �������ݐi����\������ׁA�A�v���P�[�V�����Ǘ��p�f�[�^��n��
* canvas			: �������ރL�����o�X
* file_path			: �������ރt�@�C���̃p�X
* compression_level	: ZIP���k���x��
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
* ReadImageFile�֐�
* �摜�t�@�C����ǂݍ��݁A�L�����o�X���쐬���ăt�@�C�����e�𔽉f����
* ����
* app		: �i����\������ׁA�A�v���P�[�V�����Ǘ��f�[�^��n��
* file_path	: �ǂݍ��ރt�@�C���p�X
* �Ԃ�l
*	�쐬&�t�@�C�����e�𔽉f�����L�����o�X�B���s����NULL
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
