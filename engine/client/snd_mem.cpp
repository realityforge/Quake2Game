// snd_mem.c: sound caching

#include "snd_local.h"

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "../../thirdparty/stb/stb_vorbis.c"

/*
========================
ResampleSfx
========================
*/
static void ResampleSfx( sfx_t *sfx, int inrate, int inwidth, byte *data )
{
	int		outcount;
	int		srcsample;
	float	stepscale;
	int		i;
	int		sample, samplefrac, fracstep; // samplefrac has a tendency to overflow int32 with -huge- audio files
	sfxcache_t	*sc;

	sc = sfx->cache;
	if ( !sc ) {
		return;
	}

	stepscale = (float)inrate / dma.speed; // this is usually 0.5, 1, or 2

	outcount = sc->length / stepscale;
	sc->length = outcount;
	if ( sc->loopstart != -1 )
		sc->loopstart = sc->loopstart / stepscale;

	sc->speed = dma.speed;
	sc->width = inwidth;
	sc->stereo = 0;

	// resample / decimate to the current source rate

	if ( stepscale == 1 && inwidth == 1 && sc->width == 1 )
	{
		// fast special case
		for ( i = 0; i < outcount; i++ )
		{
			( (signed char *)sc->data )[i] = (int)( (unsigned char)( data[i] ) - 128 );
		}
	}
	else
	{
		// general case
		samplefrac = 0;
		fracstep = stepscale * 256;
		for ( i = 0; i < outcount; i++ )
		{
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if ( inwidth == 2 ) {
				sample = LittleShort( ( (short *)data )[srcsample] );
			} else {
				sample = (int)( (unsigned char)( data[srcsample] ) - 128 ) << 8;
			}
			if ( sc->width == 2 ) {
				( (short *)sc->data )[i] = sample;
			} else {
				( (signed char *)sc->data )[i] = sample >> 8;
			}
		}
	}
}

static bool IsWav( byte *wav )
{
	return *( (int32 *)wav ) == MakeFourCC( 'R', 'I', 'F', 'F' );
}

static bool IsOgg( byte *ogg )
{
	return *( (int32 *)ogg ) == MakeFourCC( 'O', 'g', 'g', 'S' );
}

/*
===============================================================================

WAV loading

===============================================================================
*/

byte	*data_p;
byte 	*iff_end;
byte 	*last_chunk;
byte 	*iff_data;
int 	iff_chunk_len;

static short GetLittleShort()
{
	short val = 0;
	val = *data_p;
	val = val + ( *( data_p + 1 ) << 8 );
	data_p += 2;
	return val;
}

static int GetLittleLong()
{
	int val = 0;
	val = *data_p;
	val = val + ( *( data_p + 1 ) << 8 );
	val = val + ( *( data_p + 2 ) << 16 );
	val = val + ( *( data_p + 3 ) << 24 );
	data_p += 4;
	return val;
}

static void FindNextChunk( const char *name )
{
	while ( 1 )
	{
		data_p = last_chunk;

		if ( data_p >= iff_end )
		{
			// didn't find the chunk
			data_p = nullptr;
			return;
		}

		data_p += 4;
		iff_chunk_len = GetLittleLong();
		if ( iff_chunk_len < 0 )
		{
			data_p = nullptr;
			return;
		}

		data_p -= 8;
		last_chunk = data_p + 8 + ( ( iff_chunk_len + 1 ) & ~1 );
		if ( Q_strncmp( (char*)data_p, name, 4 ) == 0 )
		{
			return;
		}
	}
}

static void FindChunk( const char *name )
{
	last_chunk = iff_data;
	FindNextChunk( name );
}

