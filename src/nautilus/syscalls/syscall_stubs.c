#include <nautilus/nautilus.h>

#define DEBUG(fmt, args...) DEBUG_PRINT("syscall_stubs: " fmt, ##args)

uint64_t sys_lstat(uint64_t filename, uint64_t statbuf) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (lstat)");
  return 0;
}

uint64_t sys_poll(uint64_t ufds, uint64_t nfds, uint64_t timeout_msecs) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (poll)");
  return 0;
}

uint64_t sys_brk(uint64_t brk) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (brk)");
  return 0;
}

uint64_t sys_rt_sigaction(uint64_t sig, uint64_t act, uint64_t oact,
                          uint64_t sigsetsize) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (rt_sigaction)");
  return 0;
}

uint64_t sys_rt_sigprocmask(uint64_t how, uint64_t nset, uint64_t oset,
                            uint64_t sigsetsize) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (rt_sigprocmask)");
  return 0;
}

uint64_t sys_rt_sigreturn() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (rt_sigreturn)");
  return 0;
}

uint64_t sys_ioctl(uint64_t fd, uint64_t cmd, uint64_t arg) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (ioctl)");
  return 0;
}

uint64_t sys_pread64(uint64_t fd, uint64_t buf, uint64_t count, uint64_t pos) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (pread64)");
  return 0;
}

uint64_t sys_pwrite64(uint64_t fd, uint64_t buf, uint64_t count, uint64_t pos) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (pwrite64)");
  return 0;
}

uint64_t sys_readv(uint64_t fd, uint64_t vec, uint64_t vlen) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (readv)");
  return 0;
}

uint64_t sys_writev(uint64_t fd, uint64_t vec, uint64_t vlen) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (writev)");
  return 0;
}

uint64_t sys_access(uint64_t filename, uint64_t mode) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (access)");
  return 0;
}

uint64_t sys_pipe(uint64_t fildes) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (pipe)");
  return 0;
}

uint64_t sys_select(uint64_t n, uint64_t inp, uint64_t outp, uint64_t exp) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (select)");
  return 0;
}

uint64_t sys_sched_yield() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sched_yield)");
  return 0;
}

uint64_t sys_mremap(uint64_t brk) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mremap)");
  return 0;
}

uint64_t sys_msync(uint64_t start, uint64_t len, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (msync)");
  return 0;
}

uint64_t sys_mincore(uint64_t start, uint64_t len, uint64_t vec) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mincore)");
  return 0;
}

uint64_t sys_madvise(uint64_t start, uint64_t len_in, uint64_t behavior) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (madvise)");
  return 0;
}

uint64_t sys_shmget(uint64_t key, uint64_t size, uint64_t shmflg) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (shmget)");
  return 0;
}

uint64_t sys_shmat(uint64_t shmid, uint64_t shmaddr, uint64_t shmflg) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (shmat)");
  return 0;
}

uint64_t sys_shmctl(uint64_t shmid, uint64_t cmd, uint64_t buf) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (shmctl)");
  return 0;
}

uint64_t sys_dup(uint64_t fildes) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (dup)");
  return 0;
}

uint64_t sys_dup2(uint64_t oldfd, uint64_t newfd) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (dup2)");
  return 0;
}

uint64_t sys_pause() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (pause)");
  return 0;
}

uint64_t sys_getitimer(uint64_t which, uint64_t value) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getitimer)");
  return 0;
}

uint64_t sys_alarm(uint64_t seconds) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (alarm)");
  return 0;
}

uint64_t sys_setitimer(uint64_t which, uint64_t value, uint64_t ovalue) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setitimer)");
  return 0;
}

uint64_t sys_sendfile(uint64_t out_fd, uint64_t in_fd, uint64_t offset,
                      uint64_t count) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sendfile)");
  return 0;
}

uint64_t sys_socket(uint64_t family, uint64_t type, uint64_t protocol) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (socket)");
  return 0;
}

uint64_t sys_connect(uint64_t fd, uint64_t uservaddr, uint64_t addrlen) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (connect)");
  return 0;
}

uint64_t sys_accept(uint64_t fd, uint64_t upeer_sockaddr,
                    uint64_t upeer_addrlen) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (accept)");
  return 0;
}

uint64_t sys_sendto(uint64_t fd, uint64_t buff, uint64_t len, uint64_t flags,
                    uint64_t addr, uint64_t addr_len) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sendto)");
  return 0;
}

uint64_t sys_recvfrom(uint64_t fd, uint64_t ubuf, uint64_t size, uint64_t flags,
                      uint64_t addr, uint64_t addr_len) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (recvfrom)");
  return 0;
}

uint64_t sys_sendmsg(uint64_t fd, uint64_t msg, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sendmsg)");
  return 0;
}

uint64_t sys_recvmsg(uint64_t fd, uint64_t msg, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (recvmsg)");
  return 0;
}

