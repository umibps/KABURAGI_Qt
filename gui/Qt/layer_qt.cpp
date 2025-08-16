#include <QImage>
#include <QPainter>
#include <QDrag>
#include <QMimeData>
#include <QApplication>
#include <QScrollBar>
#include "layer_qt.h"
#include "qt_widgets.h"
#include "mainwindow.h"
#include "../../layer_window.h"
#include "../../draw_window.h"
#include "../../application.h"
#include "../../memory.h"
#include "../draw_window.h"

#include <QMessageBox>

#ifdef __cplusplus
extern "C" {
#endif

static INLINE int GetLayerIndex(LAYER* layer)
{
	LAYER *compare = layer->window->layer;
	int index = 0;

	while(layer != compare && compare != NULL)
	{
		index++;
		compare = compare->next;
	}

	return index;
}

static INLINE LAYER* GetLayerFromRowIndex(DRAW_WINDOW* canvas, int row)
{
	int target_index = canvas->num_layer - row - 1;
	LAYER *result = canvas->layer;

	if(target_index < 0)
	{
		return NULL;
	}

	for(int i = 0; i<target_index; i++, result = result->next)
	{
		if(result == NULL)
		{
			return NULL;
		}
	}

	return result;
}

void InitializeLayerContext(LAYER* layer)
{
}

void ReleaseLayerContext(LAYER* layer)
{
}

void LayerViewAddLayer(LAYER* layer, LAYER* bottom, void* view, uint16 num_layer)
{
	LayerViewWidget *view_widget = layer->window->app->widgets->layer_window->getLayerViewWidget();
	layer->widget = (LAYER_WIDGET*)MEM_CALLOC_FUNC(1, sizeof(*layer->widget));
	layer->widget->widget = new LayerWidget(NULL, layer, (const char*)"./image/eye.png",
											(const char*)"./image/pin.png", layer->window->app->widgets->layer_window);

}

void DeleteLayerWidget(LAYER* layer)
{
	if(layer->widget != NULL)
	{
		delete layer->widget->widget;
		MEM_FREE_FUNC(layer->widget);
		layer->widget = NULL;
	}
}

LayerVisibilityCheckBox::LayerVisibilityCheckBox(LAYER* layer, QWidget* parent, const char* image_file_path, qreal scale)
	: ImageCheckBox(parent, image_file_path, scale)
{
	this->layer = layer;

	if(layer != NULL)
	{
		if((layer->flags & LAYER_FLAG_INVISIBLE) == 0)
		{
			setCheckState(Qt::Checked);
		}
		else
		{
			setCheckState(Qt::Unchecked);
		}
	}
}

void LayerVisibilityCheckBox::mousePressEvent(QMouseEvent* event)
{
	QCheckBox::mousePressEvent(event);

	if(layer == NULL)
	{
		return;
	}

	if(checkState() == Qt::Unchecked)
	{
		layer->flags &= ~(LAYER_FLAG_INVISIBLE);
	}
	else
	{
		layer->flags |= LAYER_FLAG_INVISIBLE;
	}

	layer->window->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
	ForceUpdateCanvasWidget(layer->window->widgets);
}

LayerPinCheckBox::LayerPinCheckBox(LAYER* layer, QWidget* parent, const char* image_file_path, qreal scale)
	: ImageCheckBox(parent, image_file_path, scale)
{
	this->layer = layer;

	if(layer != NULL)
	{
		if(layer->flags & LAYER_CHAINED)
		{
			setCheckState(Qt::Checked);
		}
		else
		{
			setCheckState(Qt::Unchecked);
		}
	}
}

void LayerPinCheckBox::mousePressEvent(QMouseEvent* event)
{
	QCheckBox::mousePressEvent(event);

	if(layer == NULL)
	{
		return;
	}

	if(checkState() == Qt::Unchecked)
	{
		layer->flags |= LAYER_CHAINED;
	}
	else
	{
		layer->flags &= ~(LAYER_CHAINED);
	}
}

LayerOpacitySelector::LayerOpacitySelector(
	QWidget* parent,
	QString caption,
	bool button_layout_vertical,
	APPLICATION* app
) : SpinScale(parent, caption, button_layout_vertical)
{
	this->app = app;
}

void LayerOpacitySelector::valueChanged(int value)
{
	SpinScale::valueChanged(value);
	if(app != NULL)
	{
		DRAW_WINDOW *canvas = GetActiveDrawWindow(app);
		if(canvas != NULL)
		{
			LAYER *active_layer = canvas->active_layer;
			if(active_layer != NULL)
			{
				active_layer->alpha = value;
				canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
				UpdateCanvasWidget(canvas->widgets);
				active_layer->widget->widget->setLayerOpacityText(value);
			}
		}
	}
}

LayerBlendModeSelector::LayerBlendModeSelector(QWidget* parent, APPLICATION* app)
	: QComboBox(parent)
{
	this->app = app;
	connect(this, &QComboBox::currentIndexChanged, this, &LayerBlendModeSelector::blendModeChanged);
}

void LayerBlendModeSelector::blendModeChanged(int blend_mode)
{
	if(app != NULL)
	{
		DRAW_WINDOW *canvas = GetActiveDrawWindow(app);
		if(canvas != NULL)
		{
			LAYER *active_layer = canvas->active_layer;
			if(active_layer != NULL)
			{
				active_layer->layer_mode = (uint16)blend_mode;
				canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
				UpdateCanvasWidget(canvas->widgets);
				active_layer->widget->widget->setLayerBlendModeText(blend_mode);
			}
		}
	}
}

LayerMainWidget::LayerMainWidget(QWidget* parent, LAYER* layer)
	: QWidget(parent)
{
	this->layer = layer;
	this->in_dragging = false;
	this->mouse_press_x = 0;
	this->mouse_press_y = 0;
}

void LayerMainWidget::mousePressEvent(QMouseEvent* event)
{
	if(event->button() == Qt::LeftButton)
	{
		DRAW_WINDOW *canvas = layer->window;
		APPLICATION *app = canvas->app;
		canvas->active_layer = layer;
		app->widgets->layer_window->getLayerViewWidget()->setCurrentItem(
			layer->widget->widget);
		in_dragging = true;
		mouse_press_x = event->x();
		mouse_press_y = event->y();

		LayerViewSetActiveLayer(app->layer_view->layer_view, layer);
	}
}

void LayerMainWidget::mouseMoveEvent(QMouseEvent* event)
{
	if(in_dragging)
	{
		QPoint p1(mouse_press_x, mouse_press_y);
		QPoint p2(event->x(), event->y());
		if((p1 - p2).manhattanLength() >= QApplication::startDragDistance())
		{
			QDrag *drag = new QDrag(this);
			QPixmap pixmap;
			QMimeData *mime = new QMimeData;

			in_dragging = false;
			this->render(&pixmap);

			mime->setData(LayerViewWidget::layerMimeType(), NULL);

			drag->setPixmap(pixmap);
			drag->setMimeData(mime);
			drag->exec(Qt::CopyAction);

			//delete drag;
		}
	}
}

void LayerMainWidget::mouseReleaseEvent(QMouseEvent* event)
{
	if(event->button() == Qt::LeftButton)
	{
		in_dragging = false;
	}
}

void LayerMainWidget::paintEvent(QPaintEvent* event)
{
	DRAW_WINDOW *canvas = layer->window;
	if(layer == canvas->active_layer)
	{
		QPainter painter(this);
		QColor fill_color = QWidget::palette().color(QPalette::Highlight);
		painter.fillRect(this->rect(), fill_color);
	}
	else
	{
		QWidget::paintEvent(event);
	}
}

LayerWidget::LayerWidget(
	QTreeWidget* parent,
	LAYER* layer,
	const char* eye_image_file_path,
	const char* pin_image_file_path,
	LayerWindowWidget* window_widget
)
	: QTreeWidgetItem(parent),
	  main_widget(parent, layer),
	  main_layout(&this->main_widget),
	  check_box_layout(&this->main_widget),
	  check_box_widget(&this->main_widget),
	  eye(layer, &this->main_widget, eye_image_file_path, layer->window->app->gui_scale),
	  pin(layer, &this->main_widget, pin_image_file_path, layer->window->app->gui_scale),
	  thumbnail(&this->main_widget, layer, layer->window->app->gui_scale, window_widget),
	  parameters_widget(&this->main_widget),
	  parameters(&this->main_widget),
	  name(&this->main_widget),
	  mode(&this->main_widget),
	  alpha(&this->main_widget)
{
	this->layer = layer;

	QString name_str;
	name_str = name_str.fromUtf8((const char*)layer->name);
	this->name.setText(name_str);
	this->mode.setText(layer->window->app->labels->layer_window.blend_modes[layer->layer_mode]);
	QString opacity_text = QString::number(layer->alpha) + QString("%");
	this->alpha.setText(opacity_text);

	this->main_widget.setLayout(&this->main_layout);
	this->main_widget.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	this->check_box_widget.setLayout(&this->check_box_layout);
	this->check_box_layout.addWidget(&this->eye);
	this->check_box_layout.addWidget(&this->pin);
	this->check_box_layout.setSizeConstraint(QLayout::SetFixedSize);
	this->main_layout.addWidget(&this->check_box_widget);

	this->main_layout.addWidget(&this->thumbnail);

	this->parameters_widget.setLayout(&this->parameters);
	this->parameters.addWidget(&this->name);
	this->parameters.addWidget(&this->mode);
	this->parameters.addWidget(&this->alpha);
	this->main_layout.addWidget(&this->parameters_widget);

	if(layer->layer_set == NULL)
	{
		int index = layer->window->num_layer - GetLayerIndex(layer) - 1;
		window_widget->getLayerViewWidget()->insertTopLevelItem(index, this);
	}

	window_widget->getLayerViewWidget()->setItemWidget(this, 0, &this->main_widget);
}

bool LayerWidget::operator<(const LayerWidget &other) const
{
	int this_id = GetLayerIndex(this->layer);
	int other_id = GetLayerIndex(other.layer);

	return this_id < other_id;
}

LAYER* LayerWidget::getLayer()
{
	return layer;
}

void LayerWidget::updateLayerThumbnail()
{
	thumbnail.update();
}

void LayerWidget::updateLayerName(QString name)
{
	this->name.setText(name);
}

void LayerWidget::updateWidget()
{
	main_widget.update();
}

void LayerWidget::setLayerOpacityText(int opacity)
{
	this->alpha.setText(QString("%1%").arg(opacity));
}

void LayerWidget::setLayerBlendModeText(int blend_mode)
{
	if(layer != NULL)
	{
		DRAW_WINDOW* canvas = layer->window;
		if(canvas != NULL)
		{
			APPLICATION *app = canvas->app;
			this->mode.setText(app->labels->layer_window.blend_modes[blend_mode]);
		}
	}
}

LayerItem::LayerItem(LayerItem* parent, LAYER* layer)
	: parent_item(parent)
{
	this->layer = layer;
}

LayerItem::~LayerItem()
{

}

void LayerItem::appendChild(LayerItem* child)
{
	child_items.append(child);
}

LayerItem* LayerItem::child(int row)
{
	if(row < 0 || row >= child_items.size())
	{
		return NULL;
	}

	return child_items.at(row);
}

int LayerItem::childCount() const
{
	return child_items.count();
}

int LayerItem::columnCount() const
{
	LAYER *parent_layer = layer->layer_set;
	int column = 0;

	while(parent_layer != NULL)
	{
		parent_layer = parent_layer->layer_set;
		column++;
	}

	return column;
}

QVariant LayerItem::data(int column) const
{
	if(column < 0)
	{
		return QVariant();
	}

	return QVariant::fromValue((void*)layer);
}

LayerItem* LayerItem::parentItem()
{
	return parent_item;
}

int LayerItem::row() const
{
	if(parent_item != NULL)
	{
		return parent_item->child_items.indexOf(const_cast<LayerItem*>(this));
	}

	return 0;
}

LayerWidgetItemModel::LayerWidgetItemModel(LAYER* layer, QObject* parent, LayerItem* parent_item)
	: QAbstractItemModel(parent)
{
	this->root_item = new LayerItem(parent_item, layer);
}

LayerWidgetItemModel::~LayerWidgetItemModel()
{
	delete root_item;
}

int LayerWidgetItemModel::columnCount(const QModelIndex &parent) const
{
	if(parent.isValid())
	{
		return static_cast<LayerItem*>(parent.internalPointer())->columnCount();
	}

	return root_item->columnCount();
}

QVariant LayerWidgetItemModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() == false)
	{
		return QVariant();
	}

	if(role != Qt::DisplayRole)
	{
		return QVariant();
	}

	LayerItem *item = static_cast<LayerItem*>(index.internalPointer());

	return item->data(index.column());
}

