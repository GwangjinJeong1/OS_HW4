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
char* output_file;
FILE* output_fp;
int total_files;
int total_duplicates;
char *real_duplicate_path;
#define PATH_MAX 1024
int terminate_threads = 0;
int count=0;
typedef struct {
    char* file_path;
    off_t file_size;
} FileInfo;

typedef struct {
    char filename[100];
    char filepath[100];
   
} DuplicateFile;
DuplicateFile duplicate_files[1000];
/*char *duple_filename;
char *duple_filepath[1000];*/
void print_progress();

int is_regular_file(const char* path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

/*int is_hidden_file(const char* filename) {
    return (strcmp(filename, ".") != 0 || strcmp(filename, "..") != 0);
}*/

int is_duplicate(const FileInfo* file1, const FileInfo* file2) {
    if (file1->file_size != file2->file_size) {
        return 0;
    }

    FILE* fp1 = fopen(file1->file_path, "rb");
    FILE* fp2 = fopen(file2->file_path, "rb");

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
    FileInfo* file_info = (FileInfo*)malloc(sizeof(FileInfo));
    file_info->file_path = strdup(file_path);

    struct stat file_stat;
    stat(file_path, &file_stat);
    file_info->file_size = file_stat.st_size;
    char * tok = strrchr(file_path, '/');
    char *file_name = tok+1;
  
    pthread_mutex_lock(&mutex);
    total_files++;
    pthread_mutex_unlock(&mutex);

    /*FILE* fp = fopen(output_file, "a");
    if (fp != NULL) {

        fprintf(fp, "%s\n", file_path);
        fclose(fp);
    }*/

    pthread_mutex_lock(&mutex);
    if (total_files % 5 == 0) {
        print_progress();
    }
    pthread_mutex_unlock(&mutex);

    DIR* dir;
    struct dirent* entry;

    if (is_regular_file(file_path)) {
        dir = opendir(".");
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
    
            char* other_file_name = strdup(entry->d_name);
            if (strcmp(file_path, other_file_name) != 0 && is_regular_file(other_file_name)) {
                FileInfo* other_file_info = (FileInfo*)malloc(sizeof(FileInfo));
                other_file_info->file_path = other_file_name;

                struct stat other_file_stat;
                stat(other_file_name, &other_file_stat);
                other_file_info->file_size = other_file_stat.st_size;

                if (is_duplicate(file_info, other_file_info)) {
                    pthread_mutex_lock(&mutex);
                    total_duplicates++;
                    pthread_mutex_unlock(&mutex);
                    

                    strcpy(duplicate_files[count].filename , file_name);
                    strcpy(duplicate_files[count].filepath , file_path);
                    
                    count++;
                    /*printf("duple : %s\n", file_path);
                    FILE* fp = fopen(output_file, "a");
                    if (fp != NULL) {
                        fprintf(fp, "aa-  %s\n", other_file_name);
                        fclose(fp);
                    }*/
                }

                free(other_file_info);
            }
            free(other_file_name);
            
        }
        closedir(dir);
    }
    
    free(file_info);
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
        //printf("path%s\n",entry->d_name );
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
/*void print_progress() {
    printf("Files processed: %d, Duplicates found: %d\n", total_files, total_duplicates);
    fflush(stdout);
}*/

void* thread_work(void* arg) {
    char* dir_path = (char*)arg;
    //printf("dir%s\n", dir_path);
    traverse_directory(dir_path);
    return NULL;
}
void print_result(){
    FILE* fp = fopen(output_file, "a");
    fprintf(fp, "[\n");
    int first=1;
    for(int i=0;i<count;i++){
        if(strcmp(duplicate_files[i].filename, duplicate_files[i+1].filename)!=0){
            if(!first){
                fprintf(fp, "    ]\n");
                fprintf(fp, "]\n");
            }
            first=1;
        }
       
        if(strcmp(duplicate_files[i].filename, duplicate_files[i+1].filename)==0){
            if(first){
                fprintf(fp, "   [\n");
                first=0;
            }
            if(i%(num_threads+1)==0)
                fprintf(fp, "%s,\n", duplicate_files[i].filepath);
        }
       
    }
}
void sigint_handler(int signum) {
    printf("\nSIGINT received. Printing result...\n");
    print_result();
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: Directory path is required.\n");
        return 1;
    }

    num_threads = 0;
    file_size_limit = 1024;
    output_file = NULL;
    output_fp = stdout;
    total_files = 0;
    total_duplicates = 0;

    int opt;
    char *optarg;
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-t=", 3)==0){
            optarg = argv[i]+3;
            
            num_threads = atoi(optarg);
           
                if (num_threads <= 0 || num_threads > 64) {
                    fprintf(stderr, "Error: Invalid number of threads.\n");
                    return 1;
                }
        }
        else if (strncmp(argv[i], "-m=", 3)==0){
            optarg = argv[i]+3;
            
            file_size_limit = atoi(optarg);
            
            break;
        }
        else if (strncmp(argv[i], "-o=",3)==0){
            optarg = argv[i]+3;
            output_file = optarg;
            output_fp = fopen(output_file, "w");
            //printf("oooo\n");
            if (output_fp == NULL) {
                fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
                return 1;
            }
            break;
        }
    
    }
/*
    while ((opt = getopt(argc, argv, "t:m:o:")) != -1) {
        switch (opt) {
            case 't':
                num_threads = atoi(optarg);
                if (num_threads <= 0 || num_threads > 64) {
                    fprintf(stderr, "Error: Invalid number of threads.\n");
                    return 1;
                }
                break;
            case 'm':
                file_size_limit = atoi(optarg);
                break;
            case 'o':
                output_file = optarg;
                output_fp = fopen(output_file, "w");
                if (output_fp == NULL) {
                    fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "Error: Invalid option '%c'\n", opt);
                return 1;
        }
    }
*/
    if (output_file == NULL) {
        output_file = "/dev/stdout";
    }

    char* dir_path = argv[argc-1];
    DIR* dir = opendir(dir_path);

    if (dir == NULL) {
        fprintf(stderr, "Error: Cannot open directory '%s'\n", dir_path);
        return 1;
    }
  signal(SIGINT, sigint_handler);
    pthread_mutex_init(&mutex, NULL);

    pthread_t main_thread;
    pthread_create(&main_thread, NULL, thread_work, dir_path);
   
    pthread_t* threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    //rintf("----\n");

    for (int i = 0; i < num_threads; i++) { // multi-threads
        pthread_create(&threads[i], NULL, thread_work, dir_path);
        //printf("ifififif\n");
    }
    while (!terminate_threads)
        sleep(1);

    while (1) {
        sleep(5);
        pthread_mutex_lock(&mutex);
        print_progress();
        pthread_mutex_unlock(&mutex);
        
    }
    
    pthread_join(main_thread, NULL);
    //printf("bbb\n");
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("the end\n");
    print_progress();
    
    print_result();
    pthread_mutex_destroy(&mutex);
    closedir(dir);
    free(threads);

    if (output_fp != stdout) {
        fclose(output_fp);
    }

    return 0;
}