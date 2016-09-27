#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h> 
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>	/* for gettimeofday */

#include <of_iface/iface_api.h>
#include <openfec/of_openfec_api.h>

#define SYMBOL_SIZE	252		/* symbol size, in bytes (must be multiple of 4 in this simple example) */
#define	DEFAULT_K	100		/* default k value */
#define CODE_RATE	0.5		/* k/n = 2/3 means we add 50% of repair symbols */
#define LOSS_RATE	0.0		/* we consider 30% of packet losses... It assumes there's no additional loss during UDP transmissions */

#define VERBOSITY 	2

#define OF_OVERHEAD 3

/* Creates a 1530 bytes packet 			*/
/* Composed by headers 					*/
/* Send towards the phy layer splitted  */

static int 
input_timeout (int filedes, unsigned int seconds, unsigned int microseconds)
{
  fd_set set;
  struct timeval timeout;

  /* Initialize the file descriptor set. */
  FD_ZERO (&set);
  FD_SET (filedes, &set);

  /* Initialize the timeout data structure. */
  timeout.tv_sec = seconds;
  timeout.tv_usec = microseconds;

  /* select returns 0 if timeout, 1 if input available, -1 if error. */
  return (select (FD_SETSIZE, &set, NULL, NULL, &timeout));
}

static int get_packet(int socket_fd, BYTE ** pkt, int * len)
{
	int readed = 0;
	int ret;
	/* I.E. read form file or socket */
	INT32		saved_len = *len;	/* save it, in case we need to do several calls to recvfrom */
	/* Timeout of 500 ms */
	if (ret = input_timeout(socket_fd, 0, ms(500)), ret > 0)
	{
		readed = read(socket_fd, *pkt, saved_len);
		if (readed <= 0)
		{
			printf("Error reading\n");
			exit(-1);
		}
	}
	else if (ret == -1)
	{
		printf("Error in select\n");
		exit(-1);
	}
	else
	{
		return OF_STATUS_ERROR;
	}
	return OF_STATUS_OK;
}

#define MAX_N	40
#define MAX_K 	20

