
// game_public.h -- game dll information visible to server

#pragma once

#include "../../common/filesystem_interface.h"
#include "../../physics/phys_public.h"

#define	GAME_API_VERSION	3

// edict->svflags

#define	SVF_NOCLIENT			0x00000001	// don't send entity to clients, even if it has effects
#define	SVF_DEADMONSTER			0x00000002	// treat as CONTENTS_DEADMONSTER for collision
#define	SVF_MONSTER				0x00000004	// treat as CONTENTS_MONSTER for collision

// edict->solid values

enum solid_t
{
	SOLID_NOT,			// no interaction with other objects
	SOLID_TRIGGER,		// only touch when inside, after moving
	SOLID_BBOX,			// touch on edge
	SOLID_BSP,			// bsp clip, touch on edge
	SOLID_PHYSICS		// rigid body
};

//===============================================================

// link_t is only used for entity area links now
struct link_t
{
	link_t	*prev, *next;
};

#define	MAX_ENT_CLUSTERS	16


struct gclient_t;
struct edict_t;


#ifndef GAME_INCLUDE

struct gclient_t
{
	player_state_t	ps;		// communicated by server to clients
	int				ping;
	// the game dll can add anything it wants after
	// this point in the structure
};


struct edict_t
{
	entityState_t	s;
	gclient_t *		client;

	qboolean		inuse;
	int				linkcount;

	// FIXME: move these fields to a server private sv_entity_t
	link_t			area;				// linked to a division node or leaf
	
	int				num_clusters;		// if -1, use headnode instead
	int				clusternums[MAX_ENT_CLUSTERS];
	int				headnode;			// unused if num_clusters != -1
	int				areanum, areanum2;

	//================================

	int				svflags;			// SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
	vec3_t			mins, maxs;
	vec3_t			absmin, absmax, size;
	//int			contents;				// I think Quake 3 has this? Need to add to the server edict structure...
	solid_t			solid;
	IPhysicsBody *	pPhysBody;
	int				clipmask;
	edict_t *		owner;

	// the game dll can add anything it wants after
	// this point in the structure
};

#endif		// GAME_INCLUDE

//===============================================================

//
// functions provided by the main engine
//
struct game_import_t
{
	// special messages
	void	(*bprintf) (int printlevel, _Printf_format_string_ const char *fmt, ...);
	void	(*dprintf) (_Printf_format_string_ const char *fmt, ...);
	void	(*cprintf) (edict_t *ent, int printlevel, _Printf_format_string_ const char *fmt, ...);
	void	(*centerprintf) (edict_t *ent, _Printf_format_string_ const char *fmt, ...);
	void	(*sound) (edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs);
	void	(*positioned_sound) (vec3_t origin, edict_t *ent, int channel, int soundinedex, float volume, float attenuation, float timeofs);

	// config strings hold all the index strings, the lightstyles,
	// and misc data like the sky definition and cdtrack.
	// All of the current configstrings are sent to clients when
	// they connect, and changes are sent to all connected clients.
	void	(*configstring) (int num, const char *string);

	void	(*error) (_Printf_format_string_ const char *fmt, ...);

	// the *index functions create configstrings and some internal server state
	int		(*modelindex) (const char *name);
	int		(*soundindex) (const char *name);
	int		(*imageindex) (const char *name);

	void	(*setmodel) (edict_t *ent, const char *name);

	// collision detection
	trace_t	(*trace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passent, int contentmask);
	int		(*pointcontents) (vec3_t point);
	qboolean	(*inPVS) (vec3_t p1, vec3_t p2);
	qboolean	(*inPHS) (vec3_t p1, vec3_t p2);
	void		(*SetAreaPortalState) (int portalnum, qboolean open);
	qboolean	(*AreasConnected) (int area1, int area2);

