#include <QPainter>
#include <QtMath>
#include "../../color.h"
#include "../../configure.h"
#include "color_chooser_qt.h"

#ifdef __cplusplus
extern "C" {
#endif

void DrawColorCircle(
	QPixmap* pixmap,
	int width,
	int height,
	FLOAT_T line_width,
	QColor background_color
)
{
	QPainter painter;
	QPen pen;
	QRectF rect(line_width * 0.5, line_width * 0.5,
				width - line_width, height - line_width);
	HSV hsv = {360, 255, 255};
	uint8 color[3];
	pen.setWidthF(line_width);

	(void)painter.begin(pixmap);
	painter.fillRect(QRect(0, 0, width, height), background_color);
	for(int arc = -210 * 16, i = 0; i < 360; arc += 16, i++)
	{
		hsv.h = 360 - i - 1;
		HSV2RGB_Pixel(&hsv, color);
		pen.setColor(QColor(color[0], color[1], color[2]));
		painter.setPen(pen);
		painter.drawArc(rect, arc, -16);
	}
	(void)painter.end();
}

void UpdateColorChooseArea(void* ui)
{
	ColorChooser *chooser = (ColorChooser*)ui;
	chooser->circle()->update();
}

#ifdef __cplusplus
}
#endif

ColorChooserCircle::ColorChooserCircle(
	QWidget* parent,
	COLOR_CHOOSER* chooser,
	ColorChooser* chooser_layout,
	int width,
	int height,
	int line_width
)
	: QWidget(parent)
{
	this->chooser_data = chooser;
	this->chooser_data->width = width;
	this->chooser_data->height = height;
	this->chooser_data->line_width = line_width;

	this->chooser_data->hsv.h = this->chooser_data->hsv.s = this->chooser_data->hsv.v = 0;
	this->chooser_data->rgb[0] = this->chooser_data->rgb[1] = this->chooser_data->rgb[2] = 0;

	int box_size = (width + line_width) / 2;
	this->chooser_data->sv_x = (width - box_size) / 2;
	this->chooser_data->sv_y = (height - box_size) / 2;
	this->chooser_data->flags = 0;

	this->chooser_layout = chooser_layout;

	setMinimumSize(width, height);

	circle_pixmap = new QPixmap(width, height);
	QColor background_color(palette().color(QPalette::Window));
	DrawColorCircle(circle_pixmap, width, height, line_width, background_color);
}

ColorChooserCircle::~ColorChooserCircle()
{
	delete circle_pixmap;
}

HSV ColorChooserCircle::getHSV(void)
{
	return chooser_data->hsv;
}

void ColorChooserCircle::setHSV(HSV hsv)
{
	int box_size = (chooser_data->width + chooser_data->line_width) / 2;
	chooser_data->sv_x = ((hsv.h) / 255.0) * box_size + (chooser_data->width - box_size) / 2;
	chooser_data->sv_y = box_size + (chooser_data->height - box_size) / 2 - hsv.v * box_size / 255.0;
	this->chooser_data->hsv = hsv;
	HSV2RGB_Pixel(&hsv, chooser_data->rgb);
	update();
}

void ColorChooserCircle::getRGB(uint8* rgb)
{
	rgb[0] = this->chooser_data->rgb[0];
	rgb[1] = this->chooser_data->rgb[1];
	rgb[2] = this->chooser_data->rgb[2];
}

void ColorChooserCircle::setRGB(const uint8* rgb)
{
	HSV hsv;
	RGB2HSV_Pixel((uint8*)rgb, &hsv);
	setHSV(hsv);

	chooser_layout->colorChange(rgb);
}

