#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <stdlib.h>

void mapearDados(size_t file_size, int fd_in){
 uint8_t *data = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd_in, 0);
    
    if (data == MAP_FAILED) {
        perror("Erro ao mapear o arquivo");
        close(fd_in);
        return EXIT_FAILURE;
    }
}