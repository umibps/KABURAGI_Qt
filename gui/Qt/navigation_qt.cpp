#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QRectF>
#include <QPen>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QSizePolicy>
#include "../../draw_window.h"
#include "../../application.h"
#include "../../navigation.h"
#include "../../memory.h"
#include "navigation_qt.h"
#include "draw_window_qt.h"
#include "mainwindow.h"

NavigationViewWidget::NavigationViewWidget(QWidget* parent, APPLICATION* app)
{
	this->app = app;
	draw_canvas_image = NULL;
	QSizePolicy size_policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setSizePolicy(size_policy);
}

NavigationViewWidget::~NavigationViewWidget()
{
	app->navigation.widgets->navigation_window = NULL;
}

void NavigationViewWidget::paintEvent(QPaintEvent* event)
{
	DRAW_WINDOW* canvas = GetActiveDrawWindow(app);

	if(canvas == NULL)
	{
		return;
	}

	QPainter painter(this);
	QSize widget_size = this->size();
	QSize scroll_size = canvas->widgets->window->size();
	QScrollBar *horizontal_scroll = canvas->widgets->window->horizontalScrollBar();
	QScrollBar *vertical_scroll =  canvas->widgets->window->verticalScrollBar();
	int scroll_position_x = horizontal_scroll->value(),
		scroll_position_y = vertical_scroll->value();
	DiplayNavigation(&app->navigation, canvas, scroll_size.width(), scroll_size.height(),
			scroll_position_x, scroll_position_y, widget_size.width(), widget_size.height(), (void*) & painter);
}

void NavigationViewWidget::mousePressEvent(QMouseEvent* event)
{

}

void NavigationViewWidget::mouseMoveEvent(QMouseEvent* event)
{

}

void NavigationViewWidget::mouseReleaseEvent(QMouseEvent* event)
{

}

void NavigationViewWidget::setDrawCanvas(DRAW_WINDOW* canvas)
{
	delete draw_canvas_image;
	draw_canvas_image = new QImage(canvas->mixed_layer->pixels,
			canvas->mixed_layer->width, canvas->mixed_layer->height,  QImage::Format_ARGB32);
}

void NavigationViewWidget::paintCanvas(void* paint_event)
{
	QPainter painter(this);
	painter.setTransform(transform);
	QPoint draw_point(0, 0);
	painter.drawImage(draw_point, *draw_canvas_image);
}

void NavigationViewWidget::setDrawCanvasTransform(
	FLOAT_T zoom_x,
	FLOAT_T zoom_y,
	FLOAT_T angle,
	FLOAT_T trans_x,
	FLOAT_T trans_y
)
{
	transform.reset();
	transform.scale(zoom_x, zoom_y);
	transform.rotate(angle);
	transform.translate(trans_x, trans_y);
}

void NavigationViewWidget::paintScrollFrame(
	FLOAT_T x,
	FLOAT_T y,
	FLOAT_T width,
	FLOAT_T height,
	void* display_context
)
{
    QPainter painter(this);
	QSize widget_size = this->size();

	QPainterPath path;
	path.moveTo(0, 0);
	path.lineTo(0, widget_size.height());
	path.lineTo(widget_size.width(), widget_size.height());
	path.lineTo(widget_size.width(), 0);
	path.closeSubpath();
	path.moveTo(x, y);
	path.lineTo(x, y+height);
	path.lineTo(x+width, y+height);
	path.lineTo(x+width, y);
	path.closeSubpath();
	QColor gray_out_color(0, 0, 0, 128);
	painter.fillPath(path, gray_out_color);

#define NAVIGATION_FRAME_COLOR 255, 0, 0
#define NAVIGATIONI_LINE_WIDTH 3
	QPainterPath line_path;
	line_path.moveTo(x, y);
	line_path.lineTo(x, y+height);
	line_path.lineTo(x+width, y+height);
	line_path.lineTo(x+width, y);
	line_path.closeSubpath();
	QPen stroke_pen(QColor(NAVIGATION_FRAME_COLOR), NAVIGATIONI_LINE_WIDTH);
	painter.strokePath(line_path, stroke_pen);
}