uint64_t sys_shutdown(uint64_t fd, uint64_t how) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (shutdown)");
  return 0;
}

uint64_t sys_bind(uint64_t fd, uint64_t umyaddr, uint64_t addrlen) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (bind)");
  return 0;
}

uint64_t sys_listen(uint64_t fd, uint64_t backlog) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (listen)");
  return 0;
}

uint64_t sys_getsockname(uint64_t usockaddr, uint64_t usockaddr_len) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getsockname)");
  return 0;
}

uint64_t sys_getpeername(uint64_t usockaddr, uint64_t usockaddr_len) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getpeername)");
  return 0;
}

uint64_t sys_socketpair(uint64_t family, uint64_t type, uint64_t protocol) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (socketpair)");
  return 0;
}

uint64_t sys_setsockopt(uint64_t usockvec) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setsockopt)");
  return 0;
}

uint64_t sys_getsockopt(uint64_t fd, uint64_t level, uint64_t optname,
                        uint64_t optval, uint64_t optlen) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getsockopt)");
  return 0;
}

uint64_t sys_clone(uint64_t fd, uint64_t level, uint64_t optname,
                   uint64_t optval, uint64_t optlen) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (clone)");
  return 0;
}

uint64_t sys_vfork() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (vfork)");
  return 0;
}

uint64_t sys_execve(uint64_t filename, uint64_t argv, uint64_t envp) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (execve)");
  return 0;
}

uint64_t sys_wait4(uint64_t upid, uint64_t stat_addr, uint64_t options,
                   uint64_t ru) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (wait4)");
  return 0;
}

uint64_t sys_kill(uint64_t pid, uint64_t sig) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (kill)");
  return 0;
}

uint64_t sys_semget(uint64_t key, uint64_t nsems, uint64_t semflg) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (semget)");
  return 0;
}

uint64_t sys_semop(uint64_t semid, uint64_t tsops, uint64_t nsops) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (semop)");
  return 0;
}

uint64_t sys_semctl(uint64_t semid, uint64_t semnum, uint64_t cmd,
                    uint64_t arg) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (semctl)");
  return 0;
}

uint64_t sys_shmdt(uint64_t shmaddr) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (shmdt)");
  return 0;
}

uint64_t sys_msgget(uint64_t key, uint64_t msgflg) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (msgget)");
  return 0;
}

uint64_t sys_msgsnd(uint64_t msqid, uint64_t msgp, uint64_t msgsz,
                    uint64_t msgflg) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (msgsnd)");
  return 0;
}

uint64_t sys_msgrcv(uint64_t msqid, uint64_t msgp, uint64_t msgsz,
                    uint64_t msgtyp, uint64_t msgflg) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (msgrcv)");
  return 0;
}

uint64_t sys_msgctl(uint64_t msqid, uint64_t cmd, uint64_t buf) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (msgctl)");
  return 0;
}

uint64_t sys_fcntl(uint64_t fd, uint64_t cmd, uint64_t arg) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fcntl)");
  return 0;
}

uint64_t sys_flock(uint64_t fd, uint64_t cmd) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (flock)");
  return 0;
}

uint64_t sys_fsync(uint64_t fd) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fsync)");
  return 0;
}

uint64_t sys_fdatasync(uint64_t fd) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fdatasync)");
  return 0;
}

uint64_t sys_truncate(uint64_t path, uint64_t length) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (truncate)");
  return 0;
}

uint64_t sys_getdents(uint64_t fd, uint64_t dirent, uint64_t count) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getdents)");
  return 0;
}

uint64_t sys_getcwd(uint64_t buf, uint64_t size) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getcwd)");
  return 0;
}

uint64_t sys_chdir(uint64_t filename) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (chdir)");
  return 0;
}

uint64_t sys_fchdir(uint64_t fd) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fchdir)");
  return 0;
}

uint64_t sys_rename(uint64_t oldname, uint64_t newname) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (rename)");
  return 0;
}

uint64_t sys_rmdir(uint64_t pathname) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (rmdir)");
  return 0;
}

uint64_t sys_creat(uint64_t pathname, uint64_t mode) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (creat)");
  return 0;
}

uint64_t sys_link(uint64_t oldname, uint64_t newname) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (link)");
  return 0;
}

uint64_t sys_unlink(uint64_t pathname) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (unlink)");
  return 0;
}

uint64_t sys_symlink(uint64_t oldname, uint64_t newname) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (symlink)");
  return 0;
}

uint64_t sys_readlink(uint64_t path, uint64_t buf, uint64_t bufsiz) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (readlink)");
  return 0;
}

uint64_t sys_chmod(uint64_t filename, uint64_t mode) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (chmod)");
  return 0;
}

uint64_t sys_fchmod(uint64_t fd, uint64_t mode) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fchmod)");
  return 0;
}

