// gl_main.c

#include "gl_local.h"

#include <vector>

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "../client/q_imgui_imp.h"

model_t		*r_worldmodel;

entity_t	*currententity;
model_t		*currentmodel;

cplane_t	frustum[4];

int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

// end vars

/*
========================
R_RotateForEntity

Transform the model matrix for a given entity
========================
*/
void R_RotateForEntity( entity_t *e )
{
	glTranslatef( e->origin[0], e->origin[1], e->origin[2] );

	glRotatef( e->angles[1], 0, 0, 1 );
	glRotatef( -e->angles[0], 0, 1, 0 );
	glRotatef( -e->angles[2], 1, 0, 0 );
}

//=================================================================================================

/*
===================================================================================================

	Model rendering

===================================================================================================
*/

/*
========================
R_DrawSpriteModel
========================
*/
static void R_DrawSpriteModel( entity_t *e )
{
	float alpha = 1.0f;
	vec3_t point;
	dsprframe_t *frame;
	float *up, *right;
	dsprite_t *psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache

	psprite = (dsprite_t *)currentmodel->extradata;

#if 0
	if ( e->frame < 0 || e->frame >= psprite->numframes )
	{
		Com_Printf( "no such sprite frame %i\n", e->frame );
		e->frame = 0;
	}
#endif
	e->frame %= psprite->numframes;

	frame = &psprite->frames[e->frame];

#if 0
	if ( psprite->type == SPR_ORIENTED )
	{	// bullet marks on walls
		vec3_t		v_forward, v_right, v_up;

		AngleVectors( currententity->angles, v_forward, v_right, v_up );
		up = v_up;
		right = v_right;
	}
	else
#endif
	{	// normal sprite
		up = tr.vUp;
		right = tr.vRight;
	}

	if ( e->flags & RF_TRANSLUCENT )
		alpha = e->alpha;

	if ( alpha != 1.0f )
		glEnable( GL_BLEND );

	glColor4f( 1, 1, 1, alpha );

	GL_ActiveTexture( GL_TEXTURE0 );

	currentmodel->skins[e->frame]->Bind();

	GL_TexEnv( GL_MODULATE );

	if ( alpha == 1.0f )
		glEnable( GL_ALPHA_TEST );
	else
		glDisable( GL_ALPHA_TEST );

	glBegin( GL_QUADS );

	glTexCoord2f( 0, 1 );
	VectorMA( e->origin, -frame->origin_y, up, point );
	VectorMA( point, -frame->origin_x, right, point );
	glVertex3fv( point );

	glTexCoord2f( 0, 0 );
	VectorMA( e->origin, frame->height - frame->origin_y, up, point );
	VectorMA( point, -frame->origin_x, right, point );
	glVertex3fv( point );

	glTexCoord2f( 1, 0 );
	VectorMA( e->origin, frame->height - frame->origin_y, up, point );
	VectorMA( point, frame->width - frame->origin_x, right, point );
	glVertex3fv( point );

	glTexCoord2f( 1, 1 );
	VectorMA( e->origin, -frame->origin_y, up, point );
	VectorMA( point, frame->width - frame->origin_x, right, point );
	glVertex3fv( point );

	glEnd();

	glDisable( GL_ALPHA_TEST );
	GL_TexEnv( GL_REPLACE );

	if ( alpha != 1.0f )
		glDisable( GL_BLEND );

	glColor4f( 1, 1, 1, 1 );
}

/*
========================
R_DrawBeam
========================
*/
#define NUM_BEAM_SEGS 6

void R_DrawBeam( entity_t *e )
{
	int	i;
	float r, g, b;

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );
	VectorScale( perpvec, e->frame / 2, perpvec );

	for ( i = 0; i < 6; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0f/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	glDisable( GL_TEXTURE_2D );
	glEnable( GL_BLEND );
	glDepthMask( GL_FALSE );

	r = ( d_8to24table[e->skinnum & 0xFF] ) & 0xFF;
	g = ( d_8to24table[e->skinnum & 0xFF] >> 8 ) & 0xFF;
	b = ( d_8to24table[e->skinnum & 0xFF] >> 16 ) & 0xFF;

	r *= 1/255.0F;
	g *= 1/255.0F;
	b *= 1/255.0F;

	glColor4f( r, g, b, e->alpha );

	glBegin( GL_TRIANGLE_STRIP );
	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		glVertex3fv( start_points[i] );
		glVertex3fv( end_points[i] );
		glVertex3fv( start_points[(i+1)%NUM_BEAM_SEGS] );
		glVertex3fv( end_points[(i+1)%NUM_BEAM_SEGS] );
	}
	glEnd();

	glEnable( GL_TEXTURE_2D );
	glDisable( GL_BLEND );
	glDepthMask( GL_TRUE );
}

