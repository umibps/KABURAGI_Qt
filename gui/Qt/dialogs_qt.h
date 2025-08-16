#ifndef _INCLUDED_DIALOGS_QT_H_
#define _INCLUDED_DIALOGS_QT_H_

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QSpinBox>
#include <QLabel>
#include <QWidget>

class NewCanvasDialog : public QDialog
{
	Q_OBJECT
#define DEFAULT_CANVAS_SIZE (1000)
public:
	NewCanvasDialog(QWidget* parent = NULL, int width = DEFAULT_CANVAS_SIZE, int height = DEFAULT_CANVAS_SIZE);
	~NewCanvasDialog();

	int GetWidth(void);
	int GetHeight(void);

private:
	QDialogButtonBox *button_box;
	QGridLayout *main_layout;
	QSpinBox *width_box, *height_box;
	QLabel *width_label, *height_label;
};

#endif // #ifndef _INCLUDED_DIALOGS_QT_H_
