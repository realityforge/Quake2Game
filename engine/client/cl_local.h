// client.h -- primary header for client

#pragma once

#include "../shared/engine.h"
#include "../renderer/ref_public.h"
#include "../../game/client/cg_public.h"

#include "snd_public.h"

#include "keycodes.h"

//=================================================================================================

extern centity_t	cl_entities[MAX_EDICTS];

// snapshots are a view of the server at a given time
struct clSnapshot_t
{
	qboolean		valid;			// cleared if delta parsing was invalid
	int				serverframe;
	int				servertime;		// server time the message is valid for (in msec)
	int				deltaframe;
	byte			areabits[MAX_MAP_AREAS/8];		// portalarea visibility bits
	player_state_t	playerstate;
	int				num_entities;
	int				parse_entities;	// non-masked index into cl_parse_entities array
};

/*
===================================================================================================

	the clientActive_t structure is wiped completely at every
	new gamestate_t, potentially several times during an established connection

===================================================================================================
*/

#define MAX_CLIENTWEAPONMODELS		20		// PGM -- upped from 16 to fit the chainfist vwep

struct clientinfo_t
{
	char		name[MAX_QPATH];
	char		cinfo[MAX_QPATH];
	material_t	*skin;
	material_t	*icon;
	char		iconname[MAX_QPATH];
	model_t		*model;
	model_t		*weaponmodel[MAX_CLIENTWEAPONMODELS];
};

// cl_view
extern char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
extern int num_cl_weaponmodels;

struct clientActive_t
{
	int			timeoutcount;

	int			timedemo_frames;
	int			timedemo_start;

	qboolean	refresh_prepped;	// false if on new level or new ref dll
	qboolean	sound_prepped;		// ambient sounds can start
	qboolean	force_refdef;		// vid has changed, so we can't use a paused refdef

	int			parse_entities;		// index (not anded off) into cl_parse_entities[]

	usercmd_t	cmd;
	usercmd_t	cmds[CMD_BACKUP];	// each mesage will send several old cmds
	int			cmd_time[CMD_BACKUP];	// time sent, for calculating pings
	vec3_t		predicted_origins[CMD_BACKUP];	// for debug comparing against server

	float		predicted_step;				// for stair up smoothing
	unsigned	predicted_step_time;

	pmove_state_t	predMove;		// generated by CL_PredictMovement
	vec3_t			predAngles;
	vec3_t			predError;

	clSnapshot_t	frame;				// received from server
	int				surpressCount;		// number of messages rate supressed
	clSnapshot_t	frames[UPDATE_BACKUP];

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	vec3_t		viewangles;

	int			time;			// this is the time value that the client
								// is rendering at.  always <= cls.realtime
	float		lerpfrac;		// between oldframe and frame

	refdef_t	refdef;

	vec3_t		v_forward, v_right, v_up;	// set when refdef.angles is set

	//
	// transient data from server
	//
	char		layout[1024];		// general 2D overlay
	int			inventory[MAX_ITEMS];

	//
	// non-gameserver infornamtion
	// FIXME: move this cinematic stuff into the cin_t structure
	fsHandle_t	cinematic_file;
	int			cinematictime;		// cls.realtime for first cinematic frame
	int			cinematicframe;
	byte		cinematicpalette[768];

	//
	// server state information
	//
	qboolean	attractloop;		// running the attract loop, any key will menu
	int			servercount;		// server identification for prespawns
	int			playernum;

	char		configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];

	//
	// locally derived information from server state
	//
	model_t		*model_draw[MAX_MODELS];
	cmodel_t	*model_clip[MAX_MODELS];

	sfx_t		*sound_precache[MAX_SOUNDS];
	material_t	*image_precache[MAX_IMAGES];

	clientinfo_t	clientinfo[MAX_CLIENTS];
	clientinfo_t	baseclientinfo;
};

extern clientActive_t	cl;

/*
===================================================================================================

	the clientStatic_t structure is never wiped, and is used even when
	no client connection is active at all

===================================================================================================
*/

// connection state
enum connstate_t
{
	ca_uninitialized,
	ca_disconnected, 	// not talking to a server
	ca_connecting,		// sending request packets to the server
	ca_connected,		// netchan_t established, waiting for svc_serverdata
	ca_active			// game views should be displayed
};

