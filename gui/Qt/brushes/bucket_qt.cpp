#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include "../../application.h"
#include "brushes_qt.h"
#include "brushes_widgets_qt.h"
#include "../tool_box_qt.h"
#include "../../brushes.h"

class BucketThresholdSelector : public SpinScale
{
public:
	BucketThresholdSelector(
		QWidget* parent = NULL,
		BUCKET* bucket = NULL
	) : SpinScale(parent, QWidget::tr("Threshold"))
	{
		this->bucket = bucket;
		if(bucket != NULL)
		{
			setValue(bucket->threshold);
			setMinimum(1);
			setMaximum(250);
		}

		connect(this, &SpinScale::valueChanged, this, &BucketThresholdSelector::changeThreshold);
	}

	void changeThreshold(int threshold)
	{
		bucket->threshold = threshold;
	}

private:
	BUCKET *bucket;
};

class BucketTargetSelector : public QRadioButton
{
public:
	BucketTargetSelector(
		QWidget* parent = NULL,
		BUCKET* bucket = NULL,
		eBUCKET_TARGET target = BUCKET_TARGET_ACTIVE_LAYER
	)
		: QRadioButton(parent)
	{
		this->bucket = bucket;
		this->target = target;

		QWidget::connect(this, &QRadioButton::toggled, this, &BucketTargetSelector::targetChange);
	}

	void targetChange()
	{
		if(isChecked())
		{
			bucket->target = target;
		}
	}

private:
	BUCKET *bucket;
	eBUCKET_TARGET target;
};

class BucketModeSelector : public QRadioButton
{
public:
	BucketModeSelector(
		QWidget* parent = NULL,
		BUCKET* bucket = NULL,
		eBUCKET_SELECT_MODE select_mode = BUCKET_SELECT_MODE_RGB
	)
		: QRadioButton(parent)
	{
		this->bucket = bucket;
		this->select_mode = select_mode;

		QWidget::connect(this, &QRadioButton::toggled, this, &BucketModeSelector::modeChange);
	}

	void modeChange()
	{
		if(isChecked())
		{
			bucket->select_mode = select_mode;
		}
	}

private:
	BUCKET* bucket;
	eBUCKET_SELECT_MODE select_mode;
};

class BucketExtendSelector : public SpinScale
{
public:
	BucketExtendSelector(QWidget* parent = NULL, BUCKET* bucket = NULL)
		: SpinScale(parent, QWidget::tr("Extend"))
	{
		this->bucket = bucket;
		if(bucket != NULL)
		{
			this->setValue(bucket->extend);
		}
		connect(this, &SpinScale::valueChanged, this, &BucketExtendSelector::setExtend);
	}

	void setExtend(int extend)
	{
		bucket->extend = extend;
	}

private:

	BUCKET *bucket;
};

BucketGUI::BucketGUI(BRUSH_CORE* core) : BrushGUI_Base(core)
{

}

QWidget* BucketGUI::createDetailUI()
{
	APPLICATION *app = core->app;
	BUCKET *bucket = (BUCKET*)core;

	QVBoxLayout *radio_button_layout;
	QGroupBox *group_box;

	ToolBoxWidget *tool_box = (ToolBoxWidget*)app->tool_box.widgets;
	QWidget *vbox_widget = new QWidget(tool_box->getDetailLayoutWidget());
	QVBoxLayout *vbox = new QVBoxLayout(tool_box->getDetailLayoutWidget());

	vbox_widget->setLayout(vbox);

	BucketThresholdSelector *threshold_selector = new BucketThresholdSelector(vbox_widget, bucket);
	vbox->addWidget(threshold_selector);

	BrushOpacitySelector *opacity_selector = new BrushOpacitySelector(vbox_widget,
		app->labels->tool_box.flow, this);
	vbox->addWidget(opacity_selector);

	BucketExtendSelector *extend_selector = new BucketExtendSelector(vbox_widget, bucket);
	vbox->addWidget(extend_selector);

	radio_button_layout = new QVBoxLayout(vbox_widget);
	group_box = new QGroupBox(vbox_widget);
	group_box->setLayout(radio_button_layout);
	group_box->setTitle(QWidget::tr("Area Detect Target"));

	BucketTargetSelector *target_active_layer =
		new BucketTargetSelector(vbox_widget, bucket, BUCKET_TARGET_ACTIVE_LAYER);
	target_active_layer->setText(QWidget::tr("Active Layer"));
	radio_button_layout->addWidget(target_active_layer);
	BucketTargetSelector *target_canvas =
		new BucketTargetSelector(vbox_widget, bucket, BUCKET_TARGET_CANVAS);
	target_canvas->setText(QWidget::tr("Canvas"));
	radio_button_layout->addWidget(target_canvas);
	switch(bucket->target)
	{
	case BUCKET_TARGET_ACTIVE_LAYER:
		target_active_layer->setChecked(true);
		break;
	case BUCKET_TARGET_CANVAS:
		target_canvas->setChecked(true);
		break;
	default:
		bucket->target = BUCKET_TARGET_ACTIVE_LAYER;
		target_active_layer->setChecked(true);
	}
	vbox->addWidget(group_box);

	radio_button_layout = new QVBoxLayout(vbox_widget);
	group_box = new QGroupBox(vbox_widget);
	group_box->setLayout(radio_button_layout);
	group_box->setTitle(QWidget::tr("Area Detect Mode"));

	BucketModeSelector *rgb_mode =
		new BucketModeSelector(vbox_widget, bucket, BUCKET_SELECT_MODE_RGB);
	rgb_mode->setText(QWidget::tr("RGB"));
	radio_button_layout->addWidget(rgb_mode);
	BucketModeSelector *rgba_mode =
		new BucketModeSelector(vbox_widget, bucket, BUCKET_SELECT_MODE_RGBA);
	rgba_mode->setText(QWidget::tr("RGBA"));
	radio_button_layout->addWidget(rgba_mode);
	BucketModeSelector *alpha_mode =
		new BucketModeSelector(vbox_widget, bucket, BUCKET_SELECT_MODE_ALPHA);
	alpha_mode->setText(QWidget::tr("Alpha"));
	radio_button_layout->addWidget(alpha_mode);
	switch(bucket->select_mode)
	{
	case BUCKET_SELECT_MODE_RGB:
		rgb_mode->setChecked(true);
		break;
	case BUCKET_SELECT_MODE_RGBA:
		rgba_mode->setChecked(true);
		break;
	case BUCKET_SELECT_MODE_ALPHA:
		alpha_mode->setChecked(true);
		break;
	default:
		bucket->select_mode = BUCKET_SELECT_MODE_RGB;
		rgb_mode->setChecked(true);
	}
	vbox->addWidget(group_box);

	return vbox_widget;
}

void BucketGUI::setExtend(int extend)
{
	BUCKET *bucket = (BUCKET*)core;
	bucket->extend = extend;
}

#ifdef __cplusplus
extern "C" {
#endif

void* BucketGUI_New(APPLICATION* app, BRUSH_CORE* core)
{
	ToolBoxWidget* tool_box = (ToolBoxWidget*)app->widgets;

	QWidget *detail_layout_widget = tool_box->getDetailLayoutWidget();
	BucketGUI *bucket = new BucketGUI(core);
	core->brush_data = (void*)bucket;

	return (void*)bucket;
}

#ifdef __cplusplus
}
#endif
