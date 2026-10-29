/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ramfs.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lock.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_vfs.h"

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#define RAMFS_MAX_INSTANCES 4

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
#define RAMFS_USE_VFS_OPS 1
#else
#define RAMFS_USE_VFS_OPS 0
#endif

typedef enum {
    RAMFS_NODE_DIR = 0,
    RAMFS_NODE_FILE,
} ramfs_node_type_t;

typedef struct ramfs_node {
    char *name;
    ramfs_node_type_t type;
    struct ramfs_node *parent;
    struct ramfs_node *children;
    struct ramfs_node *next;
    uint8_t *data;
    size_t size;
    time_t mtime;
    unsigned open_count;
    unsigned dir_open_count;
} ramfs_node_t;

typedef struct {
    bool used;
    int flags;
    off_t offset;
    ramfs_node_t *node;
} ramfs_file_t;

typedef struct {
    char base_path[ESP_VFS_PATH_MAX + 1];
    size_t max_files;
    size_t max_bytes;
    size_t used_bytes;
    uint32_t caps;
    _lock_t lock;
    ramfs_node_t root;
    ramfs_file_t files[];
} ramfs_ctx_t;

#ifdef CONFIG_VFS_SUPPORT_DIR
typedef struct {
    DIR dir;
    ramfs_node_t *node;
    long offset;
    struct dirent cur_dirent;
} ramfs_dir_t;
#endif

static const char *TAG = "ramfs";
static ramfs_ctx_t *s_contexts[RAMFS_MAX_INSTANCES];

static bool ramfs_caps_are_valid(uint32_t caps)
{
    return caps != 0 && (caps & MALLOC_CAP_8BIT) != 0;
}

static void *ramfs_ctx_malloc(const ramfs_ctx_t *ctx, size_t size)
{
    return heap_caps_malloc(size, ctx->caps);
}

static void *ramfs_ctx_calloc(const ramfs_ctx_t *ctx, size_t count, size_t size)
{
    return heap_caps_calloc(count, size, ctx->caps);
}

static void *ramfs_ctx_realloc(const ramfs_ctx_t *ctx, void *ptr, size_t size)
{
    return heap_caps_realloc(ptr, size, ctx->caps);
}

static char *ramfs_ctx_strdup(const ramfs_ctx_t *ctx, const char *src)
{
    size_t len;
    char *dst;

    if (!src) {
        errno = EINVAL;
        return NULL;
    }

    len = strlen(src) + 1;
    dst = ramfs_ctx_malloc(ctx, len);
    if (!dst) {
        errno = ENOMEM;
        return NULL;
    }
    memcpy(dst, src, len);
    return dst;
}

