#ifndef _INCLUDED_NAVIGATION_H_
#define _INCLUDED_NAVIGATION_H_

#include "types.h"

typedef struct _NAVIGATION
{
	NAVIGATION_WIDGETS *widgets;
	uint8 *pixels;
	uint8 *reverse_buffer;
	int width;			// �i�r�Q�[�V�����E�B�W�F�b�g�̕�
	int height;			// �i�r�Q�[�V�����E�B�W�F�b�g�̍���
	int draw_width;		// �摜�\���̈�̕�
	int draw_height;	// �摜�\���̈�̍���

	struct	// �摜�\���p�̍��W��g�嗦�f�[�^
	{
		FLOAT_T x, y;				// �i�r�Q�[�V�����ɕ\�����錻�\���̈�̘g�̍��W
		FLOAT_T ratio_x, ratio_y;	// �摜�`�掞�̉�/�c�g�嗦
		FLOAT_T width, height;		// �i�r�Q�[�V�����ɕ\������g�̃T�C�Y
		FLOAT_T angle;				// �L�����o�X�̉�]�p
		uint16 zoom;				// �i�r�Q�[�V�����̊g��k����
	} point;
} NAVIGATION;

#ifdef __cplusplus
extern "C" {
#endif

EXTERN NAVIGATION_WIDGETS* CreateNavigationWidgets(APPLICATION* app);

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
EXTERN void ChangeNavigationRotate(
	NAVIGATION* navigation,
	DRAW_WINDOW* canvas,
	int canvas_area_width,
	int canvas_area_height,
	int navigation_width,
	int navigation_height
);

EXTERN void ChangeNavigationDrawWindow(
	NAVIGATION* navigation,
	DRAW_WINDOW* canvas,
	int canvas_area_width,
	int canvas_area_height,
	int navigation_width,
	int navigation_height
);

EXTERN void DiplayNavigation(
	NAVIGATION* navigation,
	DRAW_WINDOW* canvas,
	int canvas_area_width,
	int canvas_area_height,
	int canvas_scroll_x,
	int canvas_scroll_y,
	int navigation_width,
	int navigation_height,
	void* draw_context
);

EXTERN void NavigationDrawCanvas(NAVIGATION_WIDGETS* widgets, void* draw_context);

EXTERN void NavigationDrawScrollFrame(
	NAVIGATION_WIDGETS* widgets,
	FLOAT_T x,
	FLOAT_T y,
	FLOAT_T width,
	FLOAT_T height,
	void* display_context
);

EXTERN void NavigationUpdate(NAVIGATION_WIDGETS* widgets);

#ifdef __cplusplus
}
#endif

#endif	/* _INCLUDED_NAVIGATION_H_ */
