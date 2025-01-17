
#include "cl_local.h"

static cvar_t *	v_addParticles;
static cvar_t *	v_addLights;
static cvar_t *	v_addEntities;
static cvar_t *	v_addBlend;

static cvar_t *	v_testParticles;
static cvar_t *	v_testEntities;
static cvar_t *	v_testLights;
static cvar_t *	v_testBlend;

static cvar_t *	v_stats;

// development tools for weapons
int			g_gunFrame;
model_t *	g_gunModel;

char		cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
int			num_cl_weaponmodels;

/*
===================================================================================================

	The scene / view

	This struct represents a scene that is sent to the renderer,
	the renderer sources its own BSP.

	If we ever need more than one view this can be easily stuck into a class.

===================================================================================================
*/

struct clView_t
{
	dlight_t		dlights[MAX_DLIGHTS];
	entity_t		entities[MAX_ENTITIES];
	particle_t		particles[MAX_PARTICLES];
	lightstyle_t	lightstyles[MAX_LIGHTSTYLES];

	int numDLights;
	int numEntities;
	int numParticles;
};

static clView_t clView;

void V_AddDLight( vec3_t org, float intensity, float r, float g, float b )
{
	if ( clView.numDLights >= MAX_DLIGHTS ) {
		return;
	}

	dlight_t &dl = clView.dlights[clView.numDLights++];

	VectorCopy( org, dl.origin );
	dl.color[0] = r;
	dl.color[1] = g;
	dl.color[2] = b;
	dl.intensity = intensity;
}

void V_AddEntity( entity_t *ent )
{
	if ( clView.numEntities >= MAX_ENTITIES ) {
		return;
	}

	clView.entities[clView.numEntities++] = *ent;
}

void V_AddParticle( vec3_t org, int color, float alpha )
{
	if ( clView.numParticles >= MAX_PARTICLES ) {
		return;
	}

	particle_t &p = clView.particles[clView.numParticles++];

	VectorCopy( org, p.origin );
	p.color = color;
	p.alpha = alpha;
}

void V_AddLightStyle( int style, float r, float g, float b )
{
	if ( style < 0 || style >= MAX_LIGHTSTYLES ) {
		Com_Errorf( "Bad light style %i", style );
	}

	lightstyle_t &ls = clView.lightstyles[style];

	ls.rgb[0] = r;
	ls.rgb[1] = g;
	ls.rgb[2] = b;
	ls.white = r + g + b;
}

// create 32 dlights
static void V_TestDLights()
{
	int			i, j;
	float		f, r;
	dlight_t *	dl;

	static_assert( MAX_DLIGHTS >= 32 );

	clView.numDLights = 32;
	memset( clView.dlights, 0, sizeof( dlight_t ) * clView.numDLights );

	for ( i = 0; i < clView.numDLights; i++ )
	{
		dl = &clView.dlights[i];

		r = 64 * ( (i%4) - 1.5f );
		f = 64 * (i/4) + 128;

		for ( j = 0; j < 3; j++)
		{
			dl->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*f +
			cl.v_right[j]*r;
		}

		dl->color[0] = ((i%6)+1) & 1;
		dl->color[1] = (((i%6)+1) & 2)>>1;
		dl->color[2] = (((i%6)+1) & 4)>>2;
		dl->intensity = 200;
	}
}

// create 32 playermodels
static void V_TestEntities()
{
	int			i, j;
	float		f, r;
	entity_t *	ent;

	clView.numEntities = 32;
	memset( clView.entities, 0, sizeof( entity_t ) * clView.numEntities );

	for ( i = 0; i < clView.numEntities; i++ )
	{
		ent = &clView.entities[i];

		r = 64 * ( (i%4) - 1.5f );
		f = 64 * (i/4) + 128;

		for ( j = 0; j < 3; j++)
		{
			ent->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*f +
			cl.v_right[j]*r;
		}

		ent->model = cl.baseclientinfo.model;
		ent->skin = cl.baseclientinfo.skin;
	}
}

