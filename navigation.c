#include "navigation.h"
#include "draw_window.h"
#include "gui/navigation.h"

/*
* ChangeNavigationRotate�֐�
* �i�r�Q�[�V�����̉�]�p�x�ύX���ɌĂяo���֐�
* ����
* navigation			: �i�r�Q�[�V�����E�B���h�E�̏��
* canvas				: �\������L�����o�X
* canvas_area_width		: �L�����o�X��\�����Ă���X�N���[���G���A�̕�
* canvas_area_height	: �L�����o�X��\�����Ă���X�N���[���G���A�̍���
* navigation_width		: �i�r�Q�[�V�����̃L�����o�X�\���G���A�̕�
* navigation_height		: �i�r�Q�[�V�����̃L�����o�X�\���G���A�̍���
*/
void ChangeNavigationRotate(
	NAVIGATION* navigation,
	DRAW_WINDOW* canvas,
	int canvas_area_width,
	int canvas_area_height,
	int navigation_width,
	int navigation_height
)
{
	FLOAT_T zoom_x, zoom_y;
	FLOAT_T trans_x, trans_y;
	FLOAT_T half_size;
	int half_width, half_height;

	half_size = (canvas->width > canvas->height) ?
		canvas->width * ROOT2 : canvas->height * ROOT2;

	half_width = navigation_width / 2;
	half_height = navigation_height / 2;

	zoom_x = ((FLOAT_T)canvas->width) / navigation_width;
	zoom_y = ((FLOAT_T)canvas->height) / navigation_height;

	if((canvas->flags & DRAW_WINDOW_DISPLAY_HORIZON_REVERSE) == 0)
	{
		trans_x = - half_width * zoom_x +
					(half_width * (1/ROOT2) * zoom_x * canvas->cos_value
					+ half_height * (1/ROOT2) * zoom_x * canvas->sin_value);
		trans_x /= zoom_x;
		trans_y = -half_height * zoom_y - (
					half_width * (1/ROOT2) * zoom_y * canvas->sin_value
					- half_height * (1/ROOT2) * zoom_y * canvas->cos_value);
		trans_y /= zoom_y;

		NavigationSetDrawCanvasMatrix(navigation->widgets, zoom_x*ROOT2, zoom_y*ROOT2,
										canvas->angle, trans_x, trans_y);
	}
	else
	{
		trans_x = -half_width * zoom_x +
			(-half_width * (1 / ROOT2) * zoom_x * canvas->cos_value + half_height * (1 / ROOT2) * zoom_x * canvas->sin_value);
		trans_x /= zoom_x;
		trans_y = -half_height * zoom_y - (
			-half_width * (1 / ROOT2) * zoom_y * canvas->sin_value - half_height * (1 / ROOT2) * zoom_y * canvas->cos_value);
		trans_y /= zoom_y;

		NavigationSetDrawCanvasMatrix(navigation->widgets, - zoom_x * ROOT2, zoom_y * ROOT2,
			canvas->angle, trans_x, trans_y);
	}
}

void ChangeNavigationDrawWindow(
	NAVIGATION* navigation,
	DRAW_WINDOW* canvas,
	int canvas_area_width,
	int canvas_area_height,
	int navigation_width,
	int navigation_height
)
{
	NavigationSetCanvasPattern(navigation->widgets, canvas);

	// �i�r�Q�[�V�����̘g�̕�
	navigation->point.width = (FLOAT_T)canvas_area_width / ((FLOAT_T)canvas->disp_size);
	if(navigation->point.width > 1.0)
	{
		navigation->point.width = 1.0;
	}
	// �i�r�Q�[�V�����̘g�̍���
	navigation->point.height = (FLOAT_T)canvas_area_height / ((FLOAT_T)canvas->disp_size);
	if(navigation->point.height > 1.0)
	{
		navigation->point.height = 1.0;
	}

	navigation->draw_width = canvas_area_width;
	navigation->draw_height = canvas_area_height;

	// �g�\�����W�␳�p�̒l
	navigation->point.ratio_x =
		(1.0 / canvas->disp_size) * navigation_width;
	navigation->point.ratio_y =
		(1.0 / canvas->disp_size) * navigation_height;

	// �L�����o�X�̉�]�p�Ɗg��k�������L��
	navigation->point.angle = canvas->angle;
	navigation->point.zoom = canvas->zoom;

	// �i�r�Q�[�V�����ɃL�����o�X���g��k��&��]�\�����邽�߂̍s����X�V
	ChangeNavigationRotate(navigation, canvas, canvas_area_width, canvas_area_height,
							navigation_width, navigation_height);
}

void DiplayNavigation(
	NAVIGATION* navigation,
	DRAW_WINDOW* canvas,
	int canvas_area_width,
	int canvas_area_height,
	int canvas_scroll_x,
	int canvas_scroll_y,
	int navigation_width,
	int navigation_height,
	void* draw_context
)
{
	int stride;

	stride = navigation_width * 4;
	// �i�r�Q�[�V�����̃T�C�Y���ύX����Ă���s�N�Z���f�[�^�̃��������m��
	if(navigation->draw_width != navigation_width
		|| navigation->draw_height != navigation_height)
	{
		navigation->pixels = (uint8*)MEM_REALLOC_FUNC(navigation->pixels,
										navigation_height * stride);
		navigation->reverse_buffer = (uint8*)MEM_REALLOC_FUNC(
										navigation->reverse_buffer, navigation_height * stride);

		ChangeNavigationDrawWindow(navigation, canvas,
						canvas_area_width, canvas_area_height, navigation_width, navigation_height);
	}
	// �g��k�����܂��̓E�B���h�E�T�C�Y�܂��͉�]�p���ύX����Ă�����
		// �i�r�Q�[�V�����ɃL�����o�X���g��k��&��]�\�����邽�߂̍s����X�V
	else if(navigation->point.zoom != canvas->zoom
			|| navigation->point.angle != canvas->angle
			|| navigation->draw_width != canvas_area_height
			|| navigation->draw_height != canvas_area_height
	)
	{
		ChangeNavigationDrawWindow(navigation, canvas,
			canvas_area_width, canvas_area_height, navigation_width, navigation_height);
	}

	// �L�����o�X���k�����ăi�r�Q�[�V�����ɕ`��
	NavigationDrawCanvas(navigation->widgets, draw_context);

	// ���E���]�\�����̃s�N�Z���f�[�^����͂��������ĕs�v�H
	// if((canvas->flags & DRAW_WINDOW_DISPLAY_HORIZON_REVERSE) != 0)

	// �L�����o�X���X�N���[���G���A�͈̔͂Ɏ��܂�Ȃ��ꍇ�ɂ�
		// ���݂̕\���ʒu��g�ŕ\������
	if(canvas_area_width <= canvas->disp_size || canvas_area_height <= canvas->disp_size)
	{
		FLOAT_T x, y, width, height;

		x = canvas_scroll_x;
		navigation->point.x = x;
		x *= navigation->point.ratio_x;
		width = navigation->point.width * navigation_width;
		if(x + width > navigation_width)
		{
			width = navigation_width - x;
		}

		y = canvas_scroll_y;
		navigation->point.y = y;
		y *= navigation->point.ratio_y;
		height = navigation->point.height * navigation_height;
		if(y + height > navigation_height)
		{
			height = navigation_height - y;
		}

		NavigationDrawScrollFrame(navigation->widgets, x, y, width, height, draw_context);
	}
}
