#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>

#include <of_iface/iface_api.h>

int 
input_timeout (int filedes, unsigned int microseconds){
	fd_set set;
	struct timeval timeout;
	/* Initialize the file descriptor set. */
	FD_ZERO (&set);
	FD_SET (filedes, &set);

	/* Initialize the timeout data structure. */
	timeout.tv_sec = 0;
	timeout.tv_usec = microseconds;

	/* select returns 0 if timeout, 1 if input available, -1 if error. */
	return (select (FD_SETSIZE, &set, NULL, NULL, &timeout));
}

void 
dump_buffer(BYTE	*buf, UINT32	len32)
{
	UINT32	j = 0;

	for (j = 0; j < len32; j++) 
	{
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
	int to_net = open("/dev/tun1", O_RDWR);
	system("ifconfig tun1 192.168.2.3 192.168.2.2 up");
	int socket_fd;
	BYTE recvbuffer[1530 + 500];
	int recvlen;
	int decodedlen;
	int id;
	socket_fd = initialise_server_socket("dump.sock");
	id = 0;
	int i = 0;
	int ret;
	int rv;
	while(1){
		if (input_timeout(socket_fd, 1000) > 0)
		{
			printf("Something to read\n");
			if (fec_packet_recv(socket_fd, recvbuffer, &recvlen, &id) == 0)
			{
				memcpy(&decodedlen, recvbuffer, sizeof(UINT32));
				printf("Packet len: %d, Decoded len: %d --- Next ID to receive: %d\n", recvlen, decodedlen, id);
				if (decodedlen > recvlen || decodedlen <= 0){
					printf("Erroneous packet\n");
				}else{
					write(to_net, recvbuffer + 4, decodedlen);
				}
			}
		}
		if (input_timeout(to_net, 1000) > 0)
		{
			ret = read(to_net, recvbuffer+4, 1500);
			printf("Readed from Tunnel 1 %d bytes\n", ret);
			if (ret > 0)
			{
				memcpy(recvbuffer, &ret, sizeof(UINT32));
				fec_packet_send(socket_fd, recvbuffer, ret + sizeof(UINT32), i);
				i = (i+1)%2;
			}
			printf("Sent\n");
		}
	}	
	exit(0);
	return 0;
}