NavigationWindowWidget::NavigationWindowWidget(QWidget* parent, APPLICATION* app)
	: QDockWidget(QWidget::tr("Navigation"), parent),
	  view(parent, app),
	vbox_widget(this),
	vbox_layout(&this->vbox_widget),
	zoom_selector(this),
	rotate_selector(this),
	reset_zoom_button(this),
	reset_rotate_button(this)
{
	this->app = app;

	zoom_selector.setCaption(tr("Canvas Zoom"));
	rotate_selector.setCaption(tr("Canvas Rotate"));

	zoom_selector.setMinimum(10);
	zoom_selector.setMaximum(400);
	zoom_selector.setValue(100);
	rotate_selector.setMinimum(-180);
	rotate_selector.setMaximum(180);

	vbox_widget.setLayout(&vbox_layout);

	vbox_layout.addWidget(&this->view);
	vbox_layout.addWidget(&this->zoom_selector);
	vbox_layout.addWidget(&this->rotate_selector);

	reset_zoom_button.setText(tr("Reset Zoom"));
	reset_rotate_button.setText(tr("Reset Rotate"));

	vbox_layout.addWidget(&this->reset_zoom_button);
	vbox_layout.addWidget(&this->reset_rotate_button);

	setWidget(&vbox_widget);
}

NavigationWindowWidget::~NavigationWindowWidget()
{

}

void NavigationWindowWidget::setDrawCanvasTransform(
	FLOAT_T zoom_x,
	FLOAT_T zoom_y,
	FLOAT_T angle,
	FLOAT_T trans_x,
	FLOAT_T trans_y
)
{
	view.setDrawCanvasTransform(zoom_x, zoom_y, angle, trans_x, trans_y);
}

void NavigationWindowWidget::setDrawCanvas(DRAW_WINDOW* canvas)
{
	view.setDrawCanvas(canvas);
}

void NavigationWindowWidget::paintCanvas(void* paint_event)
{
	view.paintCanvas(paint_event);
}

void NavigationWindowWidget::paintScrollFrame(
	FLOAT_T x,
	FLOAT_T y,
	FLOAT_T width,
	FLOAT_T height,
	void* display_context
)
{
	view.paintScrollFrame(x, y, width, height, display_context);
}

#ifdef __cplusplus
extern "C" {
#endif

NAVIGATION_WIDGETS* CreateNavigationWidgets(APPLICATION* app)
{
	NAVIGATION_WIDGETS *widgets = (NAVIGATION_WIDGETS*)MEM_ALLOC_FUNC(sizeof(*widgets));
	widgets->navigation_window = new NavigationWindowWidget(app->widgets->main_window, app);
	app->widgets->navigation = widgets->navigation_window;

	return widgets;
}

void NavigationSetDrawCanvasMatrix(
	NAVIGATION_WIDGETS* widgets,
	FLOAT_T zoom_x,
	FLOAT_T zoom_y,
	FLOAT_T angle,
	FLOAT_T trans_x,
	FLOAT_T trans_y
)
{
	widgets->navigation_window->setDrawCanvasTransform(
		zoom_x, zoom_y, angle, trans_x, trans_y);
}

void NavigationSetCanvasPattern(NAVIGATION_WIDGETS* widgets, DRAW_WINDOW* canvas)
{
	widgets->navigation_window->setDrawCanvas(canvas);
}

void NavigationDrawCanvas(NAVIGATION_WIDGETS* widgets, void* paint_event)
{
	widgets->navigation_window->paintCanvas(paint_event);
}

void NavigationDrawScrollFrame(
	NAVIGATION_WIDGETS* widgets,
	FLOAT_T x,
	FLOAT_T y,
	FLOAT_T width,
	FLOAT_T height,
	void* display_context
)
{
	widgets->navigation_window->paintScrollFrame(
		x, y, width, height, display_context
	);
}

void NavigationUpdate(NAVIGATION_WIDGETS* widgets)
{
	if(widgets->navigation_window != NULL)
	{
		widgets->navigation_window->update();
	}
}

#ifdef __cplusplus
}
#endif