static void ramfs_copy_string(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    len = strnlen(src, dst_size - 1);
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static size_t ramfs_find_context_index(const char *base_path)
{
    for (size_t i = 0; i < RAMFS_MAX_INSTANCES; i++) {
        if (s_contexts[i] && strcmp(s_contexts[i]->base_path, base_path) == 0) {
            return i;
        }
    }
    return RAMFS_MAX_INSTANCES;
}

static size_t ramfs_find_free_context_index(void)
{
    for (size_t i = 0; i < RAMFS_MAX_INSTANCES; i++) {
        if (!s_contexts[i]) {
            return i;
        }
    }
    return RAMFS_MAX_INSTANCES;
}

static ramfs_node_t *ramfs_find_child(ramfs_node_t *dir, const char *name)
{
    for (ramfs_node_t *node = dir ? dir->children : NULL; node; node = node->next) {
        if (strcmp(node->name, name) == 0) {
            return node;
        }
    }
    return NULL;
}

static bool ramfs_dir_is_empty(const ramfs_node_t *dir)
{
    return !dir || dir->children == NULL;
}

static bool ramfs_node_is_busy(const ramfs_node_t *node)
{
    return node && (node->open_count > 0 || node->dir_open_count > 0);
}

static bool ramfs_tree_is_busy(const ramfs_node_t *node)
{
    if (!node) {
        return false;
    }
    if (ramfs_node_is_busy(node)) {
        return true;
    }
    for (const ramfs_node_t *child = node->children; child; child = child->next) {
        if (ramfs_tree_is_busy(child)) {
            return true;
        }
    }
    return false;
}

static bool ramfs_tree_has_open_dir(const ramfs_node_t *node)
{
    if (!node) {
        return false;
    }
    if (node->dir_open_count > 0) {
        return true;
    }
    for (const ramfs_node_t *child = node->children; child; child = child->next) {
        if (ramfs_tree_has_open_dir(child)) {
            return true;
        }
    }
    return false;
}

static bool ramfs_is_descendant_of(const ramfs_node_t *node, const ramfs_node_t *ancestor)
{
    for (const ramfs_node_t *cur = node; cur; cur = cur->parent) {
        if (cur == ancestor) {
            return true;
        }
    }
    return false;
}

static void ramfs_free_node_tree(ramfs_ctx_t *ctx, ramfs_node_t *node)
{
    while (node && node->children) {
        ramfs_node_t *child = node->children;
        node->children = child->next;
        ramfs_free_node_tree(ctx, child);
    }

    if (node && node->parent) {
        heap_caps_free(node->data);
        heap_caps_free(node->name);
        heap_caps_free(node);
    }
}

static int ramfs_validate_component(const char *name)
{
    if (!name || !name[0] || strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || strlen(name) > NAME_MAX) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

static ramfs_node_t *ramfs_resolve_node(ramfs_ctx_t *ctx, const char *path)
{
    char *copy;
    char *saveptr = NULL;
    char *part;
    ramfs_node_t *cur = &ctx->root;

    if (!path || path[0] != '/') {
        errno = EINVAL;
        return NULL;
    }
    if (strcmp(path, "/") == 0) {
        return &ctx->root;
    }

    copy = ramfs_ctx_strdup(ctx, path + 1);
    if (!copy) {
        ESP_LOGE(TAG, "resolve alloc failed");
        errno = ENOMEM;
        return NULL;
    }

    for (part = strtok_r(copy, "/", &saveptr); part; part = strtok_r(NULL, "/", &saveptr)) {
        if (ramfs_validate_component(part) != 0 || cur->type != RAMFS_NODE_DIR) {
            heap_caps_free(copy);
            errno = cur->type == RAMFS_NODE_DIR ? errno : ENOTDIR;
            return NULL;
        }
        cur = ramfs_find_child(cur, part);
        if (!cur) {
            heap_caps_free(copy);
            errno = ENOENT;
            return NULL;
        }
    }

    heap_caps_free(copy);
    return cur;
}

static ramfs_node_t *ramfs_resolve_parent(ramfs_ctx_t *ctx, const char *path, char *name, size_t name_size)
{
    char *copy;
    char *saveptr = NULL;
    char *part;
    char *next;
    ramfs_node_t *cur = &ctx->root;

    if (!path || path[0] != '/' || strcmp(path, "/") == 0 || !name || name_size == 0) {
        errno = EINVAL;
        return NULL;
    }

    copy = ramfs_ctx_strdup(ctx, path + 1);
    if (!copy) {
        ESP_LOGE(TAG, "parent resolve alloc failed");
        errno = ENOMEM;
        return NULL;
    }

    part = strtok_r(copy, "/", &saveptr);
    while (part) {
        if (ramfs_validate_component(part) != 0) {
            heap_caps_free(copy);
            return NULL;
        }

        next = strtok_r(NULL, "/", &saveptr);
        if (!next) {
            ramfs_copy_string(name, name_size, part);
            heap_caps_free(copy);
            return cur;
        }

        cur = ramfs_find_child(cur, part);
        if (!cur) {
            heap_caps_free(copy);
            errno = ENOENT;
            return NULL;
        }
        if (cur->type != RAMFS_NODE_DIR) {
            heap_caps_free(copy);
            errno = ENOTDIR;
            return NULL;
        }
        part = next;
    }

    heap_caps_free(copy);
    errno = EINVAL;
    return NULL;
}

static ramfs_node_t *ramfs_create_node(ramfs_ctx_t *ctx, ramfs_node_t *parent, const char *name, ramfs_node_type_t type)
{
    ramfs_node_t *node = ramfs_ctx_calloc(ctx, 1, sizeof(*node));

    if (!node) {
        ESP_LOGE(TAG, "node alloc failed");
        errno = ENOMEM;
        return NULL;
    }

    node->name = ramfs_ctx_strdup(ctx, name);
    if (!node->name) {
        heap_caps_free(node);
        ESP_LOGE(TAG, "node name alloc failed");
        errno = ENOMEM;
        return NULL;
    }

    node->type = type;
    node->parent = parent;
    node->mtime = time(NULL);
    node->next = parent->children;
    parent->children = node;
    return node;
}

static void ramfs_detach_node(ramfs_node_t *node)
{
    ramfs_node_t **link;

    if (!node || !node->parent) {
        return;
    }

    for (link = &node->parent->children; *link; link = &(*link)->next) {
        if (*link == node) {
            *link = node->next;
            node->next = NULL;
            return;
        }
    }
}

static int ramfs_next_fd(ramfs_ctx_t *ctx)
{
    for (size_t i = 0; i < ctx->max_files; i++) {
        if (!ctx->files[i].used) {
            return (int)i;
        }
    }
    ESP_LOGE(TAG, "open failed: no free file descriptors");
    errno = ENFILE;
    return -1;
}

static int ramfs_resize_file(ramfs_ctx_t *ctx, ramfs_node_t *node, size_t new_size)
{
    uint8_t *new_data;
    size_t old_size;

    if (!node || node->type != RAMFS_NODE_FILE) {
        errno = EINVAL;
        return -1;
    }

    old_size = node->size;
    if (new_size > old_size && new_size - old_size > ctx->max_bytes - ctx->used_bytes) {
        ESP_LOGE(TAG, "resize failed: capacity exceeded, requested=%u used=%u max=%u", (unsigned)new_size, (unsigned)ctx->used_bytes, (unsigned)ctx->max_bytes);
        errno = ENOSPC;
        return -1;
    }

    if (new_size == 0) {
        heap_caps_free(node->data);
        node->data = NULL;
    } else {
        new_data = ramfs_ctx_realloc(ctx, node->data, new_size);
        if (!new_data) {
            ESP_LOGE(TAG, "resize alloc failed, size=%u", (unsigned)new_size);
            errno = ENOMEM;
            return -1;
        }
        node->data = new_data;
        if (new_size > old_size) {
            memset(node->data + old_size, 0, new_size - old_size);
        }
    }

    ctx->used_bytes = ctx->used_bytes - old_size + new_size;
    node->size = new_size;
    node->mtime = time(NULL);
    return 0;
}

static void ramfs_fill_stat(const ramfs_node_t *node, struct stat *st)
{
    memset(st, 0, sizeof(*st));
    st->st_size = node->type == RAMFS_NODE_FILE ? (off_t)node->size : 0;
    st->st_mode = S_IRWXU | S_IRWXG | S_IRWXO | (node->type == RAMFS_NODE_DIR ? S_IFDIR : S_IFREG);
    st->st_mtime = node->mtime;
    st->st_atime = node->mtime;
    st->st_ctime = node->mtime;
}

static esp_err_t ramfs_errno_to_esp(int err)
{
    switch (err) {
    case 0:
        return ESP_OK;
    case ENOENT:
        return ESP_ERR_NOT_FOUND;
    case ENOMEM:
        return ESP_ERR_NO_MEM;
    case EINVAL:
    case ENOTDIR:
    case EISDIR:
        return ESP_ERR_INVALID_ARG;
    case EBUSY:
        return ESP_ERR_INVALID_STATE;
    default:
        return ESP_FAIL;
    }
}

static esp_err_t ramfs_find_context_by_full_path(const char *path, ramfs_ctx_t **out_ctx, const char **out_inner_path)
{
    size_t best_len = 0;
    ramfs_ctx_t *best_ctx = NULL;

    if (!path || path[0] != '/' || !out_ctx || !out_inner_path) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < RAMFS_MAX_INSTANCES; i++) {
        ramfs_ctx_t *ctx = s_contexts[i];
        size_t base_len;

        if (!ctx) {
            continue;
        }
        base_len = strlen(ctx->base_path);
        if (strncmp(path, ctx->base_path, base_len) == 0 && (path[base_len] == '\0' || path[base_len] == '/') && base_len > best_len) {
            best_ctx = ctx;
            best_len = base_len;
        }
    }

    if (!best_ctx) {
        return ESP_ERR_NOT_FOUND;
    }

    *out_ctx = best_ctx;
    *out_inner_path = path[best_len] ? path + best_len : "/";
    return ESP_OK;
}

static char *ramfs_path_join(const char *dir, const char *name)
{
    size_t dir_len;
    size_t name_len;
    bool need_sep;
    char *path;

    if (!dir || !name) {
        errno = EINVAL;
        return NULL;
    }

    dir_len = strlen(dir);
    name_len = strlen(name);
    need_sep = dir_len > 0 && dir[dir_len - 1] != '/';
    path = malloc(dir_len + (need_sep ? 1 : 0) + name_len + 1);
    if (!path) {
        errno = ENOMEM;
        return NULL;
    }

    memcpy(path, dir, dir_len);
    if (need_sep) {
        path[dir_len++] = '/';
    }
    memcpy(path + dir_len, name, name_len + 1);
    return path;
}

static char *ramfs_tmp_path_dup(const char *path)
{
    size_t len;
    char *tmp;

    if (!path || !path[0]) {
        errno = EINVAL;
        return NULL;
    }

    len = strlen(path);
    tmp = malloc(len + sizeof(".tmp"));
    if (!tmp) {
        errno = ENOMEM;
        return NULL;
    }
    snprintf(tmp, len + sizeof(".tmp"), "%s.tmp", path);
    return tmp;
}

static esp_err_t ramfs_ensure_dir(const char *path)
{
    struct stat st;

    if (!path || !path[0]) {
        return ESP_ERR_INVALID_ARG;
    }
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? ESP_OK : ESP_ERR_INVALID_ARG;
    }
    if (mkdir(path, 0775) == 0 || errno == EEXIST) {
        return ESP_OK;
    }
    ESP_LOGE(TAG, "mkdir failed for %s: errno=%d", path, errno);
    return ramfs_errno_to_esp(errno);
}

