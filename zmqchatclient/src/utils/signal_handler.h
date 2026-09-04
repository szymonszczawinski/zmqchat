#define _GNU_SOURCE
#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define S_NOTIFY_MSG " "
#define S_ERROR_MSG  "error writing to self-pipe in signal handler\n"

static int g_signal_pipe_write_fd = -1;

// Handler sygnału: bezpiecznie (async-signal-safe) zapisuje bajt do potoku
static void _s_signal_handler(int signal_value)
{
    (void)signal_value;
    if (g_signal_pipe_write_fd != -1)
    {
        ssize_t rc = write(g_signal_pipe_write_fd, S_NOTIFY_MSG, sizeof(S_NOTIFY_MSG));
        if (rc != sizeof(S_NOTIFY_MSG))
        {
            ssize_t err_rc = write(STDERR_FILENO, S_ERROR_MSG, sizeof(S_ERROR_MSG) - 1);
            (void)err_rc;
            _exit(1);
        }
    }
}

// Ustawia tryb O_NONBLOCK na deskryptorze
static int _make_fd_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * Inicjalizuje obsługę sygnałów (SIGINT, SIGTERM) przy użyciu Self-Pipe Trick.
 * @param read_fd Wskaźnik, pod który zostanie zapisany deskryptor do odczytu dla zmq_poll.
 * @return 0 w przypadku sukcesu, -1 w przypadku błędu.
 */
static inline int setup_signal_handler(int* read_fd)
{
    int pipefds[2];

    if (pipe(pipefds) != 0)
    {
        perror("setup_signal_handler: pipe failed");
        return -1;
    }

    if (_make_fd_nonblocking(pipefds[0]) < 0 || _make_fd_nonblocking(pipefds[1]) < 0)
    {
        perror("setup_signal_handler: fcntl O_NONBLOCK failed");
        close(pipefds[0]);
        close(pipefds[1]);
        return -1;
    }

    g_signal_pipe_write_fd = pipefds[1];

    struct sigaction action;
    action.sa_handler = _s_signal_handler;
    action.sa_flags   = 0;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) < 0 || sigaction(SIGTERM, &action, NULL) < 0)
    {
        perror("setup_signal_handler: sigaction failed");
        close(pipefds[0]);
        close(pipefds[1]);
        return -1;
    }

    *read_fd = pipefds[0];
    return 0;
}

/**
 * Czyszczenie zasobów potoku sygnałowego po zakończeniu pętli programu.
 * @param read_fd Deskryptor odczytu uzyskany z setup_signal_handler.
 */
static inline void cleanup_signal_handler(int read_fd)
{
    if (read_fd >= 0)
        close(read_fd);
    if (g_signal_pipe_write_fd >= 0)
        close(g_signal_pipe_write_fd);
    g_signal_pipe_write_fd = -1;
}

#endif  // SIGNAL_HANDLER_H
