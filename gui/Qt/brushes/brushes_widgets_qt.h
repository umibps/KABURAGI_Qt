#ifndef _INCLUDED_BRUSHES_WIDGETS_QT_H_
#define _INCLUDED_BRUSHES_WIDGETS_QT_H_

#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include "../spin_scale_qt.h"
#include "brushes_qt.h"

class BrushScaleSelector : public DoubleSpinScale
{
	Q_OBJECT
public:
	BrushScaleSelector(
		QWidget* parent = NULL,
		QString caption = "",
		BrushGUI_Base* brush = NULL,
		bool button_layout_vertical = true
	);
	void valueChanged(double val);
	BrushGUI_Base* brushGUI();
private:
	BrushGUI_Base *brush;
};

class BrushBaseScaleSelector : public QComboBox
{
	Q_OBJECT
public:
	BrushBaseScaleSelector(
		QWidget* parent = NULL,
		BrushScaleSelector* scale_selector = NULL,
		int index = 0
	);
	void currentIndexChanged(int index);
private:
	BrushScaleSelector *scale_selector;
};

class BrushOpacitySelector : public DoubleSpinScale
{
	Q_OBJECT
public:
	BrushOpacitySelector(
		QWidget* parent = NULL,
		QString caption = "",
		BrushGUI_Base* brush = NULL,
		bool button_layout_vertical = true
	);
	void valueChanged(double val);
private:
	BrushGUI_Base *brush;
};

class BrushOutlineHardnessSelector : public DoubleSpinScale
{
	Q_OBJECT
public:
	BrushOutlineHardnessSelector(
		QWidget* parent = NULL,
		QString caption = "",
		BrushGUI_Base* brush = NULL,
		bool button_layout_vertical = true
	);
	void valueChanged(double val);
private:
	BrushGUI_Base *brush;
};

class BrushBlurSelector : public DoubleSpinScale
{
	Q_OBJECT
public:
	BrushBlurSelector(
		QWidget* parent = NULL,
		QString caption = "",
		BrushGUI_Base* brush = NULL,
		bool button_layout_vertical = true
	);
	void valueChanged(double val);
private:
	BrushGUI_Base *brush;
};

class BrushBlendModeSelector : public QComboBox
{
	Q_OBJECT
public:
	BrushBlendModeSelector(
		QWidget* parent = NULL,
		BrushGUI_Base* brush = NULL,
		APPLICATION* app = NULL
	);
	void currentIndexChanged(int index);
private:
	BrushGUI_Base *brush;
};

class BrushPressureSizeCheckBox : public QCheckBox
{
	Q_OBJECT
public:
	BrushPressureSizeCheckBox(
		QWidget* parent = NULL,
		const char* label = "",
		BrushGUI_Base* brush = NULL
	);
	void checkStateChanged(Qt::CheckState state);
private:
	BrushGUI_Base *brush;
};

class BrushPressureOpacityCheckBox : public QCheckBox
{
	Q_OBJECT
public:
	BrushPressureOpacityCheckBox(
		QWidget* parent = NULL,
		const char* label = "",
		BrushGUI_Base* brush = NULL
	);
	void checkStateChanged(Qt::CheckState state);
private:
	BrushGUI_Base *brush;
};

class BrushAntialiasCheckBox : public QCheckBox
{
	Q_OBJECT
public:
	BrushAntialiasCheckBox(
		QWidget* parent = NULL,
		const char* label = "",
		BrushGUI_Base* brush = NULL
	);
	void checkStateChanged(Qt::CheckState state);
private:
	BrushGUI_Base *brush;
};

class BrushBlendTargetSelectorRadioButton : public QRadioButton
{
	Q_OBJECT
public:
	BrushBlendTargetSelectorRadioButton(QWidget* parent = NULL, int* store_address = NULL, int target = 0);
	BrushBlendTargetSelectorRadioButton(const QString caption = "",
										QWidget* parent = NULL, int* store_address = NULL, int target = 0);

	void toggled(bool);

private:
	int target;
	int *store_address;
};

extern QWidget* BrushBlendModeSelectorNew(
	QWidget* parent,
	BrushGUI_Base* brush,
	APPLICATION* app
);

extern QWidget* BaseScaleSelectorNew(
	QWidget* parent,
	BrushGUI_Base* brush,
	BrushScaleSelector* scale_selector,
	APPLICATION* app
);

extern QWidget* BrushPressureCheckBoxNew(
	QWidget* parent,
	BrushGUI_Base* brush,
	APPLICATION* app
);

#endif /* #ifndef _INCLUDED_BRUSHES_WIDGETS_QT_H_ */