Qt::ItemFlags LayerWidgetItemModel::flags(const QModelIndex &index) const
{
	if(index.isValid() == false)
	{
		return Qt::NoItemFlags;
	}

	return QAbstractItemModel::flags(index);
}

QVariant LayerWidgetItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		return root_item->data(section);
	}

	return QVariant();
}

QModelIndex LayerWidgetItemModel::index(int row, int column, const QModelIndex& parent) const
{
	if(hasIndex(row, column, parent) == false)
	{
		return QModelIndex();
	}

	LayerItem *parent_item;

	if(parent.isValid() == false)
	{
		parent_item = root_item;
	}
	else
	{
		parent_item = static_cast<LayerItem*>(parent.internalPointer());
	}

	LayerItem *child_item = parent_item->child(row);

	if(child_item != NULL)
	{
		return createIndex(row, column, child_item);
	}

	return QModelIndex();
}

QModelIndex LayerWidgetItemModel::parent(const QModelIndex &index) const
{
	if(index.isValid() == false)
	{
		return QModelIndex();
	}

	LayerItem *child_item = static_cast<LayerItem*>(index.internalPointer());
	LayerItem *parent_item = child_item->parentItem();

	if(parent_item == root_item)
	{
		return QModelIndex();
	}

	return createIndex(parent_item->row(), 0, parent_item);
}

