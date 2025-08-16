#include <QStyle>
#include <QStyleOptionFrame>
#include <QStyleOptionSpinBox>
#include <QPainter>
#include <QMouseEvent>
#include <QLineEdit>
#include <QFontMetrics>
#include "spin_scale_qt.h"

SpinScaleLineEdit::SpinScaleLineEdit(QWidget* parent, SpinScale* spin_scale, QString caption)
	: QLineEdit(parent)
{
	this->spin_scale = spin_scale;
	this->caption = caption;
	mouse_grabbing = false;
	setAlignment(Qt::AlignRight | Qt::AlignVCenter);
}

void SpinScaleLineEdit::mousePressEvent(QMouseEvent* event)
{
	QLineEdit::mousePressEvent(event);
	
	if(event->button() == Qt::LeftButton)
	{
		QPoint mouse_position = event->pos();
		QRect rectangle;

		rectangle = this->rect();
		if(mouse_position.x() >= 0 && mouse_position.x() <= rectangle.width())
		{
			SpinScale *scale = (SpinScale*)spin_scale;
			double value_range = scale->maximum() - scale->minimum();
			int set_value = (int)
				(((double)mouse_position.x() / (double)rectangle.width())  * (double)value_range)
					+ scale->minimum();
			scale->setValue(set_value);
			mouse_grabbing = true;
			scale->repaint();
			scale->valueChanged(set_value);
		}
	}
}

void SpinScaleLineEdit::mouseMoveEvent(QMouseEvent* event)
{
	QLineEdit::mouseMoveEvent(event);
	
	if(mouse_grabbing)
	{
		QPoint mouse_position = event->pos();
		SpinScale *scale = spin_scale;
		QRect rectangle = rect();
		int set_value;
		if(mouse_position.x() <= 0)
		{
			set_value = scale->minimum();
		}
		else if(mouse_position.x() >= rectangle.width())
		{
			set_value = scale->maximum();
		}
		else
		{
			double value_range = scale->maximum() - scale->minimum();
			set_value = (int)
				(((double)mouse_position.x() / (double)rectangle.width())  * (double)value_range)
					+ scale->minimum();
		}
		scale->setValue(set_value);
		scale->repaint();
		scale->valueChanged(set_value);
	}
}

void SpinScaleLineEdit::mouseReleaseEvent(QMouseEvent* event)
{
	QLineEdit::mouseReleaseEvent(event);
	
	if(event->button() == Qt::LeftButton)
	{
		mouse_grabbing = false;
	}
}

void SpinScaleLineEdit::paintEvent(QPaintEvent* event)
{
#define TEXT_X_MARGIN 4
	QLineEdit::paintEvent(event);
	
	if(caption != QString(""))
	{
		QPainter painter(this);
	
		QStyleOptionFrame panel;
		initStyleOption(&panel);
		QRect r = rect();
		r.setX(TEXT_X_MARGIN);
		r.setWidth(r.width()-TEXT_X_MARGIN);
	
		QFontMetrics fm = fontMetrics();
		int draw_y = (r.height() - fm.height() + 1) / 2;
		r.setY(draw_y);
		r.setHeight(r.height() - draw_y);
	
		painter.drawText(r, caption);
	}
#undef TEXT_X_MARGIN
}

void SpinScaleLineEdit::setCaption(const char* caption)
{
	this->caption = QString(caption);
}

void SpinScaleLineEdit::setCaption(const QString caption)
{
	this->caption = caption;
}

#define FILL_ALPHA 0x80
SpinScale::SpinScale(
	QWidget* parent,
	QString caption,
	bool button_laytout_vertical
)
	: QSpinBox(parent), line_edit(parent, this, caption)
{
	if(caption != QString(""))
	{
		line_edit.setAlignment(Qt::AlignRight);
	}
	
	if(button_laytout_vertical)
	{
		const QString style_sheet(
			"QSpinBox::up-button {subcontrol-position: top right;}"
			"QSpinBox::down-button {subcontrol-position: bottom right;}");
		setStyleSheet(style_sheet);
	}
	fill_color = QSpinBox::palette().color(QPalette::Highlight);
	fill_color.setAlpha(FILL_ALPHA);
	
	this->setLineEdit(&this->line_edit);
}

