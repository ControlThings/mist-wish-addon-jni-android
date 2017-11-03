//
// Created by jan on 12/5/16.
//

#ifndef REFERENCE_FILESYSTEM_H
#define REFERENCE_FILESYSTEM_H

/* Prototypes for filesystem functions implemented later in this file */
wish_file_t my_fs_open(const char *filename);
int32_t my_fs_close(wish_file_t fileId);
int32_t my_fs_read(wish_file_t fileId, void* buf, size_t count);
int32_t my_fs_write(wish_file_t fileId, const void* buf, size_t count);
int32_t my_fs_lseek(wish_file_t fileId, wish_offset_t offset, int whence);
int32_t my_fs_rename(const char *oldname, const char *newname);
int32_t my_fs_remove(const char *file_name);

#endif //REFERENCE_FILESYSTEM_H