uint64_t sys_chown(uint64_t filename, uint64_t uservaddr, uint64_t group) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (chown)");
  return 0;
}

uint64_t sys_fchown(uint64_t fd, uint64_t uservaddr, uint64_t group) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fchown)");
  return 0;
}

uint64_t sys_lchown(uint64_t filename, uint64_t uservaddr, uint64_t group) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (lchown)");
  return 0;
}

uint64_t sys_umask(uint64_t mask) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (umask)");
  return 0;
}

uint64_t sys_getrlimit(uint64_t resource, uint64_t rlim) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getrlimit)");
  return 0;
}

uint64_t sys_getrusage(uint64_t who, uint64_t ru) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getrusage)");
  return 0;
}

uint64_t sys_sysinfo(uint64_t info) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sysinfo)");
  return 0;
}

uint64_t sys_times(uint64_t tbuf) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (times)");
  return 0;
}

uint64_t sys_ptrace(uint64_t request, uint64_t pid, uint64_t addrlen,
                    uint64_t data) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (ptrace)");
  return 0;
}

uint64_t sys_getuid() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getuid)");
  return 0;
}

uint64_t sys_syslog(uint64_t type, uint64_t buf, uint64_t len) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (syslog)");
  return 0;
}

uint64_t sys_getgid() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getgid)");
  return 0;
}

uint64_t sys_setuid(uint64_t uid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setuid)");
  return 0;
}

uint64_t sys_setgid(uint64_t gid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setgid)");
  return 0;
}

uint64_t sys_geteuid() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (geteuid)");
  return 0;
}

uint64_t sys_getegid() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getegid)");
  return 0;
}

uint64_t sys_setpgid(uint64_t pid, uint64_t pgid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setpgid)");
  return 0;
}

uint64_t sys_getppid() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getppid)");
  return 0;
}

uint64_t sys_getpgrp() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getpgrp)");
  return 0;
}

uint64_t sys_setsid() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setsid)");
  return 0;
}

uint64_t sys_setreuid(uint64_t ruid, uint64_t euid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setreuid)");
  return 0;
}

uint64_t sys_setregid(uint64_t rgid, uint64_t egid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setregid)");
  return 0;
}

uint64_t sys_getgroups(uint64_t gidsetsize, uint64_t grouplist) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getgroups)");
  return 0;
}

uint64_t sys_setgroups(uint64_t gidsetsize, uint64_t grouplist) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setgroups)");
  return 0;
}

uint64_t sys_setresuid(uint64_t ruid, uint64_t euid, uint64_t suid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setresuid)");
  return 0;
}

uint64_t sys_getresuid(uint64_t ruidp, uint64_t euidp, uint64_t suidp) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getresuid)");
  return 0;
}

uint64_t sys_setresgid(uint64_t rgid, uint64_t egid, uint64_t sgid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setresgid)");
  return 0;
}

uint64_t sys_getresgid(uint64_t rgidp, uint64_t egidp, uint64_t sgidp) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getresgid)");
  return 0;
}

uint64_t sys_getpgid(uint64_t pid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getpgid)");
  return 0;
}

uint64_t sys_setfsuid(uint64_t uid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setfsuid)");
  return 0;
}

uint64_t sys_setfsgid(uint64_t gid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setfsgid)");
  return 0;
}

uint64_t sys_getsid(uint64_t pid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getsid)");
  return 0;
}

uint64_t sys_capget(uint64_t header, uint64_t dataptr) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (capget)");
  return 0;
}

uint64_t sys_capset(uint64_t header, uint64_t data) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (capset)");
  return 0;
}

uint64_t sys_rt_sigpending(uint64_t uset, uint64_t sigsetsize) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (rt_sigpending)");
  return 0;
}

uint64_t sys_rt_sigtimedwait(uint64_t uthese, uint64_t uinfo, uint64_t uts,
                             uint64_t sigsetsize) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (rt_sigtimedwait)");
  return 0;
}

uint64_t sys_rt_sigqueueinfo(uint64_t pid, uint64_t sig, uint64_t uinfo) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (rt_sigqueueinfo)");
  return 0;
}

uint64_t sys_rt_sigsuspend(uint64_t unewset, uint64_t sigsetsize) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (rt_sigsuspend)");
  return 0;
}

uint64_t sys_sigaltstack(uint64_t uss, uint64_t uoss) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sigaltstack)");
  return 0;
}

uint64_t sys_utime(uint64_t filename, uint64_t times) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (utime)");
  return 0;
}

uint64_t sys_mknod(uint64_t filename, uint64_t mode, uint64_t dev) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mknod)");
  return 0;
}

uint64_t sys_uselib(uint64_t library) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (uselib)");
  return 0;
}

uint64_t sys_personality(uint64_t personality) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (personality)");
  return 0;
}

uint64_t sys_ustat(uint64_t dev, uint64_t ubuf) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (ustat)");
  return 0;
}

