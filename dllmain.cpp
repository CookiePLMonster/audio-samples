#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "MemoryMgr.h"
#include "Patterns.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
	if ( reason == DLL_PROCESS_ATTACH)
	{


	}
	return TRUE;
}