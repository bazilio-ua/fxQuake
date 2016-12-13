/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// net.h -- quake's interface to the networking layer


#ifdef _WIN32
/* windows includes and compatibility macros */

// TODO: add winsock2 here
#include <winsock.h>

/* there is no in_addr_t on windows: define it as the type of the S_addr of in_addr structure */
typedef u_long	in_addr_t; /* uint32_t */
/* on windows, socklen_t is to be a winsock2 thing */
#ifndef IP_MSFILTER_SIZE
typedef int	socklen_t;
#endif /* IP_MSFILTER_SIZE */
typedef SOCKET	sys_socket_t;

#else
/* unix includes and compatibility macros */

typedef int	sys_socket_t;
#define INVALID_SOCKET	(-1)
#define SOCKET_ERROR	(-1)

#endif /* _WIN32 */


/* macros which may still be missing */

#ifndef INADDR_ANY
#define INADDR_ANY ((in_addr_t) 0x00000000)
#endif /* INADDR_ANY */

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK	((in_addr_t) 0x7f000001) /* 127.0.0.1 */
#endif /* INADDR_LOOPBACK */

#ifndef INADDR_BROADCAST
#define INADDR_BROADCAST ((in_addr_t) 0xffffffff)
#endif /* INADDR_BROADCAST */

#ifndef INADDR_NONE
#define INADDR_NONE	((in_addr_t) 0xffffffff)
#endif /* INADDR_NONE */

#ifndef MAXHOSTNAMELEN
/* SUSv2 guarantees that `Host names are limited to 255 bytes'.
 POSIX 1003.1-2001 guarantees that `Host names (not including
 the terminating NUL) are limited to HOST_NAME_MAX bytes'. */
#define MAXHOSTNAMELEN		256
#endif /* MAXHOSTNAMELEN */


struct qsockaddr
{
/* struct sockaddr has unsigned char 'dummy' as the first member in BSD variants 
 and the family member is also an unsigned char instead of an unsigned short. 
 This should matter only when UNIX is defined */
#if defined __FreeBSD__ || defined __OpenBSD__ || defined __NetBSD__ || defined __APPLE__ && defined __MACH__
	byte dummy;
	byte qsa_family;
#else
	short qsa_family;
#endif
	byte qsa_data[14];
};

#define NET_NAME_ID			"QUAKE"
#define PROQUAKE_VERSION	5.00 // for network communications (compat. with PQ)

#define NET_NAMELEN			64

#define NET_MAXMESSAGE		65536 // was 8192
#define NET_HEADERSIZE		(2 * sizeof(unsigned int)) // 8
#define NET_DATAGRAMSIZE	(MAX_DATAGRAM + NET_HEADERSIZE)

// NetHeader flags
#define NETFLAG_LENGTH_MASK	0x0000ffff
#define NETFLAG_DATA		0x00010000
#define NETFLAG_ACK			0x00020000
#define NETFLAG_NAK			0x00040000
#define NETFLAG_EOM			0x00080000
#define NETFLAG_UNRELIABLE	0x00100000
#define NETFLAG_CTL			0x80000000


#define NET_PROTOCOL_VERSION	3

// This is the network info/connection protocol.  It is used to find Quake
// servers, get info about them, and connect to them.  Once connected, the
// Quake game protocol (documented elsewhere) is used.
//
//
// General notes:
//	game_name is currently always "QUAKE", but is there so this same protocol
//		can be used for future games as well; can you say Quake2?
//
// CCREQ_CONNECT
//		string	game_name				"QUAKE"
//		byte	net_protocol_version	NET_PROTOCOL_VERSION
//
// CCREQ_SERVER_INFO
//		string	game_name				"QUAKE"
//		byte	net_protocol_version	NET_PROTOCOL_VERSION
//
// CCREQ_PLAYER_INFO
//		byte	player_number
//
// CCREQ_RULE_INFO
//		string	rule
//
//
//
// CCREP_ACCEPT
//		long	port
//
// CCREP_REJECT
//		string	reason
//
// CCREP_SERVER_INFO
//		string	server_address
//		string	host_name
//		string	level_name
//		byte	current_players
//		byte	max_players
//		byte	protocol_version	NET_PROTOCOL_VERSION
//
// CCREP_PLAYER_INFO
//		byte	player_number
//		string	name
//		long	colors
//		long	frags
//		long	connect_time
//		string	address
//
// CCREP_RULE_INFO
//		string	rule
//		string	value

