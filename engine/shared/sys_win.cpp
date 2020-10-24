// sys_win.c

#include "common.h"
#include "conproc.h"
#include "../client/winquake.h"	// Hack?

//#define DEMO

qboolean		ActiveApp, Minimized;

static HANDLE	hinput, houtput;

unsigned		sys_msg_time;
unsigned		sys_frame_time;

int		g_argc;
char	**g_argv;

/*
===============================================================================

SYSTEM IO

===============================================================================
*/

[[noreturn]]
void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[MAX_PRINT_MSG];

	CL_Shutdown ();
	Qcommon_Shutdown ();

	va_start (argptr, error);
	Com_vsprintf (text, error, argptr);
	va_end (argptr);

	// cl_hwnd can be null
	MessageBoxA(cl_hwnd, text, "Error", MB_OK | MB_ICONERROR);

// shut down QHOST hooks if necessary
	DeinitConProc ();

	exit (1);
}

[[noreturn]]
void Sys_Quit (void)
{
	CL_Shutdown();
	Qcommon_Shutdown ();
	if (dedicated && dedicated->value)
		FreeConsole ();

// shut down QHOST hooks if necessary
	DeinitConProc ();

	exit (0);
}

//================================================================

/*
================
Sys_CopyProtect

================
*/
void Sys_CopyProtect (void)
{

}

//================================================================

/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
	if (dedicated->value)
	{
		if (!AllocConsole ())
			Sys_Error ("Couldn't create dedicated server console");
		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);
	
		// let QHOST hook in
		InitConProc (g_argc, g_argv);
	}
}

static char	console_text[256];
static int	console_textlen;

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
	INPUT_RECORD	recs[1024];
	DWORD	numread, numevents, dummy;
	int		ch;

	if (!dedicated || !dedicated->value)
		return NULL;

	for ( ;; )
	{
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents == 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);	

						if (console_textlen)
						{
							console_text[console_textlen] = 0;
							console_textlen = 0;
							return console_text;
						}
						break;

					case '\b':
						if (console_textlen)
						{
							console_textlen--;
							WriteFile(houtput, "\b \b", 3, &dummy, NULL);	
						}
						break;

					default:
						if (ch >= ' ')
						{
							if (console_textlen < sizeof(console_text)-2)
							{
								WriteFile(houtput, &ch, 1, &dummy, NULL);	
								console_text[console_textlen] = ch;
								console_textlen++;
							}
						}

						break;

				}
			}
		}
	}

	return NULL;
}


/*
================
Sys_ConsoleOutput

Print text to the dedicated console
================
*/
void Sys_ConsoleOutput (const char *string)
{
	DWORD	dummy;
	char	text[256];

	if (!dedicated || !dedicated->value)
		return;

	if (console_textlen)
	{
		text[0] = '\r';
		memset(&text[1], ' ', console_textlen);
		text[console_textlen+1] = '\r';
		text[console_textlen+2] = 0;
		WriteFile(houtput, text, console_textlen+2, &dummy, NULL);
	}

	WriteFile(houtput, string, strlen(string), &dummy, NULL);

	if (console_textlen)
		WriteFile(houtput, console_text, console_textlen, &dummy, NULL);
}


/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents (void)
{
    MSG        msg;

	while (PeekMessageW (&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessageW (&msg, NULL, 0, 0))
			Sys_Quit ();
		sys_msg_time = msg.time;
      	TranslateMessage (&msg);
      	DispatchMessageW (&msg);
	}

	// grab frame time
	sys_frame_time = Sys_Milliseconds(); // FIXME: should this be at start?
}


/*
================
Sys_GetClipboardData

================
*/
char *Sys_GetClipboardData( void )
{
	char *data = NULL;
	char *cliptext;

	if ( OpenClipboard( NULL ) != 0 )
	{
		HANDLE hClipboardData;

		if ( ( hClipboardData = GetClipboardData( CF_TEXT ) ) != 0 )
		{
			if ( ( cliptext = (char*)GlobalLock( hClipboardData ) ) != 0 ) 
			{
				data = (char*)malloc( GlobalSize( hClipboardData ) + 1 );
				strcpy( data, cliptext );
				GlobalUnlock( hClipboardData );
			}
		}
		CloseClipboard();
	}
	return data;
}

/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/

/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate (void)
{
	ShowWindow ( cl_hwnd, SW_RESTORE);
	SetForegroundWindow ( cl_hwnd );
}

/*
========================================================================

GAME DLL

========================================================================
*/

static HINSTANCE	game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	if (!FreeLibrary (game_library))
		Com_Error (ERR_FATAL, "FreeLibrary failed for game library");
	game_library = NULL;
}

typedef void *(*GetGameAPI_t) (void *);

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetGameAPI (void *parms)
{
	GetGameAPI_t GetGameAPI;
	char	name[MAX_OSPATH];
	char	*path;
	char	cwd[MAX_OSPATH];

	const char *gamename = "game" BLD_ARCHITECTURE ".dll";

#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif

	if (game_library)
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	// check the current debug directory first for development purposes
	GetCurrentDirectoryA (sizeof(cwd), cwd);
	Com_sprintf (name, "%s/%s/%s", cwd, debugdir, gamename);
	game_library = LoadLibraryA ( name );
	if (game_library)
	{
		Com_DPrintf ("LoadLibrary (%s)\n", name);
	}
	else
	{
#if 0
		// check the current directory for other development purposes
		Com_sprintf (name, "%s/%s", cwd, gamename);
		game_library = LoadLibrary ( name );
		if (game_library)
		{
			Com_DPrintf ("LoadLibrary (%s)\n", name);
		}
		else
#endif
		{
			// now run through the search paths
			path = NULL;
			while (1)
			{
				path = FS_NextPath (path);
				if (!path)
					return NULL;		// couldn't find one anywhere
				Com_sprintf (name, "%s/%s", path, gamename);
				game_library = LoadLibraryA (name);
				if (game_library)
				{
					Com_DPrintf ("LoadLibrary (%s)\n",name);
					break;
				}
			}
		}
	}

	GetGameAPI = (GetGameAPI_t)GetProcAddress (game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame ();
		return NULL;
	}

	return GetGameAPI (parms);
}

//=======================================================================

/*
==================
WinMain

==================
*/
HINSTANCE	g_hInstance;

int main(int argc, char **argv)
{
	int time, oldtime, newtime;

	g_hInstance = GetModuleHandleW(NULL);

	g_argc = argc; g_argv = argv;

	// no abort/retry/fail errors
	// Slart: Was SetErrorMode
	SetThreadErrorMode(SEM_FAILCRITICALERRORS, NULL);

	Qcommon_Init (argc, argv);
	oldtime = Sys_Milliseconds ();

    /* main window message loop */
	while (1)
	{
		// if at a full screen console, don't update unless needed
		if (Minimized || (dedicated && dedicated->value) )
		{
			Sleep (1);
		}

		do
		{
			newtime = Sys_Milliseconds ();
			time = newtime - oldtime;
		} while (time < 1);

		Qcommon_Frame (time);

		oldtime = newtime;
	}

	// never gets here
    return TRUE;
}