#ifndef __ERVP_FLASH_FS_H__
#define __ERVP_FLASH_FS_H__

#include "lfs.h"

#ifndef FLASHFS_MAX_OPEN_FILES
#define FLASHFS_MAX_OPEN_FILES 4
#endif

#ifndef EOF
#define EOF ((int)-1)
#endif

#ifndef SEEK_SET
#define SEEK_SET (0)
#define SEEK_CUR (1)
#define SEEK_END (2)
#endif

typedef struct flashfs_file FLASHFS_FILE;
typedef lfs_dir_t flashfs_dir_t;
typedef struct lfs_info flashfs_info_t;
typedef struct lfs_fsinfo flashfs_fsinfo_t;

int flashfs_format(void);
int flashfs_mount(void);
int flashfs_unmount(void);

int flashfs_remove(const char* path);
int flashfs_rename(const char* oldpath, const char* newpath);
int flashfs_stat(const char* path, flashfs_info_t* info);
lfs_ssize_t flashfs_getattr(const char* path, uint8_t type, void* buffer, lfs_size_t size);
int flashfs_setattr(const char* path, uint8_t type, const void* buffer, lfs_size_t size);
int flashfs_removeattr(const char* path, uint8_t type);

FLASHFS_FILE* flashfs_fopen(const char* path, const char* mode);
int flashfs_fclose(FLASHFS_FILE* fp);
int flashfs_fflush(FLASHFS_FILE* fp);
size_t flashfs_fread(void* buffer, size_t size, size_t count, FLASHFS_FILE* fp);
size_t flashfs_fwrite(const void* buffer, size_t size, size_t count, FLASHFS_FILE* fp);
int flashfs_fgetc(FLASHFS_FILE* fp);
int flashfs_fputc(int c, FLASHFS_FILE* fp);
char* flashfs_fgets(char* buffer, int n, FLASHFS_FILE* fp);
int flashfs_feof(FLASHFS_FILE* fp);
int flashfs_ferror(FLASHFS_FILE* fp);
int flashfs_fseek(FLASHFS_FILE* fp, long offset, int whence);
long flashfs_ftell(FLASHFS_FILE* fp);
void flashfs_frewind(FLASHFS_FILE* fp);
int flashfs_ftruncate(FLASHFS_FILE* fp, long size);
long flashfs_get_file_size(FLASHFS_FILE* fp);

int flashfs_mkdir(const char* path);
int flashfs_dir_open(flashfs_dir_t* dir, const char* path);
int flashfs_dir_close(flashfs_dir_t* dir);
int flashfs_dir_read(flashfs_dir_t* dir, flashfs_info_t* info);
int flashfs_dir_seek(flashfs_dir_t* dir, lfs_off_t off);
lfs_soff_t flashfs_dir_tell(flashfs_dir_t* dir);
int flashfs_dir_rewind(flashfs_dir_t* dir);

int flashfs_fs_stat(flashfs_fsinfo_t* fsinfo);
lfs_ssize_t flashfs_fs_size(void);
int flashfs_fs_traverse(int (*cb)(void*, lfs_block_t), void* data);
int flashfs_fs_mkconsistent(void);
int flashfs_fs_gc(void);
int flashfs_fs_grow(lfs_size_t block_count);

#endif