// download type
enum dltype_t
{
	dl_none,
	dl_model,
	dl_sound,
	dl_skin,
	dl_single
};

enum keydest_t { key_game, key_console, key_message, key_menu };

struct clientStatic_t
{
	connstate_t	state;
	keydest_t	key_dest;

	int			framecount;
	int			realtime;			// always increasing, no clamping, etc
	float		frametime;			// seconds since last frame

// screen rendering information
	float		disable_screen;		// showing loading plaque between levels
									// or changing rendering dlls
									// if time gets > 30 seconds ahead, break it
	int			disable_servercount;	// when we receive a frame and cl.servercount
									// > cls.disable_servercount, clear disable_screen

// connection information
	char		servername[MAX_OSPATH];	// name of server from original connect
	float		connect_time;		// for connection retransmits

	int			quakePort;			// a 16 bit value that allows quake servers
									// to work around address translating routers
	netchan_t	netchan;
	int			serverProtocol;		// in case we are doing some kind of version hack

	int			challenge;			// from the server to use for connecting

	fsHandle_t	download;			// file transfer from server
	char		downloadtempname[MAX_OSPATH];
	char		downloadname[MAX_OSPATH];
	int			downloadnumber;
	dltype_t	downloadtype;
	int			downloadpercent;

// demo recording info must be here, so it isn't cleared on level change
	fsHandle_t	demofile;
	bool		demorecording;
	bool		demowaiting;	// don't record until a non-delta message is received
};

extern clientStatic_t	cls;

//=================================================================================================

// the cl_parse_entities must be large enough to hold UPDATE_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original 
#define	MAX_PARSE_ENTITIES 1024
extern entity_state_t cl_parse_entities[MAX_PARSE_ENTITIES];

//
// cvars
//
extern cvar_t	*cl_drawviewmodel;
extern cvar_t	*cl_predict;
extern cvar_t	*cl_footsteps;
extern cvar_t	*cl_noskins;
extern cvar_t	*cl_autoskins;

extern cvar_t	*cl_upspeed;
extern cvar_t	*cl_forwardspeed;
extern cvar_t	*cl_sidespeed;

extern cvar_t	*cl_yawspeed;
extern cvar_t	*cl_pitchspeed;

extern cvar_t	*cl_run;

extern cvar_t	*cl_anglespeedkey;

extern cvar_t	*cl_shownet;
extern cvar_t	*cl_showmiss;
extern cvar_t	*cl_showclamp;

extern cvar_t	*sensitivity;

extern cvar_t	*m_pitch;
extern cvar_t	*m_yaw;
extern cvar_t	*m_forward;
extern cvar_t	*m_side;

extern cvar_t	*cl_lightlevel;	// FIXME HACK

extern cvar_t	*cl_paused;
extern cvar_t	*cl_timedemo;

extern cvar_t	*cl_vwep;

//=================================================================================================

// net_chan
extern netadr_t		net_from;
extern sizebuf_t	net_message;

//=================================================================================================

//
// cl_cgame
//
extern cgame_export_t *cge;

void CL_InitCGame();
void CL_ShutdownCGame();

//
// cl_vid
//
struct viddef_t
{
	int width, height;		// coordinates from main game
};

extern viddef_t viddef;		// global video state

void VID_Init();
void VID_Shutdown();

//
// cl_main
//
void CL_Init();

void CL_FixUpGender();
void CL_Disconnect();
void CL_Disconnect_f();
void CL_PingServers_f();
void CL_Snd_Restart_f();
void CL_RequestNextDownload();

// demos
void CL_WriteDemoMessage();

void CL_Quit_f();

//
// cl_input
//
struct kbutton_t
{
	int		down[2];		// key nums holding it down
	uint	downtime;		// msec timestamp
	uint	msec;			// msec down this frame
	int		state;
};

void CL_InitInput();
void CL_SendCmd();

void CL_ClearState();

void CL_ReadPackets();

void CL_BaseMove( usercmd_t *cmd );

void IN_CenterView();

float CL_KeyState( kbutton_t *key );
const char *Key_KeynumToString( int keynum );

