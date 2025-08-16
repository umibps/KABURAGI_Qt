#include <QWidget>
#include <QVBoxLayout>
#include "../../application.h"
#include "brushes_qt.h"
#include "brushes_widgets_qt.h"
#include "../tool_box_qt.h"
#include "../../brushes.h"

#ifdef __cplusplus
extern "C" {
#endif

EraserGUI::EraserGUI(BRUSH_CORE* core) : DefaultBrushGUI(core)
{
	BrushCoreSetCirclePattern(core, core->radius, core->outline_hardness, core->blur,
		core->opacity, *core->color);
}

QWidget* EraserGUI::createDetailUI()
{
	APPLICATION *app = core->app;
	ERASER *eraser = (ERASER*)core;

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

	QWidget* pressure_setting = BrushPressureCheckBoxNew(vbox_widget, this, app);
	vbox->addWidget(pressure_setting);

	BrushAntialiasCheckBox *antialias =
		new BrushAntialiasCheckBox(vbox_widget, app->labels->tool_box.anti_alias, this);
	vbox->addWidget(antialias);

	// ã‹l‚ß•\¦
	vbox->addStretch();

	return vbox_widget;
}

void* EraserGUI_New(APPLICATION* app, BRUSH_CORE* core)
{
	ToolBoxWidget *tool_box = (ToolBoxWidget*)app->widgets;

	QWidget *detail_layout_widget = tool_box->getDetailLayoutWidget();
	EraserGUI *eraser = new EraserGUI(core);
	core->brush_data = (void*)eraser;

	return (void*)eraser;
}

#ifdef __cplusplus
}
#endif
