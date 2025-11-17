#ifndef FM_FS_H
#define FM_FS_H

#include <sys/types.h>
#include <sys/stat.h>

typedef struct fm_entry {
    char name[512];
    char path[1024];
    struct stat st;
    int is_dir;
} fm_entry;

// Read directory entries into dynamically allocated array; returns count, entries is malloc'd (caller frees)
int fm_read_dir(const char *path, fm_entry **entries_out);

// Create directory
int fm_mkdir(const char *path);

// Remove file or empty directory
int fm_remove(const char *path);

// Rename or move
int fm_rename(const char *oldpath, const char *newpath);

// Copy file (simple, not preserving metadata)
int fm_copy_file(const char *src, const char *dst);

// Create empty file (similar to touch)
int fm_create_file(const char *path);

// Open file in editor (nano)
int fm_edit_file(const char *path);

// Read file content into buffer; returns bytes read on success, -1 on error
// Caller must free *content_out
ssize_t fm_read_file(const char *path, char **content_out);

#endif // FM_FS_H