//	note:
//		There are two address forms used above.  The short form is just a
//		port number.  The address that goes along with the port is defined as
//		"whatever address you receive this reponse from".  This lets us use
//		the host OS to solve the problem of multiple host addresses (possibly
//		with no routing between them); the host will use the right address
//		when we reply to the inbound connection request.  The long from is
//		a full address and port in a string.  It is used for returning the
//		address of a server that is not running locally.

#define CCREQ_CONNECT		0x01
#define CCREQ_SERVER_INFO	0x02
#define CCREQ_PLAYER_INFO	0x03
#define CCREQ_RULE_INFO		0x04
#define CCREQ_RCON		0x05

#define CCREP_ACCEPT		0x81
#define CCREP_REJECT		0x82
#define CCREP_SERVER_INFO	0x83
#define CCREP_PLAYER_INFO	0x84
#define CCREP_RULE_INFO		0x85
#define CCREP_RCON		0x86

// support for mods
#define MOD_NONE		0x00
#define MOD_PROQUAKE		0x01

struct net_landriver_s;
struct net_driver_s;

// rcon
extern sizebuf_t	rcon_message;
extern qboolean		rcon_active;

typedef struct qsocket_s
{
	struct qsocket_s		*next;
	double					connectTime;
	double					lastMessageTime;
	double					lastSendTime;

	qboolean				disconnected;
	qboolean				canSend;
	qboolean				sendNext;

	struct net_driver_s		*driver;
	struct net_landriver_s	*landriver;
	sys_socket_t			net_socket;
	int						mtu;
	void					*driverdata;

	unsigned int			ackSequence;
	unsigned int			sendSequence;
	unsigned int			unreliableSendSequence;
	int						sendMessageLength;
	byte					sendMessage[NET_MAXMESSAGE];

	unsigned int			receiveSequence;
	unsigned int			unreliableReceiveSequence;
	int						receiveMessageLength;
	byte					receiveMessage[NET_MAXMESSAGE];

	struct qsockaddr		addr;
	char					address[NET_NAMELEN];

	// new stuff here
	byte					mod; // (compat. with PQ)
	byte					mod_version;	// = floor(version * 10) (must fit in one byte)
	byte					mod_flags; // reserved (compat. with PQ)
	int						client_port; // ProQuake NAT fix
	qboolean				net_wait; // wait for the client to send a packet to the private port
} qsocket_t;

extern qsocket_t	*net_activeSockets;
extern qsocket_t	*net_freeSockets;

typedef struct net_landriver_s
{
	char		*name;
	qboolean	initialized;
	sys_socket_t		controlSock;
	sys_socket_t		(*Init) (void);
	void		(*Shutdown) (void);
	void		(*Listen) (qboolean state);
	sys_socket_t	(*OpenSocket) (int port);
	int 		(*CloseSocket) (sys_socket_t net_socket);
	sys_socket_t	(*CheckNewConnections) (void);
	int 		(*Read) (sys_socket_t net_socket, byte *buf, int len, struct qsockaddr *addr);
	int 		(*Write) (sys_socket_t net_socket, byte *buf, int len, struct qsockaddr *addr);
	int 		(*Broadcast) (sys_socket_t net_socket, byte *buf, int len);
	char *		(*AddrToString) (struct qsockaddr *addr);
	int 		(*StringToAddr) (char *string, struct qsockaddr *addr);
	int 		(*GetSocketAddr) (sys_socket_t net_socket, struct qsockaddr *addr);
	int 		(*GetNameFromAddr) (struct qsockaddr *addr, char *name);
	int 		(*GetAddrFromName) (char *name, struct qsockaddr *addr);
	int 		(*GetDefaultMTU) (void);
	int			(*AddrCompare) (struct qsockaddr *addr1, struct qsockaddr *addr2);
	int			(*GetSocketPort) (struct qsockaddr *addr);
	int			(*SetSocketPort) (struct qsockaddr *addr, int port);
} net_landriver_t;

