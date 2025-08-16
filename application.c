#include "application.h"
#include "ini_file.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
* GetActiveDrawWindow関数
* アクティブな描画領域を取得する
* 引数
* app		: アプリケーションを管理する構造体のアドレス
* 返り値
*	アクティブな描画領域
*/
DRAW_WINDOW* GetActiveDrawWindow(APPLICATION* app)
{
	DRAW_WINDOW *ret = app->draw_window[app->active_window];
	if(ret == NULL)
	{
		return NULL;
	}

	if(ret->focal_window != NULL)
	{
		return ret->focal_window;
	}
	return ret;
}

/*
* ReadInitializeFile関数
* 初期化ファイルを読み込む
* 引数
* app		: アプリケーションを管理する構造体のアドレス
* file_path	: 初期化ファイルのパス
* 返り値
*	正常終了:0	失敗:負の値
*/
int ReadInitializeFile(APPLICATION* app, const char* file_path)
{
	INI_FILE_PTR file;
	FILE *fp;
	size_t file_size;
	
	if((fp = fopen(file_path, "rb")) == NULL)
	{
		return -1;
	}
	
	(void)fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	rewind(fp);
	file = CreateIniFile((void*)fp, (size_t (*)(void*, size_t, size_t, void*))fread, file_size, INI_READ);
	if(file == NULL)
	{
		return -2;
	}
	
	app->brush_file_path = IniFileStrdup(file, "BRUSH_DATA", "PATH");
	
	file->delete_func(file);
	(void)fclose(fp);
	
	return 0;
}

#ifdef __cplusplus
}
#endif
