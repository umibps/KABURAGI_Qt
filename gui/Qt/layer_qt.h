#ifndef _INCLUDED_LAYER_QT_H_
#define _INCLUDED_LAYER_QT_H_

#include <set>
#include <QWidget>
#include <QPixmap>
#include <QImage>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QAbstractItemModel>
#include <QDropEvent>
#include <QGraphicsView>
#include <QLabel>
#include <QDockWidget>
#include <QPushButton>
#include <QCheckBox>
#include "../../layer.h"
#include "qt_widgets.h"

class LayerItem;
class LayerWindowWidget;

class LayerVisibilityCheckBox : public ImageCheckBox
{
public:
	LayerVisibilityCheckBox(LAYER* layer = NULL, QWidget* parent = NULL,
					const char* image_file_path = NULL, qreal scale = 1.0);
protected:
	void mousePressEvent(QMouseEvent* event) override;
private:
	LAYER *layer;
};

class LayerPinCheckBox : public ImageCheckBox
{
public:
	LayerPinCheckBox(LAYER* layer = NULL, QWidget* parent = NULL,
		const char* image_file_path = NULL, qreal scale = 1.0);
protected:
	void mousePressEvent(QMouseEvent* event) override;
private:
	LAYER* layer;
};

class LayerOpacitySelector : public SpinScale
{
public:
	LayerOpacitySelector(QWidget* parent = NULL, QString caption = "",
							bool button_layout_vertical = true, APPLICATION* app = NULL);
	void valueChanged(int value);
private:
	APPLICATION *app;
};

class LayerBlendModeSelector : public QComboBox
{
	Q_OBJECT
public:
	LayerBlendModeSelector(QWidget* parent = NULL, APPLICATION* app = NULL);
protected:
	void blendModeChanged(int blend_mode);
private:
	APPLICATION *app;
};

class LayerThumbnailWidget : public QGraphicsView
{
public:
	LayerThumbnailWidget(QWidget* parent = NULL, LAYER* layer = NULL,
						 FLOAT_T scale = 1.0, LayerWindowWidget* window_widget = NULL);
	~LayerThumbnailWidget();
protected:
	void paintEvent(QPaintEvent* event) override;
private:
	LAYER *layer;
	FLOAT_T scale;
	FLOAT_T zoom;
	int size;
	uint8 *pixels;
	QImage image;
	LayerWindowWidget *window_widget;
};

class LayerMainWidget : public QWidget
{
public:
	LayerMainWidget(QWidget* parent = NULL, LAYER* layer = NULL);

protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void paintEvent(QPaintEvent* event) override;

private:
	LAYER *layer;
	int mouse_press_x, mouse_press_y;
	bool in_dragging;
};

class LayerWidget : public QTreeWidgetItem
{
public:
	LayerWidget(QTreeWidget* parent = NULL, LAYER* layer = NULL, const char* eye_image_file_path = NULL,
				const char* pin_image_file_path = NULL, LayerWindowWidget* window_widget = NULL);
	bool operator < (const LayerWidget& other) const;
	LAYER* getLayer();
	void updateLayerThumbnail();
	void updateLayerName(QString name);
	void updateWidget();
	void setLayerOpacityText(int opacity);
	void setLayerBlendModeText(int blend_mode);
protected:
private:
	LayerMainWidget main_widget;
	QHBoxLayout main_layout;
	QWidget check_box_widget;
	QVBoxLayout check_box_layout;
	LayerVisibilityCheckBox eye;
	LayerPinCheckBox pin;
	LayerThumbnailWidget thumbnail;
	QWidget parameters_widget;
	QVBoxLayout parameters;
	QLabel name;
	QLabel mode;
	QLabel alpha;
	LAYER *layer;
};

class LayerItem
{
public:
	LayerItem(LayerItem* parent_item = NULL, LAYER* = NULL);
	~LayerItem();

	void appendChild(LayerItem* child);

	LayerItem *child(int row);
	int childCount() const;
	int columnCount() const;
	QVariant data(int column) const;
	int row() const;
	LayerItem* parentItem();

private:
	LAYER *layer;
	LayerItem *parent_item;
	QVector<LayerItem*> child_items;
};

class LayerWidgetItemModel : public QAbstractItemModel
{
public:
	LayerWidgetItemModel(LAYER* layer = NULL, QObject* parent = NULL, LayerItem* parent_item = NULL);
	~LayerWidgetItemModel();

	QVariant data(const QModelIndex& index, int role) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	QVariant headerData(int section, Qt::Orientation orientation,
						int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column,
					  const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &index) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

private:
	LayerItem *root_item;
};

struct _LAYER_WIDGET
{
	LayerWidget *widget;
	LayerItem *item;
};

class LayerViewWidget : public QTreeWidget
{
	Q_OBJECT
public:
	LayerViewWidget(QWidget* parent = NULL);
	void dropEvent(QDropEvent* event) override;
	void setCurrentCanvas(DRAW_WINDOW* canvas);
	static QString layerMimeType() {return QStringLiteral("KABURAGI_Canvas/layer");}
protected:
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragMoveEvent(QDragMoveEvent* event) override;
private:
	DRAW_WINDOW *current_canvas;
	LayerWidgetItemModel view_model;
};

class LayerWindowWidget : public QDockWidget
{
	Q_OBJECT
public:
	LayerWindowWidget(QWidget* parent = NULL, APPLICATION* app = NULL);
	LayerViewWidget* getLayerViewWidget();
	const uint8* getThumbnailBackgroundPixels();

	void setActiveLayer(LAYER* layer);
	void initializeLayerBlendModeComboBox(void);
	void moveActiveLayer(LAYER* layer);

	void setActiveLayerOpacity(int opacity);

public slots:
	void newLayerButtonClicked();
	void deleteLayerButtonClicked();
	void layerOpacityChanged(int opacity);
	void lockOpacityChanged(bool state);
	void maskUnderLayerChanged(bool state);
private:
	QScrollArea scroll_area;
	
	APPLICATION *app;
	uint8 *thumnail_background_pixels;

	QWidget main_layout_widget;
	QVBoxLayout main_layout;

	QCheckBox lock_opacity;
	QCheckBox mask_with_under;
	
	QPushButton merge_down;

	LayerViewWidget layer_view;

	QWidget buttons_layout_widget;
	QGridLayout buttons_layout;
	QPushButton new_layer_button;
	QPushButton delete_layer_button;

	LayerWidgetItemModel view_model;

	LayerBlendModeSelector blend_mode_selector;
	QLabel blend_mode_label;
	QWidget blend_mode_layout_widget;
	QHBoxLayout blend_mode_layout;
	LayerOpacitySelector layer_opacity;
};

#ifdef __cplusplus
extern "C" {
#endif

extern void UpdateLayerWidgets(DRAW_WINDOW* canvas);

#ifdef __cplusplus
}
#endif

#endif // #ifndef _INCLUDED_LAYER_QT_H_
