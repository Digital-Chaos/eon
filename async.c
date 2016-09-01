#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include "utlist.h"
#include "mle.h"

// Return a new async_proc_t
async_proc_t* async_proc_new(editor_t* editor, void* owner, async_proc_t** owner_aproc, char* shell_cmd, int rw, int destroy_on_eof, async_proc_cb_t callback) {
    async_proc_t* aproc;
    aproc = calloc(1, sizeof(async_proc_t));
    aproc->editor = editor;
    aproc->destroy_on_eof = destroy_on_eof;
    async_proc_set_owner(aproc, owner, owner_aproc);
    if (rw) {
        if (!util_popen2(shell_cmd, NULL, &aproc->rfd, &aproc->wfd)) {
            goto async_proc_new_failure;
        }
        aproc->rpipe = fdopen(aproc->rfd, "r");
        aproc->wpipe = fdopen(aproc->wfd, "w");
    } else {
        if (!(aproc->rpipe = popen(shell_cmd, "r"))) {
            goto async_proc_new_failure;
        }
        aproc->rfd = fileno(aproc->rpipe);
    }
    aproc->callback = callback;
    DL_APPEND(editor->async_procs, aproc);
    return aproc;

async_proc_new_failure:
    free(aproc);
    return NULL;
}

// Set aproc owner
int async_proc_set_owner(async_proc_t* aproc, void* owner, async_proc_t** owner_aproc) {
    if (aproc->owner_aproc) {
        *aproc->owner_aproc = NULL;
    }
    *owner_aproc = aproc;
    aproc->owner = owner;
    aproc->owner_aproc = owner_aproc;
    return MLE_OK;
}

// Destroy an async_proc_t
int async_proc_destroy(async_proc_t* aproc) {
    DL_DELETE(aproc->editor->async_procs, aproc);
    if (aproc->owner_aproc) *aproc->owner_aproc = NULL;
    if (aproc->rpipe) pclose(aproc->rpipe);
    if (aproc->wpipe) pclose(aproc->wpipe);
    free(aproc);
    return MLE_OK;
}

// Manage async procs, giving priority to user input. Return 1 if drain should
// be called again, else return 0.
int async_proc_drain_all(async_proc_t* aprocs, int* ttyfd) {
    int maxfd;
    fd_set readfds;
    struct timeval timeout;
    async_proc_t* aproc;
    async_proc_t* aproc_tmp;
    char buf[1024 + 1];
    size_t nbytes;
    int rc;

    // Exit early if no aprocs
    if (!aprocs) return 0;

    // Open ttyfd if not already open
    if (!*ttyfd) {
        if ((*ttyfd = open("/dev/tty", O_RDONLY)) < 0) {
            // TODO error
            return 0;
        }
    }

    // Set timeout to 1s
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    // Add tty to readfds
    FD_ZERO(&readfds);
    FD_SET(*ttyfd, &readfds);

    // Add async procs to readfds
    // Simultaneously check for solo, which takes precedence over everything
    maxfd = *ttyfd;
    DL_FOREACH(aprocs, aproc) {
        if (aproc->is_solo) {
            FD_ZERO(&readfds);
            FD_SET(aproc->rfd, &readfds);
            maxfd = aproc->rfd;
            break;
        } else {
            FD_SET(aproc->rfd, &readfds);
            if (aproc->rfd > maxfd) maxfd = aproc->rfd;
        }
    }

    // Perform select
    rc = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
    if (rc < 0) {
        return 0; // TODO Display errors
    } else if (rc == 0) {
        return 1; // Nothing to read, call again
    }

    if (FD_ISSET(*ttyfd, &readfds)) {
        // Immediately give priority to user input
        return 0;
    } else {
        // Read async procs
        DL_FOREACH_SAFE(aprocs, aproc, aproc_tmp) {
            // Read and invoke callback
            if (FD_ISSET(aproc->rfd, &readfds)) {
                nbytes = fread(&buf, sizeof(char), 1024, aproc->rpipe);
                buf[nbytes] = '\0';
                aproc->callback(aproc, buf, nbytes);
            }
            // Close and free if eof, error, or timeout
            if (ferror(aproc->rpipe) || aproc->is_done || (aproc->destroy_on_eof && feof(aproc->rpipe))) {
                async_proc_destroy(aproc);
            }
        }
    }

    return 1;
}