int LayerWidgetItemModel::rowCount(const QModelIndex &parent) const
{
	LayerItem *parent_item;
	if(parent.column() > 0)
	{
		return 0;
	}

	if(parent.isValid() == false)
	{
		parent_item = root_item;
	}
	else
	{
		parent_item = static_cast<LayerItem*>(parent.internalPointer());
	}

	return parent_item->childCount();
}

LayerThumbnailWidget::LayerThumbnailWidget(
	QWidget* parent,
	LAYER* layer,
	FLOAT_T scale,
	LayerWindowWidget* window_widget
)
	: QGraphicsView(parent)
{
	int int_size = (int)(LAYER_THUMBNAIL_SIZE * scale);
	this->layer = layer;
	this->scale = scale;
	this->size = int_size;
	this->pixels = (uint8*)MEM_CALLOC_FUNC(int_size * int_size * 4, sizeof(uint8));
	this->image = QImage(this->pixels, int_size, int_size, int_size * 4, QImage::Format_ARGB32);
	this->window_widget = window_widget;

	if(layer->width > layer->height)
	{
		this->zoom = (LAYER_THUMBNAIL_SIZE * scale) / layer->width;
	}
	else
	{
		this->zoom = (LAYER_THUMBNAIL_SIZE * scale) / layer->height;
	}

	this->setFixedSize(this->size, this->size);
	this->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
}

