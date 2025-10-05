#include <string.h>
#include <math.h>
#include <QPainter>
#include <QPaintEvent>
#include <QBrush>
#include "../../draw_window.h"
#include "../../application.h"
#include "draw_window_qt.h"
#include "qt_widgets.h"
#include "../gui.h"
#include "../../memory.h"

void CanvasMainWidget::mousePressEvent(QMouseEvent* event)
{
	EVENT_STATE state;
	MousePressEventToEventState((void*)event, &state);
	MouseButtonPressEvent(canvas, &state);
	if(event->button() == Qt::MouseButton::LeftButton)
	{
		grabMouse();
		setFocus();
	}
}

void CanvasMainWidget::mouseMoveEvent(QMouseEvent* event)
{
	EVENT_STATE state;
	MouseMotionEventToEventState((void*)event, &state);
	MouseMotionNotifyEvent(canvas, &state);
}

void CanvasMainWidget::mouseReleaseEvent(QMouseEvent* event)
{
	EVENT_STATE state;
	MouseReleaseEventToEventState((void*)event, &state);
	MouseButtonReleaseEvent(canvas, &state);
	if(event->button() == Qt::MouseButton::LeftButton)
	{
		releaseMouse();
	}
}

void CanvasMainWidget::tabletEvent(QTabletEvent* event)
{
	EVENT_STATE state;
	TabletEventToEventState((void*)event, &state);
	switch(event->type())
	{
	case QEvent::TabletPress:
		MouseButtonPressEvent(canvas, &state);
		break;
	case QEvent::TabletMove:
		MouseMotionNotifyEvent(canvas, &state);
		break;
	case QEvent::TabletRelease:
		MouseButtonReleaseEvent(canvas, &state);
		break;
	}
}

void CanvasMainWidget::enterEvent(QEnterEvent* event)
{
	this->canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
	this->canvas->flags &= ~(DRAW_WINDOW_UPDATE_PART);
	this->repaint();
}

void CanvasMainWidget::leaveEvent(QEvent* event)
{
	this->canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
	this->canvas->flags &= ~(DRAW_WINDOW_UPDATE_PART);
	this->repaint();
}
