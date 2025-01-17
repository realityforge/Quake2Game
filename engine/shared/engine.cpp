//=================================================================================================
// Central command
//=================================================================================================

#include "engine.h"

#include <csetjmp>
#include <numeric>

#include "../../thirdparty/tracy/Tracy.hpp"

extern void SCR_EndLoadingPlaque();
extern void Key_Init();
extern void Key_Shutdown();

static constexpr auto StatsLogFile_Name = "stats.log";
static constexpr auto LogFile_Name = "qconsole.log";

static jmp_buf		abortframe;		// an ERR_DROP occured, exit the entire frame

fsHandle_t			log_stats_file;
static fsHandle_t	logfile;

cvar_t *	com_speeds;
cvar_t *	com_logStats;
cvar_t *	com_developer;
cvar_t *	com_timeScale;
cvar_t *	com_fixedTime;
cvar_t *	com_logFile;			// 1 = buffer log, 2 = flush after each print
cvar_t *	com_showTrace;
cvar_t *	dedicated;

static int		server_state;

// com_speeds times
int		time_before_game;
int		time_after_game;
int		time_before_ref;
int		time_after_ref;

static thread_local bool	isMainThread;

static mutex_t				s_printMutex;		// Mutex for Com_Print

/*
============================================================================

CLIENT / SERVER interactions

============================================================================
*/

static int	rd_target;
static char	*rd_buffer;
static int	rd_buffersize;
static rd_flush_t rd_flush;

void Com_BeginRedirect (int target, char *buffer, int buffersize, rd_flush_t flush)
{
	if (!target || !buffer || !buffersize || !flush)
		return;
	rd_target = target;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = (rd_flush_t)flush;

	*rd_buffer = 0;
}

void Com_EndRedirect (void)
{
	rd_flush(rd_target, rd_buffer);

	rd_target = 0;
	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
}

/*
========================
Com_Print

Both client and server can use this, and it will output
to the apropriate place.
========================
*/

void CopyAndStripColorCodes( char *dest, strlen_t destSize, const char *src )
{
	const char *last;
	int c;

	last = dest + destSize - 1;
	while ( ( dest < last ) && ( c = *src ) != 0 ) {
		if ( src[0] == C_COLOR_ESCAPE && src + 1 != last && IsColorIndex( src[1] ) ) {
			src++;
		}
		else {
			*dest++ = c;
		}
		src++;
	}
	*dest = '\0';
}

void Com_Print( const char *msg )
{
	char newMsg[MAX_PRINT_MSG];

	// create a copy of the msg for places that don't want the colour codes
	CopyAndStripColorCodes( newMsg, sizeof( newMsg ), msg );

	// Lock mutex
	Sys_MutexLock( s_printMutex );

	if ( rd_target ) {
		if ( ( strlen( newMsg ) + strlen( rd_buffer ) ) > ( rd_buffersize - 1 ) ) {
			rd_flush( rd_target, rd_buffer );
			*rd_buffer = 0;
		}
		strcat( rd_buffer, newMsg );

		// Unlock mutex
		Sys_MutexUnlock( s_printMutex );

		return;
	}

	Sys_OutputDebugString( newMsg );

	UI::Console::Print( msg );

	// also echo to debugging console
	Sys_ConsoleOutput( newMsg );

	// logfile
	if ( com_logFile && com_logFile->GetBool() )
	{
		if ( !logfile )
		{
			if ( com_logFile->GetInt() > 2 )
			{
				logfile = FileSystem::OpenFileAppend( LogFile_Name );
			}
			else
			{
				logfile = FileSystem::OpenFileWrite( LogFile_Name );
			}
		}
		if ( logfile )
		{
			FileSystem::PrintFile( newMsg, logfile );
		}
		if ( com_logFile->GetInt() > 1 )
		{
			// force it to save every time
			FileSystem::FlushFile( logfile );
		}
	}

	// Unlock mutex
	Sys_MutexUnlock( s_printMutex );
}

