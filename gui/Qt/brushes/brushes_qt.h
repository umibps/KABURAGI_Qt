#ifndef _INCLUDED_BRUSHES_QT_H_
#define _INCLUDED_BRUSHES_QT_H_

#include <QWidget>
#include "../../brush_core.h"
#include "../spin_scale_qt.h"

class BrushGUI_Base
{
public:
	BrushGUI_Base(BRUSH_CORE* core);
	
	virtual QWidget* createDetailUI();
	virtual void brushScaleChanged(FLOAT_T scale);
	virtual void brushOpacityChanged(FLOAT_T opacity);
	virtual void brushOutlineHardnessChanged(FLOAT_T outline_hardness);
	virtual void brushBlurChanged(FLOAT_T blur);
	virtual void brushBlendModeChanged(int blend_mode);
	virtual void setBrushPressureSize(int enabled);
	virtual void setBrushPressureOpacity(int enabled);
	virtual void setAntialias(int enabled);
	
	BRUSH_CORE* brushCore();
	int baseScale();
	
protected:
	BRUSH_CORE *core;
	int base_scale;
};

class DefaultBrushGUI : public BrushGUI_Base
{
public:
	DefaultBrushGUI(BRUSH_CORE* core);
	
	QWidget* createDetailUI();
	virtual void brushScaleChanged(FLOAT_T scale);
	virtual void brushOpacityChanged(FLOAT_T opacity);
	virtual void brushOutlineHardnessChanged(FLOAT_T outline_hardness);
	virtual void brushBlurChanged(FLOAT_T blur);
	virtual void brushBlendModeChanged(int blend_mode);
};

class PencilGUI : public DefaultBrushGUI
{
public:
	PencilGUI(BRUSH_CORE* core);
	
	QWidget* createDetailUI();
};

class EraserGUI : public DefaultBrushGUI
{
public:
	EraserGUI(BRUSH_CORE* core);

	QWidget* createDetailUI();
};

class BlendBrushGUI : public DefaultBrushGUI
{
public:
	BlendBrushGUI(BRUSH_CORE* core);

	QWidget* createDetailUI();

public slots:
	void blendTargetUnderLayer(bool state);
	void blendTargetCanvas(bool state);
};

#endif  /* #ifndef _INCLUDED_BRUSHES_QT_H_ */
