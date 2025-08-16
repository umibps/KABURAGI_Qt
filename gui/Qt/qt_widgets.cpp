#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QStyle>
#include <QSize>
#include <QStyleOptionSpinBox>
#include <QPainter>
#include <QLineEdit>
#include <QMouseEvent>
#include <QLayout>
#include <QLayoutItem>
#include <QTimer>
#include <QFileDialog>
#include "qt_widgets.h"
#include "../../types.h"
#include "../../application.h"
#include "mainwindow.h"

ImageCheckBox::ImageCheckBox(QWidget* parent, const char* image_file_path, qreal scale)
	: QCheckBox(parent),
	  image(QString::fromUtf8(image_file_path))
{
	if(scale != 1.0)
	{
		image = image.scaled(image.width() * scale, image.height() * scale);
	}

	setMinimumSize(image.width() + line_width * 2, image.height() + line_width * 2);
	setMaximumSize(image.width() + line_width * 2, image.height() + line_width * 2);
}

void ImageCheckBox::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	painter.setPen(QPen(QBrush(Qt::black), line_width));
	painter.setBrush(Qt::white);
	painter.drawRect(rect());
	if(isChecked())
	{
		painter.drawImage(line_width, line_width, image);
	}
}

void ClearLayout(QLayout* layout)
{
	QLayoutItem *child;

	while((child = layout->takeAt(0)) != NULL)
	{
		QWidget *child_widget;
		if(child->layout() != NULL)
		{
			ClearLayout(child->layout());
		}
		else if((child_widget = child->widget()) != NULL)
		{
			delete child_widget;
		}
		delete child;
	}
}

Application::Application(int& argc, char** argv) : QApplication(argc, argv)
{

}

#include <QMetaEnum>
template<typename EnumType>
QString ToString(const EnumType& enumValue)
{
	const char* enumName = qt_getEnumName(enumValue);
	const QMetaObject* metaObject = qt_getEnumMetaObject(enumValue);
	if(metaObject)
	{
		const int enumIndex = metaObject->indexOfEnumerator(enumName);
		return QString("%1::%2::%3").arg(metaObject->className(), enumName, metaObject->enumerator(enumIndex).valueToKey(enumValue));
	}

	return QString("%1::%2").arg(enumName).arg(static_cast<int>(enumValue));
}

bool Application::event(QEvent* event)
{
	//printf("%s\n", ToString(event->type()).toUtf8().data());
	if(event->type() == QEvent::TabletEnterProximity)
	{
		emit changeCurrentDevice(true);
		return true;
	}
	else if(event->type() == QEvent::TabletLeaveProximity)
	{
		emit changeCurrentDevice(false);
		return true;
	}
	return QApplication::event(event);
}

void Application::changeCurrentDevice(bool is_stylus)
{

}