void Com_Printf( _Printf_format_string_ const char *fmt, ... )
{
	va_list		argptr;
	char		msg[MAX_PRINT_MSG];

	va_start( argptr, fmt );
	Q_vsprintf_s( msg, fmt, argptr );
	va_end( argptr );

	Com_Print( msg );
}

/*
========================
Com_DPrint

A Com_Print that only shows up if the "developer" cvar is set
========================
*/

void Com_DPrint( const char *msg )
{
	if ( !com_developer || !com_developer->GetBool() ) {
		return;
	}

	Com_Printf( S_COLOR_YELLOW "%s", msg );
}

void Com_DPrintf( _Printf_format_string_ const char *fmt, ... )
{
	if ( !com_developer || !com_developer->GetBool() ) {
		return;
	}

	va_list argptr;
	char msg[MAX_PRINT_MSG];

	va_start( argptr, fmt );
	Q_vsprintf_s( msg, fmt, argptr );
	va_end( argptr );

	Com_Printf( S_COLOR_YELLOW "%s", msg );
}

/*
========================
Com_Error

The peaceful option
Equivalent to an old Com_Error( ERR_DROP )
========================
*/

[[noreturn]]
void Com_Error( const char *msg )
{
	static bool recursive;

	if ( recursive )
	{
		// This is should never happen
		// in fact this string should be optimised away if everything goes to plan
		Com_FatalError( "Recursive error, tell a developer!\n" );
	}

	recursive = true;

	Com_Printf( S_COLOR_RED "ERROR: %s\n", msg );

	SV_Shutdown( va( S_COLOR_RED "Server crashed: %s\n", msg ), false );
	CL_Drop();

	recursive = false;

	longjmp( abortframe, -1 );
}

[[noreturn]]
void Com_Errorf( _Printf_format_string_ const char *fmt, ... )
{
	va_list argptr;
	char msg[MAX_PRINT_MSG];

	va_start( argptr, fmt );
	Q_vsprintf_s( msg, fmt, argptr );
	va_end( argptr );

	Com_Error( msg );
}

/*
========================
Com_FatalError

The nuclear option
Equivalent to an old Com_Error( ERR_FATAL )
Kills the server, kills the client, shuts the engine down and quits the program
========================
*/

[[noreturn]]
void Com_FatalError( const char *msg )
{
	SV_Shutdown( va( S_COLOR_RED "Server fatal crashed: %s", msg ), false );
	CL_Shutdown();
	Com_Shutdown();

	Sys_Error( PLATTEXT( "Engine Error" ), msg );
}

[[noreturn]]
void Com_FatalErrorf( _Printf_format_string_ const char *fmt, ... )
{
	va_list argptr;
	char msg[MAX_PRINT_MSG];

	va_start( argptr, fmt );
	Q_vsprintf_s( msg, fmt, argptr );
	va_end( argptr );

	Com_FatalError( msg );
}

//=================================================================================================

/*
========================
Com_Disconnect

Equivalent to an old Com_Error( ERR_DISCONNECT )
Drops the client from the server and returns to the beginning of the main loop
========================
*/
[[noreturn]]
void Com_Disconnect()
{
	Com_Print( "Server disconnected\n" );
	CL_Drop();
	longjmp( abortframe, -1 );
}

/*
========================
Com_Quit

Both client and server can use this, and it will
do the apropriate things.
========================
*/
[[noreturn]]
void Com_Quit( int code )
{
	SV_Shutdown( "Server quit\n", false );
	CL_Shutdown();
	Com_Shutdown();

	Sys_Quit( code );
}

//=================================================================================================

/*
========================
Com_ServerState
========================
*/
int Com_ServerState()
{
	return server_state;
}

/*
========================
Com_SetServerState
========================
*/
void Com_SetServerState( int state )
{
	server_state = state;
}

//=================================================================================================

