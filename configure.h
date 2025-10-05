#ifndef _INCLUDED_CONFIGURE_H_
#define _INCLUDED_CONFIGURE_H_

#define APPLICATION_INITIALIZE_FILE_PATH "./application.ini"
#define COMMON_TOOL_INITIALIZE_FILE_PATH "./common_tools.ini"
#define BRUSH_INITIALIZE_FILE_PATH "./brushes.ini"
#define VECTOR_BRUSH_INITIALIZE_FILE_PATH "./vector_brushes.ini"

#define CHECK_MEMORY_POOL 0
#define USE_BGR_COLOR_SPACE 1

#define PRIOR_PRESSURE_TO_PRESS_EVENT 0

#define USE_STATIC_LIB 0

#ifdef _X64
# define BUILD64BIT 1
#endif

#ifndef USE_3D_LAYER
# define USE_3D_LAYER 1
#endif

#define USE_TBB 1

#define FRAME_RATE 60
#define AUTO_SAVE_INTERVAL 60

#define MAX_ZOOM 400	// 最大拡大率
#define MIN_ZOOM 10		// 最小の拡大率

// 最低筆圧
#define MINIMUM_PRESSURE 0.005
// ペンをタブレットから離したしたと判定する閾値
#define RELEASE_PRESSURE 0.003

#define AUTO_SAVE(X) AutoSave((X))

// #define OLD_SELECTION_AREA

#if defined(_WIN32)
# define DYNAMICL_LIBRARY_EXTENTION "dll"
#elif defined(_MAC_OS_)
# define DYNAMICL_LIBRARY_EXTENTION "dylib"
#else
# define DYNAMICL_LIBRARY_EXTENTION "so"
#endif

#endif	// #ifndef _INCLUDED_CONFIGURE_H_
