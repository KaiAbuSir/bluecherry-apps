/*   pelcocmd.c
 *   Copyright 2011, Bluecherry (http://www.bluecherrydvr.com/)
 *
 *   Based on sendserial.c:
 *   Copyright 2004, Kenneth Lavrsen
 *
 *   This program is published under the GNU Public license
 */

#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h> 

#define STEPPER_BAUDRATE B2400

static void printUsage()
{
	printf("usage: pelcocmd [-a address] [-p panSpeed] [-t tiltSpeed] [-s stopTime] [-d /dev/ttyX] command\n");
	printf("commands: left right up down in out stop\n");
	printf("presets:  save go clear [1-20]\n");
	printf("extended: zeropan flip (may not be supported)\n");
}

static void pelcoChecksum(char data[7])
{
	int i = 0;
	data[6] = 0;
	for (i = 1; i < 6; ++i)
		data[6] += data[i];
}

int main(int argc, char *argv[])
{
	const char *device = "/dev/ttyUSB0";
	const char *command = 0;
	char data[7] = { 0xff, 0x01, 0x00, 0x00, 0x20, 0x20, 0x00 };
	int inputStopDelay = -1, stopDelay = -1;

	int opt;
	while ((opt = getopt(argc, argv, "p:t:d:a:s:")) >= 0)
	{
		switch (opt)
		{
		case 'p':
			data[4] = (char)atoi(optarg);
			break;
		case 't':
			data[5] = (char)atoi(optarg);
			break;
		case 'd':
			device = optarg;
			break;
		case 'a':
			data[1] = (char)atoi(optarg);
			break;
		case 's':
			inputStopDelay = atoi(optarg);
			break;
		default:
			printUsage();
			return 1;
		}
	}

	if (optind >= argc)
	{
		printUsage();
		return 1;
	}
	command = argv[optind++];

	if (strcmp(command, "stop") == 0)
		data[3] = 0;
	else if (strcmp(command, "right") == 0)
		data[3] = 1 << 1, stopDelay = inputStopDelay;
	else if (strcmp(command, "left") == 0)
		data[3] = 1 << 2, stopDelay = inputStopDelay;
	else if (strcmp(command, "up") == 0)
		data[3] = 1 << 3, stopDelay = inputStopDelay;
	else if (strcmp(command, "down") == 0)
		data[3] = 1 << 4, stopDelay = inputStopDelay;
	else if (strcmp(command, "in") == 0)
		data[3] = 1 << 5, stopDelay = inputStopDelay;
	else if (strcmp(command, "out") == 0)
		data[3] = 1 << 6, stopDelay = inputStopDelay;
	else if (strcmp(command, "zeropan") == 0)
		data[3] = 0x07, data[4] = 0, data[5] = 0x22;
	else if (strcmp(command, "flip") == 0)
		data[3] = 0x07, data[4] = 0, data[5] = 0x21;
	else if (strcmp(command, "save") == 0 || strcmp(command, "go") == 0 || strcmp(command, "clear") == 0)
	{
		int preset = 0;
		if (optind >= argc || (preset = atoi(argv[optind++])) < 1 || preset > 20)
		{
			printf("specify preset number, between 1 and 20\n");
			return 1;
		}
		data[4] = 0;
		data[5] = preset;

		if (command[0] == 's') data[3] = 0x03;
		else if (command[0] == 'c') data[3] = 0x05;
		else if (command[0] == 'g') data[3] = 0x07;
	}
	else
	{
		printf("unknown command '%s'\n", command);
		return 1;
	}

	pelcoChecksum(data);

	int serdevice;
	if ((serdevice = open(device, O_RDWR | O_NOCTTY)) < 0)
	{
		perror("Unable to open serial device");
		return 1;
	}

	struct termios adtio;
	memset (&adtio, 0, sizeof(adtio));
	adtio.c_cflag= STEPPER_BAUDRATE | CS8 | CLOCAL | CREAD;
	adtio.c_iflag= IGNPAR;
	adtio.c_oflag= 0;
	adtio.c_lflag= 0;   /* non-canon, no echo */
	adtio.c_cc[VTIME]=0;   /* timer unused */
	adtio.c_cc[VMIN]=0;   /* blocking read until 1 char */
	tcflush (serdevice, TCIFLUSH);
	if (tcsetattr(serdevice, TCSANOW, &adtio) < 0)
	{
		perror("Unable to initialize serial device");
		return 1;
	}

	if (write(serdevice, data, sizeof(data)) < sizeof(data))
		perror("Write failed");

	if (stopDelay >= 0)
	{
		usleep(stopDelay*1000);
		data[2] = data[3] = data[6] = 0;
		pelcoChecksum(data);
		if (write(serdevice, data, sizeof(data)) < sizeof(data))
			perror("Write failed");
	}

	close(serdevice);
	return 0;
}
