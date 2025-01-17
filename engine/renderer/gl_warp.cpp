/*
=============================================================

Code for drawing sky and water polygons

=============================================================
*/

#include "gl_local.h"

struct skyVertex_t
{
	float x, y, z;
	float s, t;
};

extern model_t		*loadmodel;

static char			skyname[MAX_QPATH];
static float		skyrotate;
static vec3_t		skyaxis;
static material_t	*sky_images[6];
static GLuint		skyVAO, skyVBO;

static msurface_t	*warpface;

static constexpr float c_subdivide_size = 64; // 1024

static void BoundPoly(int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i = 0; i < numverts; i++)
		for (j = 0; j < 3; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

static void SubdividePolygon(int numverts, float *verts)
{
	int		i, j, k;
	vec3_t	mins, maxs;
	float	m;
	float	*v;
	vec3_t	front[64], back[64];
	int		f, b;
	float	dist[64];
	float	frac;
	glpoly_t *poly;
	float	s, t;
	vec3_t	total;
	float	total_s, total_t;

	if (numverts > 60)
		Com_Errorf("numverts = %i", numverts);

	BoundPoly(numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5f;
		m = c_subdivide_size * floor(m / c_subdivide_size + 0.5f);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy(verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy(v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy(v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j + 1] > 0))
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon(f, front[0]);
		SubdividePolygon(b, back[0]);
		return;
	}

	// add a point in the center to help keep warp valid
	poly = (glpoly_t *)Hunk_Alloc(sizeof(glpoly_t) + ((numverts - 4) + 2) * VERTEXSIZE * sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts + 2;
	VectorClear(total);
	total_s = 0;
	total_t = 0;
	for (i = 0; i < numverts; i++, verts += 3)
	{
		VectorCopy(verts, poly->verts[i + 1]);
		s = DotProduct(verts, warpface->texinfo->vecs[0]);
		t = DotProduct(verts, warpface->texinfo->vecs[1]);

		total_s += s;
		total_t += t;
		VectorAdd(total, verts, total);

		poly->verts[i + 1][3] = s;
		poly->verts[i + 1][4] = t;
	}

	VectorScale(total, (1.0f / numverts), poly->verts[0]);
	poly->verts[0][3] = total_s / numverts;
	poly->verts[0][4] = total_t / numverts;

	// copy first vertex to last
	memcpy(poly->verts[i + 1], poly->verts[1], sizeof(poly->verts[0]));
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface(msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;

	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i = 0; i < fa->numedges; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy(vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon(numverts, verts[0]);
}

//=========================================================

// speed up sin calculations - Ed
float r_turbsin[]
{
#include "warpsin.inl"
};
static constexpr float c_turbscale = (256.0f / (2.0f * M_PI_F));

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolys(msurface_t *fa)
{
	glpoly_t	*p, *bp;
	float		*v;
	int			i;
	float		s, t, os, ot;
	float		scroll;
	float		rdt = tr.refdef.time;

	if (fa->texinfo->flags & SURF_FLOWING)
		scroll = -64 * ((tr.refdef.time * 0.5f) - (int)(tr.refdef.time * 0.5f));
	else
		scroll = 0;
	for (bp = fa->polys; bp; bp = bp->next)
	{
		p = bp;

		glBegin(GL_TRIANGLE_FAN);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE)
		{
			os = v[3];
			ot = v[4];

			s = os + r_turbsin[(int)((ot * 0.125f + tr.refdef.time) * c_turbscale) & 255]; // slart: ftol

			s += scroll;
			s *= (1.0f / 64);

			t = ot + r_turbsin[(int)((os * 0.125f + rdt) * c_turbscale) & 255]; // slart: ftol

			t *= (1.0f / 64);

			glTexCoord2f(s, t);
			glVertex3fv(v);
		}
		glEnd();
	}
}


/*
===================================================================================================

	Skybox

===================================================================================================
*/

static const vec3_t skyclip[6]
{
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1}
};

// 1 = s, 2 = t, 3 = 2048
static const int st_to_vec[6][3]
{
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down
};

// s = [0]/[2], t = [1]/[2]
static const int vec_to_st[6][3]
{
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}

//	{-1,2,3},
//	{1,2,-3}
};

static float skymins[2][6], skymaxs[2][6];

static void DrawSkyPolygon(int nump, vec3_t vecs)
{
	int		i, j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float *vp;

	// decide which face it maps to
	VectorClear(v);
	for (i = 0, vp = vecs; i < nump; i++, vp += 3)
	{
		VectorAdd(vp, v, v);
	}
	av[0] = fabsf(v[0]);
	av[1] = fabsf(v[1]);
	av[2] = fabsf(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i = 0; i < nump; i++, vecs += 3)
	{
		j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];
		if (dv < 0.001f)
			continue;	// don't divide by zero
		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j - 1] / dv;
		else
			s = vecs[j - 1] / dv;
		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j - 1] / dv;
		else
			t = vecs[j - 1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

#define	ON_EPSILON		0.1f		// point on plane side epsilon
#define	MAX_CLIP_VERTS	64
static void ClipSkyPolygon( int nump, vec3_t vecs, int stage )
{
	const float *norm;
	float *v;
	bool	front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;

	if ( nump > MAX_CLIP_VERTS - 2 ) {
		Com_Error( "ClipSkyPolygon: MAX_CLIP_VERTS" );
	}
	if ( stage == 6 ) {
		// fully clipped, so draw it
		DrawSkyPolygon( nump, vecs );
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for ( i = 0, v = vecs; i < nump; i++, v += 3 )
	{
		d = DotProduct( v, norm );
		if ( d > ON_EPSILON )
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if ( d < -ON_EPSILON )
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if ( !front || !back )
	{	// not clipped
		ClipSkyPolygon( nump, vecs, stage + 1 );
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy( vecs, ( vecs + ( i * 3 ) ) );
	newc[0] = newc[1] = 0;

	for ( i = 0, v = vecs; i < nump; i++, v += 3 )
	{
		switch ( sides[i] )
		{
		case SIDE_FRONT:
			VectorCopy( v, newv[0][newc[0]] );
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy( v, newv[1][newc[1]] );
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy( v, newv[0][newc[0]] );
			newc[0]++;
			VectorCopy( v, newv[1][newc[1]] );
			newc[1]++;
			break;
		}

		if ( sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i] ) {
			continue;
		}

		d = dists[i] / ( dists[i] - dists[i + 1] );
		for ( j = 0; j < 3; j++ )
		{
			e = v[j] + d * ( v[j + 3] - v[j] );
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon( newc[0], newv[0][0], stage + 1 );
	ClipSkyPolygon( newc[1], newv[1][0], stage + 1 );
}

/*
========================
R_AddSkySurface
========================
*/
void R_AddSkySurface( const msurface_t *fa )
{
	vec3_t verts[MAX_CLIP_VERTS];

	// Calculate vertex values for skybox
	for ( uint32 i = 0; i < fa->numIndices; ++i )
	{
		VectorSubtract( g_worldData.vertices[g_worldData.indices[fa->firstIndex + i]].pos, tr.refdef.vieworg, verts[i] );
	}

	ClipSkyPolygon( fa->numIndices, verts[0], 0 );
}

/*
========================
R_ClearSkyBox
========================
*/
void R_ClearSkyBox()
{
	for ( int i = 0; i < 6; i++ )
	{
		skymins[0][i] = skymins[1][i] = 9999.0f;
		skymaxs[0][i] = skymaxs[1][i] = -9999.0f;
	}
}

#define SQRT3INV	0.57735026919f	// 1 / sqrt(3)

static void MakeSkyVec( skyVertex_t *skyVertex, float s, float t, int axis )
{
	vec3_t		v, b;
	int			j, k;

	// SlartTodo: 4096 = zfar
	constexpr float width = 4096.0f * SQRT3INV;

	b[0] = s * width;
	b[1] = t * width;
	b[2] = width;

	for ( j = 0; j < 3; j++ )
	{
		k = st_to_vec[axis][j];
		if ( k < 0 ) {
			v[j] = -b[-k - 1];
		} else {
			v[j] = b[k - 1];
		}
	}

	// avoid bilerp seam
	s = ( s + 1.0f ) * 0.5f;
	t = ( t + 1.0f ) * 0.5f;

	skyVertex->x = v[0];
	skyVertex->y = v[1];
	skyVertex->z = v[2];
	skyVertex->s = s;
	skyVertex->t = 1.0f - t;
}

/*
========================
R_DrawSkyBox
========================
*/
static const int skytexorder[6]{ 0, 2, 1, 3, 4, 5 };

void R_DrawSkyBox()
{
	int i;

	if ( skyrotate ) {
		// check for no sky at all
		for ( i = 0; i < 6; ++i ) {
			if ( skymins[0][i] < skymaxs[0][i] &&
				skymins[1][i] < skymaxs[1][i] ) {
				break;
			}
		}
		if ( i == 6 ) {
			return;		// nothing visible
		}
	}

	skyVertex_t skyVertices[6 * 4];
	material_t *skyDrawCalls[6];
	int numSides;

	for ( i = 0, numSides = 0; i < 6; ++i )
	{
		if ( skyrotate ) {
			// hack, forces full sky to draw when rotating
			skymins[0][i] = -1.0f;
			skymins[1][i] = -1.0f;
			skymaxs[0][i] = 1.0f;
			skymaxs[1][i] = 1.0f;
		}

		if ( skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i] ) {
			continue;
		}

		MakeSkyVec( skyVertices + ( numSides * 4 ) + 0, skymins[0][i], skymins[1][i], i );
		MakeSkyVec( skyVertices + ( numSides * 4 ) + 1, skymins[0][i], skymaxs[1][i], i );
		MakeSkyVec( skyVertices + ( numSides * 4 ) + 2, skymaxs[0][i], skymaxs[1][i], i );
		MakeSkyVec( skyVertices + ( numSides * 4 ) + 3, skymaxs[0][i], skymins[1][i], i );

		skyDrawCalls[numSides] = sky_images[skytexorder[i]];

		++numSides;
	}

#if 0
	using namespace DirectX;
	vec3_t &viewOrg = tr.refdef.vieworg;
	vec3_t &viewAng = tr.refdef.viewangles;

	vec3_t forward;

	AngleVectors( viewAng, forward, nullptr, nullptr );

	XMVECTOR translate = XMVectorSet( viewOrg[0], viewOrg[1], viewOrg[2], 0.0f );
	XMVECTOR rotate = XMVectorSet(  )
	XMVECTOR focusPoint = XMVectorSet( viewOrg[0] + forward[0], viewOrg[1] + forward[1], viewOrg[2] + forward[2], 0.0f );
	XMVECTOR upAxis = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );

	XMMATRIX workMatrix = XMMatrixLookAtRH( eyePosition, focusPoint, upAxis );
	XMFLOAT4X4A viewMatrix;
	XMStoreFloat4x4A( &viewMatrix, workMatrix );
#endif

	GL_UseProgram( glProgs.skyProg );

	glPushMatrix();
	glTranslatef( tr.refdef.vieworg[0], tr.refdef.vieworg[1], tr.refdef.vieworg[2] );
	glRotatef( tr.refdef.time * skyrotate, skyaxis[0], skyaxis[1], skyaxis[2] );

	float viewMatrix[4][4];
	glGetFloatv( GL_MODELVIEW_MATRIX, (GLfloat *)viewMatrix );

	glPopMatrix();

	glUniformMatrix4fv( 2, 1, GL_FALSE, (const GLfloat *)&viewMatrix );
	glUniformMatrix4fv( 3, 1, GL_FALSE, (const GLfloat *)&tr.projMatrix );
	glUniform1i( 4, 0 );

	glBindVertexArray( skyVAO );
	glBindBuffer( GL_ARRAY_BUFFER, skyVBO );

	GL_ActiveTexture( GL_TEXTURE0 );

	glBufferData( GL_ARRAY_BUFFER, numSides * ( 4 * sizeof( skyVertex_t ) ), skyVertices, GL_STREAM_DRAW );

	for ( i = 0; i < numSides; ++i )
	{
		skyDrawCalls[i]->Bind();
		glDrawArrays( GL_TRIANGLE_FAN, i * 4, 4 );
	}
}

/*
========================
R_SetSky
========================
*/
static const char *suf[6]{ "rt", "bk", "lf", "ft", "up", "dn" };

void R_SetSky( const char *name, float rotate, vec3_t axis )
{
	char pathname[MAX_QPATH];

	Q_strcpy_s( skyname, name );
	skyrotate = rotate;
	VectorCopy( axis, skyaxis );

	for ( int i = 0; i < 6; i++ )
	{
		Q_sprintf_s( pathname, "materials/env/%s%s.mat", skyname, suf[i] );

		sky_images[i] = GL_FindMaterial( pathname );
	}
}

void Sky_Init()
{
	glGenVertexArrays( 1, &skyVAO );
	glGenBuffers( 1, &skyVBO );

	glBindVertexArray( skyVAO );
	glBindBuffer( GL_ARRAY_BUFFER, skyVBO );

	glEnableVertexAttribArray( 0 );
	glEnableVertexAttribArray( 1 );

	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof( skyVertex_t ), (void *)( 0 ) );
	glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, sizeof( skyVertex_t ), (void *)( 3 * sizeof( GLfloat ) ) );
}

void Sky_Shutdown()
{
	glDeleteBuffers( 1, &skyVBO );
	glDeleteVertexArrays( 1, &skyVAO );
}
