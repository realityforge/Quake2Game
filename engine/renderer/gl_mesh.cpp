/*
===================================================================================================

	Model Rendering

===================================================================================================
*/

#include "gl_local.h"

#include <vector>

/*
===================================================================================================

	Generic Operations

===================================================================================================
*/

static trace_t R_LightTrace( vec3_t start, vec3_t end )
{
	vec3_t mins{}, maxs{};

	return CM_BoxTrace( start, end, mins, maxs, 0, MASK_OPAQUE );
}

void R_FourNearestLights( vec3_t origin, renderLight_t *finalLights, vec3_t ambientColor )
{
	memset( finalLights, 0, sizeof( renderLight_t ) * MAX_LIGHTS );

	if ( !( tr.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		R_LightPoint( origin, ambientColor );

		// build a list of all lights in our PVS

		std::vector<staticLight_t> lightsInPVS;
		lightsInPVS.reserve( 32 );

		trace_t trace;

		for ( int i = 0; i < mod_numStaticLights; ++i )
		{
			// hack into the server code, this doesn't call any server functions so we're safe, it's just a wrapper
			extern qboolean PF_inPVS( vec3_t p1, vec3_t p2 );

			if ( PF_inPVS( origin, mod_staticLights[i].origin ) )
			{
				// TODO: is this really necessary?
				trace = R_LightTrace( origin, mod_staticLights[i].origin );
				if ( trace.fraction == 1.0f )
				{
					lightsInPVS.push_back( mod_staticLights[i] );
				}
			}
		}

		if ( lightsInPVS.size() != 0 )
		{
			int skipIndices[MAX_LIGHTS]{ -1, -1, -1, -1 };

			// for all the lights in our PVS, find the four values that have the shortest distance to the origin, do this MAX_LIGHTS times
			for ( int iter1 = 0; iter1 < MAX_LIGHTS && iter1 < (int)lightsInPVS.size(); ++iter1 )
			{
				float smallestDistance = MAX_TRACE_LENGTH;
				int smallestIndex = 0;

				// in all our PVS lights, find the smallest distance, then mark it as skippable
				for ( int iter2 = 0; iter2 < (int)lightsInPVS.size(); ++iter2 )
				{
					float distance = VectorDistance( origin, lightsInPVS[iter2].origin );
					if ( distance < smallestDistance )
					{
						bool skip = false;
						// check our skip indices
						for ( int iter3 = 0; iter3 < MAX_LIGHTS; ++iter3 )
						{
							if ( skipIndices[iter3] == iter2 )
							{
								skip = true;
							}
						}
						if ( !skip )
						{
							smallestDistance = distance;
							smallestIndex = iter2;
						}
					}
				}

				// mark off the smallest index
				skipIndices[iter1] = smallestIndex;

				renderLight_t &finalLight = finalLights[iter1];
				const staticLight_t &staticLight = lightsInPVS[smallestIndex];

				VectorCopy( staticLight.origin, finalLight.position );
				VectorCopy( staticLight.color, finalLight.color );
				finalLight.intensity = static_cast<float>( staticLight.intensity ); // compensate
			}
		}
	}
	else
	{
		ambientColor[0] = 0.41f;
		ambientColor[1] = 0.41f;
		ambientColor[2] = 0.41f;

		// we have no world, so set up some fake lights
		finalLights[0].position[0] = 128.0f;
		finalLights[0].position[1] = 128.0f;
		finalLights[0].position[2] = 128.0f;
		finalLights[0].color[0] = 1.0f;
		finalLights[0].color[1] = 1.0f;
		finalLights[0].color[2] = 1.0f;
		finalLights[0].intensity = 200.0f;
	}
}

/*
===================================================================================================

	Alias Models

===================================================================================================
*/

#define POWERSUIT_SCALE		4.0f

#define NUMVERTEXNORMALS	162

static const float r_avertexnormals[NUMVERTEXNORMALS][3]{
#include "anorms.inl"
};

static vec4_t s_lerped[MAX_VERTS];

static vec3_t shadevector;
static vec3_t shadelight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
static const float r_avertexnormal_dots[SHADEDOT_QUANT][256]{
#include "anormtab.inl"
};

static const float *shadedots = r_avertexnormal_dots[0];

static void GL_LerpVerts(
	int numVerts, const dtrivertx_t *verts, const dtrivertx_t *oldVerts,
	const vec3_t move, const vec3_t frontv, const vec3_t backv,
	vec4_t lerp )
{
	for ( int i = 0; i < numVerts; ++i, ++verts, ++oldVerts, lerp += 4 )
	{
		lerp[0] = move[0] + oldVerts->v[0]*backv[0] + verts->v[0]*frontv[0];
		lerp[1] = move[1] + oldVerts->v[1]*backv[1] + verts->v[1]*frontv[1];
		lerp[2] = move[2] + oldVerts->v[2]*backv[2] + verts->v[2]*frontv[2];
	}
}

//
// interpolates between two frames and origins
// FIXME: batch lerp all vertexes
//
static void GL_DrawAliasFrameLerp( const dmdl_t *paliashdr, float backlerp )
{
	float 	l;
	daliasframe_t	*frame, *oldframe;
	dtrivertx_t	*ov, *verts;
	int		*order;
	int		count;
	float	frontlerp;
	float	alpha;
	vec3_t	move, delta, vectors[3];
	vec3_t	frontv, backv;
	int		i;
	int		index_xyz;
	float	*lerp;

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames 
		+ currententity->frame * paliashdr->framesize);
	verts = frame->verts;

	oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames 
		+ currententity->oldframe * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

	if ( currententity->flags & RF_TRANSLUCENT )
	{
		alpha = currententity->alpha;
	}
	else
	{
		alpha = 1.0f;
	}

	frontlerp = 1.0f - backlerp;

	// move should be the delta back to the previous frame * backlerp
	VectorSubtract( currententity->oldorigin, currententity->origin, delta );
	AngleVectors( currententity->angles, vectors[0], vectors[1], vectors[2] );

	move[0] = DotProduct( delta, vectors[0] );		// forward
	move[1] = -DotProduct( delta, vectors[1] );		// left
	move[2] = DotProduct( delta, vectors[2] );		// up

	VectorAdd( move, oldframe->translate, move );

	for ( i = 0; i < 3; i++ )
	{
		move[i] = backlerp * move[i] + frontlerp * frame->translate[i];
	}

	for ( i = 0; i < 3; i++ )
	{
		frontv[i] = frontlerp * frame->scale[i];
		backv[i] = backlerp * oldframe->scale[i];
	}

	lerp = s_lerped[0];

	GL_LerpVerts( paliashdr->num_xyz, verts, ov, move, frontv, backv, lerp );

	if ( r_vertex_arrays->GetBool() )
	{
		static float colorArray[MAX_VERTS * 4];

		glEnableClientState( GL_VERTEX_ARRAY );
		glVertexPointer( 3, GL_FLOAT, 16, s_lerped );	// padded for SIMD

		glEnableClientState( GL_COLOR_ARRAY );
		glColorPointer( 3, GL_FLOAT, 0, colorArray );

		//
		// pre light everything
		//
		for ( i = 0; i < paliashdr->num_xyz; i++ )
		{
			float l = shadedots[verts[i].lightnormalindex];

			colorArray[i * 3 + 0] = l * shadelight[0];
			colorArray[i * 3 + 1] = l * shadelight[1];
			colorArray[i * 3 + 2] = l * shadelight[2];
		}

		if ( GLEW_EXT_compiled_vertex_array && r_ext_compiled_vertex_array->GetBool() )
		{
			glLockArraysEXT( 0, paliashdr->num_xyz );
		}

		while ( 1 )
		{
			// get the vertex count and primitive type
			count = *order++;
			if ( !count )
			{
				// done
				break;
			}

			if ( count < 0 )
			{
				count = -count;
				glBegin( GL_TRIANGLE_FAN );
			}
			else
			{
				glBegin( GL_TRIANGLE_STRIP );
			}

			do
			{
				// texture coordinates come from the draw list
				glTexCoord2f( ( (float *)order )[0], ( (float *)order )[1] );
				index_xyz = order[2];

				order += 3;

				// normals and vertexes come from the frame list
				//l = shadedots[verts[index_xyz].lightnormalindex];

				//glColor4f (l* shadelight[0], l*shadelight[1], l*shadelight[2], alpha);
				glArrayElement( index_xyz );

			} while ( --count );

			glEnd();
		}

		if ( GLEW_EXT_compiled_vertex_array && r_ext_compiled_vertex_array->GetBool() )
		{
			glUnlockArraysEXT();
		}
	}
	else
	{
		while ( 1 )
		{
			// get the vertex count and primitive type
			count = *order++;
			if ( !count )
			{
				// done
				break;
			}

			if ( count < 0 )
			{
				count = -count;
				glBegin( GL_TRIANGLE_FAN );
			}
			else
			{
				glBegin( GL_TRIANGLE_STRIP );
			}

			do
			{
				// texture coordinates come from the draw list
				glTexCoord2f( ( (float *)order )[0], ( (float *)order )[1] );
				index_xyz = order[2];
				order += 3;

				// normals and vertexes come from the frame list
				l = shadedots[verts[index_xyz].lightnormalindex];

				glColor4f( l * shadelight[0], l * shadelight[1], l * shadelight[2], alpha );
				glVertex3fv( s_lerped[index_xyz] );
			} while ( --count );

			glEnd();
		}
	}
}