/*
========================
R_DrawThingy

Draws the thingy
========================
*/
static void R_DrawThingy( const vec3_t origin, const vec3_t angles, uint32 color )
{
	using namespace DirectX;

	XMMATRIX modelMatrix = XMMatrixMultiply(
		XMMatrixRotationX( DEG2RAD( angles[ROLL] ) ) * XMMatrixRotationY( DEG2RAD( angles[PITCH] ) ) * XMMatrixRotationZ( DEG2RAD( angles[YAW] ) ),
		//XMMatrixRotationRollPitchYaw( 0.0f, 0.0f, 0.0f ),
		XMMatrixTranslation( origin[0], origin[1], origin[2] )
	);

	XMFLOAT4X4 modelMatrixStore;
	XMStoreFloat4x4( &modelMatrixStore, modelMatrix );

	GL_UseProgram( glProgs.debugMeshProg );

	glUniformMatrix4fv( 1, 1, GL_FALSE, (const GLfloat *)&modelMatrixStore );
	glUniformMatrix4fv( 2, 1, GL_FALSE, (const GLfloat *)&tr.viewMatrix );
	glUniformMatrix4fv( 3, 1, GL_FALSE, (const GLfloat *)&tr.projMatrix );

	glBindVertexArray( tr.debugMeshVAO );
	glBindBuffer( GL_ARRAY_BUFFER, tr.debugMeshVBO );

	//const uint32 color = colors::white;

	//glUniform4uiv( 4, 1, GL_TRUE, &color );

	glDrawArrays( GL_TRIANGLE_FAN, 0, 11 );

	GL_UseProgram( 0 );
}

/*
========================
R_DrawNullModel

Draws a little thingy in place of a model, cool!
========================
*/
static void R_DrawNullModel( entity_t *e )
{
	R_DrawThingy( e->origin, e->angles, colors::white );
}

/*
========================
R_DrawLights

Draws all lights in the staticlights structure
========================
*/
static void R_DrawLights()
{
	if ( !r_drawlights->GetBool() ) {
		return;
	}

	vec3_t noAngle{};

	for ( int i = 0; i < mod_numStaticLights; ++i )
	{
		staticLight_t &light = mod_staticLights[i];

		R_DrawThingy( light.origin, noAngle, colors::white );
	}
}

/*
========================
R_DrawEntity

This actually performs rendering
========================
*/
static void R_DrawEntity( entity_t *e )
{
	currententity = e;

	if ( e->flags & RF_BEAM )
	{
		R_DrawBeam( e );
	}
	else
	{
		currentmodel = e->model;

		if ( !e->model )
		{
			R_DrawNullModel( e );
			return;
		}
		switch ( e->model->type )
		{
		case mod_smf:
			R_DrawStaticMeshFile( e );
			break;
		case mod_alias:
			R_DrawAliasModel( e );
			break;
		case mod_iqm:
			R_DrawIQM( e );
			break;
		case mod_brush:
			R_DrawBrushModel( e );
			break;
		case mod_sprite:
#ifndef GL_USE_CORE_PROFILE
			R_DrawSpriteModel( e );
#endif
			break;
		default:
			Com_Error( "Bad modeltype" );
		}
	}
}

/*
========================
R_DrawEntities
========================
*/
static void R_DrawEntities()
{
	ZoneScoped

	if ( !r_drawentities->GetBool() ) {
		return;
	}

	// draw non-transparent first

	for ( int i = 0; i < tr.refdef.num_entities; ++i )
	{
		if ( tr.refdef.entities[i].flags & RF_TRANSLUCENT ) {
			continue;
		}

		if ( tr.refdef.entities[i].flags & RF_DEPTHHACK ) {
			tr.pViewmodelEntity = tr.refdef.entities + i;
			continue;
		}

		R_DrawEntity( tr.refdef.entities + i );
	}

	// draw transparent entities
	// we could sort these if it ever becomes a problem...

	glDepthMask( GL_FALSE );	// no z writes

	for ( int i = 0; i < tr.refdef.num_entities; ++i )
	{
		if ( !( tr.refdef.entities[i].flags & RF_TRANSLUCENT ) ) {
			continue;
		}

		if ( tr.refdef.entities[i].flags & RF_DEPTHHACK ) {
			tr.pViewmodelEntity = tr.refdef.entities + i;
			continue;
		}

		R_DrawEntity( tr.refdef.entities + i );
	}

	glDepthMask( GL_TRUE );		// back to writing
}