LayerThumbnailWidget::~LayerThumbnailWidget()
{
	MEM_FREE_FUNC(pixels);
}

void LayerThumbnailWidget::paintEvent(QPaintEvent* event)
{
	QPainter painter(viewport());

	const uint8 *src = window_widget->getThumbnailBackgroundPixels();
	(void)memcpy(pixels, src,
				 size * size * 4);

	painter.drawImage(0, 0, image);

	painter.scale(zoom, zoom);
	painter.drawImage(0, 0, image);
}

LayerViewWidget::LayerViewWidget(QWidget* parent)
	: QTreeWidget(parent),
	  view_model(NULL, this, NULL),
	current_canvas(NULL)
{
	setSelectionMode(QAbstractItemView::SingleSelection);
	setDragEnabled(true);
	setAcceptDrops(true);
	viewport()->setAcceptDrops(true);
	setDropIndicatorShown(true);
	setDragDropMode(QAbstractItemView::InternalMove);
	setHeaderHidden(true);
}

void LayerViewWidget::dropEvent(QDropEvent *event)
{
	QModelIndex dropped_index = indexAt((event->pos()));

	if(dropped_index.isValid() == false)
	{
		return;
	}

	LAYER *active = current_canvas->active_layer;
	LAYER *new_prev = GetLayerFromRowIndex(current_canvas, dropped_index.row());

	delete active->widget->widget;
	ChangeLayerOrder(active, new_prev, &current_canvas->layer);
	if(active->layer_set == NULL)
	{
		active->widget->widget = new LayerWidget(NULL, active, (const char*)"./image/eye.png",
												 (const char*)"./image/pin.png", current_canvas->app->widgets->layer_window);
		insertTopLevelItem(current_canvas->num_layer - GetLayerIndex(active) - 1,
						   active->widget->widget);
	}
}

void LayerViewWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasFormat(LayerViewWidget::layerMimeType()))
	{
		event->accept();
	}
	else
	{
		event->ignore();
	}
}

void LayerViewWidget::dragMoveEvent(QDragMoveEvent* event)
{
	if(event->mimeData()->hasFormat(LayerViewWidget::layerMimeType()))
	{
		event->setDropAction(Qt::CopyAction);
		event->accept();
	}
	else
	{
		event->ignore();
	}
}

LayerWindowWidget::LayerWindowWidget(QWidget* parent, struct _APPLICATION* app)
	: QDockWidget(QWidget::tr("Layer View"), parent),
	  scroll_area(parent),
	  main_layout_widget(this),
	  main_layout(this),
	  blend_mode_layout_widget(this),
	  blend_mode_layout(this),
	  blend_mode_selector(this, app),
	  blend_mode_label(this),
	  layer_opacity(&this->main_layout_widget, tr("Opacity"), true, app),
	  layer_view(&this->main_layout_widget),
	  buttons_layout_widget(this),
	  buttons_layout(this),
	  lock_opacity(tr("Lock layer opacity"), this),
	  mask_with_under(tr("Mask with under layer"), this),
	  new_layer_button(&this->buttons_layout_widget),
	  delete_layer_button(&this->buttons_layout_widget)
{
#define MINIMUM_OPACITY 0
#define MAXIMUM_OPACITY 100
#define INITIAL_OPACITY_VALUE 100
	this->app = app;

	initializeLayerBlendModeComboBox();

	this->layer_opacity.setValue(INITIAL_OPACITY_VALUE);
	
	this->blend_mode_selector.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	this->scroll_area.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	this->layer_view.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	this->main_layout_widget.setLayout(&this->main_layout);
	this->buttons_layout_widget.setLayout(&this->buttons_layout);
	this->blend_mode_layout_widget.setLayout(&this->blend_mode_layout);
	this->main_layout.addWidget(&blend_mode_layout_widget);
	this->blend_mode_label.setText(QString(tr("Blend mode")) + QString(" : "));
	this->blend_mode_layout.addWidget(&blend_mode_label);
	this->blend_mode_layout.addWidget(&blend_mode_selector);
	this->main_layout.addWidget(&layer_opacity);
	this->main_layout.addWidget(&buttons_layout_widget);
	this->main_layout.addWidget(&lock_opacity);
	this->main_layout.addWidget(&mask_with_under);
	this->main_layout.addWidget(&scroll_area, 1);	// stretchを1にすることでスクロールの表示エリアを拡張する
	this->scroll_area.setWidget(&layer_view);

	this->main_layout_widget.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	this->scroll_area.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	this->scroll_area.setWidgetResizable(true);
	
	layer_opacity.setMinimum(MINIMUM_OPACITY);
	layer_opacity.setMaximum(MAXIMUM_OPACITY);
	layer_opacity.setValue(INITIAL_OPACITY_VALUE);

	this->new_layer_button.setIcon(QIcon("./image/new_document.png"));
	connect(&new_layer_button, SIGNAL(clicked()),
								   this, SLOT(newLayerButtonClicked()));
	this->buttons_layout.addWidget(&this->new_layer_button, 0, 0);

	this->delete_layer_button.setIcon(QIcon("./image/trash_box.png"));
	connect(&delete_layer_button, SIGNAL(clicked()),
				this, SLOT(deleteLayerButtonClicked()));
	this->buttons_layout.addWidget(&this->delete_layer_button, 0, 1);

	connect(&lock_opacity, &QCheckBox::toggled, this, &LayerWindowWidget::lockOpacityChanged);
	connect(&mask_with_under, &QCheckBox::toggled, this, &LayerWindowWidget::maskUnderLayerChanged);

	setWidget(&main_layout_widget);

	if(app != NULL)
	{
		int int_size = (int)(LAYER_THUMBNAIL_SIZE * app->gui_scale);
		this->thumnail_background_pixels = (uint8*)MEM_CALLOC_FUNC(
					int_size * int_size * 4, sizeof(uint8));
		uint8 *line_pixel_data[2];
		int step = int_size / 8;
		int stride = int_size * 4;
		bool white;
		int index;
		int i, j;

		line_pixel_data[0] = (uint8*)MEM_ALLOC_FUNC(stride);
		line_pixel_data[1] = (uint8*)MEM_ALLOC_FUNC(stride);

		white = false;
		for(i = 0, j = 0; i < int_size; i++, j++)
		{
			if(j >= step)
			{
				white = !white;
				j = 0;
			}
			uint8 pixel_value = (white == false) ? 0xff : 0x88;
			line_pixel_data[0][i*4+0] = pixel_value;
			line_pixel_data[0][i*4+1] = pixel_value;
			line_pixel_data[0][i*4+2] = pixel_value;
			line_pixel_data[0][i*4+3] = 0xff;
		}

		white = true;
		for(i = 0, j = 0; i < int_size; i++, j++)
		{
			if(j >= step)
			{
				white = !white;
				j = 0;
			}
			uint8 pixel_value = (white == false) ? 0xff : 0x88;
			line_pixel_data[1][i*4+0] = pixel_value;
			line_pixel_data[1][i*4+1] = pixel_value;
			line_pixel_data[1][i*4+2] = pixel_value;
			line_pixel_data[1][i*4+3] = 0xff;
		}

		white = false;
		for(i = 0, j = 0, index = 1; i < int_size; i++, j++)
		{
			if(j >= step)
			{
				white = !white;
				index = (white == false) ? 1 : 0;
				j = 0;
			}
			(void)memcpy(&this->thumnail_background_pixels[i * stride],
					line_pixel_data[index], stride);
		}
	}
}