//
// Returns true if the model shouldn't be drawn
//
static bool R_CullAliasModel( vec3_t mins, vec3_t maxs, entity_t *e )
{
	int i;
	vec3_t		bbox[8];
	dmdl_t		*paliashdr;
	vec3_t		vectors[3];
	vec3_t		thismins, oldmins, thismaxs, oldmaxs;
	daliasframe_t *pframe, *poldframe;
	vec3_t angles;

	paliashdr = (dmdl_t *)currentmodel->extradata;

	if ( ( e->frame >= paliashdr->num_frames ) || ( e->frame < 0 ) )
	{
		Com_Printf("R_CullAliasModel %s: no such frame %d\n", 
			currentmodel->name, e->frame);
		e->frame = 0;
	}
	if ( ( e->oldframe >= paliashdr->num_frames ) || ( e->oldframe < 0 ) )
	{
		Com_Printf("R_CullAliasModel %s: no such oldframe %d\n", 
			currentmodel->name, e->oldframe);
		e->oldframe = 0;
	}

	pframe = ( daliasframe_t * ) ( ( byte * ) paliashdr + 
									  paliashdr->ofs_frames +
									  e->frame * paliashdr->framesize);

	poldframe = ( daliasframe_t * ) ( ( byte * ) paliashdr + 
									  paliashdr->ofs_frames +
									  e->oldframe * paliashdr->framesize);

	/*
	** compute axially aligned mins and maxs
	*/
	if ( pframe == poldframe )
	{
		for ( i = 0; i < 3; i++ )
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i]*255;
		}
	}
	else
	{
		for ( i = 0; i < 3; i++ )
		{
			thismins[i] = pframe->translate[i];
			thismaxs[i] = thismins[i] + pframe->scale[i]*255;

			oldmins[i]  = poldframe->translate[i];
			oldmaxs[i]  = oldmins[i] + poldframe->scale[i]*255;

			if ( thismins[i] < oldmins[i] )
				mins[i] = thismins[i];
			else
				mins[i] = oldmins[i];

			if ( thismaxs[i] > oldmaxs[i] )
				maxs[i] = thismaxs[i];
			else
				maxs[i] = oldmaxs[i];
		}
	}

	/*
	** compute a full bounding box
	*/
	for ( i = 0; i < 8; i++ )
	{
		vec3_t   tmp;

		if ( i & 1 )
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if ( i & 2 )
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if ( i & 4 )
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];

		VectorCopy( tmp, bbox[i] );
	}

	/*
	** rotate the bounding box
	*/
	VectorCopy( e->angles, angles );
	angles[YAW] = -angles[YAW];
	AngleVectors( angles, vectors[0], vectors[1], vectors[2] );

	for ( i = 0; i < 8; i++ )
	{
		vec3_t tmp;

		VectorCopy( bbox[i], tmp );

		bbox[i][0] = DotProduct( vectors[0], tmp );
		bbox[i][1] = -DotProduct( vectors[1], tmp );
		bbox[i][2] = DotProduct( vectors[2], tmp );

		VectorAdd( e->origin, bbox[i], bbox[i] );
	}

	{
		int p, f, aggregatemask = ~0;

		for ( p = 0; p < 8; p++ )
		{
			int mask = 0;

			for ( f = 0; f < 4; f++ )
			{
				float dp = DotProduct( frustum[f].normal, bbox[p] );

				if ( ( dp - frustum[f].dist ) < 0 )
				{
					mask |= ( 1 << f );
				}
			}

			aggregatemask &= mask;
		}

		if ( aggregatemask )
		{
			return true;
		}

		return false;
	}
}