/*
========================
R_DrawViewmodel

Draws the viewmodel
========================
*/
static void R_DrawViewmodel()
{
	if ( !tr.pViewmodelEntity ) {
		return;
	}

	R_DrawEntity( tr.pViewmodelEntity );
	tr.pViewmodelEntity = nullptr;
}

/*
===================================================================================================

	Particle rendering

	Particles aren't models, so they get drawn in a huge clump after the world and entities.
	Eventually we want these to become textured, and be read from a script or something
	because right now they're kinda boring

===================================================================================================
*/

// 16 bytes
struct partPoint_t
{
	float x, y, z;		// 12 bytes
	byte r, g, b, a;	// 4 bytes
};

static GLuint s_partVAO;
static GLuint s_partVBO;

static std::vector<partPoint_t> s_partVector;

/*
========================
Particles_Init
========================
*/
void Particles_Init()
{
	glGenVertexArrays( 1, &s_partVAO );
	glGenBuffers( 1, &s_partVBO );

	glBindVertexArray( s_partVAO );
	glBindBuffer( GL_ARRAY_BUFFER, s_partVBO );

	glEnableVertexAttribArray( 0 );
	glEnableVertexAttribArray( 1 );

	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof( partPoint_t ), (void *)( 0 ) );
	glVertexAttribPointer( 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof( partPoint_t ), (void *)( 3 * sizeof( GLfloat ) ) );
}

/*
========================
Particles_Shutdown
========================
*/
void Particles_Shutdown()
{
	glDeleteBuffers( 1, &s_partVBO );
	glDeleteVertexArrays( 1, &s_partVAO );
}

/*
========================
R_DrawParticles
========================
*/
static void R_DrawParticles()
{
	ZoneScoped

	if ( tr.refdef.num_particles <= 0 ) {
		return;
	}

	int i;
	particle_t *p;

	// ensure we have enough room
	s_partVector.resize( tr.refdef.num_particles );

	for ( i = 0, p = tr.refdef.particles; i < tr.refdef.num_particles; i++, p++ )
	{
		partPoint_t &point = s_partVector[i];

		point.x = p->origin[0];
		point.y = p->origin[1];
		point.z = p->origin[2];

		point.r = ((byte *)&d_8to24table[p->color])[0];
		point.g = ((byte *)&d_8to24table[p->color])[1];
		point.b = ((byte *)&d_8to24table[p->color])[2];
		point.a = static_cast<byte>( p->alpha * 255.0f );
	}

	GL_UseProgram( glProgs.particleProg );
	glUniformMatrix4fv( 2, 1, GL_FALSE, (const GLfloat *)&tr.projMatrix );
	glUniformMatrix4fv( 3, 1, GL_FALSE, (const GLfloat *)&tr.viewMatrix );

	glBindVertexArray( s_partVAO );
	glBindBuffer( GL_ARRAY_BUFFER, s_partVBO );

	glBufferData( GL_ARRAY_BUFFER, tr.refdef.num_particles * sizeof( partPoint_t ), (void *)s_partVector.data(), GL_STREAM_DRAW );

	s_partVector.clear();

	glDepthMask( GL_FALSE );
	glEnable( GL_BLEND );

	glDrawArrays( GL_POINTS, 0, tr.refdef.num_particles );

	glDisable( GL_BLEND );
	glDepthMask( GL_TRUE );
}

//=================================================================================================

static StaticCvar r_lockfrustum( "r_lockfrustum", "0", 0 );

