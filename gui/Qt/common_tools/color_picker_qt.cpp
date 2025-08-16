#include <QObject>
#include <QMenu>
#include <QPixmap>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QVBoxLayout>
#include <QRadioButton>
#include "../../color.h"
#include "../../application.h"
#include "../draw_window_qt.h"
#include "../../common_tools.h"
#include "../common_tools/common_tools_qt.h"
#include "../tool_box_qt.h"

void ColorHistoryPopupMenu(
	DRAW_WINDOW* canvas,
	const QPoint& position,
	QObject* callback_object,
	void (*set_menu_callback)(QTableWidget*, void*)
)
{
	APPLICATION *app;
	
	app = canvas->app;
	if(app->tool_box.color_chooser.num_color_history == 0)
	{
		return;
	}
	
	QTableWidget *table = new QTableWidget(canvas->widgets->window);

	int row, colomn;
	for(int i = 0, row = 0, column = 0; app->tool_box.color_chooser.num_color_history; i++, colomn++)
	{
		if(column >= PICKER_POPUP_TABLE_WIDTH)
		{
			row++;
			column = 0;
		}
		
		QPixmap icon_pixmap(PICKER_POPUP_ICON_SIZE, PICKER_POPUP_ICON_SIZE);
		uint8 *color = app->tool_box.color_chooser.color_history[i];
		icon_pixmap.fill(QColor(color[0], color[1], color[2], 255));
		QIcon icon(icon_pixmap);
		
		QTableWidgetItem *item = table->itemAt(column, row);
		item->setIcon(icon);
	}
	
	QWidget *action_widget = new QWidget();
	QVBoxLayout *layout = new QVBoxLayout(action_widget);
	layout->addWidget(table);
	action_widget->setLayout(layout);
	
	set_menu_callback(table, callback_object);
	
	QWidgetAction *action = new QWidgetAction(canvas->widgets->window);
	action->setDefaultWidget(action_widget);
	
	QMenu *menu = new QMenu(canvas->widgets->window);
	menu->addAction(action);
	
	table->adjustSize();
	menu->setFixedSize(table->size());
	
	menu->exec(canvas->widgets->window->mapToGlobal(position));
}

class ColorPickerModeButton : public QRadioButton
{
private:
	COLOR_PICKER *picker;
	eCOLOR_PICKER_SOURCE mode;
public:
	ColorPickerModeButton(QWidget* parent, COLOR_PICKER* picker, eCOLOR_PICKER_SOURCE mode)
		: QRadioButton(parent)
	{
		this->picker = picker;
		this->mode = mode;
	}
	
	void toggle()
	{
		QRadioButton::toggle();
		
		if(isChecked())
		{
			picker->mode = mode;
		}
	}
};

ColorPickerGUI::ColorPickerGUI(COMMON_TOOL_CORE* core)
	: CommonToolsGUI_Base(core)
{

}

QWidget* ColorPickerGUI::createDetailUI()
{
	COLOR_PICKER *picker = (COLOR_PICKER*)this->core;
	APPLICATION *app = core->app;

	ToolBoxWidget *tool_box = (ToolBoxWidget*)app->tool_box.widgets;
	QWidget *vbox_widget = new QWidget(tool_box->getDetailLayoutWidget());
	QVBoxLayout *layout = new QVBoxLayout(tool_box->getDetailLayoutWidget());
	
	vbox_widget->setLayout(layout);
	
	ColorPickerModeButton *layer_button = new ColorPickerModeButton(vbox_widget, picker, COLOR_PICKER_SOURCE_ACTIVE_LAYER);
	layer_button->setText(app->labels->tool_box.select.active_layer);
	ColorPickerModeButton *canvas_button = new ColorPickerModeButton(vbox_widget, picker, COLOR_PICKER_SOURCE_CANVAS);
	canvas_button->setText(app->labels->tool_box.select.canvas);
	
	layout->addWidget(layer_button);
	layout->addWidget(canvas_button);
	
	return vbox_widget;
}

#ifdef __cplusplus
extern "C" {
#endif

void* ColorPickerGUI_New(APPLICATION* app, COMMON_TOOL_CORE* core)
{
	ToolBoxWidget* tool_box = (ToolBoxWidget*)app->widgets;

	QWidget* detail_layout_widget = tool_box->getDetailLayoutWidget();
	ColorPickerGUI *picker = new ColorPickerGUI(core);
	core->tool_data = (void*)picker;

	return (void*)picker;
}

#ifdef __cplusplus
}
#endif
