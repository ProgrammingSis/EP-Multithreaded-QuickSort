#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "color.c"

#define DATA_SIZE 97


// Estrutura para passar parâmetros para as threads
typedef struct {
    uint8_t* data;   // Ponteiro para os dados
    size_t start;    // Índice inicial
    size_t end;      // Índice final
    size_t record_size; // Tamanho de cada registro
} ThreadArgs;

// Função de troca para o quicksort
void swap(uint8_t *a, uint8_t *b, size_t size) {
    uint8_t temp[size];
    memcpy(temp, a, size);
    memcpy(a, b, size);
    memcpy(b, temp, size);
}

// Particionamento do quicksort
size_t partition(uint8_t *data, size_t start, size_t end, size_t record_size) {
    uint8_t *pivot = data + end * record_size;
    size_t i = start;

    for (size_t j = start; j < end; j++) {
        if (*(int32_t *)(data + j * record_size) <= *(int32_t *)pivot) {
            swap(data + i * record_size, data + j * record_size, record_size);
            i++;
        }
    }
    swap(data + i * record_size, data + end * record_size, record_size);
    return i;
}

// Implementação do quicksort
void quicksort(uint8_t *data, size_t start, size_t end, size_t record_size) {
    if (start < end) {
        size_t pivot = partition(data, start, end, record_size);
        quicksort(data, start, pivot - 1, record_size);
        quicksort(data, pivot + 1, end, record_size);
    }
}

// Função executada por cada thread
void *thread_quicksort(void *args) {
    ThreadArgs *thread_args = (ThreadArgs *)args;
    quicksort(thread_args->data, thread_args->start, thread_args->end - 1, thread_args->record_size);
    return NULL;
}

// Função de intercalamento
void merge_sections(uint8_t *data, size_t start, size_t middle, size_t end, size_t record_size) {
    size_t left_size = middle - start;
    size_t right_size = end - middle;

    uint8_t *left = malloc(left_size * record_size);
    uint8_t *right = malloc(right_size * record_size);

    memcpy(left, data + start * record_size, left_size * record_size);
    memcpy(right, data + middle * record_size, right_size * record_size);

    size_t i = 0, j = 0, k = start;

    while (i < left_size && j < right_size) {
        if (*(int32_t *)(left + i * record_size) <= *(int32_t *)(right + j * record_size)) {
            memcpy(data + k * record_size, left + i * record_size, record_size);
            i++;
        } else {
            memcpy(data + k * record_size, right + j * record_size, record_size);
            j++;
        }
        k++;
    }

    while (i < left_size) {
        memcpy(data + k * record_size, left + i * record_size, record_size);
        i++;
        k++;
    }

    while (j < right_size) {
        memcpy(data + k * record_size, right + j * record_size, record_size);
        j++;
        k++;
    }

    free(left);
    free(right);
}

// Função principal
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <entrada> <saída> <threads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];
    
    int num_threads = atoi(argv[3]);
    printf(CYAN "\n Num_threads: [%i]\n\n" RESET, num_threads);

    if (num_threads <= 0) {
        num_threads = sysconf(_SC_NPROCESSORS_ONLN); // Obter número de CPUs disponíveis
    }

    // Abrir o arquivo de entrada
    int fd_in = open(input_file, O_RDONLY);

    if (fd_in < 0) {
        perror("Erro ao abrir o arquivo de entrada");
        return EXIT_FAILURE;
    }

    struct stat sb;
    if (fstat(fd_in, &sb) < 0) {
        perror("Erro ao obter informações do arquivo");
        close(fd_in);
        return EXIT_FAILURE;
    }

    size_t file_size = sb.st_size;
    size_t record_size = 100;
    size_t num_records = file_size / record_size;

    // Mapear o arquivo de entrada na memória
    uint8_t *data = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd_in, 0);
    
    if (data == MAP_FAILED) {
        perror("Erro ao mapear o arquivo");
        close(fd_in);
        return EXIT_FAILURE;
    }

    close(fd_in);

    // Criar threads
    pthread_t threads[num_threads];
    ThreadArgs args[num_threads];
    size_t records_per_thread = num_records / num_threads;

    for (int i = 0; i < num_threads; i++) {
        args[i].data = data;
        args[i].start = i * records_per_thread;
        args[i].end = (i == num_threads - 1) ? num_records : (i + 1) * records_per_thread;
        args[i].record_size = record_size;
        pthread_create(&threads[i], NULL, thread_quicksort, &args[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Intercalação
    for (int i = 1; i < num_threads; i++) {
        merge_sections(data, 0, args[i].start, args[i].end, record_size);
    }

    // Escrever no arquivo de saída
    int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd_out < 0) {
        perror("Erro ao abrir o arquivo de saída");
        munmap(data, file_size);
        return EXIT_FAILURE;
    }

    printf("Escrevendo no arquivo de saída...\n\n");

    for (size_t i = 0; i < num_records; i++) {
    printf("Registro %zu: \n", i);
    printf(MAGENTA  "Chave = %d " RESET, *(int32_t *)(data + i * record_size));
    printf("Dados: ");
    for (size_t j = 4; j < record_size; j++) { // Começa após os 4 bytes da chave
        printf("%02x ", *(data + i * record_size + j));
    }
    printf("\n\n");
}

    write(fd_out, data, file_size);
    fsync(fd_out);
    close(fd_out);

    // Desmapear o arquivo
    munmap(data, file_size);

    return EXIT_SUCCESS;
}
