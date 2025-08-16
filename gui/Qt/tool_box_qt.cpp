#include <stdio.h>
#include <QSpacerItem>
#include "../../ini_file.h"
#include "brush_button_qt.h"
#include "common_tool_button_qt.h"
#include "tool_box_qt.h"
#include "qt_widgets.h"

ToolBoxWidget::ToolBoxWidget(QWidget* parent, APPLICATION* app, FLOAT_T gui_scale)
	: QDockWidget(tr("Tool Box"), parent),
	  color_chooser(parent, &app->tool_box.color_chooser, (int)(COLOR_CIRCLE_SIZE * gui_scale),
					(int)(COLOR_CIRCLE_SIZE * gui_scale), (int)(COLOR_CIRCLE_WIDTH * gui_scale)),
	  color_chooser_widget(parent),
	  main_layout(parent),
	  main_layout_widget(parent),
	  brush_splitter(Qt::Vertical, parent),
	  brush_table_scroll(parent),
	  brush_detail_scroll(parent),
	  common_tool_table(parent),
	  common_tool_table_group(parent),
	  common_tool_table_widget(parent),
	  brush_table(parent),
	  brush_table_group(parent),
	  detail_layout(parent),
	  detail_layout_widget(parent)
{
#define BRUSH_SPLITTER_WIDTH 4
	this->app = app;
	app->tool_box.widgets = (TOOL_WINDOW_WIDGETS*)this;
	color_chooser_widget.setLayout(&color_chooser);
	main_layout_widget.setLayout(&main_layout);
	setWidget(&main_layout_widget);
	main_layout.addWidget(&color_chooser_widget);

	common_tool_table.setSizeConstraint(QLayout::SetFixedSize);
	common_tool_table.setContentsMargins(0, 0, 0, 0);
	common_tool_table.setSpacing(0);
	common_tool_table_widget.setLayout(&common_tool_table);
	main_layout.addWidget(&common_tool_table_widget);

	brush_table.setSizeConstraint(QLayout::SetFixedSize);
	brush_table.setContentsMargins(0, 0, 0, 0);
	brush_table.setSpacing(0);
	brush_table_widget.setLayout(&brush_table);
	brush_table_scroll.setWidget(&brush_table_widget);
	brush_table_scroll.setWidgetResizable(true);
	main_layout.addWidget(&brush_splitter);

	brush_splitter.setHandleWidth(BRUSH_SPLITTER_WIDTH);
	brush_splitter.addWidget(&brush_table_scroll);
	detail_layout_widget.setLayout(&detail_layout);
	brush_splitter.addWidget(&detail_layout_widget);

	connect(&color_chooser, &ColorChooser::colorChanged,
				this, &ToolBoxWidget::toolColorChange);
}

ToolBoxWidget::~ToolBoxWidget()
{

}

void ToolBoxWidget::changeCurrentTool(eLAYER_TYPE layer_type)
{
	ClearLayout(&brush_table);

	switch(layer_type)
	{
	case TYPE_NORMAL_LAYER:
		createBrushButtonTable();
		break;
	case TYPE_VECTOR_LAYER:
		createVectorBrushButtonTable();
		break;
	case TYPE_TEXT_LAYER:
		createTextLayerButtonTable();
		break;
	case TYPE_LAYER_SET:
		break;
	case TYPE_ADJUSTMENT_LAYER:
		createAdjustmentLayerTable();
		break;
#if defined(USE_3D_LAYER) && USE_3D_LAYER != 0
	case TYPE_3D_LAYER:
		create3dLayerTable();
		break;
#endif
	default:
		break;
	}
}

void ToolBoxWidget::changeDetailUI(eLAYER_TYPE layer_type, void* target_tool, int is_common_tool)
{
	if(app->tool_box.current_detail_ui == target_tool)
	{
		return;
	}
	
	BoxClear((void*)&detail_layout);
	if(is_common_tool)
	{
		COMMON_TOOL_CORE *core = (COMMON_TOOL_CORE*)target_tool;
		core->create_detail_ui(app, core);
	}
	else
	{
		switch(layer_type)
		{
		case TYPE_NORMAL_LAYER:
			{
				BRUSH_CORE *core = (BRUSH_CORE*)target_tool;
				core->create_detail_ui(app, core);
			}
			break;
		case TYPE_VECTOR_LAYER:
			{
				VECTOR_BRUSH_CORE *core = (VECTOR_BRUSH_CORE*)target_tool;
				core->create_detail_ui(app, core);
			}
		case TYPE_TEXT_LAYER:
			break;
		case TYPE_3D_LAYER:
			break;
		case TYPE_LAYER_SET:
			break;
		case TYPE_ADJUSTMENT_LAYER:
			break;
		}
	}
	
	app->tool_box.current_detail_ui = target_tool;
}

QWidget* ToolBoxWidget::getDetailLayoutWidget()
{
	return &detail_layout_widget;
}

void ToolBoxWidget::createCommonToolButtonTable()
{
	for(int y = 0; y < COMMON_TOOL_TABLE_HEIGHT; y++)
	{
		for(int x = 0; x < COMMON_TOOL_TABLE_WIDTH; x++)
		{
			char *image_path = NULL;
			bool checkable = false;
			if(app->tool_box.common_tools[y][x] != NULL)
			{
				image_path = app->tool_box.common_tools[y][x]->image_file_path;
				checkable = true;
			}
			CommonToolButton* button = new CommonToolButton(this, image_path, app->tool_box.common_tools[y][x]);
			button->setCheckable(checkable);
			common_tool_table.addWidget(button, y, x, Qt::AlignLeft | Qt::AlignTop);
			common_tool_table_group.addButton(button);

			common_tool_buttons[y][x] = button;
		}
	}
}

