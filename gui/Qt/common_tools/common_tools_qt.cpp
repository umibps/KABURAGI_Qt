#include "common_tools_qt.h"
#include "../tool_box_qt.h"
#include "../../application.h"
#include "../../gui.h"

CommonToolsGUI_Base::CommonToolsGUI_Base(
	COMMON_TOOL_CORE* core
)
{
	this->core = core;
}

QWidget* CommonToolsGUI_Base::createDetailUI()
{
	return NULL;
}

#ifdef __cplusplus
extern "C" {
#endif

void CreateDefaultCommonToolDetailUI(APPLICATION* app, COMMON_TOOL_CORE* core)
{
	ToolBoxWidget* tool_box = (ToolBoxWidget*)app->tool_box.widgets;
	CommonToolsGUI_Base *tool = (CommonToolsGUI_Base*)core->tool_data;
	QLayout* layout = tool_box->getDetailLayoutWidget()->layout();

	BoxClear((void*)layout);

	layout->addWidget(tool->createDetailUI());
}

#ifdef __cplusplus
}
#endif
