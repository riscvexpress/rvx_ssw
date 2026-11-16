#include "flash_info.h"
#include "trvp_spi_flash.h"
#include "ervp_platform_controller_api.h"
#include "ervp_assert.h"
#include "ervp_flash_fs.h"

#ifndef FLASH_SECTOR_SIZE
#error "this board has no flash for a file system"
#endif

#define FLASHFS_CACHE_SIZE 256
#define FLASHFS_LOOKAHEAD_SIZE 32

struct flashfs_file {
	lfs_file_t lfs_file;
	struct lfs_file_config config;
	uint8_t cache[FLASHFS_CACHE_SIZE] __attribute__((aligned(4)));
	int in_use;
	int eof;
	int error;
};

static lfs_t lfs;
static FLASHFS_FILE file_pool[FLASHFS_MAX_OPEN_FILES];
static uint8_t read_cache[FLASHFS_CACHE_SIZE] __attribute__((aligned(4)));
static uint8_t prog_cache[FLASHFS_CACHE_SIZE] __attribute__((aligned(4)));
static uint8_t lookahead_buffer[FLASHFS_LOOKAHEAD_SIZE] __attribute__((aligned(4)));

static unsigned int get_flash_addr(const struct lfs_config* c, lfs_block_t block, lfs_off_t off)
{
	return get_flash_fs_baseaddr() + (block * c->block_size) + off;
}

static int flash_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size)
{
	spi_flash_configure();
	spi_flash_read(get_flash_addr(c, block, off), size, buffer);
	return LFS_ERR_OK;
}

static int flash_prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size)
{
	spi_flash_configure();
	spi_flash_program(get_flash_addr(c, block, off), size, (unsigned char*)buffer);
	return LFS_ERR_OK;
}

static int flash_erase(const struct lfs_config* c, lfs_block_t block)
{
	spi_flash_configure();
	spi_flash_erase_sector(get_flash_addr(c, block, 0));
	return LFS_ERR_OK;
}

static int flash_sync(const struct lfs_config* c)
{
	return LFS_ERR_OK;
}

static struct lfs_config config = {
	.read = flash_read,
	.prog = flash_prog,
	.erase = flash_erase,
	.sync = flash_sync,
	.read_size = 1,
	.prog_size = SPI_FLASH_PAGE_SIZE,
	.block_size = FLASH_SECTOR_SIZE,
	.block_cycles = 500,
	.cache_size = FLASHFS_CACHE_SIZE,
	.lookahead_size = FLASHFS_LOOKAHEAD_SIZE,
	.read_buffer = read_cache,
	.prog_buffer = prog_cache,
	.lookahead_buffer = lookahead_buffer,
};

static int set_block_count(void)
{
	unsigned int size = get_flash_fs_size();
	if((get_flash_fs_baseaddr() == 0) || (size == 0) || ((size % FLASH_SECTOR_SIZE) != 0))
		return LFS_ERR_INVAL;
	config.block_count = size / FLASH_SECTOR_SIZE;
	return LFS_ERR_OK;
}

int flashfs_format(void)
{
	int result = set_block_count();
	if(result == LFS_ERR_OK)
		result = lfs_format(&lfs, &config);
	return result;
}

int flashfs_mount(void)
{
	int result = set_block_count();
	if(result == LFS_ERR_OK)
		result = lfs_mount(&lfs, &config);
	return result;
}

static int close_file(FLASHFS_FILE* fp)
{
	fp->in_use = 0;
	return lfs_file_close(&lfs, &fp->lfs_file);
}

int flashfs_unmount(void)
{
	int i, close_result, result = LFS_ERR_OK;
	for(i = 0; i < FLASHFS_MAX_OPEN_FILES; i++)
	{
		if(file_pool[i].in_use)
		{
			close_result = close_file(&file_pool[i]);
			if(result == LFS_ERR_OK)
				result = close_result;
		}
	}
	close_result = lfs_unmount(&lfs);
	return (result == LFS_ERR_OK) ? close_result : result;
}

