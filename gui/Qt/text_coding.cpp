#include <QStringConverter>
#include "../../memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
* FromUTF8関数
* UTF-8の文字列を指定されたコーディングの文字列に変換する
* 引数
* text			: UTF-8の文字列
* text_coding	: 変換後のコーディング
* 返り値
*	変換後の文字列 (使用後にMEM_FREE_FUNC必要)
*/
char* FromUTF8(const char* text, const char* text_coding)
{
	QStringEncoder encoder(text_coding);
	QString utf8_string(text);

	QByteArray data = encoder.encode(utf8_string);

	return MEM_STRDUP_FUNC(data.data());
}

/*
* ToUTF8関数
* UTF-8の文字列へ指定されたコーディングの文字列から変換する
* 引数
* text			: 変換元の文字列
* text_coding	: 変換元のコーディング
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