void ColorChooserCircle::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);

	painter.drawPixmap(0, 0, this->chooser_data->width, this->chooser_data->height, *circle_pixmap);

	// 中心の四角形を描画
		// 白→選択中の色
	HSV hsv = {this->chooser_data->hsv.h, 0xFF, 0xFF};
	uint8 color[3];
	if(hsv.h < 0)
	{
		hsv.h += 360;
	}
	else if(hsv.h >= 360)
	{
		hsv.h %= 360;
	}
	HSV2RGB_Pixel(&hsv, color);
	int box_size = (this->chooser_data->width + this->chooser_data->line_width) / 2;
	QLinearGradient gradient((this->chooser_data->width - box_size) / 2, (this->chooser_data->height - box_size) / 2,
							 (this->chooser_data->width + box_size) / 2, (this->chooser_data->height - box_size) / 2);
	gradient.setColorAt(0, Qt::white);
	gradient.setColorAt(1.0, QColor(color[0], color[1], color[2]));
	painter.setBrush(gradient);
	QRect rect((this->chooser_data->width - box_size) / 2, (this->chooser_data->height - box_size) / 2,
			   box_size, box_size);
	painter.drawRect(rect);
	// 黒のグラデーションを流す
	gradient.setFinalStop((this->chooser_data->width - box_size) / 2, (this->chooser_data->height + box_size) / 2);
	gradient.setColorAt(0, QColor(0, 0, 0, 0));
	gradient.setColorAt(1.0, QColor(0, 0, 0, 0xFF));
	painter.setBrush(gradient);
	painter.drawRect(rect);

	// 選択中の位置を示す円を描画
	painter.setBrush(Qt::transparent);
	FLOAT_T d = box_size / 255.0;
	FLOAT_T draw_x = this->chooser_data->width / 2 - box_size / 2 + d * this->chooser_data->hsv.s;
	FLOAT_T draw_y = this->chooser_data->height / 2 + box_size / 2 - d * this->chooser_data->hsv.v;

	if(chooser_data->hsv.h >= 360)
	{
		chooser_data->hsv.h -= 360;
	}
	HSV2RGB_Pixel(&chooser_data->hsv, color);
	QPen pen(QColor(color[0], color[1], color[2]));
	pen.setWidthF(this->chooser_data->line_width * 0.2);
	painter.setPen(pen);
	painter.drawEllipse(QPointF(draw_x, draw_y),
						this->chooser_data->line_width * (9.0/15.0), this->chooser_data->line_width * (9.0/15.0));
	FLOAT_T arg = (this->chooser_data->hsv.h - 135) * M_PI / 180.0;
	int r = this->chooser_data->width / 2;
	draw_x = this->chooser_data->width / 2 + cos(arg) * (r - this->chooser_data->line_width/2);
	draw_y = this->chooser_data->height / 2 + sin(arg) * (r - this->chooser_data->line_width/2);
	painter.drawEllipse(QPointF(draw_x, draw_y),
						this->chooser_data->line_width * (9.0/15.0), this->chooser_data->line_width * (9.0/15.0));
}

void ColorChooserCircle::mousePressEvent(QMouseEvent* event)
{
	if(event->button() == Qt::LeftButton)
	{
		FLOAT_T d = sqrt((FLOAT_T)((event->x()-this->chooser_data->width/2)*(event->x()-this->chooser_data->width/2)
								   + (event->y()-this->chooser_data->height/2)*(event->y()-this->chooser_data->height/2)));
		FLOAT_T arg = atan2(event->y()-this->chooser_data->height/2, event->x()-this->chooser_data->width/2);
		int r = this->chooser_data->width / 2;
		int box_size = (chooser_data->width + chooser_data->line_width) / 2;
		if(d > r-this->chooser_data->line_width && d < r+this->chooser_data->line_width)
		{
			HSV hsv = {0, 0xFF, 0xFF};
			hsv.h = (int16)(arg * 180 / M_PI) + 135;
			if(hsv.h < 0)
			{
				hsv.h += 360;
			}
			else if(hsv.h >= 360)
			{
				hsv.h %= 360;
			}

			this->chooser_data->hsv.h = hsv.h;
			this->chooser_data->flags |= MOVING_CIRCLE;

			FLOAT_T draw_x = (chooser_data->sv_x - (chooser_data->width - box_size) / 2) / box_size;
			FLOAT_T draw_y = 1.0 - (chooser_data->sv_y - (chooser_data->height - box_size) / 2) / box_size;
			if(draw_x > 1.0)
			{
				draw_x = 1.0;
			}
			if(draw_y < 0.0)
			{
				draw_y = 0.0;
			}
			hsv.s = (uint8)(draw_x * 255);
			hsv.v = (uint8)(draw_y * 255);
			HSV2RGB_Pixel(&hsv, chooser_data->rgb);

			update();

			emit colorCircleChanged(chooser_data->rgb);
		}
		else if(event->x() >= (chooser_data->width-box_size)/2 && event->y() >= (chooser_data->height-box_size)/2
				&& event->x() <= (chooser_data->width+box_size)/2 && event->y() <= (chooser_data->height+box_size)/2)
		{
			HSV hsv = this->chooser_data->hsv;
			FLOAT_T draw_x = chooser_data->width/2 + d * cos(arg);
			FLOAT_T draw_y = chooser_data->height/2 + d * sin(arg);
			hsv.s = 255,	hsv.v = 255;

			chooser_data->flags |= MOVING_SQUARE;

			chooser_data->sv_x = draw_x,  chooser_data->sv_y = draw_y;

			draw_x = (draw_x - (chooser_data->width - box_size) / 2) / box_size;
			draw_y = 1.0 - (draw_y - (chooser_data->height - box_size) / 2) / box_size;
			if(draw_x > 1.0)
			{
				draw_x = 1.0;
			}
			if(draw_y < 0.0)
			{
				draw_y = 0.0;
			}
			hsv.s = (uint8)(draw_x * 255);
			hsv.v = (uint8)(draw_y * 255);
			this->chooser_data->hsv.s = hsv.s;
			this->chooser_data->hsv.v = hsv.v;
			HSV2RGB_Pixel(&hsv, chooser_data->rgb);

			update();

			emit colorCircleChanged(chooser_data->rgb);
		}
	}
}

