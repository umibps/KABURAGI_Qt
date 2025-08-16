#include <QHBoxLayout>
#include <QLabel>
#include "brushes_widgets_qt.h"
#include "../../application.h"

BrushScaleSelector::BrushScaleSelector(
	QWidget* parent,
	QString caption,
	BrushGUI_Base* brush,
	bool button_layout_vertical
)   : DoubleSpinScale(parent, caption, button_layout_vertical)
{
	this->brush = brush;
	if(brush != NULL)
	{
		BRUSH_CORE *core = brush->brushCore();
		if(core != NULL)
		this->setValue(core->radius * 2);
	}
}

void BrushScaleSelector::valueChanged(double val)
{
	emit DoubleSpinScale::valueChanged(val);
	brush->brushScaleChanged(val);
}

BrushGUI_Base* BrushScaleSelector::brushGUI()
{
	return this->brush;
}

BrushBaseScaleSelector::BrushBaseScaleSelector(
	QWidget* parent,
	BrushScaleSelector* scale_selector,
	int index
)	: QComboBox(parent)
{
	this->scale_selector = scale_selector;
	
	setCurrentIndex(index);
}

void BrushBaseScaleSelector::currentIndexChanged(int index)
{
	scale_selector->brushGUI()->brushCore()->base_scale = index;
	QComboBox::currentIndexChanged(index);
	
	if(scale_selector == NULL)
	{
		return;
	}
	
	switch(index)
	{
	case 0:
		scale_selector->setMinimum(0.1);
		scale_selector->setMaximum(10);
		scale_selector->setSingleStep(0.1);
		break;
	case 1:
		scale_selector->setMinimum(1);
		scale_selector->setMaximum(100);
		scale_selector->setSingleStep(1);
		break;
	case 2:
		scale_selector->setMinimum(5);
		scale_selector->setMaximum(500);
		scale_selector->setSingleStep(1);
		break;
	}
}

BrushOpacitySelector::BrushOpacitySelector(
	QWidget* parent,
	QString caption,
	BrushGUI_Base* brush,
	bool button_layout_vertical
)   : DoubleSpinScale(parent, caption, button_layout_vertical)
{
	this->brush = brush;
}

void BrushOpacitySelector::valueChanged(double val)
{
	emit DoubleSpinScale::valueChanged(val);
	brush->brushOpacityChanged(val);
}

BrushOutlineHardnessSelector::BrushOutlineHardnessSelector(
	QWidget* parent,
	QString caption,
	BrushGUI_Base* brush,
	bool button_layout_vertical
)   : DoubleSpinScale(parent, caption, button_layout_vertical)
{
	this->brush = brush;
}

void BrushOutlineHardnessSelector::valueChanged(double val)
{
	emit DoubleSpinScale::valueChanged(val);
	brush->brushOutlineHardnessChanged(val);
}

BrushBlurSelector::BrushBlurSelector(
	QWidget* parent,
	QString caption,
	BrushGUI_Base* brush,
	bool button_layout_vertical
)   : DoubleSpinScale(parent, caption, button_layout_vertical)
{
	this->brush = brush;
}

void BrushBlurSelector::valueChanged(double val)
{
	brush->brushBlurChanged(val);
}

BrushBlendModeSelector::BrushBlendModeSelector(
	QWidget* parent,
	BrushGUI_Base* brush,
	APPLICATION* app
)   : QComboBox(parent)
{
	this->brush = brush;
	for(int i=0; i<LAYER_BLEND_SLELECTABLE_NUM; i++)
	{
		addItem(app->labels->layer_window.blend_modes[i]);
	}
	setCurrentIndex(brush->brushCore()->blend_mode);
}

void BrushBlendModeSelector::currentIndexChanged(int index)
{
	emit QComboBox::currentIndexChanged(index);
	brush->brushBlendModeChanged(index);
}

BrushPressureSizeCheckBox::BrushPressureSizeCheckBox(
	QWidget* parent,
	const char* label,
	BrushGUI_Base* brush
)   : QCheckBox(label, parent)
{
	this->brush = brush;
}

