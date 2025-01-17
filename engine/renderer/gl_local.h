
#pragma once

// we are part of the client, but we opt to recieve input from it via refdef_t instead
#include "../shared/engine.h"

#include "ref_types.h"
#include "ref_public.h"

#ifndef _WIN32
#include "../../thirdparty/DirectXMath/Inc/DirectXMath.h"
#else
#include <DirectXMath.h>
#endif
static_assert( DIRECTX_MATH_VERSION >= 317 );

#ifdef _WIN32
// only need system headers on win
#include "../../core/sys_includes.h"
#endif

#include "GL/glew.h"
#ifdef _WIN32
#include "GL/wglew.h"
#endif

#include "../../thirdparty/tracy/Tracy.hpp"

#define Q_MAX_TEXTURE_UNITS		16

#define DEFAULT_CLEARCOLOR		0.25f, 0.25f, 0.25f, 1.0f

#define MAT_EXT ".mat"

// Uncomment to disable codelines that touch deprecated functions
//#define GL_USE_CORE_PROFILE

//#define NO_JOLT_DEBUG

//=============================================================================

#include "gl_model.h"
#include "gl_jaffamodel.h"
#include "gl_iqm.h"

#define BACKFACE_EPSILON	0.01f

/*
===============================================================================

	gl_misc.cpp

===============================================================================
*/

// Screenshots

void GL_Screenshot_PNG_f();
void GL_Screenshot_TGA_f();

// Console commands

void R_ExtractWad_f();
void R_UpgradeWals_f();

// OpenGL state helpers

void		GL_ActiveTexture( GLenum texture );
void		GL_BindTexture( GLuint texnum );
void		GL_UseProgram( GLuint program );
void		GL_TexEnv( GLint value );

void		GL_CheckErrors();

/*
===============================================================================

	gl_init.cpp

===============================================================================
*/

// GLEW covers all our bases so this is empty right now
struct glConfig_t
{
	int unused;
};

struct glState_t
{
	bool	fullscreen;

	int     prev_mode;

	GLuint	currentTextures[Q_MAX_TEXTURE_UNITS];
	GLenum	activeTexture;
	GLuint	currentProgram;
};

struct perfCounters_t
{
	uint32		worldPolys;
	uint32		worldDrawCalls;
	uint32		aliasPolys;

	void Reset()
	{
		worldPolys = 0;
		worldDrawCalls = 0;
		aliasPolys = 0;
	}
};

struct renderSystemGlobals_t
{
	refdef_t		refdef;
	perfCounters_t	pc;
	int				registrationSequence;
	int				visCount;				// bumped when going to a new PVS
	int				frameCount;				// used for dlight push checking

	entity_t *pViewmodelEntity;				// so we can render this particular entity last after everything else, but before the UI

	// Matrices
	DirectX::XMFLOAT4X4A projMatrix;		// the projection matrix
	DirectX::XMFLOAT4X4A viewMatrix;		// the view matrix
	// Angle vectors
	vec3_t vForward, vRight, vUp;

	GLuint debugMeshVAO;
	GLuint debugMeshVBO;
};

extern glState_t				glState;
extern glConfig_t				glConfig;
extern renderSystemGlobals_t	tr;			// named "tr" to ease porting from other branches

extern	model_t *r_worldmodel;

//
// cvars
//
extern cvar_t *r_norefresh;
extern cvar_t *r_drawentities;
extern cvar_t *r_drawworld;
extern cvar_t *r_drawlights;
extern cvar_t *r_speeds;
extern cvar_t *r_fullbright;
extern cvar_t *r_novis;
extern cvar_t *r_nocull;
extern cvar_t *r_lerpmodels;
extern cvar_t *r_lefthand;

// FIXME: This is a HACK to get the client's light level
extern cvar_t *r_lightlevel;

extern cvar_t *r_vertex_arrays;

extern cvar_t *r_ext_multitexture;
extern cvar_t *r_ext_compiled_vertex_array;

extern cvar_t *r_lightmap;
extern cvar_t *r_shadows;
extern cvar_t *r_mode;
extern cvar_t *r_dynamic;
extern cvar_t *r_modulate;
extern cvar_t *r_picmip;
extern cvar_t *r_showtris;
extern cvar_t *r_wireframe;
extern cvar_t *r_finish;
extern cvar_t *r_clear;
extern cvar_t *r_cullfaces;
extern cvar_t *r_polyblend;
extern cvar_t *r_flashblend;
extern cvar_t *r_overbright;
extern cvar_t *r_swapinterval;
extern cvar_t *r_lockpvs;
extern cvar_t *r_nodebug;

extern cvar_t *r_fullscreen;
extern cvar_t *r_gamma;
extern cvar_t *r_multisamples;

extern cvar_t *r_basemaps;
extern cvar_t *r_specmaps;
extern cvar_t *r_normmaps;
extern cvar_t *r_emitmaps;