/*
========================
Info_Print
========================
*/
void Info_Print( const char *s )
{
	char	key[512];
	char	value[512];
	char *	o;
	int		l;

	if ( *s == '\\' ) {
		s++;
	}

	while ( *s ) {
		o = key;
		while ( *s && *s != '\\' ) {
			*o++ = *s++;
		}

		l = o - key;
		if ( l < 20 ) {
			memset( o, ' ', 20 - l );
			key[20] = 0;
		}
		else {
			*o = 0;
		}
		Com_Print( key );

		if ( !*s ) {
			Com_Print( "MISSING VALUE\n" );
			return;
		}

		o = value;
		s++;
		while ( *s && *s != '\\' ) {
			*o++ = *s++;
		}
		*o = 0;

		if ( *s ) {
			s++;
		}
		Com_Printf( "%s\n", value );
	}
}

//=================================================================================================

static const byte chktbl[1024]{
0x84, 0x47, 0x51, 0xc1, 0x93, 0x22, 0x21, 0x24, 0x2f, 0x66, 0x60, 0x4d, 0xb0, 0x7c, 0xda,
0x88, 0x54, 0x15, 0x2b, 0xc6, 0x6c, 0x89, 0xc5, 0x9d, 0x48, 0xee, 0xe6, 0x8a, 0xb5, 0xf4,
0xcb, 0xfb, 0xf1, 0x0c, 0x2e, 0xa0, 0xd7, 0xc9, 0x1f, 0xd6, 0x06, 0x9a, 0x09, 0x41, 0x54,
0x67, 0x46, 0xc7, 0x74, 0xe3, 0xc8, 0xb6, 0x5d, 0xa6, 0x36, 0xc4, 0xab, 0x2c, 0x7e, 0x85,
0xa8, 0xa4, 0xa6, 0x4d, 0x96, 0x19, 0x19, 0x9a, 0xcc, 0xd8, 0xac, 0x39, 0x5e, 0x3c, 0xf2,
0xf5, 0x5a, 0x72, 0xe5, 0xa9, 0xd1, 0xb3, 0x23, 0x82, 0x6f, 0x29, 0xcb, 0xd1, 0xcc, 0x71,
0xfb, 0xea, 0x92, 0xeb, 0x1c, 0xca, 0x4c, 0x70, 0xfe, 0x4d, 0xc9, 0x67, 0x43, 0x47, 0x94,
0xb9, 0x47, 0xbc, 0x3f, 0x01, 0xab, 0x7b, 0xa6, 0xe2, 0x76, 0xef, 0x5a, 0x7a, 0x29, 0x0b,
0x51, 0x54, 0x67, 0xd8, 0x1c, 0x14, 0x3e, 0x29, 0xec, 0xe9, 0x2d, 0x48, 0x67, 0xff, 0xed,
0x54, 0x4f, 0x48, 0xc0, 0xaa, 0x61, 0xf7, 0x78, 0x12, 0x03, 0x7a, 0x9e, 0x8b, 0xcf, 0x83,
0x7b, 0xae, 0xca, 0x7b, 0xd9, 0xe9, 0x53, 0x2a, 0xeb, 0xd2, 0xd8, 0xcd, 0xa3, 0x10, 0x25,
0x78, 0x5a, 0xb5, 0x23, 0x06, 0x93, 0xb7, 0x84, 0xd2, 0xbd, 0x96, 0x75, 0xa5, 0x5e, 0xcf,
0x4e, 0xe9, 0x50, 0xa1, 0xe6, 0x9d, 0xb1, 0xe3, 0x85, 0x66, 0x28, 0x4e, 0x43, 0xdc, 0x6e,
0xbb, 0x33, 0x9e, 0xf3, 0x0d, 0x00, 0xc1, 0xcf, 0x67, 0x34, 0x06, 0x7c, 0x71, 0xe3, 0x63,
0xb7, 0xb7, 0xdf, 0x92, 0xc4, 0xc2, 0x25, 0x5c, 0xff, 0xc3, 0x6e, 0xfc, 0xaa, 0x1e, 0x2a,
0x48, 0x11, 0x1c, 0x36, 0x68, 0x78, 0x86, 0x79, 0x30, 0xc3, 0xd6, 0xde, 0xbc, 0x3a, 0x2a,
0x6d, 0x1e, 0x46, 0xdd, 0xe0, 0x80, 0x1e, 0x44, 0x3b, 0x6f, 0xaf, 0x31, 0xda, 0xa2, 0xbd,
0x77, 0x06, 0x56, 0xc0, 0xb7, 0x92, 0x4b, 0x37, 0xc0, 0xfc, 0xc2, 0xd5, 0xfb, 0xa8, 0xda,
0xf5, 0x57, 0xa8, 0x18, 0xc0, 0xdf, 0xe7, 0xaa, 0x2a, 0xe0, 0x7c, 0x6f, 0x77, 0xb1, 0x26,
0xba, 0xf9, 0x2e, 0x1d, 0x16, 0xcb, 0xb8, 0xa2, 0x44, 0xd5, 0x2f, 0x1a, 0x79, 0x74, 0x87,
0x4b, 0x00, 0xc9, 0x4a, 0x3a, 0x65, 0x8f, 0xe6, 0x5d, 0xe5, 0x0a, 0x77, 0xd8, 0x1a, 0x14,
0x41, 0x75, 0xb1, 0xe2, 0x50, 0x2c, 0x93, 0x38, 0x2b, 0x6d, 0xf3, 0xf6, 0xdb, 0x1f, 0xcd,
0xff, 0x14, 0x70, 0xe7, 0x16, 0xe8, 0x3d, 0xf0, 0xe3, 0xbc, 0x5e, 0xb6, 0x3f, 0xcc, 0x81,
0x24, 0x67, 0xf3, 0x97, 0x3b, 0xfe, 0x3a, 0x96, 0x85, 0xdf, 0xe4, 0x6e, 0x3c, 0x85, 0x05,
0x0e, 0xa3, 0x2b, 0x07, 0xc8, 0xbf, 0xe5, 0x13, 0x82, 0x62, 0x08, 0x61, 0x69, 0x4b, 0x47,
0x62, 0x73, 0x44, 0x64, 0x8e, 0xe2, 0x91, 0xa6, 0x9a, 0xb7, 0xe9, 0x04, 0xb6, 0x54, 0x0c,
0xc5, 0xa9, 0x47, 0xa6, 0xc9, 0x08, 0xfe, 0x4e, 0xa6, 0xcc, 0x8a, 0x5b, 0x90, 0x6f, 0x2b,
0x3f, 0xb6, 0x0a, 0x96, 0xc0, 0x78, 0x58, 0x3c, 0x76, 0x6d, 0x94, 0x1a, 0xe4, 0x4e, 0xb8,
0x38, 0xbb, 0xf5, 0xeb, 0x29, 0xd8, 0xb0, 0xf3, 0x15, 0x1e, 0x99, 0x96, 0x3c, 0x5d, 0x63,
0xd5, 0xb1, 0xad, 0x52, 0xb8, 0x55, 0x70, 0x75, 0x3e, 0x1a, 0xd5, 0xda, 0xf6, 0x7a, 0x48,
0x7d, 0x44, 0x41, 0xf9, 0x11, 0xce, 0xd7, 0xca, 0xa5, 0x3d, 0x7a, 0x79, 0x7e, 0x7d, 0x25,
0x1b, 0x77, 0xbc, 0xf7, 0xc7, 0x0f, 0x84, 0x95, 0x10, 0x92, 0x67, 0x15, 0x11, 0x5a, 0x5e,
0x41, 0x66, 0x0f, 0x38, 0x03, 0xb2, 0xf1, 0x5d, 0xf8, 0xab, 0xc0, 0x02, 0x76, 0x84, 0x28,
0xf4, 0x9d, 0x56, 0x46, 0x60, 0x20, 0xdb, 0x68, 0xa7, 0xbb, 0xee, 0xac, 0x15, 0x01, 0x2f,
0x20, 0x09, 0xdb, 0xc0, 0x16, 0xa1, 0x89, 0xf9, 0x94, 0x59, 0x00, 0xc1, 0x76, 0xbf, 0xc1,
0x4d, 0x5d, 0x2d, 0xa9, 0x85, 0x2c, 0xd6, 0xd3, 0x14, 0xcc, 0x02, 0xc3, 0xc2, 0xfa, 0x6b,
0xb7, 0xa6, 0xef, 0xdd, 0x12, 0x26, 0xa4, 0x63, 0xe3, 0x62, 0xbd, 0x56, 0x8a, 0x52, 0x2b,
0xb9, 0xdf, 0x09, 0xbc, 0x0e, 0x97, 0xa9, 0xb0, 0x82, 0x46, 0x08, 0xd5, 0x1a, 0x8e, 0x1b,
0xa7, 0x90, 0x98, 0xb9, 0xbb, 0x3c, 0x17, 0x9a, 0xf2, 0x82, 0xba, 0x64, 0x0a, 0x7f, 0xca,
0x5a, 0x8c, 0x7c, 0xd3, 0x79, 0x09, 0x5b, 0x26, 0xbb, 0xbd, 0x25, 0xdf, 0x3d, 0x6f, 0x9a,
0x8f, 0xee, 0x21, 0x66, 0xb0, 0x8d, 0x84, 0x4c, 0x91, 0x45, 0xd4, 0x77, 0x4f, 0xb3, 0x8c,
0xbc, 0xa8, 0x99, 0xaa, 0x19, 0x53, 0x7c, 0x02, 0x87, 0xbb, 0x0b, 0x7c, 0x1a, 0x2d, 0xdf,
0x48, 0x44, 0x06, 0xd6, 0x7d, 0x0c, 0x2d, 0x35, 0x76, 0xae, 0xc4, 0x5f, 0x71, 0x85, 0x97,
0xc4, 0x3d, 0xef, 0x52, 0xbe, 0x00, 0xe4, 0xcd, 0x49, 0xd1, 0xd1, 0x1c, 0x3c, 0xd0, 0x1c,
0x42, 0xaf, 0xd4, 0xbd, 0x58, 0x34, 0x07, 0x32, 0xee, 0xb9, 0xb5, 0xea, 0xff, 0xd7, 0x8c,
0x0d, 0x2e, 0x2f, 0xaf, 0x87, 0xbb, 0xe6, 0x52, 0x71, 0x22, 0xf5, 0x25, 0x17, 0xa1, 0x82,
0x04, 0xc2, 0x4a, 0xbd, 0x57, 0xc6, 0xab, 0xc8, 0x35, 0x0c, 0x3c, 0xd9, 0xc2, 0x43, 0xdb,
0x27, 0x92, 0xcf, 0xb8, 0x25, 0x60, 0xfa, 0x21, 0x3b, 0x04, 0x52, 0xc8, 0x96, 0xba, 0x74,
0xe3, 0x67, 0x3e, 0x8e, 0x8d, 0x61, 0x90, 0x92, 0x59, 0xb6, 0x1a, 0x1c, 0x5e, 0x21, 0xc1,
0x65, 0xe5, 0xa6, 0x34, 0x05, 0x6f, 0xc5, 0x60, 0xb1, 0x83, 0xc1, 0xd5, 0xd5, 0xed, 0xd9,
0xc7, 0x11, 0x7b, 0x49, 0x7a, 0xf9, 0xf9, 0x84, 0x47, 0x9b, 0xe2, 0xa5, 0x82, 0xe0, 0xc2,
0x88, 0xd0, 0xb2, 0x58, 0x88, 0x7f, 0x45, 0x09, 0x67, 0x74, 0x61, 0xbf, 0xe6, 0x40, 0xe2,
0x9d, 0xc2, 0x47, 0x05, 0x89, 0xed, 0xcb, 0xbb, 0xb7, 0x27, 0xe7, 0xdc, 0x7a, 0xfd, 0xbf,
0xa8, 0xd0, 0xaa, 0x10, 0x39, 0x3c, 0x20, 0xf0, 0xd3, 0x6e, 0xb1, 0x72, 0xf8, 0xe6, 0x0f,
0xef, 0x37, 0xe5, 0x09, 0x33, 0x5a, 0x83, 0x43, 0x80, 0x4f, 0x65, 0x2f, 0x7c, 0x8c, 0x6a,
0xa0, 0x82, 0x0c, 0xd4, 0xd4, 0xfa, 0x81, 0x60, 0x3d, 0xdf, 0x06, 0xf1, 0x5f, 0x08, 0x0d,
0x6d, 0x43, 0xf2, 0xe3, 0x11, 0x7d, 0x80, 0x32, 0xc5, 0xfb, 0xc5, 0xd9, 0x27, 0xec, 0xc6,
0x4e, 0x65, 0x27, 0x76, 0x87, 0xa6, 0xee, 0xee, 0xd7, 0x8b, 0xd1, 0xa0, 0x5c, 0xb0, 0x42,
0x13, 0x0e, 0x95, 0x4a, 0xf2, 0x06, 0xc6, 0x43, 0x33, 0xf4, 0xc7, 0xf8, 0xe7, 0x1f, 0xdd,
0xe4, 0x46, 0x4a, 0x70, 0x39, 0x6c, 0xd0, 0xed, 0xca, 0xbe, 0x60, 0x3b, 0xd1, 0x7b, 0x57,
0x48, 0xe5, 0x3a, 0x79, 0xc1, 0x69, 0x33, 0x53, 0x1b, 0x80, 0xb8, 0x91, 0x7d, 0xb4, 0xf6,
0x17, 0x1a, 0x1d, 0x5a, 0x32, 0xd6, 0xcc, 0x71, 0x29, 0x3f, 0x28, 0xbb, 0xf3, 0x5e, 0x71,
0xb8, 0x43, 0xaf, 0xf8, 0xb9, 0x64, 0xef, 0xc4, 0xa5, 0x6c, 0x08, 0x53, 0xc7, 0x00, 0x10,
0x39, 0x4f, 0xdd, 0xe4, 0xb6, 0x19, 0x27, 0xfb, 0xb8, 0xf5, 0x32, 0x73, 0xe5, 0xcb, 0x32
};

