//=================================================================================================
// Quake's interface to the networking layer
//
// This is the general header for both net_os and net_chan
//=================================================================================================

#pragma once

#include "../../common/q_types.h"

#include "sizebuf.h"

//-------------------------------------------------------------------------------------------------
// OS network implementation
//-------------------------------------------------------------------------------------------------

#define	PORT_ANY		-1

#define	MAX_MSGLEN		1400		// max length of a message
#define	PACKET_HEADER	10			// two ints and a short

// Short so netadr_t is a nice 8 bytes
enum netadrtype_t : uint16 { NA_LOOPBACK, NA_BROADCAST, NA_IP };

enum netsrc_t { NS_CLIENT, NS_SERVER };

struct netadr_t
{
	netadrtype_t	type;

	byte	ip[4];

	uint16	port;
};

void		NET_Init( void );
void		NET_Shutdown( void );

void		NET_Config( bool multiplayer );

bool		NET_GetPacket( netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message );
void		NET_SendPacket( netsrc_t sock, int length, void *data, const netadr_t &to );

bool		NET_CompareAdr( const netadr_t &a, const netadr_t &b );
bool		NET_CompareBaseAdr( const netadr_t &a, const netadr_t &b );
bool		NET_IsLocalAddress( const netadr_t &adr );
char		*NET_AdrToString( const netadr_t &a );
bool		NET_StringToAdr( const char *s, netadr_t &a );
void		NET_Sleep( int msec );

//-------------------------------------------------------------------------------------------------
// Net channels
//-------------------------------------------------------------------------------------------------

#define	MAX_LATENT	32

struct netchan_t
{
	qboolean	fatal_error;

	netsrc_t	sock;

	int			dropped;			// between last packet and previous

	int			last_received;		// for timeouts
	int			last_sent;			// for retransmits

	netadr_t	remote_address;
	int			qport;				// qport value to write when transmitting

// sequencing variables
	int			incoming_sequence;
	int			incoming_acknowledged;
	int			incoming_reliable_acknowledged;	// single bit

	int			incoming_reliable_sequence;		// single bit, maintained local

	int			outgoing_sequence;
	int			reliable_sequence;			// single bit
	int			last_reliable_sequence;		// sequence number of last send

// reliable staging and holding areas
	sizebuf_t	message;		// writing buffer to send to server
	byte		message_buf[MAX_MSGLEN - 16];		// leave space for header

// message is copied to this buffer when it is first transfered
	int			reliable_length;
	byte		reliable_buf[MAX_MSGLEN - 16];	// unacked reliable message
};

extern	netadr_t	net_from;
extern	sizebuf_t	net_message;
extern	byte		net_message_buffer[MAX_MSGLEN];

void		Netchan_Init( void );
void		Netchan_Setup( netsrc_t sock, netchan_t *chan, netadr_t adr, int qport );

qboolean	Netchan_NeedReliable( netchan_t *chan );
void		Netchan_Transmit( netchan_t *chan, int length, byte *data );
void		Netchan_OutOfBand( netsrc_t net_socket, netadr_t adr, int length, byte *data );
void		Netchan_OutOfBandPrint( netsrc_t net_socket, netadr_t adr, const char *format, ... );
qboolean	Netchan_Process( netchan_t *chan, sizebuf_t *msg );

qboolean	Netchan_CanReliable( netchan_t *chan );