SpinScale::SpinScale(
	QWidget* parent,
	int value,
	int minimum,
	int maximum,
	QString caption,
	bool button_layout_vertical
)
	: QSpinBox(parent), line_edit(parent, this, caption)
{
	if(caption != QString(""))
	{
		line_edit.setAlignment(Qt::AlignRight);
	}
	
	if(button_layout_vertical)
	{
		const QString style_sheet(
			"QSpinBox::up-button {subcontrol-position: top right;}"
			"QSpinBox::down-button {subcontrol-position: bottom right;}");
		setStyleSheet(style_sheet);
	}
	
	setValue(value);
	setMinimum(minimum);
	setMaximum(maximum);
	
	fill_color = QSpinBox::palette().color(QPalette::Highlight);
	fill_color.setAlpha(FILL_ALPHA);
	
	this->setLineEdit(&this->line_edit);
}

void SpinScale::paintEvent(QPaintEvent* event)
{
	QSpinBox::paintEvent(event);
	
	QRect fill_rectangle;
	QStyleOptionSpinBox opt;
	opt.initFrom(this);
	fill_rectangle
		= style()->subControlRect(QStyle::CC_SpinBox, &opt, QStyle::SC_SpinBoxEditField, this);
	
	int current_value = value() - minimum();
	double value_range = maximum() - minimum();
	fill_rectangle.setWidth(
		(int)(current_value/value_range*fill_rectangle.width()));
	QPainter(this).fillRect(fill_rectangle, fill_color);
}

void SpinScale::valueChanged(int value)
{
	QSpinBox::valueChanged(value);
}

void SpinScale::setCaption(const char* caption)
{
	line_edit.setCaption(caption);
}

void SpinScale::setCaption(const QString caption)
{
	line_edit.setCaption(caption);
}

DoubleSpinScaleLineEdit::DoubleSpinScaleLineEdit(
	QWidget* parent,
	DoubleSpinScale* spin_scale,
	QString caption
)
	: QLineEdit(parent)
{
	this->spin_scale = spin_scale;
	this->caption = caption;
	mouse_grabbing = false;
	setAlignment(Qt::AlignRight | Qt::AlignVCenter);
}

void DoubleSpinScaleLineEdit::mousePressEvent(QMouseEvent* event)
{
	QLineEdit::mousePressEvent(event);
	
	if(event->button() == Qt::LeftButton)
	{
		QPoint mouse_position = event->pos();
		QRect rectangle;
		rectangle = this->rect();
		if(mouse_position.x() >= 0 && mouse_position.x() <= rectangle.width())
		{
			DoubleSpinScale *scale = (DoubleSpinScale*)spin_scale;
			double value_range = scale->maximum() - scale->minimum();
			double set_value = (((double)mouse_position.x() / (double)rectangle.width())  * (double)value_range)
									+ scale->minimum();
			scale->setValue(set_value);
			mouse_grabbing = true;
			scale->repaint();
			scale->valueChanged(set_value);
		}
	}
}

void DoubleSpinScaleLineEdit::mouseMoveEvent(QMouseEvent* event)
{
	QLineEdit::mouseMoveEvent(event);
	
	if(mouse_grabbing)
	{
		QPoint mouse_position = event->pos();
		DoubleSpinScale *scale = spin_scale;
		QRect rectangle = rect();
		double set_value;
		if(mouse_position.x() <= 0)
		{
			set_value = scale->minimum();
		}
		else if(mouse_position.x() >= rectangle.width())
		{
			set_value = scale->maximum();
		}
		else
		{
			double value_range = scale->maximum() - scale->minimum();
			set_value = (((double)mouse_position.x() / (double)rectangle.width())  * (double)value_range)
							+ scale->minimum();
		}
		scale->setValue(set_value);
		scale->repaint();
		scale->valueChanged(set_value);
	}
}

