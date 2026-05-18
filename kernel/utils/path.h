#pragma once

static inline const char *path_filename(const char *path) {
    const char *last = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/')
            last = p + 1;
    }
    return last;
}

// Writes the directory portion of path into out (up to out_size bytes).
// Strips the trailing slash unless the directory is "/".
static inline void path_dirname(const char *path, char *out, int out_size) {
    const char *last_slash = 0;
    for (const char *p = path; *p; p++) {
        if (*p == '/')
            last_slash = p;
    }
    if (!last_slash) {
        out[0] = '.'; out[1] = '\0';
        return;
    }
    if (last_slash == path) {
        out[0] = '/'; out[1] = '\0';
        return;
    }
    int len = (int)(last_slash - path);
    if (len >= out_size)
        len = out_size - 1;
    for (int i = 0; i < len; i++)
        out[i] = path[i];
    out[len] = '\0';
}
