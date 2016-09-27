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
#define CODE_RATE	0.5 	/* k/n = 2/3 means we add 50% of repair symbols */
#define LOSS_RATE	0.0		/* we consider 30% of packet losses... It assumes there's no additional loss during UDP transmissions */

#define VERBOSITY 	2

#define OF_OVERHEAD	3

static int
give_packet(int socket_fd, BYTE ** pkt, int len)
{
	int i = 0;
	if (rand()%100 > 20){
		/* 100 milliseconds between writings */
		usleep(1 * 1000);
		write(socket_fd, *pkt, len);
	}
	return 0;
}

static void 
randomize_array (UINT32		**array, UINT32		arrayLen)
{
	UINT32		backup	= 0;
	UINT32		randInd	= 0;
	UINT32		seed;		/* random seed for the srand() function */
	UINT32		i;

	struct timeval	tv;
	if (gettimeofday(&tv, NULL) < 0) {
		OF_PRINT_ERROR(("gettimeofday() failed"))
		exit(-1);
	}
	seed = (int)tv.tv_usec;
	srand(seed);
	for (i = 0; i < arrayLen; i++)
	{
		(*array)[i] = i;
	}
	for (i = 0; i < arrayLen; i++)
	{
		backup = (*array)[i];
		randInd = rand()%arrayLen;
		(*array)[i] = (*array)[randInd];
		(*array)[randInd] = backup;
	}
}