uint64_t sys_statfs(uint64_t pathname, uint64_t buf) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (statfs)");
  return 0;
}

uint64_t sys_fstatfs(uint64_t fd, uint64_t buf) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fstatfs)");
  return 0;
}

uint64_t sys_sysfs(uint64_t option, uint64_t arg1, uint64_t arg2) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sysfs)");
  return 0;
}

uint64_t sys_getpriority(uint64_t which, uint64_t who) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getpriority)");
  return 0;
}

uint64_t sys_setpriority(uint64_t which, uint64_t who, uint64_t niceval) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setpriority)");
  return 0;
}

uint64_t sys_sched_setparam(uint64_t pid, uint64_t param) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sched_setparam)");
  return 0;
}

uint64_t sys_sched_getparam(uint64_t pid, uint64_t param) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sched_getparam)");
  return 0;
}

uint64_t sys_sched_setscheduler(uint64_t pid, uint64_t policy, uint64_t param) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sched_setscheduler)");
  return 0;
}

uint64_t sys_sched_getscheduler(uint64_t pid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sched_getscheduler)");
  return 0;
}

uint64_t sys_sched_get_priority_max(uint64_t policy) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sched_get_priority_max)");
  return 0;
}

uint64_t sys_sched_get_priority_min(uint64_t policy) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sched_get_priority_min)");
  return 0;
}

uint64_t sys_sched_rr_get_interval(uint64_t pid, uint64_t interval) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sched_rr_get_interval)");
  return 0;
}

uint64_t sys_mlock(uint64_t start, uint64_t len) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mlock)");
  return 0;
}

uint64_t sys_munlock(uint64_t start, uint64_t len) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (munlock)");
  return 0;
}

uint64_t sys_mlockall(uint64_t start, uint64_t len) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mlockall)");
  return 0;
}

uint64_t sys_munlockall() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (munlockall)");
  return 0;
}

uint64_t sys_vhangup() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (vhangup)");
  return 0;
}

uint64_t sys_modify_ldt(uint64_t func, uint64_t ptr, uint64_t bytecount) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (modify_ldt)");
  return 0;
}

uint64_t sys_pivot_root(uint64_t new_root, uint64_t put_old) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (pivot_root)");
  return 0;
}

uint64_t sys__sysctl(uint64_t args) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (_sysctl)");
  return 0;
}

uint64_t sys_prctl(uint64_t option, uint64_t arg2, uint64_t arg3, uint64_t arg4,
                   uint64_t arg5) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (prctl)");
  return 0;
}

uint64_t sys_arch_prctl(uint64_t task, uint64_t code, uint64_t addrlen) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (arch_prctl)");
  return 0;
}

uint64_t sys_adjtimex(uint64_t txc_p) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (adjtimex)");
  return 0;
}

uint64_t sys_setrlimit(uint64_t resource, uint64_t rlim) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setrlimit)");
  return 0;
}

uint64_t sys_chroot(uint64_t filename) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (chroot)");
  return 0;
}

uint64_t sys_sync() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sync)");
  return 0;
}

uint64_t sys_acct(uint64_t name) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (acct)");
  return 0;
}

uint64_t sys_mount(uint64_t dev_name, uint64_t dir_name, uint64_t type,
                   uint64_t flags, uint64_t data) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mount)");
  return 0;
}

uint64_t sys_umount2(uint64_t name, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (umount2)");
  return 0;
}

uint64_t sys_swapon(uint64_t specialfile, uint64_t swap_flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (swapon)");
  return 0;
}

uint64_t sys_swapoff(uint64_t specialfile) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (swapoff)");
  return 0;
}

uint64_t sys_reboot(uint64_t magic1, uint64_t magic2, uint64_t cmd,
                    uint64_t arg) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (reboot)");
  return 0;
}


uint64_t sys_setdomainname(uint64_t name, uint64_t len) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setdomainname)");
  return 0;
}

uint64_t sys_iopl(uint64_t level) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (iopl)");
  return 0;
}

uint64_t sys_ioperm(uint64_t from, uint64_t num, uint64_t turn_on) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (ioperm)");
  return 0;
}

uint64_t sys_create_module() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (create_module)");
  return 0;
}

uint64_t sys_init_module(uint64_t umod, uint64_t len, uint64_t uargs) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (init_module)");
  return 0;
}

uint64_t sys_delete_module(uint64_t name_user, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (delete_module)");
  return 0;
}

uint64_t sys_get_kernel_syms() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (get_kernel_syms)");
  return 0;
}

uint64_t sys_query_module() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (query_module)");
  return 0;
}

uint64_t sys_quotactl(uint64_t cmd, uint64_t special, uint64_t id,
                      uint64_t addr) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (quotactl)");
  return 0;
}

uint64_t sys_nfsservctl() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (nfsservctl)");
  return 0;
}

