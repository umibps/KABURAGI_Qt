#ifndef _INCLUDE_TYPES_H_
#define _INCLUDE_TYPES_H_

#include <stdio.h>
#include <math.h>

#if defined(_MSC_VER)

typedef char int8;
typedef short int16;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef long int32;
typedef unsigned long uint32;
typedef long long int int64;
typedef unsigned long long int uint64;

#else

# if !defined(_TIFFCONF_)

#  include <stdint.h>

typedef char int8;
typedef short int16;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef long long int int64;
typedef unsigned long long int uint64;

# endif

#endif

typedef double FLOAT_T;

#define MAXIMUM(A, B) (((A) > (B)) ? (A) : (B))
#define MINIMUM(A, B) (((A) < (B)) ? (A) : (B))

#define DIV_PIXEL ((FLOAT_T)0.00392157)

#define ROOT2 1.4142135623730950488016887242097

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

typedef size_t (*stream_func_t)(void*, size_t, size_t, void*);
typedef int (*seek_func_t)(void*, long, int);

#ifdef EXTERN
# undef EXTERN
#endif

#ifdef _MSC_VER
# ifdef __cplusplus
#  define EXTERN extern "C" __declspec(dllexport)
# else
#  define EXTERN extern __declspec(dllexport)
# endif
#else
# define EXTERN extern
#endif

#ifndef INLINE
# ifdef _MSC_VER
#  define INLINE __inline
# else
#  define INLINE inline
# endif
#endif

typedef struct _LAYER LAYER;
typedef struct _DRAW_WINDOW DRAW_WINDOW;
typedef struct _TOOL_BOX TOOL_BOX;
typedef struct _APPLICATION APPLICATION;
typedef struct _TOOL_WINDOW_WIDGETS TOOL_WINDOW_WIDGETS;
typedef struct _LAYER_VIEW_WIDGETS LAYER_VIEW_WIDGETS;
typedef struct _NAVIGATION_WIDGETS NAVIGATION_WIDGETS;
typedef struct _BRUSH_CORE BRUSH_CORE;
typedef struct _GENERAL_BRUSH GENERAL_BRUSH;
typedef struct _COMMON_TOOL_CORE COMMON_TOOL_CORE;
typedef struct _UPDATE_RECTANGLE UPDATE_RECTANGLE;

typedef enum _eCURSOR_INPUT_DEVICE
{
	CURSOR_INPUT_DEVICE_MOUSE,
	CURSOR_INPUT_DEVICE_PEN,
	CURSOR_INPUT_DEVICE_ERASER,
	CURSOR_INPUT_DEVICE_TOUCH
} eCURSOR_INPUT_DEVICE;

typedef enum _eMOUSE_KEY_FLAG
{
	MOUSE_KEY_FLAG_LEFT = 0x01,
	MOUSE_KEY_FLAG_RIGHT = 0x02,
	MOUSE_KEY_FLAG_CENTER = 0x04,
	MOUSE_KEY_FLAG_SHIFT = 0x08,
	MOUSE_KEY_FLAG_CONTROL = 0x10,
	MOUSE_KEY_FLAG_ALT = 0x20
} eMOUSE_KEY_FLAG;

typedef struct _EVENT_STATE
{
	FLOAT_T cursor_x, cursor_y;
	eCURSOR_INPUT_DEVICE input_device;
	FLOAT_T pressure;
	int mouse_key_flag;
	uint32 event_time;
} EVENT_STATE;

#define INITIALIZE_EVENT_STATE(STATE, X, Y, INPUT_DEVICE, PRESSURE, \
                                MOUSE_KEY_FLAG, EVENT_TIME) \
    (STATE).cursor_x = (X), (STATE).cursor_y = (Y); \
    (STATE).pressure = (PRESSURE),  (STATE).mouse_key_flag = (MOUSE_KEY_FLAG); \
    (STATE).event_time = (EVENT_TIME);

#endif	// #ifndef _INCLUDE_TYPES_H_
