#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <of_iface/iface_api.h>

void 
dump_buffer(BYTE	*buf, UINT32	len32)
{
	UINT32	j = 0;

	for (j = 0; j < len32; j++) {
		/* convert to big endian format to be sure of byte order */
		printf("%02X ", buf[j]);
		if (++j % 60 == 59)
		{
			printf("\n");
		}
	}
	printf("\n");
}

int main (void)
{
	int socket_fd;
	BYTE recvbuffer[1530 + 500];
	int recvlen;
	int decodedlen;
	int id;
	socket_fd = initialise_server_socket("dump.sock");
	id = 0;
	while(1){
		if (fec_packet_recv(socket_fd, recvbuffer, &recvlen, &id) == 0)
		{
			memcpy(&decodedlen, recvbuffer, sizeof(UINT32));
			printf("Packet len: %d, Decoded len: %d --- Next ID to receive: %d\n", recvlen, decodedlen, id);
			if (decodedlen > recvlen || decodedlen <= 0){
				printf("Erroneous packet\n");
			}else{
				printf("Correct packet\n");
				dump_buffer(recvbuffer, decodedlen);
			}
		}
	}	
	exit(0);
	return 0;
}