uint64_t sys_getpmsg() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getpmsg)");
  return 0;
}

uint64_t sys_putpmsg() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (putpmsg)");
  return 0;
}

uint64_t sys_afs_syscall() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (afs_syscall)");
  return 0;
}

uint64_t sys_tuxcall() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (tuxcall)");
  return 0;
}

uint64_t sys_security() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (security)");
  return 0;
}

uint64_t sys_readahead(uint64_t fd, uint64_t offset, uint64_t count) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (readahead)");
  return 0;
}

uint64_t sys_setxattr(uint64_t pathname, uint64_t name, uint64_t value,
                      uint64_t size, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setxattr)");
  return 0;
}

uint64_t sys_lsetxattr(uint64_t pathname, uint64_t name, uint64_t value,
                       uint64_t size, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (lsetxattr)");
  return 0;
}

uint64_t sys_fsetxattr(uint64_t fd, uint64_t name, uint64_t value,
                       uint64_t size, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fsetxattr)");
  return 0;
}

uint64_t sys_getxattr(uint64_t pathname, uint64_t name, uint64_t value,
                      uint64_t size) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getxattr)");
  return 0;
}

uint64_t sys_lgetxattr(uint64_t pathname, uint64_t name, uint64_t value,
                       uint64_t size) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (lgetxattr)");
  return 0;
}

uint64_t sys_fgetxattr(uint64_t fd, uint64_t name, uint64_t value,
                       uint64_t size) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fgetxattr)");
  return 0;
}

uint64_t sys_listxattr(uint64_t pathname, uint64_t list, uint64_t size) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (listxattr)");
  return 0;
}

uint64_t sys_llistxattr(uint64_t pathname, uint64_t list, uint64_t size) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (llistxattr)");
  return 0;
}

uint64_t sys_flistxattr(uint64_t fd, uint64_t list, uint64_t size) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (flistxattr)");
  return 0;
}

uint64_t sys_removexattr(uint64_t pathname, uint64_t name) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (removexattr)");
  return 0;
}

uint64_t sys_lremovexattr(uint64_t pathname, uint64_t name) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (lremovexattr)");
  return 0;
}

uint64_t sys_fremovexattr(uint64_t fd, uint64_t name) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fremovexattr)");
  return 0;
}

uint64_t sys_tkill(uint64_t pid, uint64_t sig) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (tkill)");
  return 0;
}

uint64_t sys_time(uint64_t tloc) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (time)");
  return 0;
}

uint64_t sys_futex(uint64_t usaddr, uint64_t op, uint64_t val, uint64_t utime,
                   uint64_t uaddr2, uint64_t val3) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (futex)");
  return 0;
}

uint64_t sys_sched_setaffinity(uint64_t pid, uint64_t len,
                               uint64_t user_mask_ptr) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sched_setaffinity)");
  return 0;
}

uint64_t sys_sched_getaffinity(uint64_t pid, uint64_t len,
                               uint64_t user_mask_ptr) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sched_getaffinity)");
  return 0;
}

uint64_t sys_set_thread_area(uint64_t u_unfo) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (set_thread_area)");
  return 0;
}

uint64_t sys_io_setup(uint64_t nr_events, uint64_t ctxp) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (io_setup)");
  return 0;
}

uint64_t sys_io_destroy(uint64_t ctx) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (io_destroy)");
  return 0;
}

uint64_t sys_io_getevents(uint64_t ctx_id, uint64_t min_nr, uint64_t nr,
                          uint64_t events, uint64_t timeout) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (io_getevents)");
  return 0;
}

uint64_t sys_io_submit(uint64_t ctx_id, uint64_t nr, uint64_t iocbpp) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (io_submit)");
  return 0;
}

uint64_t sys_io_cancel(uint64_t ctx_id, uint64_t iocb, uint64_t result) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (io_cancel)");
  return 0;
}

uint64_t sys_get_thread_area(uint64_t u_info) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (get_thread_area)");
  return 0;
}

uint64_t sys_lookup_dcookie(uint64_t cookie64, uint64_t buf, uint64_t len) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (lookup_dcookie)");
  return 0;
}

uint64_t sys_epoll_create(uint64_t size) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (epoll_create)");
  return 0;
}

uint64_t sys_epoll_ctl_old() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (epoll_ctl_old)");
  return 0;
}

uint64_t sys_epoll_wait_old() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (epoll_wait_old)");
  return 0;
}

uint64_t sys_remap_file_pages(uint64_t start, uint64_t size, uint64_t protocol,
                              uint64_t pgoff, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (remap_file_pages)");
  return 0;
}

uint64_t sys_getdents64(uint64_t fd, uint64_t dirent, uint64_t count) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (getdents64)");
  return 0;
}

uint64_t sys_set_tid_address(uint64_t tldptr) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (set_tid_address)");
  return 0;
}

