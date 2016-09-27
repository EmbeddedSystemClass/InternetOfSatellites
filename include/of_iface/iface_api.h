#ifndef __IFACE_API_H__
#define __IFACE_API_H__

#include <stdint.h>
#include <openfec/of_openfec_api.h>

#define BYTE unsigned char 
#define ms(x) x * 1000

#ifndef UINT32
#define		INT8		char
#define		INT16		short
#define		UINT8		unsigned char
#define		UINT16		unsigned short

#if defined(__LP64__) || (__WORDSIZE == 64) /* 64 bit architectures */
#define		INT32		int		/* yes, it's also true in LP64! */
#define		UINT32		unsigned int	/* yes, it's also true in LP64! */

#else  /* 32 bit architectures */

#define		INT32		int		/* int creates fewer compilations pbs than long */
#define		UINT32		unsigned int	/* int creates fewer compilations pbs than long */
#endif /* 32/64 architectures */

#endif /* !UINT32 */

#ifndef UINT64
#ifdef WIN32
#define		INT64		__int64
#define		UINT64		__uint64
#else  /* UNIX */
#define		INT64		long long
#define		UINT64		unsigned long long
#endif /* OS */
#endif /* !UINT64 */


int initialise_client_socket(char * socket_path);
int initialise_server_socket(char * socket_path);
int fec_packet_send(int socket_fd, BYTE * p, int len, int id);
int fec_packet_recv(int socket_fd, BYTE * buffer, int * packet_len, int * expected_id);

#endif