/*
========================
R_SetFrustum
========================
*/
static void R_SetFrustum()
{
	if ( r_lockfrustum.GetBool() )
	{
		return;
	}

#if 0
	/*
	** this code is wrong, since it presume a 90 degree FOV both in the
	** horizontal and vertical plane
	*/
	// front side is visible
	VectorAdd( vpn, vright, frustum[0].normal );
	VectorSubtract( vpn, vright, frustum[1].normal );
	VectorAdd( vpn, vup, frustum[2].normal );
	VectorSubtract( vpn, vup, frustum[3].normal );

	// we theoretically don't need to normalize these vectors, but I do it
	// anyway so that debugging is a little easier
	VectorNormalize( frustum[0].normal );
	VectorNormalize( frustum[1].normal );
	VectorNormalize( frustum[2].normal );
	VectorNormalize( frustum[3].normal );
#else
	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, tr.vUp, tr.vForward, -( 90 - tr.refdef.fov_x / 2 ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, tr.vUp, tr.vForward, 90 - tr.refdef.fov_x / 2 );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, tr.vRight, tr.vForward, 90 - tr.refdef.fov_y / 2 );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, tr.vRight, tr.vForward, -( 90 - tr.refdef.fov_y / 2 ) );
#endif

	for ( uint i = 0; i < 4; ++i )
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct( tr.refdef.vieworg, frustum[i].normal );
		frustum[i].signbits = SignbitsForPlane( frustum[i] );
	}
}

/*
========================
R_SetupFrame
========================
*/
static void R_SetupFrame()
{
	ZoneScoped

	mleaf_t *leaf;

	// Increment our frame counter
	++tr.frameCount;

	// Clear counters
	tr.pc.Reset();

	// Build the transformation matrix for the given view angles
	AngleVectors( tr.refdef.viewangles, tr.vForward, tr.vRight, tr.vUp );

	// current viewcluster
	if ( !( tr.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf( tr.refdef.vieworg, r_worldmodel );
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if ( !leaf->contents )
		{
			// look down a bit
			vec3_t	temp;

			VectorCopy( tr.refdef.vieworg, temp );
			temp[2] -= 16;
			leaf = Mod_PointInLeaf( temp, r_worldmodel );
			if ( !( leaf->contents & CONTENTS_SOLID ) && ( leaf->cluster != r_viewcluster2 ) ) {
				r_viewcluster2 = leaf->cluster;
			}
		}
		else
		{
			// look up a bit
			vec3_t	temp;

			VectorCopy( tr.refdef.vieworg, temp );
			temp[2] += 16;
			leaf = Mod_PointInLeaf( temp, r_worldmodel );
			if ( !( leaf->contents & CONTENTS_SOLID ) && ( leaf->cluster != r_viewcluster2 ) ) {
				r_viewcluster2 = leaf->cluster;
			}
		}
	}
}

/*
========================
R_SetupGL

GET RID OF ME!!!
========================
*/
static void R_SetupGL()
{
	ZoneScoped

	using namespace DirectX;

	// projection matrix

	XMMATRIX workMatrix = XMMatrixPerspectiveFovRH( DEG2RAD( tr.refdef.fov_y ), (float)tr.refdef.width / (float)tr.refdef.height, 4.0f, 4096.0f );
	XMStoreFloat4x4A( &tr.projMatrix, workMatrix );

	// view matrix

	vec3_t &viewOrg = tr.refdef.vieworg;
	vec3_t &viewAng = tr.refdef.viewangles;

	vec3_t forward;

	AngleVectors( viewAng, forward, nullptr, nullptr );

	XMVECTOR eyePosition = XMVectorSet( viewOrg[0], viewOrg[1], viewOrg[2], 0.0f );
	XMVECTOR focusPoint = XMVectorSet( viewOrg[0] + forward[0], viewOrg[1] + forward[1], viewOrg[2] + forward[2], 0.0f );
	XMVECTOR upAxis = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );

	workMatrix = XMMatrixLookAtRH( eyePosition, focusPoint, upAxis );
	XMStoreFloat4x4A( &tr.viewMatrix, workMatrix );

#ifndef GL_USE_CORE_PROFILE
	// send them to the fixed function pipeline
	glMatrixMode( GL_PROJECTION );
	glLoadMatrixf( (const GLfloat *)&tr.projMatrix );
	glMatrixMode( GL_MODELVIEW );
	glLoadMatrixf( (const GLfloat *)&tr.viewMatrix );
#endif

	// set drawing parms

	if ( r_cullfaces->IsModified() )
	{
		r_cullfaces->ClearModified();
		if ( r_cullfaces->GetBool() ) {
			glEnable( GL_CULL_FACE );
		} else {
			glDisable( GL_CULL_FACE );
		}
	}

	// is modified really necessary here?
	if ( r_wireframe->IsModified() ) {
		r_wireframe->ClearModified();
		glPolygonMode( GL_FRONT_AND_BACK, r_wireframe->GetBool() ? GL_LINE : GL_FILL );
	}

	glEnable( GL_DEPTH_TEST );
}