uint64_t sys_restart_syscall() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (restart_syscall)");
  return 0;
}

uint64_t sys_semtimedop(uint64_t semid, uint64_t tsops, uint64_t nsops,
                        uint64_t timeout) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (semtimedop)");
  return 0;
}

uint64_t sys_fadvise64(uint64_t fd, uint64_t offset, uint64_t len,
                       uint64_t advice) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fadvise64)");
  return 0;
}

uint64_t sys_timer_create(uint64_t which_clock, uint64_t timer_event_spec,
                          uint64_t creaded_timer_id) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (timer_create)");
  return 0;
}

uint64_t sys_timer_settime(uint64_t timer_id, uint64_t flags,
                           uint64_t new_setting, uint64_t old_setting) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (timer_settime)");
  return 0;
}

uint64_t sys_timer_gettime(uint64_t timer_id, uint64_t setting) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (timer_gettime)");
  return 0;
}

uint64_t sys_timer_getoverrun(uint64_t timer_id) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (timer_getoverrun)");
  return 0;
}

uint64_t sys_timer_delete(uint64_t timer_id) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (timer_delete)");
  return 0;
}

uint64_t sys_clock_settime(uint64_t which_clock, uint64_t tp) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (clock_settime)");
  return 0;
}

uint64_t sys_clock_nanosleep(uint64_t which_clock, uint64_t flags,
                             uint64_t rqtp, uint64_t rmtp) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (clock_nanosleep)");
  return 0;
}

uint64_t sys_exit_group(uint64_t error_code) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (exit_group)");
  return 0;
}

uint64_t sys_epoll_wait(uint64_t epfd, uint64_t events, uint64_t maxevents,
                        uint64_t timeout) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (epoll_wait)");
  return 0;
}

uint64_t sys_epoll_ctl(uint64_t epfd, uint64_t op, uint64_t fd,
                       uint64_t event) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (epoll_ctl)");
  return 0;
}

uint64_t sys_tgkill(uint64_t tgid, uint64_t pid, uint64_t sig) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (tgkill)");
  return 0;
}

uint64_t sys_utimes(uint64_t filename, uint64_t utimes) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (utimes)");
  return 0;
}

uint64_t sys_vserver() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (vserver)");
  return 0;
}

uint64_t sys_mbind(uint64_t start, uint64_t len, uint64_t mode, uint64_t nmask,
                   uint64_t maxnode, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mbind)");
  return 0;
}

uint64_t sys_set_mempolicy(uint64_t mode, uint64_t nmask, uint64_t maxnode) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (set_mempolicy)");
  return 0;
}

uint64_t sys_get_mempolicy(uint64_t policy, uint64_t nmask, uint64_t maxnode,
                           uint64_t addr, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (get_mempolicy)");
  return 0;
}

uint64_t sys_mq_open(uint64_t u_name, uint64_t oflag, uint64_t mode,
                     uint64_t u_attr) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mq_open)");
  return 0;
}

uint64_t sys_mq_unlink(uint64_t u_name) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mq_unlink)");
  return 0;
}

uint64_t sys_mq_timedsend(uint64_t mqdes, uint64_t u_msg_ptr, uint64_t msg_len,
                          uint64_t msg_prio, uint64_t u_abs_timeout) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mq_timedsend)");
  return 0;
}

uint64_t sys_mq_timedreceive(uint64_t mqdes, uint64_t u_msg_ptr,
                             uint64_t msg_len, uint64_t u_msg_prio,
                             uint64_t u_abs_timeout) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mq_timedreceive)");
  return 0;
}

uint64_t sys_mq_notify(uint64_t mqdes, uint64_t u_notification) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mq_notify)");
  return 0;
}

uint64_t sys_mq_getsetattr(uint64_t mqdes, uint64_t u_mqstat,
                           uint64_t u_omqstat) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mq_getsetattr)");
  return 0;
}

uint64_t sys_kexec_load(uint64_t entry, uint64_t nr_segments, uint64_t segments,
                        uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (kexec_load)");
  return 0;
}

uint64_t sys_waitid(uint64_t which, uint64_t upid, uint64_t infop,
                    uint64_t options, uint64_t ru) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (waitid)");
  return 0;
}

uint64_t sys_add_key(uint64_t type, uint64_t description, uint64_t payload,
                     uint64_t plen, uint64_t ringid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (add_key)");
  return 0;
}

uint64_t sys_request_key(uint64_t type, uint64_t description,
                         uint64_t callout_info, uint64_t destringid) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (request_key)");
  return 0;
}

uint64_t sys_keyctl(uint64_t option, uint64_t arg2, uint64_t arg3,
                    uint64_t arg4, uint64_t arg5) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (keyctl)");
  return 0;
}

uint64_t sys_ioprio_set(uint64_t which, uint64_t who, uint64_t ioprio) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (ioprio_set)");
  return 0;
}

