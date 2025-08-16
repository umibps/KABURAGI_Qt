#ifndef _INCLUDED_QT_WIDGETS_H_
#define _INCLUDED_QT_WIDGETS_H_

#include <QWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QApplication>
#include "spin_scale_qt.h"
#include "../../types.h"

class ImageCheckBox : public QCheckBox
{
public:
	ImageCheckBox(QWidget* parent = NULL, const char* image_file_path = NULL, qreal scale = 1.0);
protected:
	void paintEvent(QPaintEvent* event) override;
private:
	static const int line_width = 3;
	QImage image;
};

class Application : public QApplication
{
public:
	Application(int& argv, char** argc);
	bool event(QEvent* event);
signals:
	void changeCurrentDevice(bool is_stylus);
};

#ifdef __cplusplus
extern "C" {
#endif

EXTERN void ClearLayout(QLayout* layout);

#ifdef __cplusplus
}
#endif

#endif  // #ifndef _INCLUDED_QT_WIDGETS_H_
