/*
===================================================================================================

	Quake script command processing module

	Provides command text buffering (Cbuf) and command execution (Cmd)

===================================================================================================
*/

#pragma once

/*
===============================================================================
	Command buffer
===============================================================================
*/

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.

The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();

*/

void Cbuf_Init( void );
// allocates an initial text buffer that will grow as needed

void Cbuf_Shutdown( void );
// doesn't do anything, for completion's sake

void Cbuf_AddText( const char *text );
// as new commands are generated from the console or keybindings,
// the text is added to the end of the command buffer.

void Cbuf_InsertText( const char *text );
// when a command wants to issue other commands immediately, the text is
// inserted at the beginning of the buffer, before any remaining unexecuted
// commands.

void Cbuf_AddEarlyCommands( int argc, char **argv );
// adds all the +set commands from the command line

bool Cbuf_AddLateCommands( int argc, char **argv );
// adds all the remaining + commands from the command line
// Returns true if any late commands were added, which
// will keep the demoloop from immediately starting

void Cbuf_Execute( void );
// Pulls off \n terminated lines of text from the command buffer and sends
// them through Cmd_ExecuteString.  Stops when the buffer is empty.
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function!

void Cbuf_CopyToDefer( void );
void Cbuf_InsertFromDefer( void );
// These two functions are used to defer any pending commands while a map
// is being loaded

/*
===============================================================================
	Command execution
===============================================================================
*/

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

*/

typedef void ( *xcommand_t )( void );

enum cmdFlags_t : uint32
{
	CMD_STATIC	= BIT(0),	// this cmd is static (do not free)
	CMD_CHEAT	= BIT(1),	// this cmd is a cheat
};

// HACK: would be nice to find a way to iterate through commands without having this be visible
// this is only here to satisfy the console
struct cmdFunction_t
{
	const char *		pName;
	const char *		pHelp;
	xcommand_t			pFunction;
	cmdFunction_t *		pNext;

	uint32				flags;
};

// possible commands to execute
extern cmdFunction_t *cmd_functions;

void	Cmd_Init();
void	Cmd_Shutdown();

void	Cmd_AddCommand( const char *cmd_name, xcommand_t function, const char *help = nullptr );
// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory
// if function is NULL, the command will be forwarded to the server
// as a clc_stringcmd instead of executed locally
void	Cmd_RemoveCommand( const char *cmd_name );

bool Cmd_Exists( const char *cmd_name );
// used by the cvar code to check for cvar / command name overlap

const char *Cmd_CompleteCommand( const char *partial );
// attempts to match a partial command for automatic command line completion
// returns NULL if nothing fits

int		Cmd_Argc( void );
char	*Cmd_Argv( int arg );
char	*Cmd_Args( void );
// The functions that execute commands get their parameters with these
// functions. Cmd_Argv () will return an empty string, not a NULL
// if arg > argc, so string operations are always safe.

void	Cmd_TokenizeString( char *text, bool macroExpand );
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

void	Cmd_ExecuteString( char *text );
// Parses a single line of text into arguments and tries to execute it
// as if it was typed at the console

void	Cmd_ForwardToServer( void );
// contained in cl_main.cpp...
// adds the current command line as a clc_stringcmd to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.

/*
===================================================================================================

	Statically defined commands

===================================================================================================
*/

class StaticCmd : public cmdFunction_t
{
public:
	StaticCmd( const char *Name, xcommand_t Function, const char *Help = nullptr, uint32 Flags = 0 );
	~StaticCmd() = default;
};

static_assert( sizeof( StaticCmd ) == sizeof( cmdFunction_t ) );

#define CON_COMMAND(name, help, flags) \
	static void name(); \
	static StaticCmd name##_cmd(#name, name, help); \
	static void name()
