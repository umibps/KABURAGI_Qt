#include <string.h>
#include <QMenuBar>
#include <QPalette>
#include "mainwindow.h"
#include "tool_box_qt.h"
#include "layer_qt.h"
#include "qt_widgets.h"
#include "dialogs_qt.h"
#include "navigation_qt.h"
#include "../../application.h"
#include "../../navigation.h"
#include "../../memory.h"
#include "../../image_file/png_file.h"

MainWindow::MainWindow(QWidget* parent, APPLICATION* app)
	: QMainWindow(parent),
	  main_box_widget(parent),
	  main_box_layout(parent),
	  main_splitter(parent),
	  tool_bar_widget(parent),
	  tool_bar_layout(parent),
	  right_splitter(parent),
	  canvas_tab(parent),
	  file_menu(NULL),
	  new_action(NULL),
	  open_action(NULL),
	  save_action(NULL),
	  save_as_action(NULL),
	  edit_menu(NULL),
	  undo_action(NULL),
	  redo_action(NULL)
{
#define SPLITTER_WIDTH 4
	this->app = app;

	main_box_widget.setLayout(&main_box_layout);

	tool_bar_widget.setLayout(&tool_bar_layout);
	MakeToolBar(&tool_bar_layout);
	// 拡張を無効にしてウィンドウにツールバーを追加
	main_box_layout.addWidget(&tool_bar_widget, 0);

	main_splitter.setHandleWidth(SPLITTER_WIDTH);
	right_splitter.setHandleWidth(SPLITTER_WIDTH);

	this->app->widgets->tool_box = new ToolBoxWidget(parent, app);
	this->app->widgets->layer_window = new LayerWindowWidget(parent, app);
	this->app->layer_view->layer_view = this->app->widgets->layer_window;
	this->app->navigation.widgets = CreateNavigationWidgets(this->app);
	this->app->widgets->navigation = this->app->navigation.widgets->navigation_window;
	setCentralWidget(&main_box_widget);

	main_splitter.addWidget(this->app->widgets->tool_box);
	main_splitter.addWidget(&canvas_tab);
	canvas_tab.setTabsClosable(true);
	canvas_tab.setTabPosition(canvas_tab.South);

	// 拡張を有効にしてウィジェット追加
	main_box_layout.addWidget(&main_splitter, 1);
	
	QColor color = qApp->palette().color(QWidget::backgroundRole());
	this->setStyleSheet("background-color: rgb("
		+ QString::number(color.red()) + ", " + QString::number(color.green())
		+ QString::number(color.blue()) + ");");

	right_splitter.setOrientation(Qt::Vertical);
	main_splitter.addWidget(&right_splitter);

	right_splitter.addWidget(this->app->widgets->navigation);
	right_splitter.addWidget(this->app->widgets->layer_window);
}

MainWindow::~MainWindow()
{
	{
		delete this->app->widgets->tool_box;
	}
}

void MainWindow::CreateNormalMenuBar(void)
{
	file_menu = menuBar()->addMenu(tr("&File"));
	new_action = new QAction(tr("&New"), this);
	new_action->setShortcut(QKeySequence(tr("Ctrl+N")));
	(void)connect(new_action, &QAction::triggered, this, &MainWindow::new_canvas);
	file_menu->addAction(new_action);
	open_action = new QAction(tr("&Open"), this);
	open_action->setShortcut(QKeySequence(tr("Ctrl+O")));
	(void)connect(open_action, &QAction::triggered, this, &MainWindow::open_file);
	file_menu->addAction(open_action);
	save_action = new QAction(tr("&Save"), this);
	save_action->setShortcut(QKeySequence(tr("Ctrl+S")));
	(void)connect(save_action, &QAction::triggered, this, &MainWindow::save);
	file_menu->addAction(save_action);
	save_as_action = new QAction(tr("&Save as"), this);
	save_as_action->setShortcut(QKeySequence(tr("Ctrl+Shift+S")));
	(void)connect(save_as_action, &QAction::triggered, this, &MainWindow::save_as);
	file_menu->addAction(save_as_action);

	edit_menu = menuBar()->addMenu(tr("&Edit"));
	undo_action = new QAction(tr("&Undo"), this);
	undo_action->setShortcut(QKeySequence(tr("Ctrl+Z")));
	(void)connect(undo_action, &QAction::triggered, this, &MainWindow::undo);
	redo_action = new QAction(tr("&Redo"), this);
	redo_action->setShortcut(QKeySequence(tr("Ctrl+Y")));
	(void)connect(redo_action, &QAction::triggered, this, &MainWindow::redo);
}