// fills the entire particle array
static void V_TestParticles()
{
	particle_t *p;
	int			i, j;
	float		d, r, u;

	clView.numParticles = MAX_PARTICLES;
	for ( i = 0; i < MAX_PARTICLES; i++ )
	{
		d = i*0.25f;
		r = 4*((i&7)-3.5f);
		u = 4*(((i>>3)&7)-3.5f);
		p = &clView.particles[i];

		for ( j = 0; j < 3; j++ )
		{
			p->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*d +
			cl.v_right[j]*r + cl.v_up[j]*u;
		}

		p->color = 8;
		p->alpha = v_testParticles->GetFloat();
	}
}

static void V_ClearView()
{
	// SHADERWORLD - Clear out all used lights, because we naively upload
	// the entire structure (or most of it) for GL shaders
	memset( clView.dlights, 0, clView.numDLights * sizeof( dlight_t ) );
	clView.numDLights = 0;

	clView.numEntities = 0;
	clView.numParticles = 0;
}

//=================================================================================================

/*
========================
CL_PrepRefresh

Call before entering a new level, or after changing dlls
========================
*/
void CL_PrepRefresh()
{
	const char *mapName = cl.configstrings[CS_MODELS + 1];

	if ( !mapName[0] ) {
		// no map loaded
		return;
	}

	// precache map

	Com_Printf( "Precaching map: %s\n", mapName );
	SCR_UpdateScreen();

	R_BeginRegistration( mapName );

	Com_Print( "...\n" );

	// precache scr pics

	Com_Print( "Precaching pics\n" );
	SCR_UpdateScreen();

	SCR_TouchPics();

	Com_Print( "...\n" );

	// precache tent models

	cge->RegisterTEntModels();

	// make this go away

	num_cl_weaponmodels = 1;
	strcpy( cl_weaponmodels[0], "weapon.md2" );

	// precache models

	for ( int i = 1; i < MAX_MODELS && cl.configstrings[CS_MODELS + i][0]; i++ )
	{
		const char *modelName = cl.configstrings[CS_MODELS + i];

		if ( modelName[0] != '*' ) {
			Com_Printf( "%s\n", modelName );
		}

		SCR_UpdateScreen();

		if ( modelName[0] == '#' )
		{
			// special player weapon model
			if ( num_cl_weaponmodels < MAX_CLIENTWEAPONMODELS )
			{
				Q_strcpy_s( cl_weaponmodels[num_cl_weaponmodels], modelName + 1 );
				num_cl_weaponmodels++;
			}
		}
		else
		{
			cl.model_draw[i] = R_RegisterModel( modelName );
			if ( modelName[0] == '*' ) {
				cl.model_clip[i] = CM_InlineModel( modelName );
			} else {
				cl.model_clip[i] = nullptr;
			}
		}
	}

	// precache images

	Com_Print( "Precaching images\n" );
	SCR_UpdateScreen();

	for ( int i = 1; i < MAX_IMAGES && cl.configstrings[CS_IMAGES + i][0]; i++ )
	{
		cl.image_precache[i] = R_RegisterPic (cl.configstrings[CS_IMAGES+i]); // SlartMaterialSystemTodo
	}
	
	Com_Print( "...\n" );

	for ( int i = 0; i < MAX_CLIENTS; i++ )
	{
		if ( !cl.configstrings[CS_PLAYERSKINS + i][0] ) {
			continue;
		}
		Com_Printf( "Precaching client %d info\n", i );
		SCR_UpdateScreen();
		CL_ParseClientinfo( i );
		//Com_Print( "                                     \r" );
	}

	char name[MAX_QPATH];
	strcpy( name, "unnamed/male/grunt" );

	CL_LoadClientinfo( &cl.baseclientinfo, name );

	// set sky textures and speed

	Com_Print( "Precaching sky\n" );
	SCR_UpdateScreen();

	float rotate = Q_atof( cl.configstrings[CS_SKYROTATE] );
	vec3_t axis;
	sscanf( cl.configstrings[CS_SKYAXIS], "%f %f %f", &axis[0], &axis[1], &axis[2] );
	R_SetSky( cl.configstrings[CS_SKY], rotate, axis );

	// registration complete

	R_EndRegistration();

	Com_Print( "Done precaching\n" );

	// clear any lines of console text

	UI::Console::ClearNotify ();

	SCR_UpdateScreen();
	cl.refresh_prepped = true;
	cl.force_refdef = true;			// make sure we have a valid refdef

	// start the cd track
	CDAudio_Play( Q_atoi( cl.configstrings[CS_CDTRACK] ), true );
}

