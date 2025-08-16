#ifndef _INCLUDED_COMMON_TOOLS_QT_H_
#define _INCLUDED_COMMON_TOOLS_QT_H_

#include <QWidget>
#include "../../common_tools.h"

class CommonToolsGUI_Base
{
public:
	CommonToolsGUI_Base(COMMON_TOOL_CORE* core);
	
	virtual QWidget* createDetailUI();
	
protected:
	COMMON_TOOL_CORE *core;
};

class ColorPickerGUI : public CommonToolsGUI_Base
{
public:
	ColorPickerGUI(COMMON_TOOL_CORE* core);
	
	virtual QWidget* createDetailUI();
};

#endif /* #ifndef _INCLUDED_COMMON_TOOLS_QT_H_ */