void DoubleSpinScaleLineEdit::mouseReleaseEvent(QMouseEvent* event)
{
	QLineEdit::mouseReleaseEvent(event);
	
	if(event->button() == Qt::LeftButton)
	{
		mouse_grabbing = false;
	}
}

void DoubleSpinScaleLineEdit::paintEvent(QPaintEvent* event)
{
#define TEXT_X_MARGIN 4
	QLineEdit::paintEvent(event);
	
	if(caption != QString(""))
	{
		QPainter painter(this);
	
		QStyleOptionFrame panel;
		initStyleOption(&panel);
		QRect r = rect();
		r.setX(TEXT_X_MARGIN);
		r.setWidth(r.width()-TEXT_X_MARGIN);
	
		QFontMetrics fm = fontMetrics();
		int draw_y = (r.height() - fm.height() + 1) / 2;
		r.setY(draw_y);
		r.setHeight(r.height() - draw_y);
	
		painter.drawText(r, caption);
	}
#undef TEXT_X_MARGIN
}

void DoubleSpinScaleLineEdit::setCaption(const char* caption)
{
	this->caption = QString(caption);
}

void DoubleSpinScaleLineEdit::setCaption(const QString caption)
{
	this->caption = caption;
}

DoubleSpinScale::DoubleSpinScale(
	QWidget* parent,
	QString caption,
	bool button_layout_vertical
)
	: QDoubleSpinBox(parent), line_edit(parent, this, caption)
{
#define FILL_ALPHA 0x80
	if(caption != QString(""))
	{
		line_edit.setAlignment(Qt::AlignRight);
	}
	
	if(button_layout_vertical)
	{
		const QString style_sheet(
			"QDoubleSpinBox::up-button {subcontrol-position: top right;}"
			"QDoubleSpinBox::down-button {subcontrol-position: bottom right;}");
		setStyleSheet(style_sheet);
	}
	fill_color = QDoubleSpinBox::palette().color(QPalette::Highlight);
	fill_color.setAlpha(FILL_ALPHA);
	
	this->setLineEdit(&this->line_edit);
}

DoubleSpinScale::DoubleSpinScale(
	QWidget* parent,
	double value,
	double minimum,
	double maximum,
	QString caption,
	bool button_layout_vertical
)
	: QDoubleSpinBox(parent), line_edit(parent, this, caption)
{
	if(caption != QString(""))
	{
		line_edit.setAlignment(Qt::AlignRight);
	}
	
	if(button_layout_vertical)
	{
		const QString style_sheet(
			"QDoubleSpinBox::up-button {subcontrol-position: top right;}"
			"QDoubleSpinBox::down-button {subcontrol-position: bottom right;}");
		setStyleSheet(style_sheet);
	}
	
	setValue(value);
	setMinimum(minimum);
	setMaximum(maximum);
	
	fill_color = QDoubleSpinBox::palette().color(QPalette::Highlight);
	fill_color.setAlpha(FILL_ALPHA);
	
	this->setLineEdit(&this->line_edit);
}

void DoubleSpinScale::paintEvent(QPaintEvent* event)
{
	QDoubleSpinBox::paintEvent(event);
	
	QRect fill_rectangle;
	QStyleOptionSpinBox opt;
	opt.initFrom(this);
	fill_rectangle
		= style()->subControlRect(QStyle::CC_SpinBox, &opt, QStyle::SC_SpinBoxEditField, this);
	
	int current_value = value();
	double value_range = maximum() - minimum();
	fill_rectangle.setWidth(
		(int)(current_value/value_range*fill_rectangle.width()));
	QPainter(this).fillRect(fill_rectangle, fill_color);
}

void DoubleSpinScale::valueChanged(double value)
{
	QDoubleSpinBox::valueChanged(value);
}

void DoubleSpinScale::setCaption(const char* caption)
{
	line_edit.setCaption(caption);
}

void DoubleSpinScale::setCaption(const QString caption)
{
	line_edit.setCaption(caption);
}
