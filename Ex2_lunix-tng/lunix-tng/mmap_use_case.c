#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "lunix.h"

int main()
{
    int fd = open("/dev/lunix0-temp", O_RDONLY);
    
    struct lunix_msr_data_struct *msr = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
    if (msr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    printf("Magic: 0x%08X\n", msr->magic);
    
    uint32_t last_timestamp = 0;
    while (1) {
        if (msr->last_update != last_timestamp) {
            last_timestamp = msr->last_update;
            printf(" %u (Timestamp: %u)\n", msr->values[0], msr->last_update);
        }
        usleep(50000);
    }

    munmap(msr, 4096);
    
    close(fd);

    return 0;
}