/*
========================
COM_BlockSequenceCRCByte

For proxy protecting
========================
*/
byte COM_BlockSequenceCRCByte( byte *base, int length, int sequence )
{
	if ( sequence < 0 ) {
		Com_FatalError( "sequence < 0, this shouldn't happen\n" );
	}

	const byte *p = chktbl + ( sequence % ( sizeof( chktbl ) - 4 ) );

	if ( length > 60 ) {
		length = 60;
	}

	byte chkb[60 + 4];
	memcpy( chkb, base, length );

	chkb[length] = p[0];
	chkb[length + 1] = p[1];
	chkb[length + 2] = p[2];
	chkb[length + 3] = p[3];

	length += 4;

	uint16 crc = crc16::Block( chkb, length );

	int x, n;

	for ( x = 0, n = 0; n < length; n++ ) {
		x += chkb[n];
	}

	crc = ( crc ^ x ) & 0xff;

	return crc;
}

//=================================================================================================

// Compressed vertex normals
vec3_t bytedirs[NUMVERTEXNORMALS]
{
#include "../renderer/anorms.inl"
};

//=================================================================================================

/*
========================
Com_Error_f

Just throw a fatal error to
test error shutdown procedures
========================
*/
static void Com_Error_f()
{
	Com_FatalError( "Error test\n" );
}