int flashfs_remove(const char* path) { return lfs_remove(&lfs, path); }
int flashfs_rename(const char* oldpath, const char* newpath) { return lfs_rename(&lfs, oldpath, newpath); }
int flashfs_stat(const char* path, flashfs_info_t* info) { return lfs_stat(&lfs, path, info); }
lfs_ssize_t flashfs_getattr(const char* path, uint8_t type, void* buffer, lfs_size_t size) { return lfs_getattr(&lfs, path, type, buffer, size); }
int flashfs_setattr(const char* path, uint8_t type, const void* buffer, lfs_size_t size) { return lfs_setattr(&lfs, path, type, buffer, size); }
int flashfs_removeattr(const char* path, uint8_t type) { return lfs_removeattr(&lfs, path, type); }

static int parse_mode(const char* mode)
{
	int flags;
	int is_update = 0;
	if(mode == NULL)
		return 0;
	switch(mode[0])
	{
		case 'r': flags = LFS_O_RDONLY; break;
		case 'w': flags = LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC; break;
		case 'a': flags = LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND; break;
		default: return 0;
	}
	for(mode++; *mode != '\0'; mode++)
	{
		if(*mode == '+')
			is_update = 1;
		else if(*mode != 'b')
			return 0;
	}
	if(is_update)
		flags |= LFS_O_RDWR;
	return flags;
}

static int is_open(FLASHFS_FILE* fp)
{
	return (fp != NULL) && fp->in_use;
}

static int get_total_size(FLASHFS_FILE* fp, size_t size, size_t count, size_t* total)
{
	if((size == 0) || (count == 0))
		return 0;
	if(count > ((size_t)INT32_MAX / size))
	{
		fp->error = LFS_ERR_INVAL;
		return 0;
	}
	*total = size * count;
	return 1;
}

FLASHFS_FILE* flashfs_fopen(const char* path, const char* mode)
{
	int i;
	int flags = parse_mode(mode);
	if(flags == 0)
		return NULL;
	for(i = 0; i < FLASHFS_MAX_OPEN_FILES; i++)
	{
		FLASHFS_FILE* fp = &file_pool[i];
		if(!fp->in_use)
		{
			fp->config = (struct lfs_file_config){.buffer = fp->cache};
			if(lfs_file_opencfg(&lfs, &fp->lfs_file, path, flags, &fp->config) != LFS_ERR_OK)
				return NULL;
			fp->eof = 0;
			fp->error = 0;
			fp->in_use = 1;
			return fp;
		}
	}
	assert_must_msg(0, "flashfs file pool is full");
	return NULL;
}

int flashfs_fclose(FLASHFS_FILE* fp)
{
	if(!is_open(fp))
		return EOF;
	return (close_file(fp) == LFS_ERR_OK) ? 0 : EOF;
}

int flashfs_fflush(FLASHFS_FILE* fp)
{
	int result;
	if(!is_open(fp))
		return EOF;
	result = lfs_file_sync(&lfs, &fp->lfs_file);
	if(result < 0)
	{
		fp->error = result;
		return EOF;
	}
	return 0;
}

size_t flashfs_fread(void* buffer, size_t size, size_t count, FLASHFS_FILE* fp)
{
	size_t total;
	lfs_ssize_t result;
	if(!is_open(fp) || !get_total_size(fp, size, count, &total))
		return 0;
	if((fp->lfs_file.flags & LFS_O_RDONLY) == 0)
	{
		fp->error = LFS_ERR_BADF;
		return 0;
	}
	result = lfs_file_read(&lfs, &fp->lfs_file, buffer, total);
	if(result < 0)
	{
		fp->error = result;
		return 0;
	}
	if((size_t)result < total)
		fp->eof = 1;
	return result / size;
}

size_t flashfs_fwrite(const void* buffer, size_t size, size_t count, FLASHFS_FILE* fp)
{
	size_t total;
	lfs_ssize_t result;
	if(!is_open(fp) || !get_total_size(fp, size, count, &total))
		return 0;
	if((fp->lfs_file.flags & LFS_O_WRONLY) == 0)
	{
		fp->error = LFS_ERR_BADF;
		return 0;
	}
	result = lfs_file_write(&lfs, &fp->lfs_file, buffer, total);
	if(result < 0)
	{
		fp->error = result;
		return 0;
	}
	return result / size;
}

