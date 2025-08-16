#ifndef _INCLUDED_IMAGE_FILE_H_
#define _INCLUDED_IMAGE_FILE_H_

#include "../types.h"

/*
* ReadImageFile�֐�
* �摜�t�@�C����ǂݍ��݁A�L�����o�X���쐬���ăt�@�C�����e�𔽉f����
* ����
* app		: �i����\������ׁA�A�v���P�[�V�����Ǘ��f�[�^��n��
* file_path	: �ǂݍ��ރt�@�C���p�X
* �Ԃ�l
*	�쐬&�t�@�C�����e�𔽉f�����L�����o�X�B���s����NULL
*/
EXTERN DRAW_WINDOW* ReadImageFile(APPLICATION* app, char* file_path);

/*
* WriteCanvasToImageFiel�֐�
* �L�����o�X���g���q�ɉ����ăt�@�C���ɏ������݂��s��
* ����
* app				: �������ݐi����\������ׁA�A�v���P�[�V�����Ǘ��p�f�[�^��n��
* canvas			: �������ރL�����o�X
* file_path			: �������ރt�@�C���̃p�X
* compression_level	: ZIP���k���x��
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