void LayerViewWidget::setCurrentCanvas(struct _DRAW_WINDOW *canvas)
{
	LAYER *layer = canvas->layer;
	model()->removeRows(0, model()->rowCount());
	this->current_canvas = canvas;
	for(int i = 0; layer != NULL; i++, layer = layer->next)
	{
		LayerViewAddLayer(layer, canvas->layer, (void*)this, i);
	}
}

LayerViewWidget* LayerWindowWidget::getLayerViewWidget()
{
	return &layer_view;
}

const uint8* LayerWindowWidget::getThumbnailBackgroundPixels()
{
	return thumnail_background_pixels;
}

void LayerWindowWidget::newLayerButtonClicked()
{
	ExecuteMakeColorLayer(app);
}

void LayerWindowWidget::deleteLayerButtonClicked()
{
	ExecuteDeleteActiveLayer(app);
}

void LayerWindowWidget::layerOpacityChanged(int opacity)
{
	if((app->flags & APPLICATION_IN_OPEN_OPERATION) == 0)
	{
		DRAW_WINDOW *canvas = GetActiveDrawWindow(app);
		if(canvas != NULL)
		{
			canvas->active_layer->alpha = opacity;
			canvas->active_layer->widget->widget->setLayerOpacityText(opacity);
			canvas->flags |= DRAW_WINDOW_UPDATE_ACTIVE_UNDER;
			
			if(canvas->focal_window != NULL)
			{
				canvas->focal_window->active_layer->alpha = opacity;
			}
		}
	}
}