static esp_err_t ramfs_ensure_parent_dirs(const char *path)
{
    char *copy;
    char *slash;
    esp_err_t err = ESP_OK;

    if (!path || path[0] != '/') {
        return ESP_ERR_INVALID_ARG;
    }

    copy = strdup(path);
    if (!copy) {
        return ESP_ERR_NO_MEM;
    }

    slash = strrchr(copy, '/');
    if (!slash || slash == copy) {
        free(copy);
        return ESP_OK;
    }
    *slash = '\0';

    for (char *p = copy + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            err = ramfs_ensure_dir(copy);
            *p = '/';
            if (err != ESP_OK) {
                free(copy);
                return err;
            }
        }
    }
    err = ramfs_ensure_dir(copy);
    free(copy);
    return err;
}

static esp_err_t ramfs_write_file_atomic(const char *path, const uint8_t *data, size_t size)
{
    char *tmp_path;
    FILE *file;
    esp_err_t err;

    if (!path || (!data && size > 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    err = ramfs_ensure_parent_dirs(path);
    if (err != ESP_OK) {
        return err;
    }

    tmp_path = ramfs_tmp_path_dup(path);
    if (!tmp_path) {
        return ESP_ERR_NO_MEM;
    }

    unlink(tmp_path);
    file = fopen(tmp_path, "wb");
    if (!file) {
        ESP_LOGE(TAG, "open temp file failed for %s: errno=%d", tmp_path, errno);
        free(tmp_path);
        return ramfs_errno_to_esp(errno);
    }

    if (size > 0 && fwrite(data, 1, size, file) != size) {
        ESP_LOGE(TAG, "write temp file failed for %s", tmp_path);
        fclose(file);
        unlink(tmp_path);
        free(tmp_path);
        return ESP_FAIL;
    }
    if (fflush(file) != 0 || fsync(fileno(file)) != 0) {
        ESP_LOGE(TAG, "flush temp file failed for %s: errno=%d", tmp_path, errno);
        fclose(file);
        unlink(tmp_path);
        free(tmp_path);
        return ESP_FAIL;
    }
    if (fclose(file) != 0) {
        ESP_LOGE(TAG, "close temp file failed for %s: errno=%d", tmp_path, errno);
        unlink(tmp_path);
        free(tmp_path);
        return ESP_FAIL;
    }

    unlink(path);
    if (rename(tmp_path, path) != 0) {
        ESP_LOGE(TAG, "rename temp file failed from %s to %s: errno=%d", tmp_path, path, errno);
        unlink(tmp_path);
        free(tmp_path);
        return ramfs_errno_to_esp(errno);
    }

    free(tmp_path);
    return ESP_OK;
}

static ssize_t ramfs_write_at(ramfs_ctx_t *ctx, ramfs_file_t *file, const void *data, size_t size, off_t offset)
{
    size_t end;

    if (!data && size > 0) {
        errno = EINVAL;
        return -1;
    }
    if (offset < 0 || (size_t)offset > SIZE_MAX - size) {
        errno = EINVAL;
        return -1;
    }

    end = (size_t)offset + size;
    if (end > file->node->size && ramfs_resize_file(ctx, file->node, end) != 0) {
        return -1;
    }

    if (size > 0) {
        memcpy(file->node->data + offset, data, size);
        file->node->mtime = time(NULL);
    }
    return (ssize_t)size;
}

static int ramfs_open(void *p, const char *path, int flags, int mode)
{
    (void)mode;
    ramfs_ctx_t *ctx = p;
    ramfs_node_t *node;
    ramfs_node_t *parent;
    char name[NAME_MAX + 1];
    int fd;

    _lock_acquire(&ctx->lock);
    node = ramfs_resolve_node(ctx, path);
    if (!node && errno == ENOENT && (flags & O_CREAT)) {
        parent = ramfs_resolve_parent(ctx, path, name, sizeof(name));
        if (!parent) {
            _lock_release(&ctx->lock);
            ESP_LOGE(TAG, "open create failed for %s: errno=%d", path, errno);
            return -1;
        }
        if (parent->type != RAMFS_NODE_DIR) {
            _lock_release(&ctx->lock);
            errno = ENOTDIR;
            return -1;
        }
        node = ramfs_create_node(ctx, parent, name, RAMFS_NODE_FILE);
        if (!node) {
            _lock_release(&ctx->lock);
            return -1;
        }
    } else if (!node) {
        _lock_release(&ctx->lock);
        return -1;
    } else if ((flags & O_CREAT) && (flags & O_EXCL)) {
        _lock_release(&ctx->lock);
        errno = EEXIST;
        return -1;
    }

    if (node->type != RAMFS_NODE_FILE) {
        _lock_release(&ctx->lock);
        errno = EISDIR;
        return -1;
    }
    if ((flags & O_TRUNC) && (flags & O_ACCMODE) != O_RDONLY && ramfs_resize_file(ctx, node, 0) != 0) {
        _lock_release(&ctx->lock);
        return -1;
    }

    fd = ramfs_next_fd(ctx);
    if (fd < 0) {
        _lock_release(&ctx->lock);
        return -1;
    }

    ctx->files[fd].used = true;
    ctx->files[fd].flags = flags & (O_ACCMODE | O_APPEND);
    ctx->files[fd].offset = (flags & O_APPEND) ? (off_t)node->size : 0;
    ctx->files[fd].node = node;
    node->open_count++;
    _lock_release(&ctx->lock);
    return fd;
}

static int ramfs_close(void *p, int fd)
{
    ramfs_ctx_t *ctx = p;

    _lock_acquire(&ctx->lock);
    if (fd < 0 || (size_t)fd >= ctx->max_files || !ctx->files[fd].used) {
        _lock_release(&ctx->lock);
        errno = EBADF;
        return -1;
    }

    if (ctx->files[fd].node && ctx->files[fd].node->open_count > 0) {
        ctx->files[fd].node->open_count--;
    }
    memset(&ctx->files[fd], 0, sizeof(ctx->files[fd]));
    _lock_release(&ctx->lock);
    return 0;
}

static ssize_t ramfs_read(void *p, int fd, void *dst, size_t size)
{
    ramfs_ctx_t *ctx = p;
    ramfs_file_t *file;
    size_t available;
    size_t to_read;

    _lock_acquire(&ctx->lock);
    if (fd < 0 || (size_t)fd >= ctx->max_files || !ctx->files[fd].used) {
        _lock_release(&ctx->lock);
        errno = EBADF;
        return -1;
    }

    file = &ctx->files[fd];
    if ((file->flags & O_ACCMODE) == O_WRONLY) {
        _lock_release(&ctx->lock);
        errno = EBADF;
        return -1;
    }
    if (!dst && size > 0) {
        _lock_release(&ctx->lock);
        errno = EINVAL;
        return -1;
    }

    available = file->offset < (off_t)file->node->size ? file->node->size - (size_t)file->offset : 0;
    to_read = size < available ? size : available;
    if (to_read > 0) {
        memcpy(dst, file->node->data + file->offset, to_read);
        file->offset += (off_t)to_read;
    }
    _lock_release(&ctx->lock);
    return (ssize_t)to_read;
}

static ssize_t ramfs_write(void *p, int fd, const void *data, size_t size)
{
    ramfs_ctx_t *ctx = p;
    ramfs_file_t *file;
    ssize_t written;
    off_t offset;

    _lock_acquire(&ctx->lock);
    if (fd < 0 || (size_t)fd >= ctx->max_files || !ctx->files[fd].used) {
        _lock_release(&ctx->lock);
        errno = EBADF;
        return -1;
    }

    file = &ctx->files[fd];
    if ((file->flags & O_ACCMODE) == O_RDONLY) {
        _lock_release(&ctx->lock);
        errno = EBADF;
        return -1;
    }

    offset = (file->flags & O_APPEND) ? (off_t)file->node->size : file->offset;
    written = ramfs_write_at(ctx, file, data, size, offset);
    if (written >= 0) {
        file->offset = offset + written;
    }
    _lock_release(&ctx->lock);
    return written;
}

static off_t ramfs_lseek(void *p, int fd, off_t offset, int mode)
{
    ramfs_ctx_t *ctx = p;
    ramfs_file_t *file;
    off_t new_offset;

    _lock_acquire(&ctx->lock);
    if (fd < 0 || (size_t)fd >= ctx->max_files || !ctx->files[fd].used) {
        _lock_release(&ctx->lock);
        errno = EBADF;
        return -1;
    }

    file = &ctx->files[fd];
    if (mode == SEEK_SET) {
        new_offset = offset;
    } else if (mode == SEEK_CUR) {
        new_offset = file->offset + offset;
    } else if (mode == SEEK_END) {
        new_offset = (off_t)file->node->size + offset;
    } else {
        _lock_release(&ctx->lock);
        errno = EINVAL;
        return -1;
    }

    if (new_offset < 0) {
        _lock_release(&ctx->lock);
        errno = EINVAL;
        return -1;
    }

    file->offset = new_offset;
    _lock_release(&ctx->lock);
    return new_offset;
}

static ssize_t ramfs_pread(void *p, int fd, void *dst, size_t size, off_t offset)
{
    ramfs_ctx_t *ctx = p;
    ramfs_file_t *file;
    size_t available;
    size_t to_read;

    _lock_acquire(&ctx->lock);
    if (fd < 0 || (size_t)fd >= ctx->max_files || !ctx->files[fd].used || offset < 0) {
        _lock_release(&ctx->lock);
        errno = offset < 0 ? EINVAL : EBADF;
        return -1;
    }

    file = &ctx->files[fd];
    if ((file->flags & O_ACCMODE) == O_WRONLY) {
        _lock_release(&ctx->lock);
        errno = EBADF;
        return -1;
    }
    if (!dst && size > 0) {
        _lock_release(&ctx->lock);
        errno = EINVAL;
        return -1;
    }

    available = offset < (off_t)file->node->size ? file->node->size - (size_t)offset : 0;
    to_read = size < available ? size : available;
    if (to_read > 0) {
        memcpy(dst, file->node->data + offset, to_read);
    }
    _lock_release(&ctx->lock);
    return (ssize_t)to_read;
}

static ssize_t ramfs_pwrite(void *p, int fd, const void *data, size_t size, off_t offset)
{
    ramfs_ctx_t *ctx = p;
    ramfs_file_t *file;
    ssize_t written;

    _lock_acquire(&ctx->lock);
    if (fd < 0 || (size_t)fd >= ctx->max_files || !ctx->files[fd].used || offset < 0) {
        _lock_release(&ctx->lock);
        errno = offset < 0 ? EINVAL : EBADF;
        return -1;
    }

    file = &ctx->files[fd];
    if ((file->flags & O_ACCMODE) == O_RDONLY) {
        _lock_release(&ctx->lock);
        errno = EBADF;
        return -1;
    }

    written = ramfs_write_at(ctx, file, data, size, offset);
    _lock_release(&ctx->lock);
    return written;
}

static int ramfs_fstat(void *p, int fd, struct stat *st)
{
    ramfs_ctx_t *ctx = p;

    if (!st) {
        errno = EINVAL;
        return -1;
    }

    _lock_acquire(&ctx->lock);
    if (fd < 0 || (size_t)fd >= ctx->max_files || !ctx->files[fd].used) {
        _lock_release(&ctx->lock);
        errno = EBADF;
        return -1;
    }
    ramfs_fill_stat(ctx->files[fd].node, st);
    _lock_release(&ctx->lock);
    return 0;
}

static int ramfs_fcntl(void *p, int fd, int cmd, int arg)
{
    ramfs_ctx_t *ctx = p;

    _lock_acquire(&ctx->lock);
    if (fd < 0 || (size_t)fd >= ctx->max_files || !ctx->files[fd].used) {
        _lock_release(&ctx->lock);
        errno = EBADF;
        return -1;
    }

    if (cmd == F_GETFL) {
        int flags = ctx->files[fd].flags;
        _lock_release(&ctx->lock);
        return flags;
    }
    if (cmd == F_SETFL) {
        ctx->files[fd].flags = (ctx->files[fd].flags & O_ACCMODE) | (arg & O_APPEND);
        _lock_release(&ctx->lock);
        return 0;
    }
    if (cmd == F_SETLK || cmd == F_SETLKW || cmd == F_GETLK) {
        _lock_release(&ctx->lock);
        return 0;
    }

    _lock_release(&ctx->lock);
    errno = EINVAL;
    return -1;
}

static int ramfs_fsync(void *p, int fd)
{
    ramfs_ctx_t *ctx = p;

    _lock_acquire(&ctx->lock);
    if (fd < 0 || (size_t)fd >= ctx->max_files || !ctx->files[fd].used) {
        _lock_release(&ctx->lock);
        errno = EBADF;
        return -1;
    }
    _lock_release(&ctx->lock);
    return 0;
}

#ifdef CONFIG_VFS_SUPPORT_DIR
static int ramfs_stat(void *p, const char *path, struct stat *st)
{
    ramfs_ctx_t *ctx = p;
    ramfs_node_t *node;

    if (!st) {
        errno = EINVAL;
        return -1;
    }

    _lock_acquire(&ctx->lock);
    node = ramfs_resolve_node(ctx, path);
    if (!node) {
        _lock_release(&ctx->lock);
        return -1;
    }
    ramfs_fill_stat(node, st);
    _lock_release(&ctx->lock);
    return 0;
}

static int ramfs_unlink(void *p, const char *path)
{
    ramfs_ctx_t *ctx = p;
    ramfs_node_t *node;

    _lock_acquire(&ctx->lock);
    node = ramfs_resolve_node(ctx, path);
    if (!node) {
        _lock_release(&ctx->lock);
        return -1;
    }
    if (node->type != RAMFS_NODE_FILE) {
        _lock_release(&ctx->lock);
        errno = EISDIR;
        return -1;
    }
    if (ramfs_node_is_busy(node)) {
        ESP_LOGE(TAG, "unlink refused for busy file %s", path);
        _lock_release(&ctx->lock);
        errno = EBUSY;
        return -1;
    }

    ctx->used_bytes -= node->size;
    ramfs_detach_node(node);
    ramfs_free_node_tree(ctx, node);
    _lock_release(&ctx->lock);
    return 0;
}

static int ramfs_link(void *p, const char *n1, const char *n2)
{
    (void)p;
    (void)n1;
    (void)n2;
    errno = ENOTSUP;
    return -1;
}

static int ramfs_rename(void *p, const char *src, const char *dst)
{
    ramfs_ctx_t *ctx = p;
    ramfs_node_t *node;
    ramfs_node_t *dst_parent;
    ramfs_node_t *existing;
    char dst_name[NAME_MAX + 1];
    char *new_name;

    _lock_acquire(&ctx->lock);
    node = ramfs_resolve_node(ctx, src);
    if (!node || !node->parent) {
        _lock_release(&ctx->lock);
        return -1;
    }
    if (ramfs_node_is_busy(node)) {
        ESP_LOGE(TAG, "rename refused for busy node %s", src);
        _lock_release(&ctx->lock);
        errno = EBUSY;
        return -1;
    }

    dst_parent = ramfs_resolve_parent(ctx, dst, dst_name, sizeof(dst_name));
    if (!dst_parent) {
        _lock_release(&ctx->lock);
        return -1;
    }
    if (dst_parent->type != RAMFS_NODE_DIR) {
        _lock_release(&ctx->lock);
        errno = ENOTDIR;
        return -1;
    }
    if (node->type == RAMFS_NODE_DIR && ramfs_is_descendant_of(dst_parent, node)) {
        _lock_release(&ctx->lock);
        errno = EINVAL;
        return -1;
    }

    existing = ramfs_find_child(dst_parent, dst_name);
    if (existing == node) {
        _lock_release(&ctx->lock);
        return 0;
    }
    if (existing) {
        if (existing->type != node->type) {
            _lock_release(&ctx->lock);
            errno = existing->type == RAMFS_NODE_DIR ? EISDIR : ENOTDIR;
            return -1;
        }
        if (ramfs_node_is_busy(existing) || (existing->type == RAMFS_NODE_DIR && !ramfs_dir_is_empty(existing))) {
            _lock_release(&ctx->lock);
            errno = existing->type == RAMFS_NODE_DIR ? ENOTEMPTY : EBUSY;
            return -1;
        }
        ctx->used_bytes -= existing->size;
        ramfs_detach_node(existing);
        ramfs_free_node_tree(ctx, existing);
    }

    new_name = ramfs_ctx_strdup(ctx, dst_name);
    if (!new_name) {
        ESP_LOGE(TAG, "rename name alloc failed");
        _lock_release(&ctx->lock);
        errno = ENOMEM;
        return -1;
    }

    ramfs_detach_node(node);
    heap_caps_free(node->name);
    node->name = new_name;
    node->parent = dst_parent;
    node->next = dst_parent->children;
    dst_parent->children = node;
    node->mtime = time(NULL);
    _lock_release(&ctx->lock);
    return 0;
}

static DIR *ramfs_opendir(void *p, const char *name)
{
    ramfs_ctx_t *ctx = p;
    ramfs_node_t *node;
    ramfs_dir_t *dir;

    _lock_acquire(&ctx->lock);
    node = ramfs_resolve_node(ctx, name);
    if (!node) {
        _lock_release(&ctx->lock);
        return NULL;
    }
    if (node->type != RAMFS_NODE_DIR) {
        _lock_release(&ctx->lock);
        errno = ENOTDIR;
        return NULL;
    }

    dir = ramfs_ctx_calloc(ctx, 1, sizeof(*dir));
    if (!dir) {
        ESP_LOGE(TAG, "opendir alloc failed");
        _lock_release(&ctx->lock);
        errno = ENOMEM;
        return NULL;
    }
    dir->node = node;
    node->dir_open_count++;
    _lock_release(&ctx->lock);
    return (DIR *)dir;
}

static int ramfs_readdir_r(void *p, DIR *pdir, struct dirent *entry, struct dirent **out_dirent)
{
    ramfs_ctx_t *ctx = p;
    ramfs_dir_t *dir = (ramfs_dir_t *)pdir;
    ramfs_node_t *node;
    long index = 0;

    if (!pdir || !entry || !out_dirent) {
        return EINVAL;
    }

    _lock_acquire(&ctx->lock);
    node = dir->node ? dir->node->children : NULL;
    while (node && index < dir->offset) {
        node = node->next;
        index++;
    }
    if (!node) {
        *out_dirent = NULL;
        _lock_release(&ctx->lock);
        return 0;
    }

    memset(entry, 0, sizeof(*entry));
    entry->d_ino = 0;
    entry->d_type = node->type == RAMFS_NODE_DIR ? DT_DIR : DT_REG;
    ramfs_copy_string(entry->d_name, sizeof(entry->d_name), node->name);
    dir->offset++;
    *out_dirent = entry;
    _lock_release(&ctx->lock);
    return 0;
}

static struct dirent *ramfs_readdir(void *p, DIR *pdir)
{
    ramfs_dir_t *dir = (ramfs_dir_t *)pdir;
    struct dirent *out_dirent = NULL;
    int err;

    if (!dir) {
        errno = EINVAL;
        return NULL;
    }

    err = ramfs_readdir_r(p, pdir, &dir->cur_dirent, &out_dirent);
    if (err != 0) {
        errno = err;
        return NULL;
    }
    return out_dirent;
}

static long ramfs_telldir(void *p, DIR *pdir)
{
    (void)p;
    return pdir ? ((ramfs_dir_t *)pdir)->offset : -1;
}

static void ramfs_seekdir(void *p, DIR *pdir, long offset)
{
    (void)p;
    if (pdir && offset >= 0) {
        ((ramfs_dir_t *)pdir)->offset = offset;
    }
}

static int ramfs_closedir(void *p, DIR *pdir)
{
    ramfs_ctx_t *ctx = p;
    ramfs_dir_t *dir = (ramfs_dir_t *)pdir;

    if (!dir) {
        errno = EINVAL;
        return -1;
    }

    _lock_acquire(&ctx->lock);
    if (dir->node && dir->node->dir_open_count > 0) {
        dir->node->dir_open_count--;
    }
    _lock_release(&ctx->lock);
    heap_caps_free(dir);
    return 0;
}

static int ramfs_mkdir(void *p, const char *name, mode_t mode)
{
    (void)mode;
    ramfs_ctx_t *ctx = p;
    ramfs_node_t *parent;
    char leaf[NAME_MAX + 1];

    _lock_acquire(&ctx->lock);
    parent = ramfs_resolve_parent(ctx, name, leaf, sizeof(leaf));
    if (!parent) {
        _lock_release(&ctx->lock);
        return -1;
    }
    if (ramfs_find_child(parent, leaf)) {
        _lock_release(&ctx->lock);
        errno = EEXIST;
        return -1;
    }
    if (!ramfs_create_node(ctx, parent, leaf, RAMFS_NODE_DIR)) {
        _lock_release(&ctx->lock);
        return -1;
    }
    _lock_release(&ctx->lock);
    return 0;
}

static int ramfs_rmdir(void *p, const char *name)
{
    ramfs_ctx_t *ctx = p;
    ramfs_node_t *node;

    _lock_acquire(&ctx->lock);
    node = ramfs_resolve_node(ctx, name);
    if (!node || !node->parent) {
        _lock_release(&ctx->lock);
        return -1;
    }
    if (node->type != RAMFS_NODE_DIR) {
        _lock_release(&ctx->lock);
        errno = ENOTDIR;
        return -1;
    }
    if (!ramfs_dir_is_empty(node)) {
        _lock_release(&ctx->lock);
        errno = ENOTEMPTY;
        return -1;
    }
    if (ramfs_node_is_busy(node)) {
        ESP_LOGE(TAG, "rmdir refused for busy dir %s", name);
        _lock_release(&ctx->lock);
        errno = EBUSY;
        return -1;
    }

    ramfs_detach_node(node);
    ramfs_free_node_tree(ctx, node);
    _lock_release(&ctx->lock);
    return 0;
}

static int ramfs_access(void *p, const char *path, int amode)
{
    ramfs_ctx_t *ctx = p;
    ramfs_node_t *node;

    (void)amode;
    _lock_acquire(&ctx->lock);
    node = ramfs_resolve_node(ctx, path);
    _lock_release(&ctx->lock);
    return node ? 0 : -1;
}

static int ramfs_truncate(void *p, const char *path, off_t length)
{
    ramfs_ctx_t *ctx = p;
    ramfs_node_t *node;
    int ret;

    if (length < 0) {
        errno = EINVAL;
        return -1;
    }

    _lock_acquire(&ctx->lock);
    node = ramfs_resolve_node(ctx, path);
    if (!node) {
        _lock_release(&ctx->lock);
        return -1;
    }
    if (node->type != RAMFS_NODE_FILE) {
        _lock_release(&ctx->lock);
        errno = EISDIR;
        return -1;
    }
    ret = ramfs_resize_file(ctx, node, (size_t)length);
    _lock_release(&ctx->lock);
    return ret;
}

static int ramfs_ftruncate(void *p, int fd, off_t length)
{
    ramfs_ctx_t *ctx = p;
    int ret;

    if (length < 0) {
        errno = EINVAL;
        return -1;
    }

    _lock_acquire(&ctx->lock);
    if (fd < 0 || (size_t)fd >= ctx->max_files || !ctx->files[fd].used) {
        _lock_release(&ctx->lock);
        errno = EBADF;
        return -1;
    }
    ret = ramfs_resize_file(ctx, ctx->files[fd].node, (size_t)length);
    if (ret == 0 && ctx->files[fd].offset > length) {
        ctx->files[fd].offset = length;
    }
    _lock_release(&ctx->lock);
    return ret;
}

static int ramfs_utime(void *p, const char *path, const struct utimbuf *times)
{
    ramfs_ctx_t *ctx = p;
    ramfs_node_t *node;

    _lock_acquire(&ctx->lock);
    node = ramfs_resolve_node(ctx, path);
    if (!node) {
        _lock_release(&ctx->lock);
        return -1;
    }
    node->mtime = times ? times->modtime : time(NULL);
    _lock_release(&ctx->lock);
    return 0;
}

#if RAMFS_USE_VFS_OPS
static const esp_vfs_dir_ops_t s_ramfs_dir = {
    .stat_p = ramfs_stat,
    .link_p = ramfs_link,
    .unlink_p = ramfs_unlink,
    .rename_p = ramfs_rename,
    .opendir_p = ramfs_opendir,
    .closedir_p = ramfs_closedir,
    .readdir_p = ramfs_readdir,
    .readdir_r_p = ramfs_readdir_r,
    .seekdir_p = ramfs_seekdir,
    .telldir_p = ramfs_telldir,
    .mkdir_p = ramfs_mkdir,
    .rmdir_p = ramfs_rmdir,
    .access_p = ramfs_access,
    .truncate_p = ramfs_truncate,
    .ftruncate_p = ramfs_ftruncate,
    .utime_p = ramfs_utime,
};
#endif
#endif

#if RAMFS_USE_VFS_OPS
static const esp_vfs_fs_ops_t s_ramfs = {
    .write_p = ramfs_write,
    .lseek_p = ramfs_lseek,
    .read_p = ramfs_read,
    .pread_p = ramfs_pread,
    .pwrite_p = ramfs_pwrite,
    .open_p = ramfs_open,
    .close_p = ramfs_close,
    .fstat_p = ramfs_fstat,
    .fcntl_p = ramfs_fcntl,
    .fsync_p = ramfs_fsync,
#ifdef CONFIG_VFS_SUPPORT_DIR
    .dir = &s_ramfs_dir,
#endif
};
#else
static const esp_vfs_t s_ramfs = {
    .flags = ESP_VFS_FLAG_CONTEXT_PTR,
    .write_p = ramfs_write,
    .lseek_p = ramfs_lseek,
    .read_p = ramfs_read,
    .pread_p = ramfs_pread,
    .pwrite_p = ramfs_pwrite,
    .open_p = ramfs_open,
    .close_p = ramfs_close,
    .fstat_p = ramfs_fstat,
    .fcntl_p = ramfs_fcntl,
    .fsync_p = ramfs_fsync,
#ifdef CONFIG_VFS_SUPPORT_DIR
    .stat_p = ramfs_stat,
    .link_p = ramfs_link,
    .unlink_p = ramfs_unlink,
    .rename_p = ramfs_rename,
    .opendir_p = ramfs_opendir,
    .closedir_p = ramfs_closedir,
    .readdir_p = ramfs_readdir,
    .readdir_r_p = ramfs_readdir_r,
    .seekdir_p = ramfs_seekdir,
    .telldir_p = ramfs_telldir,
    .mkdir_p = ramfs_mkdir,
    .rmdir_p = ramfs_rmdir,
    .access_p = ramfs_access,
    .truncate_p = ramfs_truncate,
    .ftruncate_p = ramfs_ftruncate,
    .utime_p = ramfs_utime,
#endif
};
#endif

esp_err_t ramfs_register(const ramfs_config_t *config)
{
    const ramfs_config_t *cfg = config;
    ramfs_ctx_t *ctx;
    size_t index;
    size_t max_files;
    esp_err_t err;

    if (!cfg) {
        ESP_LOGE(TAG, "register failed: config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!cfg->base_path || cfg->base_path[0] != '/' || cfg->base_path[1] == '\0' || cfg->base_path[strlen(cfg->base_path) - 1] == '/') {
        ESP_LOGE(TAG, "register failed: invalid base path");
        return ESP_ERR_INVALID_ARG;
    }
    if (!ramfs_caps_are_valid(cfg->caps)) {
        ESP_LOGE(TAG, "register failed: invalid heap caps=0x%08x", (unsigned int)cfg->caps);
        return ESP_ERR_INVALID_ARG;
    }
    if (ramfs_find_context_index(cfg->base_path) < RAMFS_MAX_INSTANCES) {
        ESP_LOGE(TAG, "register failed: base path already registered");
        return ESP_ERR_INVALID_STATE;
    }

    index = ramfs_find_free_context_index();
    if (index == RAMFS_MAX_INSTANCES) {
        ESP_LOGE(TAG, "register failed: no free context slots");
        return ESP_ERR_NO_MEM;
    }

    max_files = cfg->max_files > 0 ? cfg->max_files : 1;
    ctx = heap_caps_calloc(1, sizeof(*ctx) + max_files * sizeof(ctx->files[0]), cfg->caps);
    if (!ctx) {
        ESP_LOGE(TAG, "register failed: context alloc failed");
        return ESP_ERR_NO_MEM;
    }

    ramfs_copy_string(ctx->base_path, sizeof(ctx->base_path), cfg->base_path);
    ctx->max_files = max_files;
    ctx->max_bytes = cfg->max_bytes;
    ctx->caps = cfg->caps;
    ctx->root.name = "";
    ctx->root.type = RAMFS_NODE_DIR;
    ctx->root.mtime = time(NULL);

#if RAMFS_USE_VFS_OPS
    err = esp_vfs_register_fs(ctx->base_path, &s_ramfs, ESP_VFS_FLAG_CONTEXT_PTR | ESP_VFS_FLAG_STATIC, ctx);
#else
    err = esp_vfs_register(ctx->base_path, &s_ramfs, ctx);
#endif
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register failed: vfs error=%s", esp_err_to_name(err));
        heap_caps_free(ctx);
        return err;
    }

    _lock_init(&ctx->lock);
    s_contexts[index] = ctx;
    return ESP_OK;
}

esp_err_t ramfs_unregister(const char *base_path)
{
    size_t index;
    ramfs_ctx_t *ctx;
    esp_err_t err;

    if (!base_path) {
        return ESP_ERR_INVALID_ARG;
    }

    index = ramfs_find_context_index(base_path);
    if (index == RAMFS_MAX_INSTANCES) {
        return ESP_ERR_INVALID_STATE;
    }

    ctx = s_contexts[index];
    _lock_acquire(&ctx->lock);
    for (size_t i = 0; i < ctx->max_files; i++) {
        if (ctx->files[i].used) {
            _lock_release(&ctx->lock);
            ESP_LOGE(TAG, "unregister refused: open files remain");
            return ESP_ERR_INVALID_STATE;
        }
    }
    if (ramfs_tree_has_open_dir(&ctx->root)) {
        _lock_release(&ctx->lock);
        ESP_LOGE(TAG, "unregister refused: open directories remain");
        return ESP_ERR_INVALID_STATE;
    }
    _lock_release(&ctx->lock);

    err = esp_vfs_unregister(ctx->base_path);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "unregister failed: vfs error=%s", esp_err_to_name(err));
        return err;
    }

    ramfs_free_node_tree(ctx, &ctx->root);
    _lock_close(&ctx->lock);
    heap_caps_free(ctx);
    s_contexts[index] = NULL;
    return ESP_OK;
}