/*
========================
CalcFov
========================
*/
float CalcFov( float fov_x, float width, float height )
{
	float	a;
	float	x;

	fov_x = Clamp( fov_x, 1.0f, 179.0f );

	x = width/tanf(fov_x/360*M_PI_F);

	a = atanf (height/x);

	a = a*360/M_PI_F;

	return a;
}

/*
===================================================================================================
	Gun frame debugging helpers
===================================================================================================
*/

static void V_Gun_Next_f()
{
	++g_gunFrame;

	Com_Printf( "frame %i\n", g_gunFrame );
}

static void V_Gun_Prev_f()
{
	--g_gunFrame;

	if ( g_gunFrame < 0 ) {
		g_gunFrame = 0;
	}

	Com_Printf( "frame %i\n", g_gunFrame );
}

static void V_Gun_Model_f()
{
	if ( Cmd_Argc() != 2 )
	{
		return;
	}

	g_gunModel = R_RegisterModel( Cmd_Argv( 1 ) );
}

static void V_Gun_Reset_f()
{
	g_gunFrame = 0;
	g_gunModel = nullptr;
}

/*
===================================================================================================
	View rendering
===================================================================================================
*/

static int entitycmpfnc( const entity_t *a, const entity_t *b )
{
	// all other models are sorted by model then skin
	if ( a->model == b->model ) {
		return (int)( (byte *)a->skin - (byte *)b->skin );
	} else {
		return (int)( (byte *)a->model - (byte *)b->model );
	}
}

/*
========================
V_RenderView

Renders the 3D scene
========================
*/
void V_RenderView()
{
	if ( cls.state != ca_active ) {
		return;
	}

	if ( !cl.refresh_prepped ) {
		// still loading
		return;
	}

	if ( cl_timedemo->GetBool() ) {
		if ( !cl.timedemo_start ) {
			cl.timedemo_start = Sys_Milliseconds();
		}
		cl.timedemo_frames++;
	}

	// an invalid frame will just use the exact previous refdef
	// we can't use the old frame if the video mode has changed, though...
	if ( cl.frame.valid && ( cl.force_refdef || !cl_paused->GetBool() ) )
	{
		cl.force_refdef = false;

		V_ClearView();

		// build a refresh entity list and calc cl.sim*
		// this also calls CL_CalcViewValues which loads
		// v_forward, etc.
		CL_AddEntities();

		if ( v_testParticles->GetBool() ) {
			V_TestParticles();
		}
		if ( v_testEntities->GetBool() ) {
			V_TestEntities();
		}
		if ( v_testLights->GetBool() ) {
			V_TestDLights();
		}
		if ( v_testBlend->GetBool() )
		{
			cl.refdef.blend[0] = 1.0f;
			cl.refdef.blend[1] = 0.5f;
			cl.refdef.blend[2] = 0.25f;
			cl.refdef.blend[3] = 0.5f;
		}

		// never let it sit exactly on a node line, because a water plane can
		// dissapear when viewed with the eye exactly on it.
		// the server protocol only specifies to 1/8 pixel, so add 1/16 in each axis
		/*
		cl.refdef.vieworg[0] += 1.0f/16;
		cl.refdef.vieworg[1] += 1.0f/16;
		cl.refdef.vieworg[2] += 1.0f/16;
		*/

		cl.refdef.width = g_vidDef.width;
		cl.refdef.height = g_vidDef.height;

		cl.refdef.fov_y = CalcFov( cl.refdef.fov_x, (float)cl.refdef.width, (float)cl.refdef.height );
		cl.refdef.time = MS2SEC( static_cast<float>( cl.time ) ); // SlartTime
		cl.refdef.frametime = cls.frametime; // SlartTime

		cl.refdef.areabits = cl.frame.areabits;

		if ( !v_addEntities->GetBool() ) {
			clView.numEntities = 0;
		}
		if ( !v_addParticles->GetBool() ) {
			clView.numParticles = 0;
		}
		if ( !v_addLights->GetBool() ) {
			clView.numDLights = 0;
		}
		if ( !v_addBlend->GetBool() ) {
			VectorClear( cl.refdef.blend );
		}

		cl.refdef.rdflags = cl.frame.playerstate.rdflags;

		cl.refdef.dlights = clView.dlights;
		cl.refdef.entities = clView.entities;
		cl.refdef.particles = clView.particles;
		cl.refdef.lightstyles = clView.lightstyles;

		cl.refdef.num_dlights = clView.numDLights;
		cl.refdef.num_entities = clView.numEntities;
		cl.refdef.num_particles = clView.numParticles;

		// sort entities for better cache locality
		qsort( clView.entities, clView.numEntities, sizeof( entity_t ), ( int ( * )( const void *, const void * ) )entitycmpfnc );
	}

	R_RenderFrame( &cl.refdef );

	if ( v_stats->GetBool() ) {
		Com_Printf( "ent:%i  lt:%i  part:%i\n", clView.numEntities, clView.numDLights, clView.numParticles );
	}
	if ( com_logStats->GetBool() && ( log_stats_file != nullptr ) ) {
		FileSystem::PrintFileFmt( log_stats_file, "%i,%i,%i,", clView.numEntities, clView.numDLights, clView.numParticles );
	}
}