void MainWindow::MakeToolBar(QHBoxLayout* tool_bar_layout)
{
	uint8 *pixels = NULL;
	int32 width, height, stride;
	FILE *fp;

	fp = fopen("./image/arrow.png", "rb");
	if(fp != NULL)
	{
		pixels = ReadPNGStream((void*)fp, (stream_func_t)fread, &width, &height, &stride);
		if(pixels != NULL)
		{
			undo_button.setIcon(QIcon(QPixmap::fromImage(
					QImage(pixels, width, height, QImage::Format_ARGB32))));
			uint8 *reverse_pixels = (uint8*)MEM_ALLOC_FUNC(height * stride);
			for(int i = 0; i < height; i++)
			{
				uint8 *src = &pixels[i*stride];
				uint8 *dst = &reverse_pixels[(i+1)*stride-4];
				for(int j = 0; j < width; j++, src += 4, dst -= 4)
				{
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					dst[3] = src[3];
				}
			}
			redo_button.setIcon(QIcon(QPixmap::fromImage(
					QImage(reverse_pixels, width, height, QImage::Format_ARGB32))));
		}
		(void)fclose(fp);
	}

	if(pixels == NULL)
	{
		undo_button.setText(tr("Undo"));
		redo_button.setText(tr("Redo"));
	}

	connect(&undo_button, &QPushButton::clicked, this, &MainWindow::undo);
	connect(&redo_button, &QPushButton::clicked, this, &MainWindow::redo);

	tool_bar_layout->addWidget(&undo_button);
	tool_bar_layout->addWidget(&redo_button);

	// 左詰め
	tool_bar_layout->addStretch(0);
}

void MainWindow::new_canvas(void)
{
	NewCanvasDialog dialog(this);
	int ret = dialog.exec();

	if(ret == QDialog::Accepted)
	{
		this->app->draw_window[this->app->window_num] = CreateDrawWindow(dialog.GetWidth(), dialog.GetHeight(), 4,
			this->app->labels->make_new.name, (void*)this->app->widgets, 0, this->app);
		DRAW_WINDOW *canvas = this->app->draw_window[this->app->window_num];
		this->app->window_num++;
		canvas->widgets = CreateDrawWindowWidgets(this->app, canvas);
		AddDrawWindow2MainWindow(this->app->widgets, canvas);
		this->app->widgets->layer_window->getLayerViewWidget()->setCurrentCanvas(canvas);
	}
}

void MainWindow::open_file(void)
{
	ExecuteOpen(app);
}

void MainWindow::save(void)
{
	ExecuteSave(app);
}

void MainWindow::save_as(void)
{
	ExecuteSaveAs(app);
}

void MainWindow::undo(void)
{
	ExecuteUndo(app);
}

void MainWindow::redo(void)
{
	ExecuteRedo(app);
}

QTabWidget* MainWindow::GetCanvasTabWidget(void)
{
	return &this->canvas_tab;
}

#ifdef __cplusplus
extern "C" {
#endif

void* GetMainWindowWidget(void* app)
{
	return (void*)((APPLICATION*)app)->widgets->main_window;
}

#ifdef __cplusplus
}
#endif
