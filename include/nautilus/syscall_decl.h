int sys_close(int fd, int b, int c, int d, int e, int f);

int sys_exit(int	exit_status, int b, int c, int d, int e, int f);

int sys_fork(int a, int b, int c, int d, int e, int f);

int sys_fstat(int fd, int st, int c, int d, int e, int f);

int sys_ftruncate(int fd, int len, int c, int d, int e, int f);

int sys_lseek(int fd, int position, int whence, int d, int e, int f);

int sys_open(int filename, int flags,int  mode, int d, int e, int f);

int sys_read(int fd, int buf, int len, int d, int e, int f);

int sys_stat(int pathname,int st, int c, int d, int e, int f);

int sys_write(int fd, int buf, int len, int d, int e, int f);

int sys_getpid(int a, int b, int c, int d, int e, int f);

int sys_gettimeofday(int timeval_ptr, int timezone_ptr);

int sys_settimeofday(int timeval_ptr, int timezone_ptr);