/*
========================
GetWavinfo
========================
*/
static wavinfo_t GetWavinfo( const char *name, byte *wav, int wavlength )
{
	wavinfo_t	info;
	int     i;
	int     format;
	int		samples;

	memset( &info, 0, sizeof( info ) );

	if ( !wav ) {
		return info;
	}

	iff_data = wav;
	iff_end = wav + wavlength;

	// find "RIFF" chunk
	FindChunk( "RIFF" );
	if ( !( data_p && !Q_strncmp( (char*)( data_p + 8 ), "WAVE", 4 ) ) )
	{
		Com_Printf( "Missing RIFF/WAVE chunks\n" );
		return info;
	}

	// get "fmt " chunk
	iff_data = data_p + 12;

	FindChunk( "fmt " );
	if ( !data_p )
	{
		Com_Printf( "Missing fmt chunk\n" );
		return info;
	}
	data_p += 8;
	format = GetLittleShort();
	if ( format != 1 )
	{
		Com_Printf( "Microsoft PCM format only\n" );
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4 + 2;
	info.width = GetLittleShort() / 8;

// get cue chunk
	FindChunk( "cue " );
	if ( data_p )
	{
		data_p += 32;
		info.loopstart = GetLittleLong();
//		Com_Printf("loopstart=%d\n", sfx->loopstart);

		// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk( "LIST" );
		if ( data_p )
		{
			if ( !Q_strncmp( (char*)data_p + 28, "mark", 4 ) )
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = GetLittleLong();	// samples in loop
				info.samples = info.loopstart + i;
//				Com_Printf("looped length: %i\n", i);
			}
		}
	}
	else
		info.loopstart = -1;

// find data chunk
	FindChunk( "data" );
	if ( !data_p )
	{
		Com_Printf( "Missing data chunk\n" );
		return info;
	}

	data_p += 4;
	samples = GetLittleLong() / info.width;

	if ( info.samples )
	{
		if ( samples < info.samples )
			Com_Errorf( "Sound %s has a bad loop length", name );
	}
	else
		info.samples = samples;

	info.dataofs = data_p - wav;

	return info;
}

//=============================================================================

/*
========================
S_LoadSound
========================
*/
sfxcache_t *S_LoadSound( sfx_t *s )
{
	char	namebuffer[MAX_QPATH];
	byte	*data;
	int		len;
	float	stepscale;
	sfxcache_t	*sc;
	char	*name;

	if ( s->name[0] == '*' ) {
		return NULL;
	}

	// see if still in memory
	sc = s->cache;
	if ( sc ) {
		return sc;
	}

	// load it in
	if ( s->truename ) {
		name = s->truename;
	} else {
		name = s->name;
	}

	if ( name[0] == '#' ) {
		Q_strcpy_s( namebuffer, name + 1 );
	} else {
		Q_sprintf_s( namebuffer, "sound/%s", name );
	}

	//Com_Printf ("loading %s\n",namebuffer);

	fsSize_t size = FileSystem::LoadFile (namebuffer, (void **)&data);
	if ( !data )
	{
		Com_Printf( "Couldn't load %s\n", namebuffer );
		return nullptr;
	}

	if ( IsWav( data ) )
	{
		wavinfo_t info;

		info = GetWavinfo( s->name, data, size );
		if ( info.channels != 1 )
		{
			FileSystem::FreeFile( data );
			Com_Printf( "%s is a stereo sample\n", s->name );
			return nullptr;
		}

		stepscale = (float)info.rate / dma.speed;
		len = info.samples / stepscale;

		len = len * info.width * info.channels;

		sc = s->cache = (sfxcache_t *)Mem_Alloc( len + sizeof( sfxcache_t ) );
		if ( !sc )
		{
			FileSystem::FreeFile( data );
			return nullptr;
		}

		sc->length = info.samples;
		sc->loopstart = info.loopstart;
		sc->speed = info.rate;
		sc->width = info.width;
		sc->stereo = info.channels;

		ResampleSfx( s, sc->speed, sc->width, data + info.dataofs );

		FileSystem::FreeFile( data );
	}
	else if ( IsOgg( data ) )
	{
		int channels, samplerate, samples;
		short *output;
		samples = stb_vorbis_decode_memory( data, size, &channels, &samplerate, &output );
		FileSystem::FreeFile( data );
		if ( samples == -1 )
		{
			Com_Printf( "Couldn't load %s\n", namebuffer );
			return nullptr;
		}
		if ( channels != 1 )
		{
			// SlartTodo: Implement channel downmixing
			Com_Printf( "%s is a stereo sample\n", s->name );
			Mem_Free( output );
			return nullptr;
		}

		stepscale = (float)samplerate / dma.speed;
		len = samples / stepscale;

		len *= sizeof( short ) * channels;

		sc = s->cache = (sfxcache_t *)Mem_Alloc( len + sizeof( sfxcache_t ) );
		if ( !sc )
		{
			Mem_Free( output );
			return nullptr;
		}

		sc->length = samples;
		sc->loopstart = -1;
		sc->speed = samplerate;
		sc->width = sizeof( short );
		sc->stereo = channels;

		ResampleSfx( s, sc->speed, sc->width, (byte *)output );

		Mem_Free( output );
	}
	else
	{
		FileSystem::FreeFile( data );
		Com_Printf( "%s is neither WAV or OGG data\n", namebuffer );
		return nullptr;
	}

	return sc;
}
