#ifndef _INCLUDED_SPIN_SCALE_QT_H_
#define _INCLUDED_SPIN_SCALE_QT_H_

#include <QString>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QColor>

class SpinScale;
class DoubleSpinScale;

class SpinScaleLineEdit : public QLineEdit
{
	Q_OBJECT
protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void paintEvent(QPaintEvent* event) override;

public:
	SpinScaleLineEdit(QWidget* parent, SpinScale* spin_scale, QString caption);
	void setCaption(const char* caption);
	void setCaption(const QString caption);

private:
	SpinScale *spin_scale;
	QString caption;
	bool mouse_grabbing;
};

class SpinScale : public QSpinBox
{
	Q_OBJECT
protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;

public:
	SpinScale(QWidget* parent = NULL, QString caption = "", bool button_layout_vertical = true);
	SpinScale(QWidget* parent, int value, int maximum, int minimum,
				QString caption = "", bool button_laytout_vertical = true);


	void setCaption(const char* caption);
	void setCaption(const QString caption);

signals:
	virtual void valueChanged(int value);

private:
	SpinScaleLineEdit line_edit;
	QColor fill_color;
};

class DoubleSpinScaleLineEdit : public QLineEdit
{
	Q_OBJECT
protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void paintEvent(QPaintEvent* event) override;

public:
	DoubleSpinScaleLineEdit(QWidget* parent, DoubleSpinScale* spin_scale, QString caption);
	void setCaption(const char* caption);
	void setCaption(const QString caption);

private:
	DoubleSpinScale *spin_scale;
	QString caption;
	bool mouse_grabbing;
};

class DoubleSpinScale : public QDoubleSpinBox
{
	Q_OBJECT
protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;

public:
	DoubleSpinScale(QWidget* parent = NULL, QString caption = "", bool button_layout_vertical = true);
	DoubleSpinScale(QWidget* parent, double value, double maximum, double minimum,
				QString caption = "", bool button_laytout_vertical = true);

	void setCaption(const char* caption);
	void setCaption(const QString caption);

signals:
	virtual void valueChanged(double value);

private:
	DoubleSpinScaleLineEdit line_edit;
	QColor fill_color;
};

#endif // #ifndef _INCLUDED_SPIN_SCALE_QT_H_