/*
========================
R_Clear

Performs a screen clear
========================
*/
static void R_Clear()
{
	if ( r_clear->GetBool() ) {
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	} else {
		glClear( GL_DEPTH_BUFFER_BIT );
	}
}

/*
========================
R_RenderView

tr.refdef must be set before the first call
========================
*/
static void R_RenderView( refdef_t *fd )
{
	ZoneScoped

	if ( r_norefresh->GetBool() ) {
		return;
	}

	tr.refdef = *fd;

	if ( !r_worldmodel && !( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		Com_Error( "R_RenderView: NULL worldmodel" );
	}

	// Remove me!
	R_PushDlights();

	if ( r_finish->GetBool() ) {
		// Block until we're done with the last frame? Weird
		//glFinish();
	}

	R_SetupFrame();

	R_SetFrustum();

	// reset state
	R_SetupGL();

	// world
	R_DrawWorld();

	// entities
	R_DrawEntities();

	// lights
	R_DrawLights();

	// particles!
	R_DrawParticles();

	// world alpha surfaces
	R_DrawAlphaSurfaces();

	// jolt debug
	R_JoltDrawBodies();

	// all operations from here on out do not need the existing depth buffer
	// so nuke it, just the viewmodel for now
	glClear( GL_DEPTH_BUFFER_BIT );

	// the viewmodel
	R_DrawViewmodel();

	// screen overlay
	//R_DrawScreenOverlay( tr.refdef.blend );

	if ( r_speeds->GetBool() )
	{
		Com_Printf(
			"%4i wpoly %4i epoly\n"
			"%4i world draw calls\n",
			tr.pc.worldPolys, tr.pc.aliasPolys,
			tr.pc.worldDrawCalls
		);
	}
}

/*
========================
R_SetLightLevel
========================
*/
void R_SetLightLevel()
{
	vec3_t shadelight;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}

	// save off light value for server to look at (BIG HACK!)

	R_LightPoint( tr.refdef.vieworg, shadelight );

	float value;

	// pick the greatest component, which should be the same
	// as the mono value returned by software
	if ( shadelight[0] > shadelight[1] )
	{
		if ( shadelight[0] > shadelight[2] )
			value = 150.0f * shadelight[0];
		else
			value = 150.0f * shadelight[2];
	}
	else
	{
		if ( shadelight[1] > shadelight[2] )
			value = 150.0f * shadelight[1];
		else
			value = 150.0f * shadelight[2];
	}

	Cvar_SetFloat( r_lightlevel, value );
}

/*
========================
R_SetMode
========================
*/
static void R_SetMode()
{
	if ( r_mode->IsModified() || r_fullscreen->IsModified() )
	{
		r_mode->ClearModified();
		r_fullscreen->ClearModified();

		if ( GLimp_SetMode( r_mode->GetInt(), r_fullscreen->GetBool() ) == true )
		{
			glState.prev_mode = r_mode->GetInt();
		}
		else
		{
			Cvar_SetInt( r_mode, glState.prev_mode );
			r_mode->ClearModified();
			Com_Printf( "R_SetMode: invalid mode\n" );

			// try setting it back to something safe
			if ( GLimp_SetMode( glState.prev_mode, false ) == false )
			{
				Com_FatalError( "R_SetMode: could not revert to safe mode\n" );
			}
		}
	}
}

/*
========================
R_BeginFrame

Public
========================
*/
void R_BeginFrame( bool imgui, int frameBuffer )
{
	GLimp_BeginFrame();

	// check if we need to set modes
	R_SetMode();

	// set framebuffer
	if ( frameBuffer != 0 ) {
		R_BindFBO( frameBuffer );
	}

	// tell imgui we're starting a new frame
	if ( imgui )
	{
		ImGui_ImplOpenGL3_NewFrame();
		qImGui::OSImp_NewFrame();
		ImGui::NewFrame();
	}

	R_Clear();
}

/*
========================
R_RenderFrame

Public
========================
*/
void R_RenderFrame( refdef_t *fd )
{
	R_RenderView( fd );
	R_SetLightLevel();
}

/*
========================
R_EndFrame

Public
========================
*/
void R_EndFrame( bool imgui )
{
	// draw our UI
	Draw_RenderBatches();

	// draw imgui, if applicable
	if ( imgui )
	{
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
	}

	GL_CheckErrors();

	GLimp_EndFrame();
}