/*
========================
Com_Quit_f

A version of the quit command for dedicated servers
the client uses the one in cl_main.cpp
========================
*/
static void Com_Quit_f()
{
	Com_Quit( EXIT_SUCCESS );
}

/*
========================
Com_Version_f

Prints the engine version to the console
========================
*/
static void Com_Version_f()
{
	Com_Printf( "%s - %s, %s\n", BLD_STRING, __DATE__, __TIME__ );
}

#define NUM_TEST_THREADS 16

static uint32 ThreadPrinter( void *params )
{
	uint *num = (uint *)params;

	Com_Printf( "Hello from thread %u\n", *num );

	return 0;
}

/*
========================
Com_PerfTest_f

Test stuff here!
========================
*/
static void Com_PerfTest_f()
{
	uint threadNums[NUM_TEST_THREADS];
	threadHandle_t threadHandles[NUM_TEST_THREADS];

	for ( uint i = 0; i < NUM_TEST_THREADS; ++i )
	{
		threadNums[i] = i;
		threadHandles[i] = Sys_CreateThread( ThreadPrinter, &threadNums[i], THREAD_NORMAL, PLATTEXT( "TestThread" ) );
	}

	Sys_WaitForMultipleThreads( threadHandles, NUM_TEST_THREADS );

	for ( uint i = 0; i < NUM_TEST_THREADS; ++i )
	{
		Sys_DestroyThread( threadHandles[i] );
	}
}

