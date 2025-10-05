#ifndef _INCLUDED_DRAW_WINDOW_QT_H_
#define _INCLUDED_DRAW_WINDOW_QT_H_

#include <QGraphicsView>
#include <QImage>
#include <QPixmap>
#include <QWidget>
#include <QTableWidget>
#include <QScrollArea>
#include <QTimer>
#include "../../draw_window.h"

class CanvasWidget;

typedef struct _DRAW_WINDOW_WIDGETS
{
	CanvasWidget *window;
	QImage *display_image;
} DRAW_WINDOW_WIDGETS;

class CanvasMainWidget : public QWidget
{
	Q_OBJECT
public:
	CanvasMainWidget(QWidget* parent = NULL, DRAW_WINDOW* canvas = NULL);
	~CanvasMainWidget();

	void colorPickerPopupMenuClicked(int row, int column);
	DRAW_WINDOW* canvas_data();
	
protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void tabletEvent(QTabletEvent* event) override;

	void enterEvent(QEnterEvent* event) override;
	void leaveEvent(QEvent* event) override;
	
	void timeoutEvent();

private slots:
	void updateCanvas();
	void changeCurrentDevice(bool is_stylus);

	bool eventFilter(QObject* object, QEvent* event);

private:
	QWidget canvas_widget;
	QTimer update_timer;
	DRAW_WINDOW *canvas;
	bool stylus_device;

	enum
	{
		DEFAULT,
		COLOR_HISTORY
	} pop_up_menu_mode;
};

class CanvasWidget : public QScrollArea
{
public:
	CanvasWidget(QWidget* parent = NULL, DRAW_WINDOW* canvas = NULL);
	~CanvasWidget();
	CanvasMainWidget* canvas_widget();

private:
	CanvasMainWidget main_widget;
};

#ifdef __cplusplus
extern "C" {
#endif

extern void SetColorPickerHistoryPopupMenuCallback(QTableWidget* table, void* canvas);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_DRAW_WINDOW_QT_H_