void ColorChooserCircle::mouseMoveEvent(QMouseEvent* event)
{
	FLOAT_T d = sqrt((FLOAT_T)((event->x()-this->chooser_data->width/2)*(event->x()-this->chooser_data->width/2)
							   + (event->y()-this->chooser_data->height/2)*(event->y()-this->chooser_data->height/2)));
	FLOAT_T arg = atan2(event->y()-this->chooser_data->height/2, event->x()-this->chooser_data->width/2);
	int box_size = (chooser_data->width + chooser_data->line_width) / 2;

	if((chooser_data->flags & MOVING_CIRCLE) != 0)
	{
		HSV hsv = {0, 0xFF, 0xFF};
		hsv.h = (int16)(arg * 180 / M_PI) + 135;
		if(hsv.h < 0)
		{
			hsv.h += 360;
		}
		else if(hsv.h >= 360)
		{
			hsv.h %= 360;
		}

		this->chooser_data->hsv.h = hsv.h;
		this->chooser_data->flags |= MOVING_CIRCLE;

		FLOAT_T draw_x = (chooser_data->sv_x - (chooser_data->width - box_size) / 2) / box_size;
		FLOAT_T draw_y = 1.0 - (chooser_data->sv_y - (chooser_data->height - box_size) / 2) / box_size;
		if(draw_x > 1.0)
		{
			draw_x = 1.0;
		}
		if(draw_y < 0.0)
		{
			draw_y = 0.0;
		}
		hsv.s = this->chooser_data->hsv.s;
		hsv.v = this->chooser_data->hsv.v;
		HSV2RGB_Pixel(&hsv, chooser_data->rgb);

		update();

		emit colorCircleChanged(chooser_data->rgb);
	}
	else if((chooser_data->flags & MOVING_SQUARE) != 0)
	{
		HSV hsv = this->chooser_data->hsv;
		FLOAT_T draw_x = chooser_data->width/2 + d * cos(arg);
		FLOAT_T draw_y = chooser_data->height/2 + d * sin(arg);
		hsv.s = 255,	hsv.v = 255;

		chooser_data->flags |= MOVING_SQUARE;

		chooser_data->sv_x = draw_x,  chooser_data->sv_y = draw_y;

		draw_x = (draw_x - (chooser_data->width - box_size) / 2) / box_size;
		draw_y = 1.0 - (draw_y - (chooser_data->height - box_size) / 2) / box_size;
		if(draw_x > 1.0)
		{
			draw_x = 1.0;
		}
		else if(draw_x < 0.0)
		{
			draw_x = 0.0;
		}
		if(draw_y < 0.0)
		{
			draw_y = 0.0;
		}
		else if(draw_y > 1.0)
		{
			draw_y = 1.0;
		}
		hsv.s = (uint8)(draw_x * 255);
		hsv.v = (uint8)(draw_y * 255);
		this->chooser_data->hsv.s = hsv.s;
		this->chooser_data->hsv.v = hsv.v;
		HSV2RGB_Pixel(&hsv, chooser_data->rgb);

		update();

		emit colorCircleChanged(chooser_data->rgb);
	}
}

void ColorChooserCircle::mouseReleaseEvent(QMouseEvent* event)
{
	chooser_data->flags &= ~(MOVING_CIRCLE | MOVING_SQUARE);
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

ColorChooserBox::ColorChooserBox(
	QWidget* parent,
	ColorChooser* chooser_layout,
	int size,
	const uint8* rgb,
	const uint8* back_rgb
)  : QWidget(parent)
{
	const uint8 default_rgb[3] = {0x00, 0x00, 0x00};
	const uint8 default_back[3] = {0xFF, 0xFF, 0xFF};
	const uint8 *set_rgb = (rgb == NULL) ? default_rgb : rgb;
	const uint8 *set_back = (back_rgb == NULL) ? default_back : back_rgb;

	this->rgb[0] = set_rgb[0];
	this->rgb[1] = set_rgb[1];
	this->rgb[2] = set_rgb[2];

	this->back_rgb[0] = set_back[0];
	this->back_rgb[1] = set_back[1];
	this->back_rgb[2] = set_back[2];

	this->size = size;

	this->chooser_layout = chooser_layout;

	color_box_pixel_data = new uint8[((size*3)/2)*((size*3)/2)*4];
	image = new QImage(color_box_pixel_data, (size*3)/2, (size*3)/2, QImage::Format_ARGB32);

	int stride = ((size*3)/2) * 4;
	(void)memset(color_box_pixel_data, 0, stride*((size*3)/2));

	UpdateColor();

	setMinimumSize((size*3)/2, (size*3)/2);
}

ColorChooserBox::~ColorChooserBox()
{
	delete image;
	delete color_box_pixel_data;
}

void ColorChooserBox::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);

	painter.drawImage(0, 0, *image);
}