extern cvar_t *r_viewmodelfov;

/*
===============================================================================

	gl_main.cpp

===============================================================================
*/

extern	entity_t	*currententity;
extern	model_t		*currentmodel;
extern	cplane_t	frustum[4];

//
// screen size info
//
extern	int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

// needed by gl_init
void Particles_Init();
void Particles_Shutdown();

void R_SetLightLevel();

/*
===============================================================================

	gl_mesh.cpp

===============================================================================
*/

#define MAX_LIGHTS 4

struct renderLight_t
{
	vec3_t position;
	vec3_t color;
	float intensity;
};

void R_FourNearestLights( vec3_t origin, renderLight_t *finalLights, vec3_t ambientColor );

/*
===============================================================================

	gl_image.cpp

===============================================================================
*/

struct image_t;

#define MAX_GLTEXTURES		2048
#define MAX_GLMATERIALS		2048

extern material_t		glmaterials[MAX_GLMATERIALS];
extern int				numglmaterials;

extern byte				g_gammatable[256];

extern material_t *		defaultMaterial;
extern material_t *		blackMaterial;
extern material_t *		whiteMaterial;
extern image_t *		flatNormalImage;

extern unsigned			d_8to24table[256];

void		GL_ImageList_f();
void		GL_MaterialList_f();

material_t	*GL_FindMaterial( const char *name, bool managed = false );

void		GL_FreeUnusedMaterials();

void		GL_InitImages();
void		GL_ShutdownImages();

void		R_DestroyAllFBOs();

// Image flags
// Mipmaps are opt-out
// Anisotropic filtering is opt-out
// Clamping is opt-in
//
// NOTABLE OVERSIGHT:
// The first material to use an image decides whether an image uses mipmaps or not
// This may confuse artists who expect a mipmapped texture, when the image was first
// referenced without them
// A solution to this problem is to generate mipmaps when required, but only once
//
using imageFlags_t = uint8;

#define IF_NONE			0	// Gaben sound pack for FLStudio 6.1
#define IF_NOMIPS		1	// Do not use or generate mipmaps (infers no anisotropy)
#define IF_NOANISO		2	// Do not use anisotropic filtering (only applies to mipmapped images)
#define IF_NEAREST		4	// Use nearest filtering (as opposed to linear filtering)
#define IF_CLAMPS		8	// Clamp to edge (S)
#define IF_CLAMPT		16	// Clamp to edge (T)
#define IF_SRGB			32	// Upload as SRGB

struct image_t
{
	char				name[MAX_QPATH];			// game path, including extension
	imageFlags_t		flags;
	int					width, height;				// source image
	GLuint				texnum;						// gl texture binding
	int					refcount;
	float				sl, tl, sh, th;				// 0,0 - 1,1 unless part of the scrap
	bool				scrap;						// true if this is part of a larger sheet

	void IncrementRefCount()
	{
		assert( refcount >= 0 );
		++refcount;
	}

	void DecrementRefCount()
	{
		--refcount;
		assert( refcount >= 0 );
	}

	void Delete()
	{
		glDeleteTextures( 1, &texnum );
		memset( this, 0, sizeof( *this ) );
	}
};

struct material_t
{
	char				name[MAX_QPATH];			// game path, including extension
	image_t *			image;						// the diffuse map
	image_t *			specImage;					// the specular map (defaults to black)
	image_t *			normImage;					// the normal map (defaults to funny blue)
	image_t *			emitImage;					// the emission map (defaults to black)
	material_t *		nextframe;					// the next frame
	uint32				alpha;						// alpha transparency, in range 0 - 255
	int32				registration_sequence;		// 0 = free, -1 = managed

	// Returns true if this material is the missing texture
	bool IsMissing() const { return this == defaultMaterial; }

	// Returns true if the image referenced is the missing image
	bool IsImageMissing() const { return image == defaultMaterial->image; }

	// Returns true if this material is perfectly okay
	bool IsOkay() const { return !IsMissing() && !IsImageMissing(); }

	void Register() {
		if ( IsOkay() ) {
			registration_sequence = tr.registrationSequence;
		}
	}

	// Bind the referenced image
	void Bind() const {
		assert( image->refcount > 0 );
		GL_BindTexture( r_basemaps->GetBool() ? image->texnum : whiteMaterial->image->texnum );
	}

	// Bind the spec image
	void BindSpec() const {
		assert( specImage->refcount > 0 );
		GL_BindTexture( r_specmaps->GetBool() ? specImage->texnum : blackMaterial->image->texnum );
	}

	// Bind the norm image
	void BindNorm() const {
		assert( normImage->refcount > 0 );
		GL_BindTexture( r_normmaps->GetBool() ? normImage->texnum : flatNormalImage->texnum );
	}

