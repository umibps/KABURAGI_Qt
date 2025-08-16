#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include "../../application.h"
#include "brushes_qt.h"
#include "brushes_widgets_qt.h"
#include "../tool_box_qt.h"
#include "../../brushes.h"

#ifdef __cplusplus
extern "C" {
#endif

BlendBrushGUI::BlendBrushGUI(BRUSH_CORE* core) : DefaultBrushGUI(core)
{
	BrushCoreSetCirclePattern(core, core->radius, core->outline_hardness, core->blur,
		core->opacity, *core->color);
}

QWidget* BlendBrushGUI::createDetailUI()
{
	APPLICATION *app = core->app;
	BLEND_BRUSH *brush = (BLEND_BRUSH*)core;

	ToolBoxWidget *tool_box = (ToolBoxWidget*)app->tool_box.widgets;
	QWidget *vbox_widget = new QWidget(tool_box->getDetailLayoutWidget());
	QVBoxLayout *vbox = new QVBoxLayout(tool_box->getDetailLayoutWidget());

	vbox_widget->setLayout(vbox);

	BrushScaleSelector *scale_selector = new BrushScaleSelector(vbox_widget,
		app->labels->tool_box.brush_scale, this);
	scale_selector->setValue(core->radius * 2);
	QWidget *base_scale_selector = BaseScaleSelectorNew(vbox_widget,
		this, scale_selector, app);
	vbox->addWidget(base_scale_selector);
	vbox->addWidget(scale_selector);

	BrushOpacitySelector *opacity_selector = new BrushOpacitySelector(vbox_widget,
		app->labels->tool_box.flow, this);
	opacity_selector->setValue(core->opacity);
	vbox->addWidget(opacity_selector);

	BrushOutlineHardnessSelector *outline_selector =
		new BrushOutlineHardnessSelector(vbox_widget, app->labels->tool_box.outline_hardness, this);
	outline_selector->setValue(core->outline_hardness);
	vbox->addWidget(outline_selector);

	QWidget *blend_mode_selector = BrushBlendModeSelectorNew(vbox_widget, this, app);
	vbox->addWidget(blend_mode_selector);

	QVBoxLayout *blend_target_layout = new QVBoxLayout(vbox_widget);
	QGroupBox *blend_target_group = new QGroupBox(vbox_widget);
	blend_target_group->setLayout(blend_target_layout);
	BrushBlendTargetSelectorRadioButton *target_is_under_layer
		= new BrushBlendTargetSelectorRadioButton(QString(app->labels->tool_box.select.under_layer),
					vbox_widget, &brush->target, BLEND_BRUSH_TARGET_UNDER_LAYER);
	blend_target_layout->addWidget(target_is_under_layer);
	BrushBlendTargetSelectorRadioButton *target_is_canvas
		= new BrushBlendTargetSelectorRadioButton(QString(app->labels->tool_box.select.canvas),
					vbox_widget, &brush->target, BLEND_BRUSH_TARGET_CANVAS);
	blend_target_layout->addWidget(target_is_canvas);
	vbox->addWidget(blend_target_group);
	if(brush->target == BLEND_BRUSH_TARGET_UNDER_LAYER)
	{
		target_is_under_layer->setChecked(true);
	}
	else
	{
		target_is_canvas->setChecked(true);
	}

	QWidget *pressure_setting = BrushPressureCheckBoxNew(vbox_widget, this, app);
	vbox->addWidget(pressure_setting);

	BrushAntialiasCheckBox *antialias =
		new BrushAntialiasCheckBox(vbox_widget, app->labels->tool_box.anti_alias, this);
	vbox->addWidget(antialias);

	// ã‹l‚ß•\Ž¦
	vbox->addStretch();

	return vbox_widget;
}

void BlendBrushGUI::blendTargetUnderLayer(bool state)
{
	if(state)
	{
		BLEND_BRUSH* brush = (BLEND_BRUSH*)core;
		brush->target = BLEND_BRUSH_TARGET_UNDER_LAYER;
	}
}

void BlendBrushGUI::blendTargetCanvas(bool state)
{
	if(state)
	{
		BLEND_BRUSH* brush = (BLEND_BRUSH*)core;
		brush->target = BLEND_BRUSH_TARGET_CANVAS;
	}
}

void* BlendBrushGUI_New(APPLICATION* app, BRUSH_CORE* core)
{
	ToolBoxWidget *tool_box = (ToolBoxWidget*)app->widgets;

	QWidget *detail_layout_widget = tool_box->getDetailLayoutWidget();
	BlendBrushGUI *brush = new BlendBrushGUI(core);
	core->brush_data = (void*)brush;

	return (void*)brush;
}

#ifdef __cplusplus
}
#endif
