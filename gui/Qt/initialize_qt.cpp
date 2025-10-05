#include <string.h>
#include <QApplication>
#include <QTranslator>
#include <QSplashScreen>
#include "mainwindow.h"
#include "qt_widgets.h"
#include "../../memory.h"
#include "tool_box_qt.h"
#include "brush_button_qt.h"

/*
* InitializeApplication関数
* アプリケーションの初期化
* 引数
* app				: アプリケーション全体を管理する構造体のアドレス
* argv				: main関数の第一引数
* argc				: main関数の第二引数
* init_file_name	: 初期化ファイルの名前
*/
void InitializeApplication(APPLICATION* app, char** argv, int argc, char* init_file_name)
{
#define INITIALIZE_FILE_NAME "application.ini"
	Application *application = new Application(argc, argv);
	QTranslator translator;
	QPixmap pixmap("./image/splash.png");
	QSplashScreen splash(pixmap);
	splash.show();

	translator.load("KABURAGI");
	application->installTranslator(&translator);
	
	(void)ReadInitializeFile(app, INITIALIZE_FILE_NAME);
	
	SetLayerBlendFunctionsArray(app->layer_blend_functions);
	SetPartLayerBlendFunctionsArray(app->part_layer_blend_functions);

	InitializeGraphics(&app->graphics);

	LoadLabels(app->labels, app->fractal_labels, NULL);

	app->widgets = CreateMainWindowWidgets(app);

	app->widgets->main_window = new MainWindow(NULL, app);
	app->widgets->main_window->CreateNormalMenuBar();
	app->widgets->main_window->show();
	
	app->tool_box.active_common_tool = app->tool_box.common_tools[0][0];
	app->tool_box.active_brush[0] = app->tool_box.brushes[0][0];
	app->tool_box.active_brush[1] = app->tool_box.brushes[0][0];
	app->tool_box.active_vector_brush[0] = app->tool_box.vector_brushes[0][0];
	app->tool_box.active_vector_brush[1] = app->tool_box.vector_brushes[0][0];

	(void)ReadCommonToolInitializeFile(app, app->common_tool_file_path);
	//(void)ReadBrushInitializeFile(app, app->brush_file_path);
	(void)ReadBrushInitializeFile(app, BRUSH_INITIALIZE_FILE_PATH);

	SetDefaultColorPickerCallbacks(app);

	app->widgets->tool_box->createCommonToolButtonTable();
	app->widgets->tool_box->createBrushButtonTable();

	QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents);

	if(app->tool_box.active_common_tool == NULL)
	{
		app->tool_box.active_common_tool = app->tool_box.common_tools[0][0];
	}

	if(app->tool_box.active_brush[INPUT_PEN] == NULL)
	{
		app->tool_box.active_brush[INPUT_PEN] = app->tool_box.brushes[0][0];
	}
	if(app->tool_box.active_brush[INPUT_ERASER] == NULL)
	{
		app->tool_box.active_brush[INPUT_ERASER] = app->tool_box.brushes[0][1];
		if(app->tool_box.active_brush[INPUT_ERASER] == NULL)
		{
			app->tool_box.active_brush[INPUT_ERASER] = app->tool_box.brushes[0][0];
		}
	}

	if(app->tool_box.active_vector_brush[INPUT_PEN] == NULL)
	{
		app->tool_box.active_vector_brush[INPUT_PEN] = app->tool_box.vector_brushes[0][0];
	}
	if(app->tool_box.active_vector_brush[INPUT_ERASER] == NULL)
	{
		app->tool_box.active_vector_brush[INPUT_ERASER] = app->tool_box.vector_brushes[0][1];
		if(app->tool_box.active_vector_brush[INPUT_ERASER] == NULL)
		{
			app->tool_box.active_vector_brush[INPUT_ERASER] = app->tool_box.vector_brushes[0][0];
		}
	}
	
	if(app->tool_box.motion_queue.max_items < MINIMUM_MOTION_QUEUE_BUFFER_SIZE)
	{
		app->tool_box.motion_queue.max_items = MINIMUM_MOTION_QUEUE_BUFFER_SIZE;
	}
	app->tool_box.motion_queue.queue =
		(MOTION_QUEUE_ITEM*)MEM_ALLOC_FUNC(sizeof(*app->tool_box.motion_queue.queue) * app->tool_box.motion_queue.max_items);
	(void)memset(app->tool_box.motion_queue.queue, 0, sizeof(*app->tool_box.motion_queue.queue) * app->tool_box.motion_queue.max_items);

	BrushButton *button =
		(BrushButton*)app->tool_box.active_brush[INPUT_PEN]->button;
	button->setChecked(true);
	app->tool_box.active_brush[INPUT_PEN]->create_detail_ui(app, app->tool_box.active_brush[INPUT_PEN]);
	app->tool_box.flags |= TOOL_USING_BRUSH;

	application->exec();
}

MAIN_WINDOW_WIDGETS_PTR CreateMainWindowWidgets(APPLICATION* app)
{
	MAIN_WINDOW_WIDGETS_PTR ret = (MAIN_WINDOW_WIDGETS_PTR)MEM_ALLOC_FUNC(sizeof(*ret));

	app->layer_view = (LAYER_VIEW_WIDGETS*)MEM_ALLOC_FUNC(sizeof(LAYER_VIEW_WIDGETS));

	(void)memset(ret, 0, sizeof(*ret));

	return ret;
}
