//Still working on
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <signal.h>

pthread_mutex_t mutex;
int num_threads;
int file_size_limit;
int total_files;
int total_duplicates;
int count;
char output_file[100];
FILE* output_fp;

typedef struct {
    char filename[100];
    char filepath[PATH_MAX];
} DuplicateFile;

DuplicateFile duplicate_files[1000];

void* thread_work(void* arg);
int is_regular_file(const char* path);
int is_duplicate(const char* file1, const char* file2);
void process_file(const char* file_path);
void traverse_directory(const char* dir_path);
void print_progress();
void print_result();
void sigint_handler(int sig);

int is_regular_file(const char* path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

int is_duplicate(const char* file1, const char* file2) {
    FILE* fp1 = fopen(file1, "rb");
    FILE* fp2 = fopen(file2, "rb");

    if (fp1 == NULL || fp2 == NULL) {
        return 0;
    }

    int result = 1;
    char byte1, byte2;

    while (!feof(fp1) && !feof(fp2)) {
        fread(&byte1, 1, 1, fp1);
        fread(&byte2, 1, 1, fp2);

        if (byte1 != byte2) {
            result = 0;
            break;
        }
    }

    fclose(fp1);
    fclose(fp2);

    return result;
}

void process_file(const char* file_path) {
    struct stat file_stat;
    stat(file_path, &file_stat);
    off_t file_size = file_stat.st_size;
    char* file_name = strrchr(file_path, '/') + 1;

    pthread_mutex_lock(&mutex);
    total_files++;
    pthread_mutex_unlock(&mutex);

    if (total_files % 5 == 0) {
        pthread_mutex_lock(&mutex);
        print_progress();
        pthread_mutex_unlock(&mutex);
    }

    DIR* dir;
    struct dirent* entry;

    if (is_regular_file(file_path)) {
        dir = opendir(".");
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char* other_file_name = entry->d_name;
            if (strcmp(file_name, other_file_name) != 0 && is_regular_file(other_file_name)) {
                if (is_duplicate(file_path, other_file_name)) {
                    pthread_mutex_lock(&mutex);
                    total_duplicates++;

                    if (count < 1000) {
                        strcpy(duplicate_files[count].filename, file_name);
                        strcpy(duplicate_files[count].filepath, file_path);
                        count++;
                    }

                    pthread_mutex_unlock(&mutex);
                }
            }
        }
        closedir(dir);
    }
}

void traverse_directory(const char* dir_path) {
    DIR* dir;
    struct dirent* entry;
    char path[PATH_MAX];

    dir = opendir(dir_path);

    if (dir == NULL) {
        fprintf(stderr, "Error: Cannot open directory '%s'\n", dir_path);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            traverse_directory(path);
        } else {
            process_file(path);
        }
    }

    closedir(dir);
}

void print_progress() {
    time_t current_time = time(NULL);
    fprintf(stderr, "Search progress: %d files processed, %d duplicates found. Time: %s", total_files, total_duplicates, ctime(&current_time));
}

void print_result() {
    FILE* fp = fopen(output_file, "w");
    fprintf(fp, "[\n");
    int first = 1;

    for (int i = 0; i < count; i++) {
        if (strcmp(duplicate_files[i].filename, duplicate_files[i + 1].filename) != 0) {
            if (!first) {
                fprintf(fp, "    ]\n");
                fprintf(fp, "]\n");
            }
            first = 1;
        }

        if (strcmp(duplicate_files[i].filename, duplicate_files[i + 1].filename) == 0) {
            if (first) {
                fprintf(fp, "   [\n");
                first = 0;
            }
            if (i % (num_threads + 1) == 0) {
                fprintf(fp, "    %s,\n", duplicate_files[i].filepath);
            }
        }
    }

    fclose(fp);
}

void sigint_handler(int sig) {
    printf("\nSIGINT received. Printing result...\n");
    print_result();
    exit(0);
}

void* thread_work(void* arg) {
    char* dir_path = (char*)arg;
    traverse_directory(dir_path);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: Directory path is required.\n");
        return 1;
    }

    num_threads = 0;
    file_size_limit = 1024;
    total_files = 0;
    total_duplicates = 0;
    count = 0;
    output_fp = NULL;

    int opt;
    char* optarg;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-t=", 3) == 0) {
            optarg = argv[i] + 3;
            num_threads = atoi(optarg);

            if (num_threads <= 0 || num_threads > 64) {
                fprintf(stderr, "Error: Invalid number of threads.\n");
                return 1;
            }
        } else if (strncmp(argv[i], "-m=", 3) == 0) {
            optarg = argv[i] + 3;
            file_size_limit = atoi(optarg);
        } else if (strncmp(argv[i], "-o=", 3) == 0) {
            optarg = argv[i] + 3;
            strcpy(output_file, optarg);
            output_fp = fopen(output_file, "w");

            if (output_fp == NULL) {
                fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
                return 1;
            }
        }
    }

    if (output_fp == NULL) {
        strcpy(output_file, "/dev/stdout");
        output_fp = stdout;
    }

    char* dir_path = argv[argc - 1];
    DIR* dir = opendir(dir_path);

    if (dir == NULL) {
        fprintf(stderr, "Error: Cannot open directory '%s'\n", dir_path);
        return 1;
    }

    closedir(dir);

    signal(SIGINT, sigint_handler);

    pthread_mutex_init(&mutex, NULL);
    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, thread_work, (void*)dir_path);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    print_result();

    free(threads);
    pthread_mutex_destroy(&mutex);

    return 0;
}
