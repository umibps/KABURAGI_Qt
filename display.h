#ifndef _INCLUDED_DISPLAY_H_
#define _INCLUDED_DISPLAY_H_

#include "draw_window.h"

EXTERN eDRAW_WINDOW_DIPSLAY_UPDATE_RESULT LayerBlendForDisplay(DRAW_WINDOW* canvas);

/*
* MixLayerForSave�֐�
* �ۑ�/�����I��/�o�P�c�c�[���̂��߂ɔw�i�s�N�Z���f�[�^�����Ń��C���[������
* ����
* canvas	: ���������{����L�����o�X
* �Ԃ�l
*	�������ꂽ���C���[ �g�p���DeleteLayer�K�v
*/
EXTERN LAYER* MixLayerForSave(DRAW_WINDOW* canvas);

#endif // #ifndef _INCLUDED_DISPLAY_H_