#ifdef __cplusplus
extern "C" {
#endif

void WidgetUpdate(void* widget)
{
	QWidget *w = (QWidget*)widget;
	
	w->update();
}

void WidgetSetSensitive(void* widget, int sensitive)
{
	QWidget *w = (QWidget*)widget;
	
	w->setEnabled(sensitive);
}

void BoxClear(void* layout)
{
	QLayout *l = (QLayout*)layout;
	
	QLayoutItem *item;
	while((item = l->takeAt(0)) != NULL)
	{
		delete item->widget();
		delete item;
	}
}

static INLINE eCURSOR_INPUT_DEVICE DeviceToInput(QPointingDevice::PointerType device)
{
	switch(device)
	{
	case QPointingDevice::PointerType::Pen:
		return CURSOR_INPUT_DEVICE_PEN;
	case QPointingDevice::PointerType::Eraser:
		return CURSOR_INPUT_DEVICE_ERASER;
	case QPointingDevice::PointerType::Finger:
		return CURSOR_INPUT_DEVICE_TOUCH;
	default:
		break;
	}
	return CURSOR_INPUT_DEVICE_MOUSE;
}

static INLINE eMOUSE_KEY_FLAG MouseButtonToFlag(Qt::MouseButton button)
{
	switch(button)
	{
	case Qt::MouseButton::LeftButton:
		return MOUSE_KEY_FLAG_LEFT;
	case Qt::MouseButton::RightButton:
		return MOUSE_KEY_FLAG_RIGHT;
	case Qt::MouseButton::NoButton:
		return MOUSE_KEY_FLAG_LEFT;
	default:
		break;
	}
	return MOUSE_KEY_FLAG_CENTER;
}

static INLINE eMOUSE_KEY_FLAG EventToModifierFlag(Qt::KeyboardModifiers modifiers)
{
	unsigned int flags = 0;
	if(modifiers & Qt::ShiftModifier)
	{
		flags |= MOUSE_KEY_FLAG_SHIFT;
	}
	if(modifiers & Qt::ControlModifier)
	{
		flags |= MOUSE_KEY_FLAG_CONTROL;
	}
	if(modifiers & Qt::AltModifier)
	{
		flags |= MOUSE_KEY_FLAG_ALT;
	}
	return (eMOUSE_KEY_FLAG)flags;
}

void MouseMotionEventToEventState(void* event, EVENT_STATE* state)
{
	QMouseEvent *mouse_event = (QMouseEvent*)event;
	state->cursor_x = mouse_event->pos().x();
	state->cursor_y = mouse_event->pos().y();
	state->input_device = DeviceToInput(mouse_event->pointerType());
	state->pressure = 0;
	state->mouse_key_flag = MouseButtonToFlag(mouse_event->button());
	state->mouse_key_flag = (eMOUSE_KEY_FLAG)(state->mouse_key_flag
											   | EventToModifierFlag(mouse_event->modifiers()));
}

void MousePressEventToEventState(void* event, EVENT_STATE* state)
{
	MouseMotionEventToEventState(event, state);
}

void MouseReleaseEventToEventState(void* event, EVENT_STATE* state)
{
	MouseMotionEventToEventState(event, state);
}

void TabletEventToEventState(void* event, EVENT_STATE* state)
{
	QTabletEvent *tablet_event = (QTabletEvent*)event;
	state->cursor_x = tablet_event->position().x();
	state->cursor_y = tablet_event->position().y();
	state->input_device = DeviceToInput(tablet_event->pointerType());
	state->pressure = tablet_event->pressure();
	state->mouse_key_flag = MOUSE_KEY_FLAG_LEFT;
	state->mouse_key_flag = (eMOUSE_KEY_FLAG)(state->mouse_key_flag
								| EventToModifierFlag(tablet_event->modifiers()));
}

void* TimerInMotionQueueExecutionNew(unsigned int time_milliseconds)
{
	QTimer *timer = new QTimer();
	
	timer->setSingleShot(true);
	timer->start(time_milliseconds);
	
	return (void*)timer;
}

int TimerElapsedTime(void* timer)
{
	QTimer *t = (QTimer*)timer;
	
	return - (t->remainingTime() - t->interval());
}

void DeleteTimer(void* timer)
{
	QTimer *t = (QTimer*)timer;
	
	delete t;
}

char* GetImageFileSaveaPath(APPLICATION* app)
{
	QString selected_filter;
	QString file_path = QFileDialog::getSaveFileName(
		app->widgets->main_window, QWidget::tr("Input save file name"),
		NULL, QWidget::tr("Original Format(*.kab);;Portable network graphic(*.png)"),
		&selected_filter
	);

	if(file_path.isEmpty())
	{
		return NULL;
	}
	char *path = MEM_STRDUP_FUNC(file_path.toLocal8Bit().data());

	return path;
}

char* GetImageFileOpenPath(APPLICATION* app)
{
	QString selected_filter;
	QString file_path = QFileDialog::getOpenFileName(
		app->widgets->main_window, QWidget::tr("Select open file"),
		NULL, QWidget::tr("All file(*.*);;Original format(*.kab);;Portable network graphic(*.png)"),
		&selected_filter
	);

	if(file_path.isEmpty())
	{
		return NULL;
	}
	char* path = MEM_STRDUP_FUNC(file_path.toLocal8Bit().data());

	return path;
}

#ifdef _DEBUG
void PrintQobjectProperties(QObject* object)
{
	const QMetaObject *meta_object = object->metaObject();
	for(int i=0; i<meta_object->propertyCount(); i++)
	{
		QMetaProperty meta_porperty = meta_object->property(i);
		const char *name = meta_porperty.name();
		QVariant value = object->property(name);
		printf("property_name:%s\tvalue:%s\n", name, value.typeName());
	}
}
#else
void PrintQobjectProperties(QObject* object) {}
#endif

#ifdef __cplusplus
}
#endif
