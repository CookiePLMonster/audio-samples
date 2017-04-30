#include "Patterns.h"

#include "DelimStringReader.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

void PatchStreams( char* streams, uint32_t count )
{
	const size_t SCRATCH_PAD_SIZE = 32767;
	const size_t MAX_ENTRY_SIZE = 24;

	DelimStringReader reader( SCRATCH_PAD_SIZE );

	GetPrivateProfileSectionA( "Samples", reader.GetBuffer(), reader.GetSize(), "audio/audio-samples.ini" );

	uint32_t entriesRead = 0;
	size_t entryLength = 0;

	while ( const char* str = reader.GetString( &entryLength ) )
	{
		if ( entryLength <= MAX_ENTRY_SIZE )
		{
			strcpy_s( streams, MAX_ENTRY_SIZE+1, str );
		}
		streams += MAX_ENTRY_SIZE+1;
		if ( ++entriesRead >= count ) break;
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
	if ( reason == DLL_PROCESS_ATTACH)
	{
		using namespace hook;

		char* streamNames = *get_pattern<char*>( "8D 0C 49 01 C1 8D 44 24 04 81 C1", 11 );
		auto streamsCountVC = pattern( "0F 84 D5 00 00 00 81 FD ? ? ? ? 0F 83 C9 00 00 00" );
		auto streamsCountIII = pattern( "0F 84 2E 03 00 00 80 BC 24 1C 01 00 00" );

		uint32_t numStreams;
		if ( !streamsCountIII.count_hint(1).empty() )
			numStreams = *streamsCountIII.get_first<uint8_t>(13);
		else if ( !streamsCountVC.count_hint(1).empty() )
			numStreams = *streamsCountVC.get_first<uint32_t>(8);
		else
			return FALSE; // Not III nor VC?

		PatchStreams( streamNames, numStreams );
	}
	return TRUE;
}