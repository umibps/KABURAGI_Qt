#ifndef _INCLUDED_COLOR_CHOOSER_QT_H_
#define _INCLUDED_COLOR_CHOOSER_QT_H_

#include <QWidget>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QTableWidget>
#include "../../color.h"

#define COLOR_CIRCLE_SIZE (32 * 4 + 16)
#define COLOR_CIRCLE_WIDTH (15)
#define COLOR_BOX_SIZE (32)

class ColorChooser;

class ColorChooserCircle : public QWidget
{
	Q_OBJECT

public:
	ColorChooserCircle(
		QWidget* parent = NULL,
		COLOR_CHOOSER* chooser = NULL,
		ColorChooser* chooser_layout = NULL,
		int width = COLOR_CIRCLE_SIZE,
		int height = COLOR_CIRCLE_SIZE,
		int line_width = COLOR_CIRCLE_WIDTH
	 );
	~ColorChooserCircle();

	HSV getHSV(void);
	void getRGB(uint8* rgb);

signals:
	void colorCircleChanged(const uint8* rgb);

public slots:
	void setHSV(HSV hsv);
	void setRGB(const uint8* rgb);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event);

private:
	const int MOVING_CIRCLE = 0x01;
	const int MOVING_SQUARE = 0x02;
	QPixmap *circle_pixmap;
	COLOR_CHOOSER *chooser_data;
	ColorChooser *chooser_layout;
};

class ColorChooserBox : public QWidget
{
	Q_OBJECT

public:
	ColorChooserBox(QWidget* parent = NULL, ColorChooser* chooser_layout = NULL,
					int size = COLOR_BOX_SIZE, const uint8* rgb = NULL, const uint8* back_rgb = NULL);
	~ColorChooserBox();

public slots:
	void setRGB(const uint8* rgb);

signals:
	void switchFrontBack(const uint8*);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;

private:
	QImage *image;
	uint8 *color_box_pixel_data;
	uint8 rgb[3], back_rgb[3];
	int size;
	void UpdateColor(void);
	ColorChooser* chooser_layout;
};

class ColorChooser : public QVBoxLayout
{
	Q_OBJECT

public:
	ColorChooser(
		QWidget* parent = NULL,
		COLOR_CHOOSER* chooser = NULL,
		int width = COLOR_CIRCLE_SIZE,
		int height = COLOR_CIRCLE_SIZE,
		int line_width = COLOR_CIRCLE_WIDTH,
		uint8 (*pallete)[3] = NULL,
		uint8 *pallete_use = NULL,
		const char* base_path = NULL,
		int color_box_size = COLOR_BOX_SIZE,
		const uint8* rgb = NULL,
		const uint8* back_rgb = NULL
	);
	~ColorChooser();
	
	ColorChooserCircle* circle();
	ColorChooserBox* box();

signals:
	void colorChanged(const uint8* rgb);

public slots:
	void colorChange(const uint8* rgb);

private:
	ColorChooserCircle chooser_circle;
	ColorChooserBox chooser_box;
	COLOR_CHOOSER *chooser;
	uint8 (*pallete_data)[3];
	uint8 *pallete_flags;
};

extern void ColorHistoryPopupMenu(
	DRAW_WINDOW* canvas,
	const QPoint& position,
	QObject* callback_object,
	void (*set_menu_callback)(QTableWidget*, void*)
);

#endif // #ifndef _INCLUDED_COLOR_CHOOSER_QT_H_

