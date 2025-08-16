#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QComboBox>
#include "../../brush_core.h"
#include "../../types.h"
#include "brushes_qt.h"
#include "brushes_widgets_qt.h"
#include "../../application.h"
#include "../tool_box_qt.h"
#include "../spin_scale_qt.h"
#include "../../gui.h"

#ifdef __cplusplus
extern "C" {
#endif

BrushGUI_Base::BrushGUI_Base(BRUSH_CORE* core)
{
	this->core = core;
}

QWidget* BrushGUI_Base::createDetailUI()
{
	return NULL;
}

void BrushGUI_Base::brushScaleChanged(FLOAT_T scale)
{
	core->radius = scale * 0.5;
}

void BrushGUI_Base::brushOpacityChanged(FLOAT_T opacity)
{
	core->opacity = opacity * 0.01;
}

void BrushGUI_Base::brushOutlineHardnessChanged(FLOAT_T outline_hardness)
{
	core->outline_hardness = outline_hardness;
}

void BrushGUI_Base::brushBlurChanged(FLOAT_T blur)
{
	core->blur = blur;
}

void BrushGUI_Base::brushBlendModeChanged(int blend_mode)
{
	core->blend_mode = (eLAYER_BLEND_MODE)blend_mode;
}

void BrushGUI_Base::setBrushPressureSize(int enabled)
{
	if(enabled)
	{
		core->flags |= BRUSH_FLAG_SIZE;
	}
	else
	{
		core->flags &= ~(BRUSH_FLAG_SIZE);
	}
}

void BrushGUI_Base::setBrushPressureOpacity(int enabled)
{
	if(enabled)
	{
		core->flags |= BRUSH_FLAG_FLOW;
	}
	else
	{
		core->flags &= ~(BRUSH_FLAG_FLOW);
	}
}

void BrushGUI_Base::setAntialias(int enabled)
{
	if(enabled)
	{
		core->flags |= BRUSH_FLAG_ANTI_ALIAS;
	}
	else
	{
		core->flags &= ~(BRUSH_FLAG_ANTI_ALIAS);
	}
}

struct _BRUSH_CORE* BrushGUI_Base::brushCore()
{
	return core;
}

int BrushGUI_Base::baseScale()
{
	return base_scale;
}

DefaultBrushGUI::DefaultBrushGUI(BRUSH_CORE* core)
					: BrushGUI_Base(core)
{
	BrushCoreSetCirclePattern(core, core->radius, core->outline_hardness * 0.01, core->blur,
								core->opacity * 0.01, *core->color);
}

void DefaultBrushGUI::brushScaleChanged(FLOAT_T scale)
{
	core->radius = scale * 0.5;
	BrushCoreSetCirclePattern(core, core->radius, core->outline_hardness, core->blur,
								core->opacity, *core->color);
	DRAW_WINDOW *active_canvas = GetActiveDrawWindow(core->app);
	FLOAT_T zoom = active_canvas != NULL ? active_canvas->zoom_rate : 1;
	core->change_zoom(zoom, core);
}

void DefaultBrushGUI::brushOpacityChanged(FLOAT_T opacity)
{

	core->opacity = opacity * 0.01;
	BrushCoreSetCirclePattern(core, core->radius, core->outline_hardness, core->blur,
								core->opacity, *core->color);
}

void DefaultBrushGUI::brushOutlineHardnessChanged(FLOAT_T outline_hardness)
{
	core->outline_hardness = outline_hardness * 0.01;
	BrushCoreSetCirclePattern(core, core->radius, core->outline_hardness, core->blur,
								core->opacity, *core->color);
}

void DefaultBrushGUI::brushBlurChanged(FLOAT_T blur)
{
	core->blur = blur * 0.01;
	BrushCoreSetCirclePattern(core, core->radius, core->outline_hardness, core->blur,
								core->opacity, *core->color);
}

void DefaultBrushGUI::brushBlendModeChanged(int blend_mode)
{
	BrushGUI_Base::brushBlendModeChanged(blend_mode);
}

QWidget* DefaultBrushGUI::createDetailUI()
{
	APPLICATION *app = core->app;
	
	ToolBoxWidget *tool_box = (ToolBoxWidget*)app->tool_box.widgets;
	QWidget *vbox_widget = new QWidget(tool_box->getDetailLayoutWidget());
	QVBoxLayout *vbox = new QVBoxLayout(tool_box->getDetailLayoutWidget());
	
	vbox_widget->setLayout(vbox);
	
	BrushScaleSelector *scale_selector = new BrushScaleSelector(vbox_widget,
			app->labels->tool_box.brush_scale, this);
	QWidget *base_scale_selector = BaseScaleSelectorNew(vbox_widget,
			this, scale_selector, app);
	vbox->addWidget(base_scale_selector);
	vbox->addWidget(scale_selector);
	
	BrushOpacitySelector *opacity_selector = new BrushOpacitySelector(vbox_widget,
			app->labels->tool_box.flow, this);
	vbox->addWidget(opacity_selector);
	
	BrushOutlineHardnessSelector *outline_selector =
		new BrushOutlineHardnessSelector(vbox_widget, app->labels->tool_box.outline_hardness, this);
	vbox->addWidget(outline_selector);
	
	BrushBlurSelector *blur_selector =
		new BrushBlurSelector(vbox_widget, app->labels->tool_box.blur, this);
	vbox->addWidget(blur_selector);
	
	QWidget *blend_mode_selector = BrushBlendModeSelectorNew(vbox_widget, this, app);
	vbox->addWidget(blend_mode_selector);
	
	QWidget *pressure_setting = BrushPressureCheckBoxNew(vbox_widget, this, app);
	vbox->addWidget(pressure_setting);
	
	BrushAntialiasCheckBox *antialias =
		new BrushAntialiasCheckBox(vbox_widget, app->labels->tool_box.anti_alias, this);
	vbox->addWidget(antialias);
		
	return vbox_widget;
}

void CreateDefaultBrushDetailUI(APPLICATION* app, BRUSH_CORE* core)
{
	ToolBoxWidget *tool_box = (ToolBoxWidget*)app->tool_box.widgets;
	BrushGUI_Base *brush = (BrushGUI_Base*)core->brush_data;
	QLayout *layout = tool_box->getDetailLayoutWidget()->layout();
	
	BoxClear((void*)layout);
	
	layout->addWidget(brush->createDetailUI());
}

#ifdef __cplusplus
}
#endif
