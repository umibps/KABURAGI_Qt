#ifndef _INCLUDED_GRAPHICS_DEVICE_H_
#define _INCLUDED_GRAPHICS_DEVICE_H_

#include "graphics_status.h"

typedef struct _GRAPHICS_DEVICE
{
	int ref_count;
	eGRAPHICS_STATUS status;

	// void *backend;
	// void *mutex;
} GRAPHICS_DEVICE;

#endif // #ifndef _INCLUDED_GRAPHICS_DEVICE_H_