	// Bind the emission image
	void BindEmit() const {
		assert( emitImage->refcount > 0 );
		GL_BindTexture( r_emitmaps->GetBool() ? emitImage->texnum : blackMaterial->image->texnum );
	}

	// Deference the referenced image and clear this struct
	void Delete() {
		image->DecrementRefCount();
		if ( image->refcount == 0 ) {
			// Save time in FreeUnusedImages
			image->Delete();
		}
		specImage->DecrementRefCount();
		if ( specImage->refcount == 0 ) {
			specImage->Delete();
		}
		normImage->DecrementRefCount();
		if ( normImage->refcount == 0 ) {
			normImage->Delete();
		}
		emitImage->DecrementRefCount();
		if ( emitImage->refcount == 0 ) {
			emitImage->Delete();
		}
		memset( this, 0, sizeof( *this ) );
	}
};

/*
===============================================================================

	gl_light.cpp

===============================================================================
*/

void	R_MarkLights( dlight_t *light, int bit, mnode_t *node );
void	R_PushDlights();
void	R_LightPoint( const vec3_t p, vec3_t color );

/*
===============================================================================

	gl_surf.cpp

===============================================================================
*/

// Must be contiguous to send to OpenGL
struct worldVertex_t
{
	vec3_t	pos;
	vec2_t	st1;		// Normal UVs
	vec2_t	st2;		// Lightmap UVs
	vec3_t	normal;
};

using worldIndex_t = uint32; // Should really be toggled between based on the vertex count of the map...

// A surface batch
struct worldMesh_t
{
	mtexinfo_t *texinfo;
	uint32			firstIndex;		// Index into s_worldLists.finalIndices
	uint32			numIndices;
	//uint32		numVertices;	// For meshoptimizer
};

// This is the data sent to OpenGL
// We use a single, large vertex / index buffer
// for all world geometry
// Surfaces store indices into the index buffer
struct worldRenderData_t
{
	std::vector<worldVertex_t>	vertices;
	std::vector<worldIndex_t>	indices;

	material_t *lastMaterial;		// This is used when building the vector below
	std::vector<worldMesh_t>	meshes;

	GLuint vao, vbo, ebo, eboSubmodels;
	GLuint lightmapTexnum;

	bool initialised;
};

extern worldRenderData_t g_worldData;

void	R_DrawBrushModel( entity_t *e );
void	R_DrawWorld();
void	R_DrawAlphaSurfaces();
void	R_RotateForEntity( entity_t *e );

bool	BspExt_Load( model_t *worldModel );
void	BspExt_Save( const model_t *worldModel, uint32 flags );

/*
===============================================================================

	gl_mesh.cpp

===============================================================================
*/

void	R_DrawAliasModel( entity_t *e );
void	R_DrawStaticMeshFile( entity_t *e );

/*
===============================================================================

	gl_draw.cpp

===============================================================================
*/

void	Draw_Init();
void	Draw_Shutdown();

void	R_DrawScreenOverlay( const vec4_t color );

void	Draw_RenderBatches();

/*
===============================================================================

	gl_warp.cpp

===============================================================================
*/

void	GL_SubdivideSurface( msurface_t *fa );

void	EmitWaterPolys( msurface_t *fa );
void	R_AddSkySurface( const msurface_t *fa );
void	R_ClearSkyBox();
void	R_DrawSkyBox();

void	Sky_Init();
void	Sky_Shutdown();

/*
===============================================================================

	gl_shader.cpp

===============================================================================
*/

struct glProgs_t
{
	GLuint guiProg;
	GLuint lineProg;
	GLuint particleProg;
	GLuint aliasProg;
	GLuint iqmProg;
	GLuint smfMeshProg;
	GLuint worldProg;
	GLuint skyProg;
	GLuint debugMeshProg;
	GLuint joltProg;
};

extern glProgs_t glProgs;

void	Shaders_Init();
void	Shaders_Shutdown();

/*
===============================================================================

	gl_joltrender.cpp

===============================================================================
*/

#ifndef NO_JOLT_DEBUG

void	R_JoltInitRenderer();
void	R_JoltShutdownRenderer();
void	R_JoltDrawBodies();

#else

#define R_JoltInitRenderer()
#define R_JoltShutdownRenderer()
#define R_JoltDrawBodies()

#endif

/*
===============================================================================

	Implementation specific functions

	glimp_win.cpp

===============================================================================
*/

void	GLimp_BeginFrame();
void	GLimp_EndFrame();

void *	GLimp_SetupContext( void *localContext, bool shareWithMain );

bool 	GLimp_Init();
void	GLimp_Shutdown();

bool    GLimp_SetMode( int mode, bool fullscreen );

void	GLimp_AppActivate( bool active );

/*
===============================================================================

	Functions needed from the main engine

===============================================================================
*/

// Client functions
bool	Sys_GetVidModeInfo( int &width, int &height, int mode );
void	VID_NewWindow( int width, int height );
