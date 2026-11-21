#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "lunix-chrdev.h"
#include "lunix-lookup.h"

static int raw_to_milli(const char *kind, uint16_t raw, long *milli)
{
	if (!strcmp(kind, "temp")) {
		*milli = lookup_temperature[raw];
	} else if (!strcmp(kind, "batt")) {
		*milli = lookup_voltage[raw];
	} else if (!strcmp(kind, "light")) {
		*milli = lookup_light[raw];
	} else {
		return -1;
	}
	return 0;
}

static void format_milli(long milli, char *buf, size_t size)
{
	unsigned long magnitude = milli < 0 ? (unsigned long)-milli : (unsigned long)milli;
	snprintf(buf, size, "%s%lu.%03lu", milli < 0 ? "-" : "", magnitude / 1000, magnitude % 1000);
}

static const char *measurement_kind(const char *device)
{
	const char *base = strrchr(device, '/');
	const char *dash;

	base = base ? base + 1 : device;
	dash = strrchr(base, '-');
	return dash ? dash + 1 : "";
}

int main(int argc, char **argv)
{
	const char *device = argc > 1 ? argv[1] : "/dev/lunix0-temp";
	const char *kind = measurement_kind(device);
	char cooked_buf[LUNIX_CHRDEV_BUFSZ] = {0};
	int fd;

	if (argc > 2) {
		fprintf(stderr, "Usage: %s [device-node]\n", argv[0]);
		return 2;
	}
	if (access(device, F_OK) < 0) {
		fprintf(stderr, "Device node not found: %s\n", device);
		return 1;
	}
	if (strcmp(kind, "temp") && strcmp(kind, "batt") && strcmp(kind, "light")) {
		fprintf(stderr, "Cannot infer measurement type from '%s'; use a *-temp, *-batt, or *-light node.\n", device);
		return 1;
	}

	fd = open(device, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s: %s\n", device, strerror(errno));
		return 1;
	}
	printf("Monitoring %s; press Ctrl+C to stop.\n", device);
	for (;;) {
		uint16_t raw;
		long milli;
		char converted[32];
		char timestamp[32];
		time_t now = time(NULL);
		struct tm local;

		memset(cooked_buf, 0, sizeof(cooked_buf));
		if (ioctl(fd, LUNIX_IOC_GET_COOKED, cooked_buf) < 0) {
			fprintf(stderr, "GET_COOKED ioctl failed: %s\n", strerror(errno));
			close(fd);
			return 1;
		}
		if (ioctl(fd, LUNIX_IOC_GET_RAW, &raw) < 0) {
			fprintf(stderr, "GET_RAW ioctl failed: %s\n", strerror(errno));
			close(fd);
			return 1;
		}
		if (raw_to_milli(kind, raw, &milli) < 0) {
			close(fd);
			return 1;
		}
		format_milli(milli, converted, sizeof(converted));
		if (localtime_r(&now, &local) == NULL ||
		    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &local) == 0)
			strcpy(timestamp, "unknown time");

		printf("[%s] cooked ioctl: %s", timestamp, cooked_buf);
		printf("[%s] raw ioctl:    %u (", timestamp, raw);
		for (int bit = 15; bit >= 0; bit--)
			putchar((raw & (1u << bit)) ? '1' : '0');
		puts(")");
		printf("[%s] raw -> cooked: %s\n\n", timestamp, converted);
		fflush(stdout);
	}
}
