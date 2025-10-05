#include <QStringConverter>
#include "../../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
* FromUTF8�֐�
* UTF-8�̕�������w�肳�ꂽ�R�[�f�B���O�̕�����ɕϊ�����
* ����
* text			: UTF-8�̕�����
* text_coding	: �ϊ���̃R�[�f�B���O
* �Ԃ�l
*	�ϊ���̕����� (�g�p���MEM_FREE_FUNC�K�v)
*/
char* FromUTF8(const char* text, const char* text_coding)
{
	QStringEncoder encoder(text_coding);
	QString utf8_string(text);

	QByteArray data = encoder.encode(utf8_string);

	return MEM_STRDUP_FUNC(data.data());
}

/*
* ToUTF8�֐�
* UTF-8�̕�����֎w�肳�ꂽ�R�[�f�B���O�̕����񂩂�ϊ�����
* ����
* text			: �ϊ����̕�����
* text_coding	: �ϊ����̃R�[�f�B���O
*/
char* ToUTF8(const char* text, const char* text_coding)
{
	QByteArray data(text);
	QStringDecoder decoder(text_coding);
	QString utf8_string = decoder.decode(data);

	return MEM_STRDUP_FUNC(utf8_string.toUtf8().data());
}

#ifdef __cplusplus
}
#endif
