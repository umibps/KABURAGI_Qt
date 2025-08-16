#include "application.h"

#include <QDir>

#ifdef _MSC_VER
# pragma comment(lib, "freetype27.lib")
# pragma comment(lib, "libpng.lib")
# pragma comment(lib, "zdll.lib")
# ifdef _DEBUG
#  pragma comment(lib, "harfbuzzd.lib")
#  pragma comment(lib, "harfbuzz-subsetd.lib")
#  pragma comment(lib, "Qt6Cored.lib")
#  pragma comment(lib, "Qt6Guid.lib")
#  pragma comment(lib, "Qt6Widgetsd.lib")
# else
#  pragma comment(lib, "harfbuzz.lib")
#  pragma comment(lib, "harfbuzz-subset.lib")
#  pragma comment(lib, "Qt6Core.lib")
#  pragma comment(lib, "Qt6Gui.lib")
#  pragma comment(lib, "Qt6Widgets.lib")
# endif
#endif

int main(int argc, char *argv[])
{
	APPLICATION app = {0};
	APPLICATION_LABELS labels = {0};
	FRACTAL_LABEL fractal_labels = {0};
	app.gui_scale = 1.0;
	app.labels = &labels;
	app.fractal_labels = &fractal_labels;
	QString path = QDir::currentPath();
	char *init_file_path = (char*)"";
	InitializeApplication(&app, argv, argc, init_file_path);

	return 0;
}

/*
#include <QApplication>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDropEvent>
#include <QPushButton>

class TreeView: public QTreeWidget
{
public:
  TreeView()
  {
	resize(200, 300);

	setSelectionMode(QAbstractItemView::SingleSelection);
	setDragEnabled(true);
	viewport()->setAcceptDrops(true);
	setDropIndicatorShown(true);
	setDragDropMode(QAbstractItemView::InternalMove);

	QTreeWidgetItem* parentItem = new QTreeWidgetItem(this);
	parentItem->setText(0, "Test");
	parentItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled);

	for(int i = 0; i < 10; ++i)
	{
	  QTreeWidgetItem* pItem = new QTreeWidgetItem(parentItem);
	  QPushButton *button = new QPushButton(QString("Number %1").arg(i));
	  //pItem->setText(0, QString("Number %1").arg(i) );
	  pItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
	  pItem->addChild(pItem);
	  this->setItemWidget(pItem, 0, button);
	}
  }

private:
  virtual void  dropEvent(QDropEvent * event)
  {
	QModelIndex droppedIndex = indexAt( event->pos() );

	if( !droppedIndex.isValid() )
	  return;

	QTreeWidget::dropEvent(event);
  }
};

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	TreeView widget;
	widget.show();

	return a.exec();
}

/*
class TreeView: public QTreeWidget
{
public:
  TreeView()
  {
	resize(200, 300);

	setSelectionMode(QAbstractItemView::SingleSelection);
	setDragEnabled(true);
	viewport()->setAcceptDrops(true);
	setDropIndicatorShown(true);
	setDragDropMode(QAbstractItemView::InternalMove);

	QTreeWidgetItem* parentItem = new QTreeWidgetItem(this);
	parentItem->setText(0, "Test");
	parentItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled);

	for(int i = 0; i < 10; ++i)
	{
	  QTreeWidgetItem* pItem = new QTreeWidgetItem(parentItem);
	  pItem->setText(0, QString("Number %1").arg(i) );
	  pItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
	  pItem->addChild(pItem);
	}
  }

private:
  virtual void  dropEvent(QDropEvent * event)
  {
	QModelIndex droppedIndex = indexAt( event->pos() );

	if( !droppedIndex.isValid() )
	  return;

	QTreeWidget::dropEvent(event);
  }
};
*/