esp_err_t ramfs_info(const char *base_path, size_t *out_total_bytes, size_t *out_used_bytes)
{
    size_t index;
    ramfs_ctx_t *ctx;

    if (!base_path) {
        return ESP_ERR_INVALID_ARG;
    }

    index = ramfs_find_context_index(base_path);
    if (index == RAMFS_MAX_INSTANCES) {
        return ESP_ERR_INVALID_STATE;
    }

    ctx = s_contexts[index];
    _lock_acquire(&ctx->lock);
    if (out_total_bytes) {
        *out_total_bytes = ctx->max_bytes;
    }
    if (out_used_bytes) {
        *out_used_bytes = ctx->used_bytes;
    }
    _lock_release(&ctx->lock);
    return ESP_OK;
}

static esp_err_t ramfs_snapshot_file(const char *ramfs_path, uint8_t **out_data, size_t *out_size)
{
    ramfs_ctx_t *ctx;
    const char *inner_path;
    ramfs_node_t *node;
    esp_err_t err;

    if (!out_data || !out_size) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_data = NULL;
    *out_size = 0;

    err = ramfs_find_context_by_full_path(ramfs_path, &ctx, &inner_path);
    if (err != ESP_OK) {
        return err;
    }

    _lock_acquire(&ctx->lock);
    node = ramfs_resolve_node(ctx, inner_path);
    if (!node) {
        err = ramfs_errno_to_esp(errno);
        _lock_release(&ctx->lock);
        return err;
    }
    if (node->type != RAMFS_NODE_FILE) {
        _lock_release(&ctx->lock);
        return ESP_ERR_INVALID_ARG;
    }
    if (ramfs_node_is_busy(node)) {
        _lock_release(&ctx->lock);
        return ESP_ERR_INVALID_STATE;
    }
    if (node->size > 0) {
        *out_data = ramfs_ctx_malloc(ctx, node->size);
        if (!*out_data) {
            ESP_LOGE(TAG, "snapshot alloc failed, size=%u", (unsigned int)node->size);
            _lock_release(&ctx->lock);
            return ESP_ERR_NO_MEM;
        }
        memcpy(*out_data, node->data, node->size);
    }
    *out_size = node->size;
    _lock_release(&ctx->lock);
    return ESP_OK;
}

