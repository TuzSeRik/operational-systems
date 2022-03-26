#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <stdatomic.h>

const uintptr_t B = 0xA71409AD;
const uint A = 154 * 1024 * 1024, D = 72, E = 159 * 1024 * 1024, G = 133, I = 136, number_of_files = (A % E != 0) ? (A / E) + 1 : A / E;
FILE* urandom;
void* memptr;
volatile atomic_bool infinite_cycle = 0;

struct t_data {
    void* cursor;
    size_t chunk_size;
    uint file_number;
};

void* generate_data(void* provided_data) {
    struct t_data* data = (struct t_data*) provided_data;
    fread(data->cursor, 1024 * 1024, data->chunk_size, urandom);
}

void* write_to_file(void* provided_data) {
    struct t_data* data = (struct t_data*) provided_data;
    char path[] = "file0";
    path[4] += data->file_number;
    uint fd = open(path, O_WRONLY | O_CREAT | __O_DIRECT);
    while (flock(fd, LOCK_EX | LOCK_NB) == -1)
        printf("I love C");

    struct stat st;
    stat(path, &st);
    uintptr_t alignment = st.st_blksize - 1;
    uint blocks = data->chunk_size / st.st_blksize;
    void* buff = malloc(st.st_blksize + alignment);
    void* write_buff = (void*) (((uintptr_t)buff + alignment) & alignment) + 1;

    for (int i = 0; i < blocks; ++i) {
        void* buff_ptr = data->cursor + st.st_blksize * i;
        memcpy(buff, buff_ptr, st.st_blksize);
        pwrite(fd, write_buff, st.st_blksize, st.st_blksize * i);
    }

    free(buff);
    flock(fd, LOCK_UN);
    close(fd);
}

void* read_from_file(void* provided_data) {
    struct t_data* data = (struct t_data*) provided_data;
    char path[] = "file0";
    path[4] += data->file_number;
    FILE* fd = fopen(path, "wr");
    uint num;
    long long unsigned int sum = 0;

    while (flock(fd, LOCK_EX | LOCK_NB) == -1)
        2+2;
    while (fread(&num, sizeof(int), 1, fd) == 1)
        sum += num;

    fclose(fd);
}

void* generate_data_multithreadingly(void* ignored) {
    pthread_t* generation_threads = malloc(D * sizeof(pthread_t));
    do {
        for (uint i = 0, ordinal = 1, covered = 0, chunk = A / D; i < D;
             ++i, ++ordinal, covered += chunk, chunk = ((A - covered) > chunk) ? A / D : A % D)
            pthread_create(&generation_threads[i], NULL, generate_data, &(struct t_data){memptr + covered, chunk});
        for (uint i = 0; i < D; i++)
            pthread_join(generation_threads[i], NULL);
    } while (infinite_cycle);
    free(generation_threads);
}

void* write_to_files_multithreadingly(void* ignored) {
    pthread_t* writing_threads = malloc(number_of_files * sizeof(pthread_t));
    do {
        for (uint i = 0, chunk = (A > E) ? E : A, covered = 0; i < number_of_files;
             covered += chunk, chunk = ((A - covered) > E) ? E : A - covered, i++)
            pthread_create(&writing_threads[i], NULL, write_to_file, &(struct t_data){memptr + covered, chunk, i});
        for (uint i = 0; i < number_of_files; i++)
            pthread_join(writing_threads[i], NULL);
    } while (infinite_cycle);
    free(writing_threads);
}

void* read_from_files_multithreadingly(void* ignored) {
    pthread_t* reading_threads = malloc(I * sizeof(pthread_t));
    do {
        for (uint i = 0, tr_number = 0, threads_per_file = I / number_of_files;
             (i < number_of_files) || (tr_number < I); tr_number++, (tr_number % threads_per_file == 0) ? i++ : i)
            pthread_create(&reading_threads[i], NULL, read_from_file, &(struct t_data){NULL, 0, i + 1});
        for (uint i = 0; i < I; i++)
            pthread_join(reading_threads[i], NULL);
    } while (infinite_cycle);
    free(reading_threads);
}

int main() {
    urandom = fopen("/dev/urandom\0", "r");
    getchar(); // До аллокации

    memptr = mmap((void*)B, A, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_FILE |MAP_PRIVATE, -1, 0);
    getchar(); // После аллокации

    generate_data_multithreadingly(NULL);
    getchar(); // После заполнения участка данными

    infinite_cycle = 1;
    pthread_t* cycle_threads = malloc(sizeof(pthread_t) * 3);
    pthread_create(&cycle_threads[0], NULL, generate_data_multithreadingly, NULL);
    pthread_create(&cycle_threads[1], NULL, write_to_files_multithreadingly, NULL);
    pthread_create(&cycle_threads[2], NULL, read_from_files_multithreadingly, NULL);
    getchar(); // Программа работает в режиме бесконечного цикла
    infinite_cycle = 0;
    pthread_join(cycle_threads[2], NULL);

    free(cycle_threads);
    munmap(memptr, A);
    fclose(urandom);
    getchar(); // После деаллокации
}