uint64_t sys_ioprio_get(uint64_t which, uint64_t who) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (ioprio_get)");
  return 0;
}

uint64_t sys_inotify_init() {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (inotify_init)");
  return 0;
}

uint64_t sys_inotify_add_watch(uint64_t fd, uint64_t pathname, uint64_t mask) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (inotify_add_watch)");
  return 0;
}

uint64_t sys_inotify_rm_watch(uint64_t fd, uint64_t wd) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (inotify_rm_watch)");
  return 0;
}

uint64_t sys_migrate_pages(uint64_t pid, uint64_t maxnode, uint64_t old_ndoes,
                           uint64_t new_nodes) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (migrate_pages)");
  return 0;
}

uint64_t sys_openat(uint64_t dfd, uint64_t filename, uint64_t flags,
                    uint64_t mode) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (openat)");
  return 0;
}

uint64_t sys_mkdirat(uint64_t dfd, uint64_t pathname, uint64_t mode) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mkdirat)");
  return 0;
}

uint64_t sys_mknodat(uint64_t dfd, uint64_t filename, uint64_t mode,
                     uint64_t dev) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (mknodat)");
  return 0;
}

uint64_t sys_fchownat(uint64_t dfd, uint64_t filename, uint64_t user_mask_ptr,
                      uint64_t group, uint64_t flag) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fchownat)");
  return 0;
}

uint64_t sys_futimesat(uint64_t dfd, uint64_t filename, uint64_t utimes) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (futimesat)");
  return 0;
}

uint64_t sys_newfstatat(uint64_t dfd, uint64_t filename, uint64_t statbuf,
                        uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (newfstatat)");
  return 0;
}

uint64_t sys_unlinkat(uint64_t dfd, uint64_t pathname, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (unlinkat)");
  return 0;
}

uint64_t sys_renameat(uint64_t olddfd, uint64_t oldname, uint64_t newdfd,
                      uint64_t newname) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (renameat)");
  return 0;
}

uint64_t sys_linkat(uint64_t olddfd, uint64_t oldname, uint64_t newdfd,
                    uint64_t newname, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (linkat)");
  return 0;
}

uint64_t sys_symlinkat(uint64_t oldname, uint64_t newdfd, uint64_t newname) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (symlinkat)");
  return 0;
}

uint64_t sys_readlinkat(uint64_t dfd, uint64_t pathname, uint64_t buf,
                        uint64_t bufsiz) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (readlinkat)");
  return 0;
}

uint64_t sys_fchmodat(uint64_t dfd, uint64_t filename, uint64_t mode) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fchmodat)");
  return 0;
}

uint64_t sys_faccessat(uint64_t dfd, uint64_t filename, uint64_t mode) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (faccessat)");
  return 0;
}

uint64_t sys_pselect6(uint64_t n, uint64_t inp, uint64_t outp, uint64_t exp,
                      uint64_t tsp, uint64_t sig) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (pselect6)");
  return 0;
}

uint64_t sys_ppoll(uint64_t ufds, uint64_t nfds, uint64_t tsp, uint64_t sigmask,
                   uint64_t sigsetsize) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (ppoll)");
  return 0;
}

uint64_t sys_unshare(uint64_t unshare_flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (unshare)");
  return 0;
}

uint64_t sys_set_robust_list(uint64_t header, uint64_t len) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (set_robust_list)");
  return 0;
}

uint64_t sys_get_robust_list(uint64_t head_ptr, uint64_t len_ptr) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (get_robust_list)");
  return 0;
}

uint64_t sys_splice(uint64_t fd_in, uint64_t off_in, uint64_t fd_out,
                    uint64_t off_out, uint64_t len, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (splice)");
  return 0;
}

uint64_t sys_tee(uint64_t fdin, uint64_t fdout, uint64_t len, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (tee)");
  return 0;
}

uint64_t sys_sync_file_range(uint64_t fd, uint64_t offset, uint64_t nbytes,
                             uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sync_file_range)");
  return 0;
}

uint64_t sys_vmsplice(uint64_t fd, uint64_t iov, uint64_t nr_segs,
                      uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (vmsplice)");
  return 0;
}

uint64_t sys_move_pages(uint64_t pid, uint64_t nr_pages, uint64_t pages,
                        uint64_t nodes, uint64_t status, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (move_pages)");
  return 0;
}

uint64_t sys_utimensat(uint64_t dfd, uint64_t filename, uint64_t utimes,
                       uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (utimensat)");
  return 0;
}

uint64_t sys_epoll_pwait(uint64_t epfd, uint64_t events, uint64_t maxevents,
                         uint64_t timeout, uint64_t sigmask,
                         uint64_t sigsetsize) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (epoll_pwait)");
  return 0;
}