void LayerWindowWidget::setActiveLayer(LAYER* layer)
{
	DRAW_WINDOW *canvas = layer->window;
	APPLICATION *app = canvas->app;
	LAYER *layer_list = canvas->layer;

	if((app->flags & APPLICATION_IN_OPEN_OPERATION) == 0)
	{
		blend_mode_selector.setCurrentIndex(layer->layer_mode);
		layer_opacity.setValue(layer->alpha);
		lock_opacity.setCheckState((layer->flags & LAYER_LOCK_OPACITY) != 0
			? Qt::Checked : Qt::Unchecked);
		mask_with_under.setCheckState((layer->flags & LAYER_MASKING_WITH_UNDER_LAYER) != 0
			? Qt::Checked : Qt::Unchecked);
	}
	
	mask_with_under.setEnabled(layer->prev != NULL);
	merge_down.setEnabled(layer->prev != NULL);

	lock_opacity.setChecked(layer->flags & LAYER_LOCK_OPACITY);
	mask_with_under.setChecked(layer->flags & LAYER_MASKING_WITH_UNDER_LAYER);

	UpdateLayerWidgets(canvas);
}

void LayerWindowWidget::initializeLayerBlendModeComboBox(void)
{
	if(app != NULL)
	{
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_NORMAL]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_ADD]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_MULTIPLY]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_SCREEN]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_OVERLAY]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_LIGHTEN]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_DARKEN]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_DODGE]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_BURN]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_HARD_LIGHT]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_SOFT_LIGHT]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_DIFFERENCE]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_EXCLUSION]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_HSL_HUE]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_HSL_SATURATION]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_HSL_COLOR]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_HSL_LUMINOSITY]));
		blend_mode_selector.addItem(QString(
			app->labels->layer_window.blend_modes[LAYER_BLEND_BINALIZE]));
	}
}

void LayerWindowWidget::moveActiveLayer(LAYER* layer)
{
	LAYER *current = layer;
	int target_index = 0;
	bool find = false;
	
	if(current == NULL)
	{
		return;
	}
	
	int num_layer = 1;
	while(current->next != NULL)
	{
		num_layer++;
		current = current->next;
	}
	while(current != NULL)
	{
		if(current == layer)
		{
			find = true;
			break;
		}
		current = current->prev;
		target_index++;
	}
	
	if(find)
	{
		QScrollBar *scroll_bar = scroll_area.horizontalScrollBar();
		int minimum = scroll_bar->minimum(), maximum = scroll_bar->maximum();
		double step = (double)(maximum - minimum) / num_layer;
		
		scroll_bar->setSliderPosition((int)(target_index * step));
	}
}

void LayerWindowWidget::setActiveLayerOpacity(int opacity)
{
	layer_opacity.setValue(opacity);
}

void LayerWindowWidget::lockOpacityChanged(bool state)
{
	SetLayerLockOpacity(app, state);
}

void LayerWindowWidget::maskUnderLayerChanged(bool state)
{
	SetLayerMaskUnder(app, state);
}

void UpdateLayerThumbnail(LAYER* layer)
{
	layer->widget->widget->updateLayerThumbnail();
}

void UpdateLayerNameLabel(LAYER* layer)
{
	QString str = QString::fromUtf8(layer->name);
	layer->widget->widget->updateLayerName(str);
}

void LayerViewSetActiveLayer(void* view, LAYER* layer)
{
	LayerWindowWidget *layer_window =  (LayerWindowWidget*)view;
	layer_window->setActiveLayer(layer);

	// LAYER_VIEW_WIDGETS *widgets = (LAYER_VIEW_WIDGETS*)view;
	// LayerWindowWidget*widget = (LayerWindowWidget*)widgets->layer_view;
	// widget->setActiveLayer(layer);
}

void LayerViewMoveActiveLayer(void* view, LAYER* layer)
{
	LayerWindowWidget *widget = (LayerWindowWidget*)view;
	
	widget->moveActiveLayer(layer);
}

void UpdateLayerWidgets(DRAW_WINDOW* canvas)
{
	LAYER *layer = canvas->layer;
	while(layer != NULL)
	{
		if(layer->widget != NULL && layer->widget->widget != NULL)
		{
			layer->widget->widget->updateWidget();
		}
		layer = layer->next;
	}
}

#ifdef __cplusplus
}
#endif
