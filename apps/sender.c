#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <of_iface/iface_api.h>

/* Creates a 1530 bytes packet 			*/
/* Composed by headers 					*/
/* Send towards the phy layer splitted  */

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

int 
create_packet(BYTE * p, int * len)
{
	int i;
	UINT32 size = *len;
	memcpy(p, &size, sizeof(UINT32));
	for (i = 0; i < 26; i++) p[i + 4] = i;
	for (i = 0; i < size - 30; i++) p[i + 30] = rand()%256;
	return 0;
}

int main (void)
{
	int socket_fd;
	BYTE buffer[1530];
	int len = 1530;
	socket_fd = initialise_client_socket("dump.sock");
	int i = 0;
	while(1){
		create_packet(buffer, &len);
		dump_buffer(buffer, len);
		fec_packet_send(socket_fd, buffer, len, i);
		if (i == 0 /*&& (rand()%2) == 0*/){
			i = 1;
		}else{
			i = 0;
		}
		/* 1 packet each 3 seconds */
		usleep(10*1000);
	}
	exit(0);
	return 0;
}