int flashfs_fgetc(FLASHFS_FILE* fp)
{
	uint8_t c;
	return (flashfs_fread(&c, 1, 1, fp) == 1) ? c : EOF;
}

int flashfs_fputc(int c, FLASHFS_FILE* fp)
{
	uint8_t value = (uint8_t)c;
	return (flashfs_fwrite(&value, 1, 1, fp) == 1) ? value : EOF;
}

char* flashfs_fgets(char* buffer, int n, FLASHFS_FILE* fp)
{
	int i = 0;
	if(n <= 0)
		return NULL;
	while(i < (n - 1))
	{
		int c = flashfs_fgetc(fp);
		if(c == EOF)
			break;
		buffer[i++] = (char)c;
		if(c == '\n')
			break;
	}
	if(i == 0)
		return NULL;
	buffer[i] = '\0';
	return buffer;
}

int flashfs_feof(FLASHFS_FILE* fp)
{
	return is_open(fp) && fp->eof;
}

int flashfs_ferror(FLASHFS_FILE* fp)
{
	return is_open(fp) ? fp->error : LFS_ERR_BADF;
}

int flashfs_fseek(FLASHFS_FILE* fp, long offset, int whence)
{
	lfs_soff_t result;
	if(!is_open(fp))
		return EOF;
	result = lfs_file_seek(&lfs, &fp->lfs_file, offset, whence);
	if(result < 0)
	{
		fp->error = result;
		return EOF;
	}
	fp->eof = 0;
	return 0;
}

long flashfs_ftell(FLASHFS_FILE* fp)
{
	return is_open(fp) ? lfs_file_tell(&lfs, &fp->lfs_file) : EOF;
}

void flashfs_frewind(FLASHFS_FILE* fp)
{
	if(flashfs_fseek(fp, 0, SEEK_SET) == 0)
		fp->error = 0;
}

int flashfs_ftruncate(FLASHFS_FILE* fp, long size)
{
	int result;
	if(!is_open(fp))
		return EOF;
	if(((fp->lfs_file.flags & LFS_O_WRONLY) == 0) || (size < 0))
	{
		fp->error = (size < 0) ? LFS_ERR_INVAL : LFS_ERR_BADF;
		return EOF;
	}
	result = lfs_file_truncate(&lfs, &fp->lfs_file, size);
	if(result < 0)
	{
		fp->error = result;
		return EOF;
	}
	return 0;
}

long flashfs_get_file_size(FLASHFS_FILE* fp)
{
	return is_open(fp) ? lfs_file_size(&lfs, &fp->lfs_file) : EOF;
}

int flashfs_mkdir(const char* path) { return lfs_mkdir(&lfs, path); }
int flashfs_dir_open(flashfs_dir_t* dir, const char* path) { return lfs_dir_open(&lfs, dir, path); }
int flashfs_dir_close(flashfs_dir_t* dir) { return lfs_dir_close(&lfs, dir); }
int flashfs_dir_read(flashfs_dir_t* dir, flashfs_info_t* info) { return lfs_dir_read(&lfs, dir, info); }
int flashfs_dir_seek(flashfs_dir_t* dir, lfs_off_t off) { return lfs_dir_seek(&lfs, dir, off); }
lfs_soff_t flashfs_dir_tell(flashfs_dir_t* dir) { return lfs_dir_tell(&lfs, dir); }
int flashfs_dir_rewind(flashfs_dir_t* dir) { return lfs_dir_rewind(&lfs, dir); }

int flashfs_fs_stat(flashfs_fsinfo_t* fsinfo) { return lfs_fs_stat(&lfs, fsinfo); }
lfs_ssize_t flashfs_fs_size(void) { return lfs_fs_size(&lfs); }
int flashfs_fs_traverse(int (*cb)(void*, lfs_block_t), void* data) { return lfs_fs_traverse(&lfs, cb, data); }
int flashfs_fs_mkconsistent(void) { return lfs_fs_mkconsistent(&lfs); }
int flashfs_fs_gc(void) { return lfs_fs_gc(&lfs); }
int flashfs_fs_grow(lfs_size_t block_count) { return lfs_fs_grow(&lfs, block_count); }
