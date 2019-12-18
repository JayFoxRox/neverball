typedef struct {
  int pad;
} DIR;

struct dirent {
  char d_name[];
};

DIR *opendir(const char *__name);
struct dirent *readdir(DIR *__dirp);
int closedir(DIR *__dirp);


