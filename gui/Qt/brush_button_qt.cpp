#include <QPainter>
#include <QFontMetrics>
#include <QDir>
#include "../../brush_core.h"
#include "brush_button_qt.h"
#include "../gui.h"
#include "tool_box_qt.h"
#include "../../application.h"

BrushButton::BrushButton(
	QWidget* parent,
	const QString& image_file_path,
	const QString& text,
	BRUSH_CORE* core
)	: QPushButton(parent),
	  pixmap(image_file_path)
{
	setCheckable(true);
	
	name = text;

	QString path = QDir().absolutePath();
	if(pixmap.isNull() && QDir(image_file_path).isAbsolute() == false)
	{
		QString absolute_path;
		if(image_file_path.data()[0] == '.')
		{
			absolute_path = QDir().absolutePath() + QString(&image_file_path.data()[1]);
		}
		else
		{
			absolute_path = QDir(QDir().absolutePath()).filePath(QString(image_file_path));
		}
		pixmap = QPixmap(absolute_path);
	}

	setMinimumSize(BRUSH_BUTTON_SIZE, BRUSH_BUTTON_SIZE);

	icon = new QIcon(pixmap);
	// this->setIcon(*icon);
	this->core = core;
	if(core != NULL)
	{
		core->button = (void*)this;
	}
}

BrushButton::~BrushButton()
{
	delete icon;
}

void BrushButton::paintEvent(QPaintEvent* event)
{
#define BUTTON_FONT_SIZE 10
#define BUTTON_TEXT_SIDE_MARGIN 4
#define BUTTON_PIXMAP_MARGIN 2
	QPushButton::paintEvent(event);

	QFont font = this->font();
	font.setPointSize(BUTTON_FONT_SIZE);
	QFontMetrics metrics(font);
	QRect text_rectangle = metrics.boundingRect(name);

	QPainter painter(this);

	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
	painter.setRenderHint(QPainter::LosslessImageRendering, true);

	int image_x = (pixmap.width() + BUTTON_PIXMAP_MARGIN < this->width())
		? this->width() - pixmap.width() - BUTTON_PIXMAP_MARGIN : 0;
	int image_y = (pixmap.height() + BUTTON_PIXMAP_MARGIN < this->height())
		? this->height() - pixmap.height() - BUTTON_PIXMAP_MARGIN : 0;
	int image_width = (pixmap.width() > this->width())
		? this->width() : pixmap.width();
	int image_height = (pixmap.height() > this->height())
		? this->height() : pixmap.height();
	painter.drawPixmap(QRect(image_x, image_y, image_width, image_height), pixmap);

	painter.setFont(font);
	painter.drawText(BUTTON_TEXT_SIDE_MARGIN, text_rectangle.height(), name);

	(void)painter.end();
}

void BrushButton::mousePressEvent(QMouseEvent* event)
{
	QPushButton::mousePressEvent(event);
	if(event->button() == Qt::LeftButton)
	{
		emit toggled(true);
	}
}

void BrushButton::toggled(bool checked)
{
	emit QPushButton::toggled(checked);

	if(checked)
	{
		APPLICATION *app = core->app;
		ToolBoxWidget *tool_box = (ToolBoxWidget*)app->tool_box.widgets;

		app->tool_box.active_brush[app->input] = this->core;

		DRAW_WINDOW *active_canvas = GetActiveDrawWindow(app);
		bool create_ui = false;
		if(active_canvas != NULL)
		{
			if(active_canvas->active_layer->layer_type == TYPE_NORMAL_LAYER)
			{
				create_ui = true;
			}
		}
		else
		{
			create_ui = true;
		}
		if(create_ui)
		{
			app->tool_box.active_brush[app->input]->create_detail_ui(
											app, app->tool_box.active_brush[app->input]);
			app->tool_box.flags |= TOOL_USING_BRUSH;
		}

		tool_box->allCommonToolButtonUnchecked();
	}
}
