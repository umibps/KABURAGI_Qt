// Visual Studio 2005以降では古いとされる関数を使用するので
	// 警告が出ないようにする
#if defined _MSC_VER && _MSC_VER >= 1400
# define _CRT_SECURE_NO_DEPRECATE
#endif

#include <string.h>
#include "memory.h"
#include "history.h"
#include "application.h"

#ifdef __cplusplus
extern "C" {
#endif

static void ReleaseRedoData(HISTORY* history)
{
	int ref;
	int i;

	for(i=0; i<history->rest_redo; i++)
	{
		ref = (history->point+i) % HISTORY_BUFFER_SIZE;
		MEM_FREE_FUNC(history->history[ref].data);
		history->history[ref].data = NULL;
	}
	history->rest_redo = 0;
}

static void AddHistoryData(
	HISTORY_DATA* history,
	const char* name,
	const void* data,
	size_t data_size,
	history_func undo,
	history_func redo
)
{
	(void)strcpy(history->name, name);
	history->data_size = data_size;
	history->undo = undo;
	history->redo = redo;
	history->data = MEM_ALLOC_FUNC(data_size);
	(void)memcpy(history->data, data, data_size);
}

void AddHistory(
	HISTORY* history,
	const char* name,
	const void* data,
	size_t data_size,
	history_func undo,
	history_func redo
)
{
	ReleaseRedoData(history);

	history->num_step++;

	if(history->rest_undo < HISTORY_BUFFER_SIZE)
	{
		history->rest_undo++;
	}

	if(history->point >= HISTORY_BUFFER_SIZE)
	{
		history->point = 0;
		MEM_FREE_FUNC(history->history->data);
		AddHistoryData(history->history, name, data, data_size, undo, redo);
	}
	else
	{
		MEM_FREE_FUNC(history->history[history->point].data);
		AddHistoryData(&history->history[history->point],
			name, data, data_size, undo, redo);
	}

	history->point++;

	history->flags |= HISTORY_UPDATED;
}

#ifdef __cplusplus
}
#endif
