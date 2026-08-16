#ifndef H_LIB_BRAIN_H
#define H_LIB_BRAIN_H

typedef unsigned long size_t;
typedef void FILE;
typedef void* DIR;

struct stat {
    unsigned int flags;
    unsigned int st_atim;
    unsigned int st_mtim;
    unsigned int st_ctim;
    size_t st_size;
};

struct dirent {
    unsigned int type;
    size_t size;
    size_t filename_length;
    char filename[260];
};

enum {
    SEEK_SET = 0,
    SEEK_CUR,
    SEEK_END,
};

void *malloc(size_t size);
void *realloc(void* ptr, size_t size);
int free(void *ptr);
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *ptr, int ch, size_t n);
FILE *fopen(const char *filename, const char *mode);
int fclose(FILE *stream);
int ftell(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, int offset, int whence);
int feof(FILE *stream);
int fflush(FILE *stream);
int fgetc(FILE *stream);
char *fgets(char *s, int n, FILE *stream);
int fputc(int ch, FILE *stream);
int fputs(const char *str, FILE *stream);
int ferror(FILE *stream);
int remove(const char *filepath);
int rename(const char *oldpath, const char *newpath);
int mkdir(const char *pathname);
int stat(const char *filepath, struct stat *buf);
int closedir(DIR *dirp);
struct dirent *readdir(DIR *dirp);
DIR *opendir(const char *pathname);

int printf(const char *fmt,...);

#endif