/*
========================
V_Sky_f

Set a specific sky and rotation speed
========================
*/
static void V_Sky_f()
{
	float	rotate;
	vec3_t	axis;

	if ( Cmd_Argc() < 2 ) {
		Com_Print( "Usage: sky <basename> <rotate> <axis x y z>\n" );
		return;
	}

	if ( Cmd_Argc() > 2 ) {
		rotate = Q_atof( Cmd_Argv( 2 ) );
	} else {
		rotate = 0.0f;
	}

	if ( Cmd_Argc() == 6 ) {
		axis[0] = Q_atof( Cmd_Argv( 3 ) );
		axis[1] = Q_atof( Cmd_Argv( 4 ) );
		axis[2] = Q_atof( Cmd_Argv( 5 ) );
	} else {
		axis[0] = 0.0f;
		axis[1] = 0.0f;
		axis[2] = 1.0f;
	}

	R_SetSky( Cmd_Argv( 1 ), rotate, axis );
}

/*
========================
V_Viewpos_f
========================
*/
static void V_Viewpos_f()
{
	Com_Printf(
		"(%i %i %i) : %i\n",
		(int)cl.refdef.vieworg[0],
		(int)cl.refdef.vieworg[1],
		(int)cl.refdef.vieworg[2],
		(int)cl.refdef.viewangles[YAW]
	);
}

/*
========================
V_Init
========================
*/
void V_Init()
{
	v_addBlend = Cvar_Get( "v_addBlend", "1", 0 );
	v_addLights = Cvar_Get( "v_addLights", "1", 0 );
	v_addParticles = Cvar_Get( "v_addParticles", "1", 0 );
	v_addEntities = Cvar_Get( "v_addEntities", "1", 0 );

	v_testBlend = Cvar_Get( "v_testBlend", "0", 0 );
	v_testParticles = Cvar_Get( "v_testParticles", "0", 0 );
	v_testEntities = Cvar_Get( "v_testEntities", "0", 0 );
	v_testLights = Cvar_Get( "v_testLights", "0", 0 );

	v_stats = Cvar_Get( "v_stats", "0", 0 );

	Cmd_AddCommand( "v_gun_next", V_Gun_Next_f );
	Cmd_AddCommand( "v_gun_prev", V_Gun_Prev_f );
	Cmd_AddCommand( "v_gun_model", V_Gun_Model_f );
	Cmd_AddCommand( "v_gun_reset", V_Gun_Model_f );

	Cmd_AddCommand( "v_sky", V_Sky_f );
	Cmd_AddCommand( "v_viewpos", V_Viewpos_f );
}
