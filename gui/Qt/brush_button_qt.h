#ifndef _INCLUDED_BRUSH_BUTTON_QT_H_
#define _INCLUDED_BRUSH_BUTTON_QT_H_

#include "../../types.h"
#include <QPushButton>
#include <QPixmap>
#include <QIcon>

#define BRUSH_BUTTON_SIZE 48

class BrushButton : public QPushButton
{
public:
	BrushButton(QWidget* parent = NULL, const QString& image_file_path = "",
				const QString& text = "", BRUSH_CORE* core = NULL);
	~BrushButton();

signals:
	void toggled(bool checked);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;

private:
	QPixmap pixmap;
	QIcon *icon;
	BRUSH_CORE *core;
	QString name;
};

#endif // #ifndef _INCLUDED_BRUSH_BUTTON_QT_H_

