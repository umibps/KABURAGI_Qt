#ifndef _INCLUDED_COMMON_TOOL_BUTTON_QT_H_
#define _INCLUDED_COMMON_TOOL_BUTTON_QT_H_

#include "../../types.h"
#include <QPushButton>
#include <QPixmap>
#include <QIcon>

class CommonToolButton : public QPushButton
{
	Q_OBJECT
public:
	CommonToolButton(QWidget* parent = NULL, const QString& image_file_path = "",
						COMMON_TOOL_CORE* core = NULL);
	~CommonToolButton();

	void toggled(bool checked);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;

private:
	QPixmap pixmap;
	QIcon *icon;
	COMMON_TOOL_CORE *core;
};

#endif	/* #ifndef _INCLUDED_COMMON_TOOL_BUTTON_QT_H_ */
