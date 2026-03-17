#include "ramfs.h"
#include "memory.h"
#include "string.h"
#include "types.h"
#include "serial.h"

#define RAMFS_MAX_ENTRIES 32
#define RAMFS_FILE_CAPACITY 4096

static struct inode* ramfs_lookup(struct inode* dir, const char* name);
static int ramfs_create(struct inode* dir, const char* name);

static int ramfs_read(struct file* file, void* buf, size_t size);
static int ramfs_write(struct file* file, const void* buf, size_t size);
int ramfs_readdir(struct inode* dir, struct dirent* out, int index);
int ramfs_mkdir(struct inode* dir, const char* name);

static struct inode_ops ramfs_inode_ops = {
    .lookup = ramfs_lookup,
    .create = ramfs_create,
    .mkdir = ramfs_mkdir,
    .unlink = NULL,
    .readdir = ramfs_readdir
};


static struct file_ops ramfs_file_ops = {
    .read = ramfs_read,
    .write = ramfs_write,
    .open = NULL,
    .close = NULL
};

typedef struct {
    char name[32];
    struct inode* inode;
} ramfs_entry;

typedef struct {
    ramfs_entry entries[RAMFS_MAX_ENTRIES];
    int count;
} ramfs_dir;

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} ramfs_file;


static struct inode* ramfs_alloc_inode(inode_type_t type)
{
    struct inode* inode = kmalloc(sizeof(struct inode));
    if (!inode)
        return NULL;

    memset(inode, 0, sizeof(struct inode));

    inode->type = type;
    inode->inode_ops = &ramfs_inode_ops;
    inode->file_ops = &ramfs_file_ops;
    inode->ref_count = 1;

    return inode;
}

struct inode* ramfs_create_root()
{
    struct inode* root = ramfs_alloc_inode(INODE_DIR);

    ramfs_dir* dir = kmalloc(sizeof(ramfs_dir));
    memset(dir, 0, sizeof(ramfs_dir));

    root->data = dir;

    return root;
}


// inode ops
struct inode* ramfs_lookup(struct inode* dir, const char* name)
{
    ramfs_dir* d = dir->data;

    if(strcmp(name, ".") == 0){
        return dir;
    }

    if(strcmp(name, "..") == 0){
        return dir->parent ? dir->parent : dir;
    }

    for (int i = 0; i < d->count; i++) {
        if (strcmp(d->entries[i].name, name) == 0) {
            return d->entries[i].inode;   // IMPORTANT
        }
    }

    return NULL;
}

static int ramfs_create(struct inode* dir, const char* name)
{
    serial_print("ramfs  create");
    ramfs_dir* d = dir->data;

    if (d->count >= RAMFS_MAX_ENTRIES)
        return -1;

    struct inode* inode = ramfs_alloc_inode(INODE_FILE);

    ramfs_file* f = kmalloc(sizeof(ramfs_file));
    f->data = kmalloc(RAMFS_FILE_CAPACITY);
    f->size = 0;
    f->capacity = RAMFS_FILE_CAPACITY;

    inode->data = f;

    strcpy(d->entries[d->count].name, name);
    d->entries[d->count].inode = inode;

    d->count++;

    return 0;
}

int ramfs_mkdir(struct inode* dir, const char* name)
{
    ramfs_dir* d = dir->data;

    if (d->count >= 64)
        return -1;

    for (int i = 0; i < d->count; i++) {
        if (strcmp(d->entries[i].name, name) == 0)
            return -1;
    }

    struct inode* new_inode = kmalloc(sizeof(struct inode));
    memset(new_inode, 0, sizeof(struct inode));

    new_inode->type = INODE_DIR;
    new_inode->inode_ops = &ramfs_inode_ops;
    new_inode->parent = dir;

    ramfs_dir* new_data = kmalloc(sizeof(ramfs_dir));
    memset(new_data, 0, sizeof(ramfs_dir));
    new_inode->data = new_data;

    
    d->entries[d->count].inode = new_inode;
    strcpy(d->entries[d->count].name, name);
    d->count++;


    serial_print("mkdir %s: parent=%p, new=%p\n", name, dir, new_inode);
serial_print("stored in parent: name=%s inode=%p\n"
            );

    return 0;
}

static int ramfs_read(struct file* file, void* buf, size_t size)
{
    ramfs_file* f = file->private_data;
    serial_print("ramfs_read f: %d",f);

    if (file->offset >= f->size)
        return 0;

    size_t remaining = f->size - file->offset;

    if (size > remaining)
        size = remaining;

    memcpy(buf, f->data + file->offset, size);

    serial_print("ramfs_read size: %d",size);
    return size;
}

static int ramfs_write(struct file* file, const void* buf, size_t size)
{
    ramfs_file* f = file->private_data;

    if (file->offset + size > f->capacity)
        size = f->capacity - file->offset;

    memcpy(f->data + file->offset, buf, size);

    if (file->offset + size > f->size)
        f->size = file->offset + size;

    return size;
}

int ramfs_readdir(struct inode* dir, struct dirent* out, int index)
{
    ramfs_dir* d = dir->data;

    if (index >= d->count)
        return -1;

    strcpy(out->name, d->entries[index].name);
    out->type = d->entries[index].inode->type;

    return 0;
}