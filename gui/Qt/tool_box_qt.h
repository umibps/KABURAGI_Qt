#ifndef _INCLUDED_TOOLB_BOX_QT_H_
#define _INCLUDED_TOOLB_BOX_QT_H_

#include <QDockWidget>
#include <QVBoxLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QGridLayout>
#include <QButtonGroup>
#include "color_chooser_qt.h"
#include "brush_button_qt.h"
#include "common_tool_button_qt.h"
#include "../../application.h"

class ToolBoxWidget : public QDockWidget
{
	Q_OBJECT
public:
	ToolBoxWidget(QWidget* parent = NULL, APPLICATION* app = NULL, FLOAT_T gui_scale = 1.0);
	~ToolBoxWidget();

	void changeCurrentTool(eLAYER_TYPE layer_type);
	void changeDetailUI(eLAYER_TYPE layer_type, void* target_tool, int is_common_tool);
	
	QWidget* getDetailLayoutWidget();

	void createCommonToolButtonTable(void);
	void createBrushButtonTable(void);
	void createVectorBrushButtonTable(void);
	void createTextLayerButtonTable(void);
	void createAdjustmentLayerTable(void);

	void allCommonToolButtonUnchecked(void);
	void allBrushButtonUnchecked(void);

#if defined(USE_3D_LAYER) && USE_3D_LAYER != 0
	void create3dLayerTable(void);
#endif

public slots:
	void toolColorChange(const uint8* rgb);

private:
	BrushButton *brush_buttons[BRUSH_TABLE_HEIGHT][BRUSH_TABLE_WIDTH];
	CommonToolButton *common_tool_buttons[COMMON_TOOL_TABLE_HEIGHT][COMMON_TOOL_TABLE_WIDTH];
	ColorChooser color_chooser;
	QVBoxLayout main_layout;
	QWidget main_layout_widget;
	QWidget color_chooser_widget;
	QSplitter brush_splitter;
	QScrollArea brush_table_scroll;
	QScrollArea brush_detail_scroll;
	QGridLayout common_tool_table;
	QButtonGroup common_tool_table_group;
	QWidget common_tool_table_widget;
	QGridLayout brush_table;
	QButtonGroup brush_table_group;
	QWidget brush_table_widget;
	QVBoxLayout detail_layout;
	QWidget detail_layout_widget;
	APPLICATION *app;
	FLOAT_T gui_scale;
};

#endif // #ifndef _INCLUDED_TOOLB_BOX_QT_H_

