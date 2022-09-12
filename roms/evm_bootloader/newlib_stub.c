

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>


#include "main.h"

void osl_putc(char c);

extern unsigned int __HEAP_START;
static void *heap = NULL;

caddr_t _sbrk(int incr) {
    void *prev_heap;

    if (heap == NULL) {
        heap = &__HEAP_START;
    }
    prev_heap = heap;
    if (((uint32_t)heap + incr) > HEAP_END) {
        //printf("heap:%p, incr:%d\n", heap, incr);

        return 0;
    }
    heap += incr;

    return (caddr_t)prev_heap;
}


int _close_r(struct _reent *pReent, int fd) {
    pReent->_errno = ENOTSUP;
    return -1;
}

int _execve_r(struct _reent *pReent, const char *filename, char *const *argv, char *const *envp) {
    pReent->_errno = ENOTSUP;
    return -1;
}

int _fcntl_r(struct _reent *pReent, int fd, int cmd, int arg) {
    pReent->_errno = ENOTSUP;
    return -1;
}

int _fork_r(struct _reent *pReent) {
    pReent->_errno = ENOTSUP;
    return -1;
}

int _fstat_r(struct _reent *pReent, int file, struct stat *st) {
    pReent->_errno = ENOTSUP;
    return -1;
}

int _getpid_r(struct _reent *pReent) {
    pReent->_errno = ENOTSUP;
    return -1;
}

int _isatty_r(struct _reent *pReent, int file) {
    pReent->_errno = ENOTSUP;
    return -1;
}

int _kill_r(struct _reent *pReent, int pid, int signal) {
    pReent->_errno = ENOTSUP;
    return -1;
}

int _link_r(struct _reent *pReent, const char *oldfile, const char *newfile) {
    pReent->_errno = ENOTSUP;
    return -1;
}

_off_t _lseek_r(struct _reent *pReent, int file, _off_t offset, int whence) {
    pReent->_errno = ENOTSUP;
    return -1;
}

int _mkdir_r(struct _reent *pReent, const char *pathname, int mode) {
    pReent->_errno = ENOTSUP;
    return -1;
}

int _open_r(struct _reent *pReent, const char *file, int flags, int mode) {
    pReent->_errno = ENOTSUP;
    return -1;
}

_ssize_t _read_r(struct _reent *pReent, int fd, void *ptr, size_t len) {
    pReent->_errno = ENOTSUP;
    return -1;
}

_ssize_t _rename_r(struct _reent *pReent, const char *oldname, const char *newname) {
    pReent->_errno = ENOTSUP;
    return -1;
}

int _stat_r(struct _reent *pReent, const char *__restrict __path, struct stat *__restrict __sbuf) {
    pReent->_errno = ENOTSUP;
    return -1;
}

_CLOCK_T_ _times_r(struct _reent *pReent, struct tms *tbuf) {
    pReent->_errno = ENOTSUP;
    return -1;
}

int _unlink_r(struct _reent *pReent, const char *filename) {
    pReent->_errno = ENOTSUP;
    return -1;
}

int _wait_r(struct _reent *pReent, int *wstat) {
    pReent->_errno = ENOTSUP;
    return -1;
}



_ssize_t _write_r(struct _reent *pReent, int fd, const void *buf, size_t nbytes) {

    int i;

    char *c = (char *)buf;

    if (fd < 3) {
        pReent->_errno = 0;
        while((*c) && (nbytes--))
        {
            osl_putc(*c++);
        }

        return i;
    }
    return -1;
}

int _gettimeofday_r(struct _reent *pReent, struct timeval *__tp, void *__tzp) {
    pReent->_errno = ENOTSUP;
    return -1;
}

/*
 * write "len" of char from "ptr" to file id "fd"
 * Return number of char written.
 *
 * Only work for STDOUT, STDIN, and STDERR
 */

int fsync(int __fd) {

    return 0;
}

char *getcwd(char *buf, size_t size) {

    return NULL;
}

void abort(void) {
    // Abort called
    printf("Abort");
    while (1)
        ;
}

void _exit(int i) {
    printf("exit");
    while (1)
        ;
}