int 
fec_packet_send(int socket_fd, BYTE * p, int len, int id)
{
	of_codec_id_t	codec_id;				/* identifier of the codec to use */
	of_session_t	*ses 		= NULL;			/* openfec codec instance identifier */
	of_parameters_t	*params		= NULL;			/* structure used to initialize the openfec session */
	void**		enc_symbols_tab	= NULL;			/* table containing pointers to the encoding (i.e. source + repair) symbols buffers */
	UINT16		k;					/* number of source symbols in the block */
	UINT16		n;					/* number of encoding symbols (i.e. source + repair) in the block */
	UINT16		esi;					/* Encoding Symbol ID, used to identify each encoding symbol */
	UINT32		i;
	UINT32*		rand_order	= NULL;			/* table used to determine a random transmission order. This randomization process
								 * is essential for LDPC-Staircase optimal performance */
	BYTE		*pkt_with_fpi	= NULL;			/* buffer containing a fixed size packet plus a header consisting only of the FPI */
	UINT32		ret		= -1;
	UINT32		last_packet_size;

	k = (UINT32) floor((double)len / (double)SYMBOL_SIZE);
	last_packet_size = len % SYMBOL_SIZE;
	if (last_packet_size = (len % SYMBOL_SIZE), last_packet_size != 0) 
	{
		k = k + 1;
	}
	n = (UINT32)floor((double)k / (double)CODE_RATE);

	/* fill in the code specific part of the of_..._parameters_t structure */
	of_rs_2_m_parameters_t	*my_params;

	printf("\nInitialize a Reed-Solomon over GF(2^m) codec instance, (n, k)=(%u, %u)...\n", n, k);
	codec_id = OF_CODEC_REED_SOLOMON_GF_2_M_STABLE;
	if ((my_params = (of_rs_2_m_parameters_t *)calloc(1, sizeof(* my_params))) == NULL)
	{
		OF_PRINT_ERROR(("no memory for codec %d\n", codec_id))
		ret = -1;
		goto end;
	}
	my_params->m = 8;
	if ((double) n > pow(2, my_params->m))
	{
		OF_PRINT_ERROR(("Choose greater M %d\n", codec_id))
		ret = -1;
		goto end;
	}
	params = (of_parameters_t *) my_params;
	params->nb_source_symbols	= k;		/* fill in the generic part of the of_parameters_t structure */
	params->nb_repair_symbols	= n - k;
	params->encoding_symbol_length	= SYMBOL_SIZE;
	/* Open and initialize the openfec session now... */
	if ((ret = of_create_codec_instance(&ses, codec_id, OF_ENCODER, VERBOSITY)) != OF_STATUS_OK)
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
	/* Initialize a set of pointers */
	if ((enc_symbols_tab = (void**) calloc(n, sizeof(void*))) == NULL) {
		OF_PRINT_ERROR(("no memory (calloc failed for enc_symbols_tab, n=%u)\n", n))
		ret = -1;
		goto end;
	}
	for (esi = 0; esi < k; esi++ )
	{
		/* Copy the blocks */
		if ((enc_symbols_tab[esi] = calloc(1, SYMBOL_SIZE)) == NULL)
		{
			OF_PRINT_ERROR(("no memory (calloc failed for enc_symbols_tab[%d])\n", esi))
			ret = -1;
			goto end;
		}
		if (esi < k - 1){
			memcpy(enc_symbols_tab[esi], p + (esi * SYMBOL_SIZE), SYMBOL_SIZE);
		}else{
			if (last_packet_size != 0){
				printf("\nCopying last packet (part): %d\n", len%SYMBOL_SIZE);
				memcpy(enc_symbols_tab[esi], p + (esi * SYMBOL_SIZE), (UINT32)(len % SYMBOL_SIZE));
			}else{
				printf("\nCopying last packet (full): %d\n", SYMBOL_SIZE);
				memcpy(enc_symbols_tab[esi], p + (esi * SYMBOL_SIZE), SYMBOL_SIZE);
			}
		}
		/* memset(enc_symbols_tab[esi], (char)(esi + 1), SYMBOL_SIZE); */
		/* instead of memset, memcpy */
	}
	for (esi = k; esi < n; esi++)
	{
		if ((enc_symbols_tab[esi] = (char*)calloc(1, SYMBOL_SIZE)) == NULL)
		{
			OF_PRINT_ERROR(("no memory (calloc failed for enc_symbols_tab[%d])\n", esi))
			ret = -1;
			goto end;
		}
		if (of_build_repair_symbol(ses, enc_symbols_tab, esi) != OF_STATUS_OK) {
			OF_PRINT_ERROR(("ERROR: of_build_repair_symbol() failed for esi=%u\n", esi))
			ret = -1;
			goto end;
		}
	}	
	if ((rand_order = (UINT32*)calloc(n, sizeof(UINT32))) == NULL)
	{
		OF_PRINT_ERROR(("no memory (calloc failed for rand_order)\n"))
		ret = -1;
		goto end;
	}
	randomize_array(&rand_order, n);
	if ((pkt_with_fpi = malloc(OF_OVERHEAD + SYMBOL_SIZE)) == NULL)
	{
		OF_PRINT_ERROR(("no memory (malloc failed for pkt_with_fpi)\n"))
		ret = -1;
		goto end;
	}

	BYTE htons_n = (BYTE) n;
	BYTE htons_k = (BYTE) k;
	for (i = 0; i < n; i++)
	{
		/* Add a pkt header wich only countains the ESI, i.e. a 32bits sequence number, in network byte order in order
		 * to be portable regardless of the local and remote byte endian representation (the receiver will do the
		 * opposite with ntohl()...) */
		/* n, k and esi can be just 1 byte each */
		/* ID an other headers can take 1 byte too */
		/* The goal is to be divisible by 4, thus: 252, 248, 244... */
		/* In our case, or we go down to 248 and we add headers up to 255 (or up to 252, does not matter) */
		/* Or we use 252, using 3 bytes header -> 1 byte for N and 1 byte for K, 1 byte for ESI(7 bits) and 1 bit for ID */
		*(UINT8*)pkt_with_fpi 	= (BYTE)( ((rand_order[i]) & 0x7F) | (id&0x01)<<7 );
		memcpy(1 + pkt_with_fpi, &htons_n, sizeof(BYTE));
		memcpy(2 + pkt_with_fpi, &htons_k, sizeof(BYTE));
		memcpy(OF_OVERHEAD + pkt_with_fpi, enc_symbols_tab[rand_order[i]], SYMBOL_SIZE);
		printf("%02d => sending symbol %u (%s)\n", i + 1, rand_order[i], (rand_order[i] < k) ? "src" : "repair");
		/* New send each pkt_with_fpi */
		if (give_packet(socket_fd, &pkt_with_fpi, SYMBOL_SIZE + OF_OVERHEAD) != 0)
		{
			ret = -1;
			goto end;
		}
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
	if (rand_order)
	{
		free(rand_order);
	}
	if (enc_symbols_tab)
	{
		for (esi = 0; esi < n; esi++)
		{
			if (enc_symbols_tab[esi])
			{
				free(enc_symbols_tab[esi]);
			}
		}
		free(enc_symbols_tab);
	}
	if (pkt_with_fpi)
	{
		free(pkt_with_fpi);
	}
	return ret;
}
