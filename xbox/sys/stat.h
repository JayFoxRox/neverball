#define F_OK 0 //FIXME: Should be in unistd.h?

struct stat { int st_size; };

int stat(const char *pathname, struct stat *statbuf);
int access(const char *pathname, int mode);

