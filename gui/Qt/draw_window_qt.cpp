#include <string.h>
#include <math.h>
#include <QObject>
#include <QPainter>
#include <QBrush>
#include <QPixmap>
#include <QTransform>
#include <QSizePolicy>
#include "../../draw_window.h"
#include "../../application.h"
#include "../../display.h"
#include "draw_window_qt.h"
#include "qt_widgets.h"
#include "mainwindow.h"
#include "../../color.h"
#include "../../memory.h"
#include "color_chooser_qt.h"

#ifdef __cplusplus
extern "C" {
#endif

void SetCanvasRotate(DRAW_WINDOW* canvas)
{
	QTransform *transform = (QTransform*)canvas->rotate;
	transform->reset();
	transform->translate(- canvas->disp_layer->width /2, - canvas->disp_layer->height / 2);
	transform->rotate(canvas->angle);
	transform->translate(canvas->disp_layer->width / 2 - canvas->trans_x,
							canvas->disp_layer->height / 2 - canvas->trans_y);
}

void InitializeCanvasContext(DRAW_WINDOW* canvas)
{
	QTransform *transform;

	transform = new QTransform();
	canvas->rotate = (void*)transform;
	SetCanvasRotate(canvas);
}

void ReleaseCanvasContext(DRAW_WINDOW* canvas)
{
	QTransform *transform = (QTransform*)canvas->rotate;
	delete transform;
	canvas->rotate = NULL;
}

void AddDrawWindow2MainWindow(MAIN_WINDOW_WIDGETS_PTR main_window, DRAW_WINDOW* canvas)
{
	QString name = QString::fromUtf8(canvas->file_name);
	main_window->main_window->GetCanvasTabWidget()->addTab(canvas->widgets->window, name);
}

DRAW_WINDOW_WIDGETS_PTR CreateDrawWindowWidgets(void* application, DRAW_WINDOW* canvas)
{
	APPLICATION *app = (APPLICATION*)application;
	DRAW_WINDOW_WIDGETS_PTR ret = (DRAW_WINDOW_WIDGETS_PTR)MEM_ALLOC_FUNC(sizeof(*ret));

	(void)memset(ret, 0, sizeof(*ret));

	ret->window = new CanvasWidget(app->widgets->main_window->GetCanvasTabWidget(), canvas);
	ret->display_image = new QImage(canvas->disp_layer->pixels, canvas->disp_layer->width,
									canvas->disp_layer->height, canvas->disp_layer->stride, QImage::Format_ARGB32);

	return ret;
}

void ReleaseDrawWindowWidget(DRAW_WINDOW_WIDGETS* widgets)
{
	delete widgets->display_image;
	delete widgets->window;

	MEM_FREE_FUNC(widgets);
}

void ResizeDrawWindowWidgets(DRAW_WINDOW* canvas)
{
	canvas->widgets->window->canvas_widget()->resize(canvas->disp_size, canvas->disp_size);
	delete canvas->widgets->display_image;
	canvas->widgets->display_image = new QImage(canvas->disp_layer->pixels, canvas->disp_layer->width,
		canvas->disp_layer->height, canvas->disp_layer->stride, QImage::Format_RGBA8888);
	canvas->widgets->window->canvas_widget()->setFixedSize(canvas->disp_size, canvas->disp_size);
	canvas->widgets->window->canvas_widget()->setMinimumSize(canvas->disp_size, canvas->disp_size);
}

CanvasMainWidget* CanvasWidget::canvas_widget()
{
	return &main_widget;
}

CanvasMainWidget::CanvasMainWidget(QWidget* parent, DRAW_WINDOW* canvas)
	: QWidget(parent),
	  update_timer(this)
{
	setAttribute(Qt::WA_StaticContents);

	unsigned int widget_size;

	pop_up_menu_mode = COLOR_HISTORY;
	stylus_device = false;

	if(canvas != NULL)
	{
		FLOAT_T width = canvas->width * canvas->zoom_rate;
		FLOAT_T height = canvas->height * canvas->zoom_rate;
		widget_size = (unsigned int)(2 * sqrt((width/2)*(width/2)+(height/2)*(height/2)) + 1);
		setFixedSize(widget_size, widget_size);
		setMinimumSize(widget_size, widget_size);
		setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
		
		setMouseTracking(true);

		update_timer.setSingleShot(false);
		update_timer.setInterval(1000/60);
		update_timer.setTimerType(Qt::CoarseTimer);
		connect(&update_timer, &QTimer::timeout, this, &CanvasMainWidget::timeoutEvent);
		update_timer.start();
	}

	this->canvas = canvas;
}

CanvasMainWidget::~CanvasMainWidget()
{

}

void CanvasMainWidget::timeoutEvent()
{
	if(canvas->focal_window == NULL)
	{
		APPLICATION *app = canvas->app;
		if(app->tool_box.motion_queue.num_items > 0)
		{
			ExecuteMotionQueue(canvas);
		}
	}
}

void CanvasMainWidget::updateCanvas()
{
	update();
}

void CanvasMainWidget::changeCurrentDevice(bool is_stylus)
{
	stylus_device = is_stylus;
}

DRAW_WINDOW* CanvasMainWidget::canvas_data()
{
	return canvas;
}

void CanvasMainWidget::colorPickerPopupMenuClicked(int row, int column)
{
	APPLICATION *app = canvas->app;
	int color_index = row * PICKER_POPUP_TABLE_WIDTH + column;
	uint8 *color = app->tool_box.color_chooser.color_history[color_index];
	HSV hsv;
	uint8 temp_color[3] = {color[0], color[1], color[2]};
	
	app->tool_box.color_chooser.color_history[color_index][0] =
		app->tool_box.color_chooser.color_history[0][0];
	app->tool_box.color_chooser.color_history[color_index][1] =
		app->tool_box.color_chooser.color_history[0][1];
	app->tool_box.color_chooser.color_history[color_index][2] =
		app->tool_box.color_chooser.color_history[0][2];
	
	app->tool_box.color_chooser.color_history[0][0] = temp_color[0];
	app->tool_box.color_chooser.color_history[0][1] = temp_color[1];
	app->tool_box.color_chooser.color_history[0][2] = temp_color[2];
	
	RGB2HSV_Pixel(app->tool_box.color_chooser.color_history[0], &hsv);
	
	app->tool_box.color_chooser.rgb[0] = app->tool_box.color_chooser.color_history[0][0];
	app->tool_box.color_chooser.rgb[1] = app->tool_box.color_chooser.color_history[0][1];
	app->tool_box.color_chooser.rgb[2] = app->tool_box.color_chooser.color_history[0][2];
}

bool CanvasMainWidget::eventFilter(QObject* object, QEvent* event)
{
	if(object == this && event->type() == QEvent::MouseMove) {
		QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
		// マウスカーソルの移動に合わせて処理
		return true; // イベントを消費
	}
	return false;
}

CanvasWidget::CanvasWidget(QWidget* parent, DRAW_WINDOW* canvas)
	: QScrollArea(parent),
	  main_widget(this, canvas)
{
	this->setWidget(&this->main_widget);
}

CanvasWidget::~CanvasWidget()
{

}

void UpdateCanvasWidget(DRAW_WINDOW_WIDGETS_PTR widgets)
{
	widgets->window->canvas_widget()->update();
}

void UpdateCanvasWidgetArea(DRAW_WINDOW_WIDGETS_PTR widgets, int x, int y, int width, int height)
{
	widgets->window->canvas_widget()->update(x, y, width, height);
}

void ForceUpdateCanvasWidget(DRAW_WINDOW_WIDGETS_PTR widgets)
{
	widgets->window->canvas_widget()->repaint();
}

void SetColorPickerHistoryPopupMenuCallback(QTableWidget* table, void* canvas)
{
	CanvasMainWidget *widget = (CanvasMainWidget*)canvas;
	QObject::connect(table, &QTableWidget::cellClicked, widget, &CanvasMainWidget::colorPickerPopupMenuClicked);
}

void ExcecuteCanvasWidgetColorHistoryPopupMenu(DRAW_WINDOW_WIDGETS_PTR widgets, FLOAT_T x, FLOAT_T y)
{
	QPoint position((int)x, (int)y);
	ColorHistoryPopupMenu(widgets->window->canvas_widget()->canvas_data(),
		position, widgets->window->canvas_widget(), SetColorPickerHistoryPopupMenuCallback);
}

#ifdef __cplusplus
}
#endif
