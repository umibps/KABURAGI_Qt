#include <string.h>
#include <math.h>
#include <QPainter>
#include <QPaintEvent>
#include <QBrush>
#include "../../draw_window.h"
#include "../../application.h"
#include "../../display.h"
#include "draw_window_qt.h"
#include "qt_widgets.h"
#include "../../memory.h"

void CanvasMainWidget::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	
	eDRAW_WINDOW_DIPSLAY_UPDATE_RESULT result =
		LayerBlendForDisplay(canvas);

	painter.setClipRect(event->rect());

	painter.fillRect(event->rect(), QWidget::palette().color(QPalette::Window));

	//painter.drawPixmap(0, 0, width(), height(), *((QPixmap*)canvas->disp_layer->context_p));
	painter.setTransform(*(QTransform*)canvas->rotate);
	painter.drawImage(QRect(0, 0, canvas->disp_layer->width, canvas->disp_layer->height),
						*((QImage*)canvas->widgets->display_image));
}