void ColorChooserBox::mousePressEvent(QMouseEvent* event)
{
	if(event->button() == Qt::LeftButton)
	{
		if(event->x() > size && event->y() < size/2)
		{
			uint8 temp_color[3];

			temp_color[0] = rgb[0];
			temp_color[1] = rgb[1];
			temp_color[2] = rgb[2];

			rgb[0] = back_rgb[0];
			rgb[1] = back_rgb[1];
			rgb[2] = back_rgb[2];

			back_rgb[0] = temp_color[0];
			back_rgb[1] = temp_color[1];
			back_rgb[2] = temp_color[2];

			UpdateColor();
			update();

			emit switchFrontBack(rgb);
		}
	}
}

void ColorChooserBox::setRGB(const uint8* rgb)
{
	this->rgb[0] = rgb[0];
	this->rgb[1] = rgb[1];
	this->rgb[2] = rgb[2];
	UpdateColor();
	update();

	chooser_layout->colorChange(rgb);
}

void ColorChooserBox::UpdateColor(void)
{
	QPainter painter(image);

	painter.setBrush(QBrush(QColor(back_rgb[0], back_rgb[1], back_rgb[2])));
	painter.setPen(Qt::black);
	painter.drawRect(QRect(size/2, size/2, size, size));

	painter.drawLine(size, size/4, size+size/4, size/4);
	painter.drawLine(size+size/4, size/4, size+size/4, size/2);
	painter.drawLine(size, size/4, size+size/8, size/4-size/8);
	painter.drawLine(size, size/4, size+size/8, size/4+size/8);
	painter.drawLine(size+size/4, size/2, size+size/4-size/8, size/2-size/8);
	painter.drawLine(size+size/4, size/2, size+size/4+size/8, size/2-size/8);

	painter.setBrush(QBrush(QColor(rgb[0], rgb[1], rgb[2])));
	painter.drawRect(QRect(0, 0, size, size));
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

ColorChooser::ColorChooser(
	QWidget* parent,
	COLOR_CHOOSER* chooser,
	int width,
	int height,
	int line_width,
	uint8 (*pallete)[3],
	uint8 *pallete_use,
	const char* base_path,
	int color_box_size,
	const uint8* rgb,
	const uint8* back_rgb
)
	: QVBoxLayout(parent),
	  chooser_circle(parent, chooser, this, width, height, line_width),
	  chooser_box(parent, this, color_box_size, rgb, back_rgb)
{
	this->pallete_data = pallete;
	this->pallete_flags = pallete_use;
	this->chooser = chooser;

	QHBoxLayout *h_layout = new QHBoxLayout;
	QVBoxLayout *v_layout = new QVBoxLayout;

	h_layout->addWidget(&chooser_circle);
	h_layout->addLayout(v_layout);

	v_layout->addWidget(&chooser_box);

	this->addLayout(h_layout);

	connect(&chooser_circle, SIGNAL(colorCircleChanged(const uint8*)),
		&chooser_box, SLOT(setRGB(const uint8*)));
	connect(&chooser_box, SIGNAL(switchFrontBack(const uint8*)),
		&chooser_circle, SLOT(setRGB(const uint8*)));
}

ColorChooser::~ColorChooser()
{

}

ColorChooserCircle* ColorChooser::circle()
{
	return &chooser_circle;
}

ColorChooserBox* ColorChooser::box()
{
	return &chooser_box;
}

void ColorChooser::colorChange(const uint8* rgb)
{
#if defined(USE_BGR_COLOR_SPACE) && USE_BGR_COLOR_SPACE != 0
	uint8 color[4];
	color[0] = rgb[2];
	color[1] = rgb[1];
	color[2] = rgb[0];
	color[3] = 0xFF;
	emit colorChanged(color);
#else
	emit colorChanged(rgb);
#endif
}