static esp_err_t ramfs_sync_node_tree_locked(const ramfs_node_t *node, const char *fatfs_path)
{
    esp_err_t err;

    if (node->type == RAMFS_NODE_FILE) {
        return ramfs_write_file_atomic(fatfs_path, node->data, node->size);
    }

    err = ramfs_ensure_dir(fatfs_path);
    if (err != ESP_OK) {
        return err;
    }

    for (const ramfs_node_t *child = node->children; child; child = child->next) {
        char *child_path = ramfs_path_join(fatfs_path, child->name);

        if (!child_path) {
            return ESP_ERR_NO_MEM;
        }
        err = ramfs_sync_node_tree_locked(child, child_path);
        free(child_path);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

static esp_err_t ramfs_check_target_not_busy(const char *ramfs_path, bool allow_dir)
{
    ramfs_ctx_t *ctx;
    const char *inner_path;
    ramfs_node_t *node;
    esp_err_t err;

    err = ramfs_find_context_by_full_path(ramfs_path, &ctx, &inner_path);
    if (err != ESP_OK) {
        return err;
    }

    _lock_acquire(&ctx->lock);
    node = ramfs_resolve_node(ctx, inner_path);
    if (!node) {
        err = errno == ENOENT ? ESP_OK : ramfs_errno_to_esp(errno);
        _lock_release(&ctx->lock);
        return err;
    }
    if (!allow_dir && node->type != RAMFS_NODE_FILE) {
        _lock_release(&ctx->lock);
        return ESP_ERR_INVALID_ARG;
    }
    err = ramfs_tree_is_busy(node) ? ESP_ERR_INVALID_STATE : ESP_OK;
    _lock_release(&ctx->lock);
    return err;
}

static esp_err_t ramfs_copy_external_file_to_ramfs(const char *fatfs_path, const char *ramfs_path)
{
    char *tmp_path;
    FILE *src;
    FILE *dst;
    uint8_t buf[256];
    esp_err_t err;

    err = ramfs_check_target_not_busy(ramfs_path, false);
    if (err != ESP_OK) {
        return err;
    }
    err = ramfs_ensure_parent_dirs(ramfs_path);
    if (err != ESP_OK) {
        return err;
    }

    tmp_path = ramfs_tmp_path_dup(ramfs_path);
    if (!tmp_path) {
        return ESP_ERR_NO_MEM;
    }
    unlink(tmp_path);

    src = fopen(fatfs_path, "rb");
    if (!src) {
        err = ramfs_errno_to_esp(errno);
        ESP_LOGE(TAG, "open source file failed for %s: errno=%d", fatfs_path, errno);
        free(tmp_path);
        return err;
    }

    dst = fopen(tmp_path, "wb");
    if (!dst) {
        err = ramfs_errno_to_esp(errno);
        ESP_LOGE(TAG, "open RAMFS temp file failed for %s: errno=%d", tmp_path, errno);
        fclose(src);
        free(tmp_path);
        return err;
    }

    while (true) {
        size_t read_len = fread(buf, 1, sizeof(buf), src);

        if (read_len > 0 && fwrite(buf, 1, read_len, dst) != read_len) {
            ESP_LOGE(TAG, "write RAMFS temp file failed for %s", tmp_path);
            err = ESP_FAIL;
            break;
        }
        if (read_len < sizeof(buf)) {
            if (ferror(src)) {
                ESP_LOGE(TAG, "read source file failed for %s", fatfs_path);
                err = ESP_FAIL;
            }
            break;
        }
    }

    if (fclose(dst) != 0 && err == ESP_OK) {
        ESP_LOGE(TAG, "close RAMFS temp file failed for %s: errno=%d", tmp_path, errno);
        err = ESP_FAIL;
    }
    fclose(src);

    if (err == ESP_OK) {
        unlink(ramfs_path);
        if (rename(tmp_path, ramfs_path) != 0) {
            ESP_LOGE(TAG, "rename RAMFS temp file failed from %s to %s: errno=%d", tmp_path, ramfs_path, errno);
            err = ramfs_errno_to_esp(errno);
        }
    }
    if (err != ESP_OK) {
        unlink(tmp_path);
    }
    free(tmp_path);
    return err;
}

static esp_err_t ramfs_load_tree_recursive(const char *fatfs_dir, const char *ramfs_dir)
{
    DIR *dir;
    struct dirent *entry;
    esp_err_t err;

    err = ramfs_check_target_not_busy(ramfs_dir, true);
    if (err != ESP_OK) {
        return err;
    }
    if (mkdir(ramfs_dir, 0775) != 0 && errno != EEXIST) {
        ESP_LOGE(TAG, "create RAMFS dir failed for %s: errno=%d", ramfs_dir, errno);
        return ramfs_errno_to_esp(errno);
    }

    dir = opendir(fatfs_dir);
    if (!dir) {
        ESP_LOGE(TAG, "open source dir failed for %s: errno=%d", fatfs_dir, errno);
        return ramfs_errno_to_esp(errno);
    }

    while ((entry = readdir(dir)) != NULL) {
        char *src_path;
        char *dst_path;
        struct stat st;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        src_path = ramfs_path_join(fatfs_dir, entry->d_name);
        dst_path = ramfs_path_join(ramfs_dir, entry->d_name);
        if (!src_path || !dst_path) {
            free(src_path);
            free(dst_path);
            closedir(dir);
            return ESP_ERR_NO_MEM;
        }

        if (stat(src_path, &st) != 0) {
            ESP_LOGE(TAG, "stat source path failed for %s: errno=%d", src_path, errno);
            free(src_path);
            free(dst_path);
            closedir(dir);
            return ramfs_errno_to_esp(errno);
        }

        if (S_ISDIR(st.st_mode)) {
            err = ramfs_load_tree_recursive(src_path, dst_path);
        } else if (S_ISREG(st.st_mode)) {
            err = ramfs_copy_external_file_to_ramfs(src_path, dst_path);
        } else {
            err = ESP_ERR_INVALID_ARG;
        }
        free(src_path);
        free(dst_path);
        if (err != ESP_OK) {
            closedir(dir);
            return err;
        }
    }

    closedir(dir);
    return ESP_OK;
}

esp_err_t ramfs_sync_file_to_fatfs(const char *ramfs_path, const char *fatfs_path)
{
    uint8_t *data = NULL;
    size_t size = 0;
    esp_err_t err;

    if (!ramfs_path || !fatfs_path || fatfs_path[0] != '/') {
        return ESP_ERR_INVALID_ARG;
    }

    err = ramfs_snapshot_file(ramfs_path, &data, &size);
    if (err == ESP_OK) {
        err = ramfs_write_file_atomic(fatfs_path, data, size);
    }
    heap_caps_free(data);
    return err;
}

esp_err_t ramfs_sync_tree_to_fatfs(const char *ramfs_dir, const char *fatfs_dir)
{
    ramfs_ctx_t *ctx;
    const char *inner_path;
    ramfs_node_t *node;
    esp_err_t err;

    if (!ramfs_dir || !fatfs_dir || fatfs_dir[0] != '/') {
        return ESP_ERR_INVALID_ARG;
    }

    err = ramfs_find_context_by_full_path(ramfs_dir, &ctx, &inner_path);
    if (err != ESP_OK) {
        return err;
    }

    _lock_acquire(&ctx->lock);
    node = ramfs_resolve_node(ctx, inner_path);
    if (!node) {
        err = ramfs_errno_to_esp(errno);
    } else if (node->type != RAMFS_NODE_DIR) {
        err = ESP_ERR_INVALID_ARG;
    } else if (ramfs_tree_is_busy(node)) {
        err = ESP_ERR_INVALID_STATE;
    } else {
        err = ramfs_sync_node_tree_locked(node, fatfs_dir);
    }
    _lock_release(&ctx->lock);
    return err;
}

esp_err_t ramfs_load_file_from_fatfs(const char *fatfs_path, const char *ramfs_path)
{
    struct stat st;

    if (!fatfs_path || !ramfs_path || fatfs_path[0] != '/') {
        return ESP_ERR_INVALID_ARG;
    }
    if (stat(fatfs_path, &st) != 0) {
        return ramfs_errno_to_esp(errno);
    }
    if (!S_ISREG(st.st_mode)) {
        return ESP_ERR_INVALID_ARG;
    }
    return ramfs_copy_external_file_to_ramfs(fatfs_path, ramfs_path);
}

esp_err_t ramfs_load_tree_from_fatfs(const char *fatfs_dir, const char *ramfs_dir)
{
    struct stat st;

    if (!fatfs_dir || !ramfs_dir || fatfs_dir[0] != '/') {
        return ESP_ERR_INVALID_ARG;
    }
    if (stat(fatfs_dir, &st) != 0) {
        return ramfs_errno_to_esp(errno);
    }
    if (!S_ISDIR(st.st_mode)) {
        return ESP_ERR_INVALID_ARG;
    }
    return ramfs_load_tree_recursive(fatfs_dir, ramfs_dir);
}
