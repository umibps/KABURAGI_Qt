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
* ReadOriginalFormat�֐�
* �Ǝ��`���̃f�[�^��ǂݍ���
* ����
* stream		: �ǂݍ��݌��f�[�^�X�g���[��
* read_function	: �ǂݍ��ݗp�֐�
* stream_size	: �f�[�^�̑��o�C�g��
* app			: �ǂݍ��ݐi���\��&�L�����o�X�쐬�ׁ̈A�A�v���P�[�V�����Ǘ��f�[�^��n��
* data_name		: �t�@�C����
* �Ԃ�l
*	�ǂݍ��񂾃f�[�^��K�p�����L�����o�X�B���s������NULL
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
