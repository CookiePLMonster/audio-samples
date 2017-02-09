#include "Patterns.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

void PatchStreams( char* streams, uint32_t count )
{
	const size_t SCRATCH_PAD_SIZE = 32767;
	const size_t MAX_ENTRY_SIZE = 24;

	char* buf = new char[ SCRATCH_PAD_SIZE ];
	GetPrivateProfileSectionA( "Samples", buf, SCRATCH_PAD_SIZE, "audio/audio-samples.ini" );

	uint32_t entriesRead = 0;
	for( char* raw = buf; *raw != '\0'; ++raw )
	{
		char thisStream[MAX_ENTRY_SIZE+1];
		char* stream = thisStream;
		for ( size_t i = 0; i < MAX_ENTRY_SIZE; ++i )
		{
			*stream++ = *raw++;
			if ( *raw == '\0' ) break;
		}
		*stream = '\0';

		bool rejectEntry = false;
		if ( *raw != '\0' )
		{
			rejectEntry = true;
			while ( *++raw != '\0' );
		}
		if ( !rejectEntry )
		{
			strcpy_s( streams, MAX_ENTRY_SIZE+1, thisStream );
		}
		streams += MAX_ENTRY_SIZE+1;
		if ( ++entriesRead >= count ) break;
	}

	delete[] buf;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
	if ( reason == DLL_PROCESS_ATTACH)
	{
		using namespace hook;

		char* streamNames = *get_pattern<char*>( "8D 0C 49 01 C1 8D 44 24 04 81 C1", 11 );
		uint32_t* streamsCountVC = get_pattern<uint32_t>( "0F 84 D5 00 00 00 81 FD ? ? ? ? 0F 83 C9 00 00 00", 8 );
		uint8_t* streamsCountIII = get_pattern<uint8_t>( "0F 84 2E 03 00 00 80 BC 24 1C 01 00 00 ?", 13 );

		uint32_t numStreams;
		if ( streamsCountIII != nullptr )
			numStreams = *streamsCountIII;
		else if ( streamsCountVC != nullptr )
			numStreams = *streamsCountVC;
		else
			return FALSE; // Not III nor VC?

		PatchStreams( streamNames, numStreams );
	}
	return TRUE;
}