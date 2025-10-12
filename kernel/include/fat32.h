#ifndef __FAT32_H
#define __FAT32_H

#include "sleeplock.h"
#include "stat.h"

#define ATTR_READ_ONLY      0x01
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04
#define ATTR_VOLUME_ID      0x08
#define ATTR_DIRECTORY      0x10
#define ATTR_ARCHIVE        0x20
#define ATTR_LONG_NAME      0x0F

#define LAST_LONG_ENTRY     0x40
#define FAT32_EOC           0x0ffffff8
#define EMPTY_ENTRY         0xe5
#define END_OF_ENTRY        0x00
#define CHAR_LONG_NAME      13
#define CHAR_SHORT_NAME     11

#define FAT32_MAX_FILENAME  255
#define FAT32_MAX_PATH      260
#define ENTRY_CACHE_NUM     50

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14

#define NMOUNT 16

struct dirent {
    char  filename[FAT32_MAX_FILENAME + 1];
    uint8   attribute;
    // uint8   create_time_tenth;
    // uint16  create_time;
    // uint16  create_date;
    // uint16  last_access_date;
    uint32  first_clus;
    // uint16  last_write_time;
    // uint16  last_write_date;
    uint32  file_size;

    uint32  cur_clus;
    uint    clus_cnt;

    /* for OS */
    uint8   dev;
    uint8   dirty;
    short   valid;
    int     ref;
    uint32  off;            // offset in the parent dir entry, for writing convenience
    struct dirent *parent;  // because FAT32 doesn't have such thing like inum, use this for cache trick
    struct dirent *next;
    struct dirent *prev;
    struct sleeplock    lock;
};

/*
为了对齐 Linux 的接口，我们需要定义一个新的结构体 dirent64，它是 SNE 和 LNE 这两种 **物理目录项** 的上层封装，我们也可以叫他 **逻辑目录项**，它实际上已经可以用于用户态编程了，它包含了：

- d_ino：inode 号，FAT32  不支持，直接设为 0 就行
- d_off：偏移量，记录的是当前这个 `dirent64` 逻辑目录项读完后，下一个逻辑目录项在父目录数据流中的起始字节位置。每次读取完一个 **逻辑目录项** 后，偏移量增加 `count * 32`，即经过的 **物理目录项** 的数量乘以 32。
- d_reclen：记录长度，固定为 sizeof(struct dirent64)
- d_type：文件类型，根据 `struct dirent` 的 `attribute` 字段判断，如果是目录，则设为 `DT_DIR`，否则设为 `DT_REG`
- d_name：文件名，从 `struct dirent` 的 `filename` 字段拷贝过来
*/
struct dirent64 {
    uint64 d_ino;
    uint64 d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[FAT32_MAX_FILENAME + 1];
};

struct mount {
    struct dirent* de;
    char path[FAT32_MAX_PATH];
    int used;
};

int             fat32_init(void);
struct dirent*  dirlookup(struct dirent *entry, char *filename, uint *poff);
char*           formatname(char *name);
void            emake(struct dirent *dp, struct dirent *ep, uint off);
struct dirent*  ealloc(struct dirent *dp, char *name, int attr);
struct dirent*  edup(struct dirent *entry);
void            eupdate(struct dirent *entry);
void            etrunc(struct dirent *entry);
void            eremove(struct dirent *entry);
void            eput(struct dirent *entry);
void            estat(struct dirent *ep, struct stat *st);
void            elock(struct dirent *entry);
void            eunlock(struct dirent *entry);
int             enext(struct dirent *dp, struct dirent *ep, uint off, int *count);
struct dirent*  ename(char *path);
struct dirent*  enameparent(char *path, char *name);
int             eread(struct dirent *entry, int user_dst, uint64 dst, uint off, uint n);
int             ewrite(struct dirent* entry, int user_src, uint64 src, uint off, uint n);

int is_mounted(const struct dirent* de);
int find_mount(const char* path);
void ekstat(struct dirent *de, struct kstat *st);

#endif