//
// cl_keys
//
extern char *	keybindings[256];
extern int		key_repeats[256];

extern int		anykeydown;
extern char		chat_buffer[];
extern int		chat_bufferlen;
extern bool		chat_team;

// called by engine, not client, these declarations are never referenced
void Key_Init();
void Key_Shutdown();

void Key_Event( int key, bool down, unsigned time );
void Key_WriteBindings( fsHandle_t handle );
void Key_SetBinding( int keynum, const char *binding );
void Key_ClearStates();

//
// cl_ents
//
void CL_ParseFrame();
void CL_AddEntities();

int CL_ParseEntityBits( uint *bits );
void CL_ParseDelta( entity_state_t *from, entity_state_t *to, int number, uint bits );
void CL_ParseFrame();

//
// cl_parse
//
extern const char *svc_strings[256];

void CL_ParseServerMessage();
void CL_LoadClientinfo( clientinfo_t *ci, char *s );
void SHOWNET( const char *s );
void CL_ParseClientinfo( int player );
void CL_Download_f();

bool CL_CheckOrDownloadFile( const char *filename );
void CL_RegisterSounds();

//
// cl_scrn
//
struct vrect_t
{
	int x, y, width, height;
};

extern cvar_t *scr_crosshair;

extern vrect_t scr_vrect;		// position of render window

void SCR_Init();
void SCR_Shutdown();

void SCR_UpdateScreen();

void SCR_DrawStringColor( int x, int y, const char *s, uint32 color );
void SCR_DrawString( int x, int y, const char *s );
void SCR_DrawAltString( int x, int y, const char *s );

void SCR_CenterPrint( const char *str );
void SCR_BeginLoadingPlaque();
void SCR_EndLoadingPlaque();

void SCR_DebugGraph( float value, uint32 color );
void CL_AddNetgraph();

void SCR_TouchPics();

void SCR_ToggleDevUI();

//
// cl_cin
//
void SCR_PlayCinematic( const char *name );
bool SCR_DrawCinematic();
void SCR_RunCinematic();
void SCR_StopCinematic();
void SCR_FinishCinematic();

//
// cl_view
//
extern int			g_gunFrame;
extern model_t *	g_gunModel;

void V_Init();
void V_RenderView();

void CL_PrepRefresh();

void V_AddEntity( entity_t *ent );
void V_AddParticle( vec3_t org, int color, float alpha );
void V_AddDLight( vec3_t org, float intensity, float r, float g, float b );
void V_AddLightStyle( int style, float r, float g, float b );

//
// cl_pred
//
void CL_CheckPredictionError();
void CL_PredictMovement();

//
// menu
//
void M_Init();
void M_Shutdown();
void M_Keydown( int key );
void M_Draw();
void M_Menu_Main_f();
void M_ForceMenuOff();
void M_AddToServerList( netadr_t adr, char *info );

//
// cl_inv
//
void CL_ParseInventory();
void CL_DrawInventory();

//
// platform-specific input
//
namespace input
{
	void Init();

	void Shutdown();

	// oportunity for devices to stick commands on the script buffer
	void Commands();

	void Frame();

	void Activate( bool active );

	// when called, the game will Com_Quit at the beginning of the next frame
	void SendQuitMessage();
}

//
// platform-specific cdaudio
//
bool CDAudio_Init();
void CDAudio_Shutdown();
void CDAudio_Play( int track, bool looping );
void CDAudio_Stop();
void CDAudio_Update();
void CDAudio_Activate( bool active );

/*
===================================================================================================

	Graphical user interface elements

===================================================================================================
*/

/*
=======================================
	Console
=======================================
*/

namespace UI::Console
{
	void Init();
	void ShowConsole( bool *pOpen );

	void ShowNotify();
	void ClearNotify();

	void Print( const char *txt );
}

/*
=======================================
	Material Editor
=======================================
*/

namespace UI::MatEdit
{
	void ShowMaterialEditor( bool *pOpen );
}

/*
=======================================
	Stats Page
=======================================
*/

namespace UI::StatsPanel
{
	void ShowStats();
}

/*
=======================================
	Model Viewer
=======================================
*/

namespace UI::ModelViewer
{
	void ShowModelViewer( bool *pOpen );
}