uint64_t sys_signalfd(uint64_t ufd, uint64_t user_mask, uint64_t sizemask) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (signalfd)");
  return 0;
}

uint64_t sys_timerfd_create(uint64_t clockid, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (timerfd_create)");
  return 0;
}

uint64_t sys_eventfd(uint64_t count) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (eventfd)");
  return 0;
}

uint64_t sys_fallocate(uint64_t fd, uint64_t mode, uint64_t offset,
                       uint64_t len) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fallocate)");
  return 0;
}

uint64_t sys_timerfd_settime(uint64_t ufd, uint64_t flags, uint64_t utmr,
                             uint64_t otmr) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (timerfd_settime)");
  return 0;
}

uint64_t sys_timerfd_gettime(uint64_t ufd, uint64_t otmr) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (timerfd_gettime)");
  return 0;
}

uint64_t sys_accept4(uint64_t fd, uint64_t upeer_sockaddr,
                     uint64_t upeer_addrlen, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (accept4)");
  return 0;
}

uint64_t sys_signalfd4(uint64_t ufd, uint64_t user_mask, uint64_t sizemask,
                       uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (signalfd4)");
  return 0;
}

uint64_t sys_eventfd2(uint64_t count) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (eventfd2)");
  return 0;
}

uint64_t sys_epoll_create1(uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (epoll_create1)");
  return 0;
}

uint64_t sys_dup3(uint64_t oldfd, uint64_t newfd, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (dup3)");
  return 0;
}

uint64_t sys_pipe2(uint64_t fildes, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (pipe2)");
  return 0;
}

uint64_t sys_inotify_init1(uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (inotify_init1)");
  return 0;
}

uint64_t sys_preadv(uint64_t fd, uint64_t vec, uint64_t vlen, uint64_t pos_l,
                    uint64_t pos_h) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (preadv)");
  return 0;
}

uint64_t sys_pwritev(uint64_t fd, uint64_t vec, uint64_t vlen, uint64_t pos_l,
                     uint64_t pos_h) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (pwritev)");
  return 0;
}

uint64_t sys_rt_tgsigqueueinfo(uint64_t tgid, uint64_t pid, uint64_t sig,
                               uint64_t uinfo) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (rt_tgsigqueueinfo)");
  return 0;
}

uint64_t sys_perf_event_open(uint64_t attr_uptr, uint64_t pid, uint64_t cpu,
                             uint64_t group_fd, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (perf_event_open)");
  return 0;
}

uint64_t sys_recvmmsg(uint64_t fd, uint64_t mmsg, uint64_t vlen, uint64_t flags,
                      uint64_t timeout) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (recvmmsg)");
  return 0;
}

uint64_t sys_fanotify_init(uint64_t flags, uint64_t event_f_flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fanotify_init)");
  return 0;
}

uint64_t sys_fanotify_mark(uint64_t fanotify_fd, uint64_t flags, uint64_t mask,
                           uint64_t dfd, uint64_t pathname) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (fanotify_mark)");
  return 0;
}

uint64_t sys_prlimit64(uint64_t pid, uint64_t resource, uint64_t new_rlim,
                       uint64_t old_rlim) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (prlimit64)");
  return 0;
}

uint64_t sys_name_to_handle_at(uint64_t dfd, uint64_t name, uint64_t handle,
                               uint64_t mnt_id, uint64_t flag) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (name_to_handle_at)");
  return 0;
}

uint64_t sys_open_by_handle_at(uint64_t mountdirfd, uint64_t handle,
                               uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (open_by_handle_at)");
  return 0;
}

uint64_t sys_clock_adjtime(uint64_t which_clock, uint64_t utx) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (clock_adjtime)");
  return 0;
}

uint64_t sys_syncfs(uint64_t fd) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (syncfs)");
  return 0;
}

uint64_t sys_sendmmsg(uint64_t fd, uint64_t mmsg, uint64_t vlen,
                      uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (sendmmsg)");
  return 0;
}

uint64_t sys_setns(uint64_t fd, uint64_t nstype) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (setns)");
  return 0;
}

uint64_t sys_process_vm_readv(uint64_t pid, uint64_t lvec, uint64_t liovcnt,
                              uint64_t rvec, uint64_t riovcnt, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (process_vm_readv)");
  return 0;
}

uint64_t sys_process_vm_writev(uint64_t pud, uint64_t lvec, uint64_t liovcnt,
                               uint64_t rvec, uint64_t riovcnt,
                               uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (process_vm_writev)");
  return 0;
}

uint64_t sys_kcmp(uint64_t pid1, uint64_t pid2, uint64_t type, uint64_t idx1,
                  uint64_t idx2) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (kcmp)");
  return 0;
}

uint64_t sys_finit_module(uint64_t fd, uint64_t uargs, uint64_t flags) {
  /// TODO: entire syscall
  DEBUG("Call to stubbed syscall (finit_module)");
  return 0;
}