void R_DrawAliasModel( entity_t *e )
{
	int				i;
	dmdl_t *		paliashdr;
	float			an;
	vec3_t			mins, maxs;
	material_t *	skin;

	// Can we be culled away?
	if ( !( e->flags & RF_WEAPONMODEL ) )
	{
		if ( R_CullAliasModel( mins, maxs, e ) ) {
			return;
		}
	}

	paliashdr = (dmdl_t *)currentmodel->extradata;

	//
	// get lighting information
	//
	// PMM - rewrote, reordered to handle new shells & mixing
	// PMM - 3.20 code .. replaced with original way of doing it to keep mod authors happy
	//
	if ( currententity->flags & ( RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE ) )
	{
		VectorClear( shadelight );
		if ( currententity->flags & RF_SHELL_HALF_DAM )
		{
			shadelight[0] = 0.56f;
			shadelight[1] = 0.59f;
			shadelight[2] = 0.45f;
		}
		if ( currententity->flags & RF_SHELL_DOUBLE )
		{
			shadelight[0] = 0.9f;
			shadelight[1] = 0.7f;
		}
		if ( currententity->flags & RF_SHELL_RED ) {
			shadelight[0] = 1.0f;
		}
		if ( currententity->flags & RF_SHELL_GREEN ) {
			shadelight[1] = 1.0f;
		}
		if ( currententity->flags & RF_SHELL_BLUE ) {
			shadelight[2] = 1.0f;
		}
	}
	else if ( currententity->flags & RF_FULLBRIGHT )
	{
		shadelight[0] = 1.0f;
		shadelight[1] = 1.0f;
		shadelight[2] = 1.0f;
	}
	else
	{
		R_LightPoint( currententity->origin, shadelight );

		// player lighting hack for communication back to server
		// big hack!
		if ( currententity->flags & RF_WEAPONMODEL )
		{
			float value;

			// pick the greatest component, which should be the same
			// as the mono value returned by software
			if ( shadelight[0] > shadelight[1] )
			{
				if ( shadelight[0] > shadelight[2] ) {
					value = 150.0f * shadelight[0];
				} else {
					value = 150.0f * shadelight[2];
				}
			}
			else
			{
				if ( shadelight[1] > shadelight[2] ) {
					value = 150.0f * shadelight[1];
				} else {
					value = 150.0f * shadelight[2];
				}
			}

			Cvar_SetFloat( r_lightlevel, value );
		}
	}

	// Apply a minimum constant light
	if ( currententity->flags & RF_MINLIGHT )
	{
		for ( i = 0; i < 3; ++i )
		{
			if ( shadelight[i] > 0.1f ) {
				break;
			}
		}
		if ( i == 3 )
		{
			shadelight[0] = 0.1f;
			shadelight[1] = 0.1f;
			shadelight[2] = 0.1f;
		}
	}

	// Bonus items will pulse with time
	if ( currententity->flags & RF_GLOW )
	{
		float scale;
		float min;

		scale = 0.1f * sin( tr.refdef.time * 7.0f );
		for ( i = 0; i < 3; ++i )
		{
			min = shadelight[i] * 0.8f;
			shadelight[i] += scale;
			if ( shadelight[i] < min ) {
				shadelight[i] = min;
			}
		}
	}

// =================
// PGM	ir goggles color override
	if ( tr.refdef.rdflags & RDF_IRGOGGLES && currententity->flags & RF_IR_VISIBLE)
	{
		shadelight[0] = 1.0f;
		shadelight[1] = 0.0f;
		shadelight[2] = 0.0f;
	}
// PGM	
// =================

	shadedots = r_avertexnormal_dots[((int)(currententity->angles[1] * (SHADEDOT_QUANT / 360.0f))) & (SHADEDOT_QUANT - 1)];
	
	an = RAD2DEG( currententity->angles[1] );
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize( shadevector );

	//
	// locate the proper data
	//

	tr.pc.aliasPolys += paliashdr->num_tris;

	//
	// draw all the triangles
	//

	glPushMatrix ();
	e->angles[PITCH] = -e->angles[PITCH];	// sigh.
	R_RotateForEntity (e);
	e->angles[PITCH] = -e->angles[PITCH];	// sigh.

	GL_TexEnv( GL_MODULATE );
	if ( currententity->flags & RF_TRANSLUCENT )
	{
		glEnable( GL_BLEND );
	}

	if ( ( currententity->frame >= paliashdr->num_frames ) || ( currententity->frame < 0 ) )
	{
		Com_Printf( "R_DrawAliasModel %s: no such frame %d\n",
			currentmodel->name, currententity->frame );
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

	if ( ( currententity->oldframe >= paliashdr->num_frames ) || ( currententity->oldframe < 0 ) )
	{
		Com_Printf( "R_DrawAliasModel %s: no such oldframe %d\n",
			currentmodel->name, currententity->oldframe );
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

	if ( !r_lerpmodels->GetBool() ) {
		currententity->backlerp = 0;
	}

	// select skin
	if ( currententity->skin )
	{
		// custom player skin
		skin = currententity->skin;
	}
	else
	{
		if ( currententity->skinnum >= MAX_MD2SKINS )
		{
			skin = currentmodel->skins[0];
		}
		else
		{
			skin = currentmodel->skins[currententity->skinnum];
			if ( !skin ) {
				skin = currentmodel->skins[0];
			}
		}
	}
	if ( !skin )
	{
		skin = defaultMaterial;	// fallback...
	}

	GL_ActiveTexture( GL_TEXTURE0 );
	skin->Bind();

	GL_UseProgram( 0 );
	glEnable( GL_TEXTURE_2D );

	GL_DrawAliasFrameLerp( paliashdr, currententity->backlerp );

	glDisable( GL_TEXTURE_2D );

	//R_DrawBounds( mins, maxs );

	if ( currententity->flags & RF_TRANSLUCENT )
	{
		glDisable( GL_BLEND );
	}
	GL_TexEnv( GL_REPLACE );

	glPopMatrix ();
}

/*
===================================================================================================

	Static Mesh Files

===================================================================================================
*/

void R_DrawStaticMeshFile( entity_t *e )
{
	using namespace DirectX;

	mSMF_t *memSMF = (mSMF_t *)e->model->extradata;

	// Matrices

	XMMATRIX modelMatrix = XMMatrixMultiply(
		//XMMatrixRotationRollPitchYaw( DEG2RAD( -e->angles[PITCH] ), DEG2RAD( -e->angles[YAW] ), DEG2RAD( e->angles[ROLL] ) ),
		XMMatrixRotationX( DEG2RAD( e->angles[ROLL] ) ) * XMMatrixRotationY( DEG2RAD( e->angles[PITCH] ) ) * XMMatrixRotationZ( DEG2RAD( e->angles[YAW] ) ),
		//XMMatrixRotationRollPitchYaw( 0.0f, 0.0f, 0.0f ),
		XMMatrixTranslation( e->origin[0], e->origin[1], e->origin[2] )
	);

	XMFLOAT4X4A modelMatrixStore;
	XMStoreFloat4x4A( &modelMatrixStore, modelMatrix );

	// lighting

	vec3_t ambientColor;
	renderLight_t finalLights[MAX_LIGHTS];
	R_FourNearestLights( e->origin, finalLights, ambientColor );

	GL_UseProgram( glProgs.smfMeshProg );

	glUniformMatrix4fv( 4, 1, GL_FALSE, (const GLfloat *)&modelMatrixStore );
	glUniformMatrix4fv( 5, 1, GL_FALSE, (const GLfloat *)&tr.viewMatrix );
	glUniformMatrix4fv( 6, 1, GL_FALSE, (const GLfloat *)&tr.projMatrix );

	glUniform3fv( 7, 1, tr.refdef.vieworg );
	glUniform3fv( 8, 1, ambientColor );

	constexpr int startIndex = 9;
	constexpr int elementsInRenderLight = 3;

	for ( int iter1 = 0, iter2 = 0; iter1 < MAX_LIGHTS; ++iter1, iter2 += elementsInRenderLight )
	{
		glUniform3fv( startIndex + iter2 + 0, 1, finalLights[iter1].position );
		glUniform3fv( startIndex + iter2 + 1, 1, finalLights[iter1].color );
		glUniform1f( startIndex + iter2 + 2, finalLights[iter1].intensity );
	}

	constexpr int indexAfterLights = startIndex + elementsInRenderLight * MAX_LIGHTS;

	glUniform1i( indexAfterLights + 0, 0 ); // diffuse
	glUniform1i( indexAfterLights + 1, 1 ); // specular
	glUniform1i( indexAfterLights + 2, 2 ); // normal
	glUniform1i( indexAfterLights + 3, 3 ); // emission

	glBindVertexArray( memSMF->vao );
	glBindBuffer( GL_ARRAY_BUFFER, memSMF->vbo );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, memSMF->ebo );

	// viewmodel FOV
	if ( currententity->flags & RF_WEAPONMODEL )
	{
		XMMATRIX newProj = XMMatrixPerspectiveFovRH( DEG2RAD( r_viewmodelfov->GetFloat() ), (float)tr.refdef.width / (float)tr.refdef.height, 4.0f, 4096.0f );
		
		XMFLOAT4X4A newStore;
		XMStoreFloat4x4A( &newStore, newProj );

		glUniformMatrix4fv( 6, 1, GL_FALSE, (const GLfloat *)&newStore );
	}

	mSMFMesh_t *meshes = reinterpret_cast<mSMFMesh_t *>( (byte *)memSMF + sizeof( mSMF_t ) );

	GLenum type = memSMF->type;

	// Sigh
	glFrontFace( GL_CCW );
	
	for ( uint32 i = 0; i < memSMF->numMeshes; ++i )
	{
		GL_ActiveTexture( GL_TEXTURE0 );
		meshes[i].material->Bind();
		GL_ActiveTexture( GL_TEXTURE1 );
		meshes[i].material->BindSpec();
		GL_ActiveTexture( GL_TEXTURE2 );
		meshes[i].material->BindNorm();
		GL_ActiveTexture( GL_TEXTURE3 );
		meshes[i].material->BindEmit();

		glDrawElements( GL_TRIANGLES, meshes[i].count, type, (void *)( (uintptr_t)meshes[i].offset ) );
	}

	// Sigh
	glFrontFace( GL_CW );
}
