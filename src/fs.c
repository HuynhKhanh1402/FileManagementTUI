#define _XOPEN_SOURCE 700
#include "fs.h"
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

static int entry_cmp(const void *pa, const void *pb) {
    const fm_entry *a = pa;
    const fm_entry *b = pb;
    if (a->is_dir && !b->is_dir) return -1;
    if (!a->is_dir && b->is_dir) return 1;
    return strcasecmp(a->name, b->name);
}

int fm_read_dir(const char *path, fm_entry **entries_out) {
    DIR *d = opendir(path);
    if (!d) return -1;
    struct dirent *ent;
    fm_entry *arr = NULL;
    int cap = 0, n = 0;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0) continue;
        if (n + 1 > cap) {
            cap = cap ? cap * 2 : 64;
            fm_entry *tmp = realloc(arr, cap * sizeof(fm_entry));
            if (!tmp) { free(arr); closedir(d); return -1; }
            arr = tmp;
        }
        fm_entry *e = &arr[n++];
        snprintf(e->name, sizeof(e->name), "%s", ent->d_name);
        snprintf(e->path, sizeof(e->path), "%s/%s", path, ent->d_name);
        if (lstat(e->path, &e->st) == -1) {
            memset(&e->st, 0, sizeof(e->st));
            e->is_dir = 0;
        } else {
            e->is_dir = S_ISDIR(e->st.st_mode);
        }
    }
    closedir(d);
    // simple sort: directories first, then name
    if (n > 0) qsort(arr, n, sizeof(fm_entry), entry_cmp);
    *entries_out = arr;
    return n;
}

int fm_mkdir(const char *path) {
    return mkdir(path, 0755);
}

int fm_remove(const char *path) {
    struct stat st;
    if (lstat(path, &st) == -1) return -1;
    if (S_ISDIR(st.st_mode)) return rmdir(path);
    return unlink(path);
}

int fm_rename(const char *oldpath, const char *newpath) {
    return rename(oldpath, newpath);
}

int fm_copy_file(const char *src, const char *dst) {
    int in = open(src, O_RDONLY);
    if (in < 0) return -1;
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) { close(in); return -1; }
    char buf[8192];
    ssize_t r;
    while ((r = read(in, buf, sizeof(buf))) > 0) {
        char *p = buf;
        ssize_t w;
        while (r > 0 && (w = write(out, p, r)) > 0) { r -= w; p += w; }
        if (w < 0) { close(in); close(out); return -1; }
    }
    close(in); close(out);
    return 0;
}

int fm_create_file(const char *path) {
    // Create empty file similar to touch command
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) return -1;
    close(fd);
    return 0;
}

int fm_edit_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;  // File doesn't exist
    }
    
    // Check if it's a regular file
    if (!S_ISREG(st.st_mode)) {
        return -2;  // Not a regular file
    }
    
    // Determine which editor to use: nano > vim > error
    const char *editor = NULL;
    
    // Check if nano is available
    if (system("command -v nano > /dev/null 2>&1") == 0) {
        editor = "nano";
    }
    // If nano not found, check for vim
    else if (system("command -v vim > /dev/null 2>&1") == 0) {
        editor = "vim";
    }
    // Neither nano nor vim available
    else {
        return -3;  // No suitable editor found
    }
    
    // Build command (simple quoting with single quotes)
    char command[PATH_MAX * 2];
    snprintf(command, sizeof(command), "%s '%s'", editor, path);
    
    // Execute editor
    int result = system(command);
    
    return result;
}

ssize_t fm_read_file(const char *path, char **content_out) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return -1;
    }
    
    // Allocate buffer for file content plus null terminator
    size_t size = st.st_size;
    char *buf = malloc(size + 1);
    if (!buf) {
        close(fd);
        return -1;
    }
    
    // Read file content
    ssize_t total = 0;
    while (total < (ssize_t)size) {
        ssize_t n = read(fd, buf + total, size - total);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            free(buf);
            close(fd);
            return -1;
        }
        total += n;
    }
    
    buf[total] = '\0';
    close(fd);
    *content_out = buf;
    return total;
}