/*
========================
Com_Init
========================
*/
void Com_Init( int argc, char **argv )
{
	ZoneScoped

	isMainThread = true;

	if ( setjmp( abortframe ) ) {
		Com_FatalError( "Error during initialization\n" );
	}

	Sys_MutexCreate( s_printMutex );

	Mem_Init();

	// prepare enough of the subsystems to handle
	// cvar and command buffer management

	Cbuf_Init();

	Cmd_Init();
	Cvar_Init();

	// Here rather than in client because we need the bind cmds and stuff
	// for the Cbuf_ calls below, not good for dedicated
	Key_Init();

	// we need to add the early commands twice, because
	// a basedir or cddir needs to be set before execing
	// config files, but we want other parms to override
	// the settings of the config files
	Cvar_AddEarlyCommands( argc, argv );

	// Systems before this are not allowed to fail

	// Blah, this is a bad place for this
#ifdef DEDICATED_ONLY
	dedicated = Cvar_Get( "dedicated", "1", CVAR_INIT, "If true, this is a dedicated server." );
#else
	dedicated = Cvar_Get( "dedicated", "0", CVAR_INIT, "If true, this is a dedicated server." );
#endif

	Sys_Init( argc, argv );
	FileSystem::Init();

	Cbuf_AddText( "exec default.cfg\n" );
	Cbuf_AddText( "exec config.cfg\n" );
	Cbuf_AddText( "exec autoexec.cfg\n" );
	Cbuf_Execute();

	// command line priority
	Cvar_AddEarlyCommands( argc, argv );

	// cvars and commands

	com_speeds = Cvar_Get( "com_speeds", "0", 0, "Spam some info to the console or something idk." );
	com_logStats = Cvar_Get( "com_logStats", "0", 0, "Logs some stuff to the console idk." );
	com_developer = Cvar_Get( "com_developer", "0", 0, "Enables developer mode." );
	com_timeScale = Cvar_Get( "com_timeScale", "1", 0, "Scale time by this amount." );
	com_fixedTime = Cvar_Get( "com_fixedTime", "0", 0, "Force time to this value." );
	com_logFile = Cvar_Get( "com_logFile", "0", 0, "Directs all logged messages to a file." );
	com_showTrace = Cvar_Get( "com_showTrace", "0", 0, "Spams the console with trace stats." );

	Cmd_AddCommand( "com_perfTest", Com_PerfTest_f, "Perftest!" );
	Cmd_AddCommand( "com_error", Com_Error_f, "Throws a Com_Error." );
	Cmd_AddCommand( "com_version", Com_Version_f, "Prints engine version information." );
	if ( dedicated->GetBool() ) {
		Cmd_AddCommand( "quit", Com_Quit_f );
	}

	NET_Init();
	Netchan_Init();
	PhysicsImpl::Init();
	CM_Init();

	SV_Init();
	CL_Init();

	// add + commands from command line
	if ( !Cbuf_AddLateCommands( argc, argv ) )
	{
		// if the user didn't give any commands, run default action
		if ( !dedicated->GetBool() ) {
			Cbuf_AddText( "d1\n" );
		} else {
			Cbuf_AddText( "dedicated_start\n" );
		}
		Cbuf_Execute();
	}
	else
	{
		// the user asked for something explicit
		// so drop the loading plaque
		SCR_EndLoadingPlaque();
	}

	Com_Print( "======== " ENGINE_VERSION " Initialized ========\n\n" );
}

