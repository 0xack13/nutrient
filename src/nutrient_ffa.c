#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "nutrient_ffa.h"

struct ffa
{
    /* The file-descriptor being used */
    int fd;

    /* The current size of the file */
    uint64_t size;

    /* The base pointer - this is the first memory location
     ** where actual data is stored. 
     */
    void *base;
};

uint64_t ffa_get_size(struct ffa *handle)
{
    return handle->size;
}

int ffa_truncate(struct ffa *handle, uint64_t size)
{
    int r = ftruncate(handle->fd, size);
    if (r < 0) {
        return r;
    }

    handle->size = size;
    return 0;
}

void *ffa_get_memory(struct ffa *handle, uint64_t offset)
{
    return (void *) (((intptr_t) handle->base) + offset);
}

struct ffa *ffa_create(const char *filename)
{
    struct ffa *ret;

    ret = malloc(sizeof(struct ffa));
    if (ret == NULL) {
        return NULL;
    }

    ret->size = 0;
    ret->fd = open(filename, O_CREAT | O_RDWR, 0644);

    if (ret->fd < 0) {
        return NULL;
    }

    return ret;
}

struct ffa *ffa_open(const char *filename)
{
    struct stat st;
    struct ffa *ret;

    ret = malloc(sizeof(struct ffa));
    if (ret == NULL) {
        return NULL;
    }

    ret->fd = open(filename, O_RDONLY);
    if (ret->fd < 0) {
        return NULL;
    }

    if (fstat(ret->fd, &st) < 0) {
        return NULL;
    }

    ret->size = st.st_size;

    ret->base = mmap(NULL, ret->size, PROT_READ, MAP_SHARED, ret->fd, 0);
    if (ret->base == MAP_FAILED) {
        close(ret->fd);
        return NULL;
    }

    return ret;
}

uint64_t ffa_free(struct ffa * handle, uint64_t offset, size_t size)
{
    /* At present ffa_free is unimplemented. A future version may keep track
    ** of freed memory and make it available for re-use. But not this one!
    */
    return 0;
}

uint64_t ffa_alloc(struct ffa * handle, size_t size)
{
    if (size <= 0) {
        errno = EINVAL;
        return FFA_ERROR;
    }

    /* Only allocate aligned segments */
    if (size % (sizeof(void *))) {
        size += sizeof(void *) - (size % (sizeof(void *)));
    }

    /* 
     ** Seek to the end of the file, plus the number of bytes
     ** we'd like to write, minus one. This extends the size 
     ** of the file by that amount as soon as a write occurs,
     ** so we write the one byte to make sure 
     */
    if (lseek(handle->fd, size - 1, SEEK_END) < 0) {
        return FFA_ERROR;
    }
    if (write(handle->fd, "\0", 1) != 1) {
        return FFA_ERROR;
    }

    /* Unmap the old data */
    if (handle->size > 0 && munmap(handle->base, handle->size) < 0) {
        return FFA_ERROR;
    }

    /* Reflect the new size of the file */
    handle->size += size;

    /* Map the new data */
    handle->base =
        mmap(handle->base, handle->size, PROT_READ | PROT_WRITE, MAP_SHARED,
             handle->fd, 0);
    if (handle->base == MAP_FAILED) {
        return FFA_ERROR;
    }

    /* Return the offset */
    return handle->size - size;
}

int ffa_sync(struct ffa *handle)
{
    return msync(handle->base, handle->size, MS_SYNC);
}

int ffa_close(struct ffa *handle)
{
    int r;

    r = munmap(handle->base, handle->size);
    if (r != 0) {
        return r;
    }

    r = close(handle->fd);
    if (r != 0) {
        return r;
    }

    free(handle);

    return 0;
}
