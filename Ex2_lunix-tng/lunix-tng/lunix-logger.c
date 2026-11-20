/*
 * lunix-logger.c
 * 
 * Userspace application that periodically reads measurements
 * from a Lunix:TNG sensor device, displays them with a timestamp,
 * and logs them to a file.
 *
 * Usage:
 *   ./lunix-logger [/dev/lunix0-temp] [measurements.log]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

static volatile sig_atomic_t running = 1;

void sig_handler(int signum)
{
	(void)signum;
	running = 0;
}

int main(int argc, char *argv[])
{
	const char *dev_path = (argc > 1) ? argv[1] : "/dev/lunix0-temp";
	const char *log_path = (argc > 2) ? argv[2] : "measurements.log";
	
	int dev_fd;
	FILE *log_fp;
	char buf[64];
	ssize_t bytes_read;

	/* Setup signal handling for clean exit on Ctrl+C */
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	printf("[*] Opening sensor device: %s\n", dev_path);
	dev_fd = open(dev_path, O_RDONLY);
	if (dev_fd < 0) {
		perror("[-] Error opening device");
		return 1;
	}

	printf("[*] Opening log file: %s\n", log_path);
	log_fp = fopen(log_path, "a");
	if (!log_fp) {
		perror("[-] Error opening log file");
		close(dev_fd);
		return 1;
	}

	printf("[*] Logging started. Press Ctrl+C to stop.\n");
	printf("--------------------------------------------------\n");

	while (running) {
		/*
		 * read() blocks inside the kernel driver until a new
		 * measurement is pushed by the sensor (auto-rewind mode)
		 */
		bytes_read = read(dev_fd, buf, sizeof(buf) - 1);
		if (bytes_read < 0) {
			if (errno == EINTR) {
				/* Interrupted by signal (Ctrl+C) */
				break;
			}
			perror("[-] read() error");
			break;
		}

		if (bytes_read > 0) {
			buf[bytes_read] = '\0';

			/* Strip trailing newline for clean formatting */
			char *nl = strchr(buf, '\n');
			if (nl) *nl = '\0';

			/* Get current timestamp */
			time_t now = time(NULL);
			struct tm *t = localtime(&now);
			char time_str[32];
			strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

			/* Display on stdout */
			printf("[%s] %s: %s\n", time_str, dev_path, buf);

			/* Write to log file and flush immediately */
			fprintf(log_fp, "[%s] %s: %s\n", time_str, dev_path, buf);
			fflush(log_fp);
		}
	}

	printf("\n[*] Stopping logger and cleaning up...\n");
	fclose(log_fp);
	close(dev_fd);
	printf("[*] Done. Log saved in %s\n", log_path);

	return 0;
}
