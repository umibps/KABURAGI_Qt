#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QDockWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushBUtton>
#include "../../application.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = 0, APPLICATION* app = NULL);
	~MainWindow();
	void CreateNormalMenuBar(void);
	void MakeToolBar(QHBoxLayout* tool_bar_layout);

	QTabWidget* GetCanvasTabWidget(void);

private:
	APPLICATION *app;

	QWidget main_box_widget;
	QVBoxLayout main_box_layout;
	QSplitter main_splitter;
	QSplitter right_splitter;
	QTabWidget canvas_tab;

	// ツールバー
	QWidget tool_bar_widget;
	QHBoxLayout tool_bar_layout;
	QPushButton undo_button;
	QPushButton redo_button;

	// 通常メニュー
	QMenu *file_menu;
	QAction *new_action;
	QAction *open_action;
	QAction *save_action;
	QAction *save_as_action;
	QMenu *edit_menu;
	QAction *undo_action;
	QAction *redo_action;

private slots:
	void new_canvas(void);
	void open_file(void);
	void save(void);
	void save_as(void);
	void undo(void);
	void redo(void);
};

class ToolBoxWidget;
class LayerWindowWidget;
class NavigationWindowWidget;

typedef struct _MAIN_WINDOW_WIDGETS
{
	MainWindow *main_window;
	ToolBoxWidget *tool_box;
	LayerWindowWidget *layer_window;
	NavigationWindowWidget *navigation;
} MAIN_WINDOW_WIDGETS;

#endif // MAINWINDOW_H
