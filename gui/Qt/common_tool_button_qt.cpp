#include <QPainter>
#include <Qdir>
#include "../../common_tools.h"
#include "common_tool_button_qt.h"
#include "tool_box_qt.h"
#include "../../application.h"

CommonToolButton::CommonToolButton(
	QWidget* parent,
	const QString& image_file_path,
	COMMON_TOOL_CORE* core
) : QPushButton(parent)
{
	setCheckable(true);

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

	icon = new QIcon(pixmap);
	this->core = core;

	QSize icon_size = pixmap.size();
	setMinimumSize(icon_size);
}

CommonToolButton::~CommonToolButton()
{

}

void CommonToolButton::paintEvent(QPaintEvent* event)
{
	QPushButton::paintEvent(event);

	QPainter painter(this);
	painter.drawPixmap(QRect(0, 0, pixmap.size().width(), pixmap.size().height()), pixmap);

	(void)painter.end();
}

void CommonToolButton::mousePressEvent(QMouseEvent* event)
{
	QPushButton::mousePressEvent(event);
	if(event->button() == Qt::LeftButton)
	{
		emit toggled(true);
	}
}

void CommonToolButton::toggled(bool checked)
{
	if(checked)
	{
		APPLICATION *app = core->app;
		ToolBoxWidget* tool_box = (ToolBoxWidget*)app->tool_box.widgets;

		app->tool_box.active_common_tool = this->core;

		app->tool_box.active_common_tool->create_detail_ui(app, app->tool_box.active_common_tool);
		core->app->tool_box.flags &= ~(TOOL_USING_BRUSH);
		tool_box->allBrushButtonUnchecked();
	}
}