/*
========================
Com_Frame
========================
*/
void Com_Frame( int frameTime )
{
	ZoneScoped

	char	*s;
	int		time_before, time_between, time_after;

	if ( setjmp( abortframe ) ) {
		// an ERR_DROP was thrown
		return;
	}

	if ( com_logStats->IsModified() )
	{
		com_logStats->ClearModified();
		if ( com_logStats->GetBool() )
		{
			if ( log_stats_file )
			{
				FileSystem::CloseFile( log_stats_file );
				log_stats_file = nullptr;
			}
			log_stats_file = FileSystem::OpenFileWrite( StatsLogFile_Name );
			if ( log_stats_file )
			{
				FileSystem::PrintFile( "entities,dlights,parts,frame time\n", log_stats_file );
			}
		}
		else
		{
			if ( log_stats_file )
			{
				FileSystem::CloseFile( log_stats_file );
				log_stats_file = nullptr;
			}
		}
	}

	if ( com_fixedTime->GetInt() != 0 ) {
		frameTime = com_fixedTime->GetInt();
	}
	else if ( com_timeScale->GetInt() != 1 ) {
		frameTime *= com_timeScale->GetInt();
		if ( frameTime < 1 ) {
			frameTime = 1;
		}
	}

	if ( com_showTrace->GetBool() )
	{
		// cmodel
		extern int c_traces, c_brush_traces;
		extern int c_pointcontents;

		Com_Printf( "%4i traces  %4i points\n", c_traces, c_pointcontents );
		c_traces = 0;
		c_brush_traces = 0;
		c_pointcontents = 0;
	}

	// Add input from the dedicated server console
	do
	{
		s = Sys_ConsoleInput();
		if ( s ) {
			Cbuf_AddText( va( "%s\n", s ) );
		}
	} while ( s );

	Cbuf_Execute();

	if ( com_speeds->GetBool() ) {
		time_before = Sys_Milliseconds();
	}

	SV_Frame( frameTime );

	if ( com_speeds->GetBool() ) {
		time_between = Sys_Milliseconds();
	}

	CL_Frame( frameTime );

	if ( com_speeds->GetBool() ) {
		time_after = Sys_Milliseconds();
	}

	if ( com_speeds->GetBool() )
	{
		int all, sv, gm, cl, rf;

		all = time_after - time_before;
		sv = time_between - time_before;
		cl = time_after - time_between;
		gm = time_after_game - time_before_game;
		rf = time_after_ref - time_before_ref;
		sv -= gm;
		cl -= rf;
		Com_Printf( "all:%3i sv:%3i gm:%3i cl:%3i rf:%3i\n", all, sv, gm, cl, rf );
	}

	FrameMark
}

/*
========================
Com_Shutdown
========================
*/
void Com_Shutdown()
{
	if ( logfile ) {
		FileSystem::CloseFile( logfile );
		logfile = nullptr;
	}

	CM_Shutdown();
	PhysicsImpl::Shutdown();
	Sys_Shutdown();
	FileSystem::Shutdown();
	Key_Shutdown();
	Cvar_Shutdown();
	Cmd_Shutdown();
	Mem_Shutdown();

	Sys_MutexDestroy( s_printMutex );
}

/*
========================
Com_IsMainThread
========================
*/
bool Com_IsMainThread()
{
	return isMainThread;
}