	// an entity will never be sent to a client or used for collision
	// if it is not passed to linkentity.  If the size, position, or
	// solidity changes, it must be relinked.
	void	(*linkentity) (edict_t *ent);
	void	(*unlinkentity) (edict_t *ent);		// call before removing an interactive edict
	int		(*BoxEdicts) (vec3_t mins, vec3_t maxs, edict_t **list,	int maxcount, int areatype);

	// network messaging
	void	(*multicast) (vec3_t origin, multicast_t to);
	void	(*unicast) (edict_t *ent, qboolean reliable);
	void	(*WriteChar) (int c);
	void	(*WriteByte) (int c);
	void	(*WriteShort) (int c);
	void	(*WriteLong) (int c);
	void	(*WriteFloat) (float f);
	void	(*WriteString) (const char *s);
	void	(*WritePosition) (vec3_t pos);	// some fractional bits
	void	(*WriteDir) (vec3_t pos);		// single byte encoded, very coarse
	void	(*WriteAngle) (float f);

	// managed memory allocation
	void	*(*TagMalloc) (int size, int tag);
	void	(*TagFree) (void *block);
	void	(*FreeTags) (int tag);

	// console variable interaction
	cvar_t	*(*cvar) (const char *var_name, const char *value, uint32 flags);
	void	(*cvar_set) (const char *var_name, const char *value);
	void	(*cvar_forceset) (cvar_t *var, const char *value);

	// ClientCommand and ServerCommand parameter access
	int		(*argc) (void);
	char	*(*argv) (int n);
	char	*(*args) (void);	// concatenation of all argv >= 1

	// add commands to the server console as if they were typed in
	// for map changing, etc
	void	(*AddCommandString) (const char *text);

	void	(*DebugGraph) (float value, uint32 color);

	IFileSystem *		fileSystem;
	IPhysicsSystem *	physSystem;
	IPhysicsScene *		physScene;
};

//
// functions exported by the game subsystem
//
struct game_export_t
{
	int			apiversion;

	// the init function will only be called when a game starts,
	// not each time a level is loaded.  Persistant data for clients
	// and the server can be allocated in init
	void		(*Init) (void);
	void		(*Shutdown) (void);

	// each new level entered will cause a call to SpawnEntities
	void		(*SpawnEntities) (const char *mapname, char *entstring, const char *spawnpoint);

	// Read/Write Game is for storing persistant cross level information
	// about the world state and the clients.
	// WriteGame is called every time a level is exited.
	// ReadGame is called on a loadgame.
	void		(*WriteGame) (char *filename, qboolean autosave);
	void		(*ReadGame) (char *filename);

	// ReadLevel is called after the default map information has been
	// loaded with SpawnEntities
	void		(*WriteLevel) (char *filename);
	void		(*ReadLevel) (char *filename);

	qboolean	(*ClientConnect) (edict_t *ent, char *userinfo);
	void		(*ClientBegin) (edict_t *ent);
	void		(*ClientUserinfoChanged) (edict_t *ent, char *userinfo);
	void		(*ClientDisconnect) (edict_t *ent);
	void		(*ClientCommand) (edict_t *ent);
	void		(*ClientThink) (edict_t *ent, usercmd_t *cmd);

	void		(*RunFrame) (void);

	// ServerCommand will be called when an "sv <command>" command is issued on the
	// server console.
	// The game can issue gi.argc() / gi.argv() commands to get the rest
	// of the parameters
	void		(*ServerCommand) (void);

	// The engine's physics system detected a collision between these two entities
	void		(*Phys_Impact) (edict_t *ent1, edict_t *ent2, cplane_t *plane, csurface_t *surf);

	//
	// global variables shared between game and server
	//

	// The edict array is allocated in the game dll so it
	// can vary in size from one game to another.
	// 
	// The size will be fixed when ge->Init() is called
	edict_t *	edicts;
	int			edict_size;
	int			num_edicts;		// current number, <= max_edicts
	int			max_edicts;
};
