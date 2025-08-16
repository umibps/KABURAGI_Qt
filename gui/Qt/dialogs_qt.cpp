#include <limits.h>
#include <QScreen>
#include <QMessageBox>
#include <QGuiApplication>
#include "../../application.h"
#include "dialogs_qt.h"
#include "../gui_enum.h"
#include "../gui.h"

NewCanvasDialog::NewCanvasDialog(QWidget* parent, int width, int height)
	: QDialog(parent)
{
	main_layout = new QGridLayout(this);
	setLayout(main_layout);

	width_label = new QLabel(tr("width"), this);
	main_layout->addWidget(width_label, 0, 0);
	width_box = new QSpinBox(this);
	width_box->setRange(1, INT_MAX);
	width_box->setValue(width);
	main_layout->addWidget(width_box, 0, 1);
	height_label = new QLabel(tr("height"), this);
	main_layout->addWidget(height_label, 1, 0);
	height_box = new QSpinBox(this);
	height_box->setRange(1, INT_MAX);
	height_box->setValue(height);
	main_layout->addWidget(height_box, 1, 1);

	button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
									  this);
	main_layout->addWidget(button_box, 2, 0);

	(void)connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	(void)connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

NewCanvasDialog::~NewCanvasDialog()
{
	delete width_box;
	delete height_box;
	delete width_label;
	delete height_label;
	delete button_box;
	delete main_layout;
}

int NewCanvasDialog::GetWidth(void)
{
	return width_box->value();
}

int NewCanvasDialog::GetHeight(void)
{
	return height_box->value();
}

#ifdef __cplusplus
extern "C" {
#endif

void* MessageDialogNew(const char* message, void* parent_window, eDIALOG_TYPE dialog_type)
{
	QMessageBox *message_box = new QMessageBox();
	message_box->setParent((QWidget*)parent_window);
	message_box->setText(message);
	return (void*)message_box;
}

eDIALOG_RESULT MessageDialogExecute(void* dialog)
{
	QMessageBox *message_box = (QMessageBox*)dialog;
	int result = message_box->exec();
	if(result == QMessageBox::Ok)
	{
		return DIALOG_RESULT_OK;
	}
	
	return DIALOG_RESULT_CLOSE;
}

void MessageDialogDestroy(void* dialog)
{
	QMessageBox *message_box = (QMessageBox*)dialog;
	delete message_box;
}

void MessageDialogMoveToParentCenter(void* dialog)
{
	if(dialog != NULL)
	{
		const QWidget *parent = ((QWidget*)dialog)->parentWidget();
		QRect rectangle = parent->geometry();
		((QWidget*)dialog)->move(rectangle.center() - ((QWidget*)dialog)->rect().center());
	}
	else
	{
		QRect screen_geometry = QGuiApplication::screens()[0]->geometry();
		int x = (screen_geometry.width() - ((QWidget*)dialog)->width()) / 2;
		int y = (screen_geometry.height() - ((QWidget*)dialog)->height()) / 2;
		((QWidget*)dialog)->move(x, y);
	}
}

void ExecuteExtendSelectionArea(APPLICATION* app)
{
#define MAX_EXTEND_PIXEL 100
#undef MAX_EXTEND_PIXEL
}

#ifdef __cplusplus
}
#endif