void BrushPressureSizeCheckBox::checkStateChanged(Qt::CheckState state)
{
	QCheckBox::checkStateChanged(state);
	brush->setBrushPressureSize(state != Qt::Unchecked);
}

BrushPressureOpacityCheckBox::BrushPressureOpacityCheckBox(
	QWidget* parent,
	const char* label,
	BrushGUI_Base* brush
)   : QCheckBox(label, parent)
{
	this->brush = brush;
}

void BrushPressureOpacityCheckBox::checkStateChanged(Qt::CheckState state)
{
	QCheckBox::checkStateChanged(state);
	brush->setBrushPressureOpacity(state != Qt::Unchecked);
}

BrushAntialiasCheckBox::BrushAntialiasCheckBox(
	QWidget* parent,
	const char* label,
	BrushGUI_Base* brush
)   : QCheckBox(label, parent)
{
	this->brush = brush;
}

void BrushAntialiasCheckBox::checkStateChanged(Qt::CheckState state)
{
	QCheckBox::checkStateChanged(state);
	brush->setAntialias(state != Qt::Unchecked);
}

BrushBlendTargetSelectorRadioButton::BrushBlendTargetSelectorRadioButton(
	QWidget* parent,
	int* store_address,
	int target
)	: QRadioButton(parent)
{
	this->store_address = store_address;
	this->target = target;
}

BrushBlendTargetSelectorRadioButton::BrushBlendTargetSelectorRadioButton(
	QString caption,
	QWidget* parent,
	int* store_address,
	int target
) : QRadioButton(caption, parent)
{
	this->store_address = store_address;
	this->target = target;
}

void BrushBlendTargetSelectorRadioButton::toggled(bool state)
{
	if(state && store_address != NULL)
	{
		*store_address = target;
	}
}

QWidget* BrushBlendModeSelectorNew(
	QWidget* parent,
	BrushGUI_Base* brush,
	APPLICATION* app
)
{
	QWidget *layout_widget = new QWidget(parent);
	QHBoxLayout *layout = new QHBoxLayout(layout_widget);
	
	QLabel *label = new QLabel(layout_widget);
	label->setText(QString(app->labels->tool_box.blend_mode) + " : ");
	layout->addWidget(label);
	
	BrushBlendModeSelector *selector = new BrushBlendModeSelector(parent, brush, app);
	layout->addWidget(selector);
	
	return layout_widget;
}

QWidget* BaseScaleSelectorNew(
	QWidget* parent,
	BrushGUI_Base* brush,
	BrushScaleSelector* scale_selector,
	APPLICATION* app
)
{
	QWidget *layout_widget = new QWidget(parent);
	QHBoxLayout *layout = new QHBoxLayout(layout_widget);
	
	QLabel *label = new QLabel(layout_widget);
	label->setText(app->labels->tool_box.base_scale);
	layout->addWidget(label);
	
	BrushBaseScaleSelector *selector =
		new BrushBaseScaleSelector(layout_widget, scale_selector, brush->baseScale());
	layout->addWidget(selector);
	
	return layout_widget;
}

QWidget* BrushPressureCheckBoxNew(
	QWidget* parent,
	BrushGUI_Base* brush,
	APPLICATION* app
)
{
	QWidget *layout_widget = new QWidget(parent);
	QHBoxLayout *layout = new QHBoxLayout(layout_widget);
	
	QLabel *label = new QLabel(layout_widget);
	label->setText(QString(app->labels->tool_box.pressure) + " : ");
	layout->addWidget(label);
	
	BrushPressureSizeCheckBox *pressure_size =
		new BrushPressureSizeCheckBox(parent, app->labels->tool_box.scale, brush);
	layout->addWidget(pressure_size);
	
	BrushPressureOpacityCheckBox *pressure_opacity =
		new BrushPressureOpacityCheckBox(parent, app->labels->tool_box.flow, brush);
	layout->addWidget(pressure_opacity);
	
	return layout_widget;
}
