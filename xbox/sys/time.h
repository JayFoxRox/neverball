struct timeval {
  unsigned int tv_sec;
  unsigned int tv_usec;
};

struct timezone {
  int pad;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);
