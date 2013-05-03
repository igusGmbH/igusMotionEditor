#include "protocol.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
	proto::SimplePacket<proto::CMD_INIT> init;
	proto::Packet<proto::CMD_RESET, proto::Reset> reset;
	memcpy(&reset.payload.key, proto::RESET_KEY, sizeof(proto::RESET_KEY));

	int fd = open(argv[1], O_RDWR);

	uint8_t i;
	for(i = 0; i <= proto::VERSION+10; ++i)
	{
		init.header.version = i;
		init.checksum = proto::packetChecksum(init.header, 0);

		write(fd, &init, sizeof(init));

		struct timeval timeout;
		timeout.tv_usec = 200 * 1000;
		timeout.tv_sec = 0;

		fd_set fds;
		FD_SET(fd, &fds);
		if(select(fd+1, &fds, NULL, NULL, &timeout) > 0)
		{
			printf("µC has protocol version %d\n", i);
			break;
		}
	}


	reset.header.version = i;
	reset.updateChecksum();

	write(fd, &reset, sizeof(reset));

	return 0;
}