int 
fec_packet_recv(int socket_fd, BYTE * buffer, int * packet_len, int * expected_id)
{
	of_codec_id_t	codec_id;					/* identifier of the codec to use */
	of_session_t	*ses 		= NULL;			/* openfec codec instance identifier */
	of_parameters_t	*params		= NULL;			/* structure used to initialize the openfec session */
	void		**src_symbols_tab= NULL;		/* table containing pointers to the source symbol buffers (no FPI here) */
	UINT16		k;								/* number of source symbols in the block */
	UINT16		n;								/* number of encoding symbols (i.e. source + repair) in the block */
	UINT16		esi;							/* Encoding Symbol ID, used to identify each encoding symbol */
	BYTE		*pkt_with_fpi	= NULL;			/* pointer to a buffer containing the FPI followed by the fixed size packet */
	INT32		len;							/* len of the received packet */
	UINT32		n_received	= 0;				/* number of symbols (source or repair) received so far */
	bool		done		= false;			/* true as soon as all source symbols have been received or recovered */
	UINT32		ret;
	UINT32		first_pkt;
	UINT8 		htons_k, htons_n;
	UINT8 		start_id;
	UINT8 		recv_id;
	BYTE 		recv_symbols_tab[MAX_N][SYMBOL_SIZE];

	len = SYMBOL_SIZE + OF_OVERHEAD;
	first_pkt = 1;
	if ((pkt_with_fpi = malloc(len)) == NULL)
	{
		ret = -1;
		goto end;
	}
	while ((ret = get_packet(socket_fd, &pkt_with_fpi, &len)) == OF_STATUS_OK)
	{
		n_received++;
		if (first_pkt)
		{
			start_id = ((*(UINT8*)pkt_with_fpi)>>7) & 0x01;
			/* First of all, check IDS */
			if (start_id != *expected_id)
			{
				ret = -1;
				goto end;
			}
			else
			{
				*expected_id = (start_id + 1) % 2;
			}
			memcpy(&htons_n, pkt_with_fpi + 1, sizeof(BYTE));
			memcpy(&htons_k, pkt_with_fpi + 2, sizeof(BYTE));
			n = (unsigned short) htons_n;
			k = (unsigned short) htons_k;

			of_rs_2_m_parameters_t	*my_params;
			codec_id = OF_CODEC_REED_SOLOMON_GF_2_M_STABLE;
			printf("First packet arrived, initialize a Reed-Solomon over GF(2^m) codec instance, (n, k)=(%u, %u)...\n", n, k);
			if ((my_params = (of_rs_2_m_parameters_t *)calloc(1, sizeof(* my_params))) == NULL)
			{
				OF_PRINT_ERROR(("no memory for codec %d\n", codec_id))
				ret = -1;
				goto end;
			}
			my_params->m = 8;
			params = (of_parameters_t *) my_params;	
			first_pkt = 0;
			params->nb_source_symbols	= k;		/* fill in the generic part of the of_parameters_t structure */
			params->nb_repair_symbols	= n - k;
			params->encoding_symbol_length	= SYMBOL_SIZE;
			/* Open and initialize the openfec decoding session now that we know the various parameters used by the sender/encoder... */
			if ((ret = of_create_codec_instance(&ses, codec_id, OF_DECODER, VERBOSITY)) != OF_STATUS_OK)
			{
				OF_PRINT_ERROR(("of_create_codec_instance() failed\n"))
				ret = -1;
				goto end;
			}
			if (of_set_fec_parameters(ses, params) != OF_STATUS_OK)
			{
				OF_PRINT_ERROR(("of_set_fec_parameters() failed for codec_id %d\n", codec_id))
				ret = -1;
				goto end;
			}
			/* allocate a table for the received encoding symbol buffers. We'll update it progressively */
			if ((src_symbols_tab = (void**) calloc(n, sizeof(void*))) == NULL)
			{
				OF_PRINT_ERROR(("no memory (calloc failed for enc_symbols_tab, n=%u)\n", n))
				ret = -1;
				goto end;
			}
		}
		recv_id = ((*(UINT8*)pkt_with_fpi)>>7) & 0x01;
		if (recv_id != start_id){
			printf("Packet ids are not the same from the first packet (and following ones)\n");
			ret = -1;
			goto end;
		}
		/* OK, new packet received... */
		esi = (*(UINT8*)pkt_with_fpi) & 0x7F;
		if (esi > n)		/* a sanity check, in case... */
		{
			OF_PRINT_ERROR(("invalid esi=%u received in a packet's FPI\n", esi))
			ret = -1;
			goto end;
		}
		/* Copy the pkt to recv_symbol matrix */
		memcpy(&recv_symbols_tab[esi][0], (BYTE*) pkt_with_fpi + OF_OVERHEAD, SYMBOL_SIZE);
		printf("recv_symbols_tab[%d]: %p\n", esi, &recv_symbols_tab[esi][0]);
		//printf("%02d => receiving symbol esi=%u (%s)\n", n_received, esi, (esi < k) ? "src" : "repair");
		/* Give the symbol without the header */
		/* send symbol recv_symbol[esi] to of_decode_ */
		if (of_decode_with_new_symbol(ses, recv_symbols_tab[esi], esi) == OF_STATUS_ERROR)
		{
			OF_PRINT_ERROR(("of_decode_with_new_symbol() failed\n"))
			ret = -1;
			goto end;
		}
		/* check if completed in case we received k packets or more */
		if ((n_received >= k) && (of_is_decoding_complete(ses) == true)) 
		{
			/* done, we recovered everything, no need to continue reception */
			done = true;
			break;
		}
		len = SYMBOL_SIZE + OF_OVERHEAD;	/* make sure len contains the size of the expected packet */
	}
	/* For REED Solomon schemes, we just need K symbols */
	if (done && n_received == k)
	{
		/* finally, get a copy of the pointers to all the source symbols, those received (that we already know) and those decoded.
		 * In case of received symbols, the library does not change the pointers (same value). */
		if (of_get_source_symbols_tab(ses, src_symbols_tab) != OF_STATUS_OK)
		{
			OF_PRINT_ERROR(("of_get_source_symbols_tab() failed\n"))
			ret = -1;
			goto end;
		}
		printf("All source symbols rebuilt after receiving %u packets\n", n_received);
		*packet_len = 0;
		for (esi = 0; esi < k; esi++) 
		{
		 	memcpy(buffer + (esi * SYMBOL_SIZE), src_symbols_tab[esi], SYMBOL_SIZE);
		 	*packet_len += SYMBOL_SIZE;
		}
	}
	else
	{
		printf("\nFailed to recover all erased source symbols after receiving %u packets\n", n_received);
		ret = -1;
		goto end;
	}
	ret = 0;
	end:
	if (ses)
	{
		of_release_codec_instance(ses);
	}
	if (params)
	{
		free(params);
	}
	if (pkt_with_fpi)
	{
		free(pkt_with_fpi);
	}
	if (src_symbols_tab)
	{
		free(src_symbols_tab);
	}
	return ret;
}