#define	MAX_NET_DRIVERS		8
extern int 				net_numlandrivers;
extern net_landriver_t	net_landrivers[MAX_NET_DRIVERS];

typedef struct net_driver_s
{
	char		*name;
	qboolean	initialized;
	int			(*Init) (void);
	void		(*Listen) (qboolean state);
	void		(*SearchForHosts) (qboolean xmit);
	qsocket_t	*(*Connect) (char *host);
	qsocket_t 	*(*CheckNewConnections) (void);
	int			(*QGetMessage) (qsocket_t *sock);
	int			(*QSendMessage) (qsocket_t *sock, sizebuf_t *data);
	int			(*SendUnreliableMessage) (qsocket_t *sock, sizebuf_t *data);
	qboolean	(*CanSendMessage) (qsocket_t *sock);
	qboolean	(*CanSendUnreliableMessage) (qsocket_t *sock);
	void		(*Close) (qsocket_t *sock);
	void		(*Shutdown) (void);
	sys_socket_t		controlSock;
} net_driver_t;

extern int			net_numdrivers;
extern net_driver_t	net_drivers[MAX_NET_DRIVERS];

extern int			DEFAULTnet_hostport;
extern int			net_hostport;

extern net_driver_t *net_driver;
extern cvar_t		hostname;

extern int		messagesSent;
extern int		messagesReceived;
extern int		unreliableMessagesSent;
extern int		unreliableMessagesReceived;

qsocket_t *NET_NewQSocket(void);
void NET_FreeQSocket(qsocket_t *);
double SetNetTime(void);


#define HOSTCACHESIZE	8

typedef struct
{
	char				name[16];
	char				map[16];
	char				cname[32];
	int					users;
	int					maxusers;
	net_driver_t		*driver;
	net_landriver_t		*ldriver;
	struct qsockaddr	addr;
} hostcache_t;

extern int hostCacheCount;
extern hostcache_t hostcache[HOSTCACHESIZE];

//============================================================================
//
// public network functions
//
//============================================================================

extern	double		net_time;
extern	struct msg_s *net_message;
extern	int			net_activeconnections;

void		NET_Init (void);
void		NET_Shutdown (void);

struct qsocket_s	*NET_CheckNewConnections (void);
// returns a new connection number if there is one pending, else -1

struct qsocket_s	*NET_Connect (char *host);
// called by client to connect to a host.  Returns -1 if not able to

qboolean NET_CanSendMessage (qsocket_t *sock);
// Returns true or false if the given qsocket can currently accept a
// message to be transmitted.

int			NET_GetMessage (struct qsocket_s *sock);
// returns data in net_message sizebuf
// returns 0 if no data is waiting
// returns 1 if a message was received
// returns 2 if an unreliable message was received
// returns -1 if the connection died

int			NET_SendMessage (struct qsocket_s *sock, sizebuf_t *data);
int			NET_SendUnreliableMessage (struct qsocket_s *sock, sizebuf_t *data);
// returns 0 if the message connot be delivered reliably, but the connection
//		is still considered valid
// returns 1 if the message was sent properly
// returns -1 if the connection died

int			NET_SendToAll (sizebuf_t *data, int blocktime, qboolean nolocals);
// This is a reliable *blocking* send to all attached clients.


void		NET_Close (struct qsocket_s *sock);
// if a dead connection is returned by a get or send function, this function
// should be called when it is convenient

// Server calls when a client is kicked off for a game related misbehavior
// like an illegal protocal conversation.  Client calls when disconnecting
// from a server.
// A netcon_t number will not be reused until this function is called for it

void NET_Poll(void);


typedef struct _PollProcedure
{
	struct _PollProcedure	*next;
	double					nextTime;
	void					(*procedure)();
	void					*arg;
} PollProcedure;

void SchedulePollProcedure(PollProcedure *pp, double timeOffset);

extern	qboolean	tcpipAvailable;
extern	char		my_tcpip_address[NET_NAMELEN];

extern	qboolean	slistInProgress;
extern	qboolean	slistSilent;
extern	qboolean	slistLocal;

void NET_Slist_f (void);