void ToolBoxWidget::createBrushButtonTable(void)
{
	for(int y=0; y<BRUSH_TABLE_HEIGHT; y++)
	{
		for(int x=0; x<BRUSH_TABLE_WIDTH; x++)
		{
			char *name = NULL, *image_path = NULL;
			bool checkable = false;
			if(app->tool_box.brushes[y][x] != NULL)
			{
				name = app->tool_box.brushes[y][x]->name;
				image_path = app->tool_box.brushes[y][x]->image_file_path;
				checkable = true;
			}
			BrushButton *button = new BrushButton(this,
					image_path, QString(name), app->tool_box.brushes[y][x]);
			button->setCheckable(checkable);
			brush_table.addWidget(button, y, x, Qt::AlignLeft | Qt::AlignTop);
			brush_table_group.addButton(button);

			brush_buttons[y][x] = button;
		}
	}

	QSpacerItem *horizontal_spacer_item = new QSpacerItem(BRUSH_BUTTON_SIZE, BRUSH_BUTTON_SIZE,
							QSizePolicy::Minimum, QSizePolicy::Minimum);
	brush_table.addItem(horizontal_spacer_item, 0, BRUSH_TABLE_WIDTH+1, 1, 1, Qt::AlignLeft);
	QSpacerItem *vertical_spacer_item = new QSpacerItem(BRUSH_BUTTON_SIZE, BRUSH_BUTTON_SIZE,
														  QSizePolicy::Minimum, QSizePolicy::Minimum);
	brush_table.addItem(vertical_spacer_item, BRUSH_TABLE_HEIGHT+1, 0, 0, 1, Qt::AlignTop);
}

void ToolBoxWidget::createVectorBrushButtonTable(void)
{

}

void ToolBoxWidget::createTextLayerButtonTable(void)
{

}

void ToolBoxWidget::createAdjustmentLayerTable(void)
{

}

void ToolBoxWidget::allCommonToolButtonUnchecked(void)
{
	// ボタングループの排他設定を解除しないと常にどれかがhチェック状態になる為
		// 処理中のみ排他設定を変更する
	common_tool_table_group.setExclusive(false);
	for(int y = 0; y < COMMON_TOOL_TABLE_HEIGHT; y++)
	{
		for(int x = 0; x < COMMON_TOOL_TABLE_WIDTH; x++)
		{
			if(common_tool_buttons[y][x] != NULL)
			{
				common_tool_buttons[y][x]->setChecked(false);
				common_tool_buttons[y][x]->update();
			}
		}
	}
	common_tool_table_group.setExclusive(true);
}

void ToolBoxWidget::allBrushButtonUnchecked(void)
{
	// ボタングループの排他設定を解除しないと常にどれかがhチェック状態になる為
		// 処理中のみ排他設定を変更する
	brush_table_group.setExclusive(false);
	for(int y = 0; y < BRUSH_TABLE_HEIGHT; y++)
	{
		for(int x = 0; x < BRUSH_TABLE_WIDTH; x++)
		{
			if(brush_buttons[y][x] != NULL)
			{
				brush_buttons[y][x]->setChecked(false);
				brush_buttons[y][x]->update();
			}
		}
	}
	brush_table_group.setExclusive(true);
}

#if defined(USE_3D_LAYER) && USE_3D_LAYER != 0
void ToolBoxWidget::create3dLayerTable(void)
{

}
#endif

void ToolBoxWidget::toolColorChange(const uint8* rgb)
{
	DRAW_WINDOW *canvas = GetActiveDrawWindow(app);
	int layer_type = TYPE_NORMAL_LAYER;

	if(canvas != NULL)
	{
		layer_type = canvas->active_layer->layer_type;
	}

	if(app->tool_box.flags & TOOL_USING_BRUSH)
	{
		switch(layer_type)
		{
		case TYPE_NORMAL_LAYER:
			if(app->tool_box.active_brush[app->input]->change_color != NULL)
			{
				app->tool_box.active_brush[app->input]->change_color(rgb, app->tool_box.active_brush[app->input]);
			}
			break;
		case TYPE_VECTOR_LAYER:
			if(app->tool_box.active_vector_brush[app->input]->color_change != NULL)
			{
				app->tool_box.active_vector_brush[app->input]->color_change(rgb, app->tool_box.active_vector_brush[app->input]);
			}
			break;
		}
	}
	else
	{
	}
}

#include "../../application.h"

#ifdef __cplusplus
extern "C" {
#endif

void ChangeCurrentTool(eLAYER_TYPE layer_type, void* application)
{
	APPLICATION *app = (APPLICATION*)application;
	ToolBoxWidget *tool_box
			= (ToolBoxWidget*)app->tool_box.widgets->tool_box;

	tool_box->changeCurrentTool(layer_type);
}

void ChangeDetailUI(
	eLAYER_TYPE layer_type,
	void* target_tool,
	int is_common_tool,
	void* application
)
{
	APPLICATION *app = (APPLICATION*)application;
	ToolBoxWidget *tool_box
			= (ToolBoxWidget*)app->tool_box.widgets->tool_box;
	tool_box->changeDetailUI(layer_type, target_tool, is_common_tool);
}

#ifdef __cplusplus
}
#endif
