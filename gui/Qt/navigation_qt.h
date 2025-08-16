#ifndef _INCLUDED_NAVIGATION_QT_H_
#define _INCLUDED_NAVIGATION_QT_H_

#include <QDockWidget>
#include <QVBoxLayout>
#include <QImage>
#include <QPushButton>
#include "../../types.h"
#include "qt_widgets.h"
#include "spin_scale_qt.h"

class NavigationViewWidget : public QWidget
{
	Q_OBJECT
public:
	NavigationViewWidget(QWidget* parent = NULL, APPLICATION* app = NULL);
	~NavigationViewWidget();

	void setDrawCanvasTransform(FLOAT_T zoom_x, FLOAT_T zoom_y, FLOAT_T angle,
								FLOAT_T trans_x, FLOAT_T trans_y);
	void setDrawCanvas(DRAW_WINDOW* canvas);
	void paintCanvas(void* paint_event);
	void paintScrollFrame(FLOAT_T x, FLOAT_T y, FLOAT_T width, FLOAT_T height,
							void* display_context);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

private:
	APPLICATION *app;
	QTransform transform;
	QImage *draw_canvas_image;
};

class NavigationWindowWidget : public QDockWidget
{
	Q_OBJECT
public:
	NavigationWindowWidget(QWidget* parent = NULL, APPLICATION* app = NULL);
	~NavigationWindowWidget();

	void setDrawCanvasTransform(FLOAT_T zoom_x, FLOAT_T zoom_y, FLOAT_T angle,
		FLOAT_T trans_x, FLOAT_T trans_y);
	void setDrawCanvas(DRAW_WINDOW* canvas);
	void paintCanvas(void* paint_event);
	void paintScrollFrame(FLOAT_T x, FLOAT_T y, FLOAT_T width, FLOAT_T height,
		void* display_context);

private:
	NavigationViewWidget view;
	QWidget vbox_widget;
	QVBoxLayout vbox_layout;
	SpinScale zoom_selector;
	SpinScale rotate_selector;
	QPushButton reset_zoom_button;
	QPushButton reset_rotate_button;
	APPLICATION *app;
};

struct _NAVIGATION_WIDGETS
{
	NavigationWindowWidget *navigation_window;
};

#endif	/* _INCLUDED_NAVIGATION_QT_H_ */
