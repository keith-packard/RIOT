/*
 * Copyright (C) 2019 Koen Zandberg
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup    sys_picolibc PicoLibc system call
 * @ingroup     sys
 * @brief       PicoLibc system call
 * @{
 *
 * @file
 * @brief       PicoLibc system call implementations
 *
 * @author      Koen Zandberg <koen@bergzand.net>
 *
 * @}
 */

#include <errno.h>
#include <stdio.h>
#include <sys/times.h>

#include "log.h"
#include "periph/pm.h"
#include "stdio_base.h"

/**
 * @brief Exit a program without cleaning up files
 *
 * If your system doesn't provide this, it is best to avoid linking with subroutines that
 * require it (exit, system).
 *
 * @param n     the exit code, 0 for all OK, >0 for not OK
 */
void __attribute__((__noreturn__))
_exit(int n)
{
    LOG_INFO("#! exit %i: powering off\n", n);
    pm_off();
    while(1);
}

/**
 * @brief Send a signal to a thread
 *
 * @param[in] pid the pid to send to
 * @param[in] sig the signal to send
 *
 * @return    always returns -1 to signal error
 */
__attribute__ ((weak))
int kill(pid_t pid, int sig)
{
    (void) pid;
    (void) sig;
    errno = ESRCH;                         /* not implemented yet */
    return -1;
}

#include "mutex.h"

static mutex_t picolibc_put_mutex = MUTEX_INIT;

#define PICOLIBC_STDOUT_BUFSIZE	64

static char picolibc_stdout[PICOLIBC_STDOUT_BUFSIZE];
static int picolibc_stdout_queued;

static void _picolibc_flush(void)
{
    if (picolibc_stdout_queued) {
	stdio_write(picolibc_stdout, picolibc_stdout_queued);
	picolibc_stdout_queued = 0;
    }
}

static int picolibc_put(char c, FILE *file)
{
    (void)file;
    mutex_lock(&picolibc_put_mutex);
    picolibc_stdout[picolibc_stdout_queued++] = c;
    if (picolibc_stdout_queued == PICOLIBC_STDOUT_BUFSIZE || c == '\n')
	_picolibc_flush();
    mutex_unlock(&picolibc_put_mutex);
    return 1;
}

static int picolibc_flush(FILE *file)
{
    (void)file;
    mutex_lock(&picolibc_put_mutex);
    _picolibc_flush();
    mutex_unlock(&picolibc_put_mutex);
    return 0;
}

static int picolibc_get(FILE *file)
{
    (void)file;
    picolibc_flush(NULL);
    char c = 0;
    stdio_read(&c, 1);
    return c;
}

FILE picolibc_stdio =
    FDEV_SETUP_STREAM(picolibc_put, picolibc_get, picolibc_flush, _FDEV_SETUP_RW);

FILE *const __iob[] = {
    &picolibc_stdio,    /* stdin  */
    &picolibc_stdio,    /* stdout */
    &picolibc_stdio,    /* stderr */
};

/*
 * All output is directed to stdio_uart, independent of the given file descriptor.
 * The write call will further block until the byte is actually written to the UART.
 */
_READ_WRITE_RETURN_TYPE write(int fd, const void *data, size_t count)
{
    (void) fd;
    return stdio_write(data, count);
}

#include <thread.h>
/**
 * @brief Get the process-ID of the current thread
 *
 * @return      the process ID of the current thread
 */
pid_t getpid(void)
{
    return thread_getpid();
}

int close(int fd)
{
    (void) fd;
    errno = ENODEV;
    return -1;
}

/**
 * Current process times (not implemented).
 *
 * @param[out]  ptms    Not modified.
 *
 * @return  -1, this function always fails. errno is set to ENOSYS.
 */
clock_t times(struct tms *ptms)
{
    (void)ptms;
    errno = ENOSYS;

    return (-1);
}
