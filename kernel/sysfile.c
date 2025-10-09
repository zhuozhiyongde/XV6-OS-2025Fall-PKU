//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//


#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/stat.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/sleeplock.h"
#include "include/file.h"
#include "include/pipe.h"
#include "include/fcntl.h"
#include "include/fat32.h"
#include "include/syscall.h"
#include "include/string.h"
#include "include/printf.h"
#include "include/vm.h"
#include "include/fcntl.h"

/**
 * @brief 递归地获取一个目录条目的绝对路径。
 * @param de        目标目录条目。
 * @param path_buf  用于存储结果的输出缓冲区。
 * @param buf_size  缓冲区的总大小。
 * @return 成功返回 0，失败返回 -1。
 */
static int get_abspath(struct dirent* de, char* path_buf, int buf_size) {
  // 递归退出，已经到达根目录
  if (de == NULL || de->parent == NULL) {
    if (buf_size < 2) {
      return -1;
    }
    strncpy(path_buf, "/", buf_size);
    return 0;
  }
  if (get_abspath(de->parent, path_buf, buf_size) < 0) {
    return -1;
  }
  int parent_len = strlen(path_buf);

  // 非根目录需要追加一个 /
  if (parent_len > 1) {
    if (parent_len + 1 >= buf_size) {
      return -1;
    }
    path_buf[parent_len++] = '/';
    path_buf[parent_len] = '\0';
  }

  safestrcpy(path_buf + parent_len, de->filename, buf_size - parent_len);
  return 0;
}

/**
 * @brief 将路径参数安全地转换为绝对路径。
 * @param path  输入的路径字符串 (in)，转换后的绝对路径 (out)。缓冲区大小应为 FAT32_MAX_PATH。
 * @param fd    目录文件描述符 dirfd。
 * @return 成功返回 0，失败返回 -1。
 */
int get_path(char* path, int fd) {
  if (path == NULL) {
    return -1;
  }
  // 绝对路径无需处理
  if (path[0] == '/') {
    return 0;
  }
  // 预处理 './'
  if (path[0] == '.' && path[1] == '/') {
    path += 2;
  }
  char base_path[FAT32_MAX_PATH];
  struct dirent* base_de = NULL;
  // 相对当前目录进行定位
  if (fd == AT_FDCWD) {
    base_de = myproc()->cwd;
  }
  // 相对于指定的 fd 定位
  else {
    if (fd < 0 || fd >= NOFILE) {
      return -1;
    }
    struct file* f = myproc()->ofile[fd];
    if (f == NULL || !(f->ep->attribute & ATTR_DIRECTORY)) {
      return -1;
    }
    base_de = f->ep;
  }
  // 获取绝对路径
  if (get_abspath(base_de, base_path, FAT32_MAX_PATH) < 0) {
    return -1;
  }
  // 使用一个临时缓冲区来安全地拼接最终路径
  char final_path[FAT32_MAX_PATH];

  safestrcpy(final_path, base_path, FAT32_MAX_PATH);
  int base_len = strlen(final_path);

  // 非根目录需要追加一个 /
  if (base_len > 1) {
    if (base_len + 1 >= sizeof(final_path)) {
      return -1;
    }
    final_path[base_len++] = '/';
    final_path[base_len] = '\0';
  }

  safestrcpy(final_path + base_len, path, FAT32_MAX_PATH - base_len);
  safestrcpy(path, final_path, FAT32_MAX_PATH);

  return 0;
}

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == NULL)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  if(argfd(0, 0, &f) < 0 || argaddr(1, &st) < 0)
    return -1;
  return filestat(f, st);
}

static struct dirent*
create(char *path, short type, int mode)
{
  struct dirent *ep, *dp;
  char name[FAT32_MAX_FILENAME + 1];

  if((dp = enameparent(path, name)) == NULL)
    return NULL;

  if (type == T_DIR) {
    mode = ATTR_DIRECTORY;
  } else if (mode & O_RDONLY) {
    mode = ATTR_READ_ONLY;
  } else {
    mode = 0;  
  }

  elock(dp);
  if ((ep = ealloc(dp, name, mode)) == NULL) {
    eunlock(dp);
    eput(dp);
    return NULL;
  }
  
  if ((type == T_DIR && !(ep->attribute & ATTR_DIRECTORY)) ||
      (type == T_FILE && (ep->attribute & ATTR_DIRECTORY))) {
    eunlock(dp);
    eput(ep);
    eput(dp);
    return NULL;
  }

  eunlock(dp);
  eput(dp);

  elock(ep);
  return ep;
}

uint64
sys_open(void)
{
  char path[FAT32_MAX_PATH];
  int fd, omode;
  struct file *f;
  struct dirent *ep;

  if(argstr(0, path, FAT32_MAX_PATH) < 0 || argint(1, &omode) < 0)
    return -1;

  if(omode & O_CREATE){
    ep = create(path, T_FILE, omode);
    if(ep == NULL){
      return -1;
    }
  } else {
    if((ep = ename(path)) == NULL){
      return -1;
    }
    elock(ep);
    // 下面这行需要修改，参见 openat 处的注释
    if((ep->attribute & ATTR_DIRECTORY) && (omode & (O_WRONLY | O_RDWR))){
      eunlock(ep);
      eput(ep);
      return -1;
    }
  }

  if((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0){
    if (f) {
      fileclose(f);
    }
    eunlock(ep);
    eput(ep);
    return -1;
  }

  if(!(ep->attribute & ATTR_DIRECTORY) && (omode & O_TRUNC)){
    etrunc(ep);
  }

  f->type = FD_ENTRY;
  f->off = (omode & O_APPEND) ? ep->file_size : 0;
  f->ep = ep;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  eunlock(ep);

  return fd;
}

/**
 * @brief 实现 openat 系统调用
 * @param dirfd 目录文件描述符
 * @param path 文件路径
 * @param flags 标志位，等于原 open 函数的 omode
 * @param mode 模式，规定当文件被新创建时应有的文件权限
 * @return 文件描述符
 * @note openat 相较于 open 多了一个参数，即目录文件描述符，它会根据 dirfd + path 来确定文件绝对路径，然后复用 open 的逻辑
 * @note sys_open: 相对路径基于当前工作目录，或直接使用绝对路径。
 * @note sys_openat: 相对路径基于指定的 fd 所代表的目录，或直接使用绝对路径。
 */
uint64
sys_openat(void) {
  char path[FAT32_MAX_PATH];
  int dirfd, flags, mode, fd;
  struct file* f;
  struct dirent* ep;

  if (
    argint(0, &dirfd) < 0 ||
    argstr(1, path, FAT32_MAX_PATH) < 0 ||
    argint(2, &flags) < 0 ||
    argint(3, &mode) < 0
    ) {
    return -1;
  }

  if (strlen(path) == 0) {
    return -1;
  }

  if (get_path(path, dirfd) < 0) {
    return -1;
  }

  if (flags & O_CREATE) {
    ep = create(path, T_FILE, mode);
    if (ep == NULL) {
      return -1;
    }
  }
  else {
    if ((ep = ename(path)) == NULL) {
      return -1;
    }
    elock(ep);
    // 注意下面这行，目的是：如果一个文件是目录，那么禁止任何带有“写”意图的打开方式
    // 这里需要修改第二个条件，判断是否可写的条件从 flags != O_RDONLY 改为 flags & (O_WRONLY | O_RDWR)
    // 测试发现传入的 ep->attribute 为 16，也就是 ATTR_DIRECTORY 0x10
    // 如果使用 flags != O_RDONLY 得到 1 导致判断为真，导致返回 -1
    // 如果使用 flags & (O_WRONLY | O_RDWR) 得到 0x10 & (0x01 | 0x02) = 0，导致判断为假，不会返回 -1
    // 对于 open() 在此处的判断类似
    if ((ep->attribute & ATTR_DIRECTORY) && (flags & (O_WRONLY | O_RDWR))) {
      eunlock(ep);
      eput(ep);
      return -1;
    }
  }

  if ((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0) {
    if (f) {
      fileclose(f);
    }
    eunlock(ep);
    eput(ep);
    return -1;
  }

  if (!(ep->attribute & ATTR_DIRECTORY) && (flags & O_TRUNC)) {
    etrunc(ep);
  }

  f->type = FD_ENTRY;
  f->off = (flags & O_APPEND) ? ep->file_size : 0;
  f->ep = ep;
  f->readable = !(flags & O_WRONLY);
  f->writable = (flags & O_WRONLY) || (flags & O_RDWR);

  eunlock(ep);

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[FAT32_MAX_PATH];
  struct dirent *ep;

  if(argstr(0, path, FAT32_MAX_PATH) < 0 || (ep = create(path, T_DIR, 0)) == 0){
    return -1;
  }
  eunlock(ep);
  eput(ep);
  return 0;
}

uint64
sys_chdir(void)
{
  char path[FAT32_MAX_PATH];
  struct dirent *ep;
  struct proc *p = myproc();
  
  if(argstr(0, path, FAT32_MAX_PATH) < 0 || (ep = ename(path)) == NULL){
    return -1;
  }
  elock(ep);
  if(!(ep->attribute & ATTR_DIRECTORY)){
    eunlock(ep);
    eput(ep);
    return -1;
  }
  eunlock(ep);
  eput(p->cwd);
  p->cwd = ep;
  return 0;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  if(argaddr(0, &fdarray) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  // if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
  //    copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
  if(copyout2(fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout2(fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}

// To open console device.
uint64
sys_dev(void)
{
  int fd, omode;
  int major, minor;
  struct file *f;

  if(argint(0, &omode) < 0 || argint(1, &major) < 0 || argint(2, &minor) < 0){
    return -1;
  }

  if(omode & O_CREATE){
    panic("dev file on FAT");
  }

  if(major < 0 || major >= NDEV)
    return -1;

  if((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    return -1;
  }

  f->type = FD_DEVICE;
  f->off = 0;
  f->ep = 0;
  f->major = major;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  return fd;
}

// To support ls command
uint64
sys_readdir(void)
{
  struct file *f;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argaddr(1, &p) < 0)
    return -1;
  return dirnext(f, p);
}


/**
 * @brief 实现 getcwd 系统调用
 * @param addr 路径字符串需要存放到的目标地址
 * @param size 目标地址大小
 * @note 这段代码的实现是这样的，先构建一个 path 缓冲区，然后从末尾开始写，形成类似 [ ...垃圾数据... | /home/user\0 ] 这样的路径
 * @note 然后，再利用 memmove 将这个路径移动到开始，形成 [ /home/user\0 | ...垃圾数据... ] 这样的路径
 * @note 最后，再利用 copyout2 将这个路径从内核栈中拷贝到用户空间
 * @return 0 成功，-1 失败
 */
uint64
sys_getcwd(void) {
  uint64 addr;
  int size;
  if (argaddr(0, &addr) < 0 || argint(1, &size) < 0)
    return NULL;

  struct dirent* de = myproc()->cwd;
  char path[FAT32_MAX_PATH];

  char* s = path + sizeof(path) - 1;
  *s = '\0';

  if (de->parent == NULL) {
    s--;
    *s = '/';
  }
  else {
    while (de->parent) {
      int len = strlen(de->filename);
      s -= len;
      if (s < path)
        return NULL;
      memmove(s, de->filename, len);

      s--;
      if (s < path)
        return NULL;
      *s = '/';

      de = de->parent;
    }
  }

  memmove(path, s, strlen(s) + 1);

  int path_length = strlen(path) + 1;
  if (path_length > size) {
    return NULL;
  }

  if (copyout2(addr, path, strlen(path) + 1) < 0)
    return NULL;

  return addr;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct dirent *dp)
{
  struct dirent ep;
  int count;
  int ret;
  ep.valid = 0;
  ret = enext(dp, &ep, 2 * 32, &count);   // skip the "." and ".."
  return ret == -1;
}

uint64
sys_remove(void)
{
  char path[FAT32_MAX_PATH];
  struct dirent *ep;
  int len;
  if((len = argstr(0, path, FAT32_MAX_PATH)) <= 0)
    return -1;

  char *s = path + len - 1;
  while (s >= path && *s == '/') {
    s--;
  }
  if (s >= path && *s == '.' && (s == path || *--s == '/')) {
    return -1;
  }
  
  if((ep = ename(path)) == NULL){
    return -1;
  }
  elock(ep);
  if((ep->attribute & ATTR_DIRECTORY) && !isdirempty(ep)){
      eunlock(ep);
      eput(ep);
      return -1;
  }
  elock(ep->parent);      // Will this lead to deadlock?
  eremove(ep);
  eunlock(ep->parent);
  eunlock(ep);
  eput(ep);

  return 0;
}

// Must hold too many locks at a time! It's possible to raise a deadlock.
// Because this op takes some steps, we can't promise
uint64
sys_rename(void)
{
  char old[FAT32_MAX_PATH], new[FAT32_MAX_PATH];
  if (argstr(0, old, FAT32_MAX_PATH) < 0 || argstr(1, new, FAT32_MAX_PATH) < 0) {
      return -1;
  }

  struct dirent *src = NULL, *dst = NULL, *pdst = NULL;
  int srclock = 0;
  char *name;
  if ((src = ename(old)) == NULL || (pdst = enameparent(new, old)) == NULL
      || (name = formatname(old)) == NULL) {
    goto fail;          // src doesn't exist || dst parent doesn't exist || illegal new name
  }
  for (struct dirent *ep = pdst; ep != NULL; ep = ep->parent) {
    if (ep == src) {    // In what universe can we move a directory into its child?
      goto fail;
    }
  }

  uint off;
  elock(src);     // must hold child's lock before acquiring parent's, because we do so in other similar cases
  srclock = 1;
  elock(pdst);
  dst = dirlookup(pdst, name, &off);
  if (dst != NULL) {
    eunlock(pdst);
    if (src == dst) {
      goto fail;
    } else if (src->attribute & dst->attribute & ATTR_DIRECTORY) {
      elock(dst);
      if (!isdirempty(dst)) {    // it's ok to overwrite an empty dir
        eunlock(dst);
        goto fail;
      }
      elock(pdst);
    } else {                    // src is not a dir || dst exists and is not an dir
      goto fail;
    }
  }

  if (dst) {
    eremove(dst);
    eunlock(dst);
  }
  memmove(src->filename, name, FAT32_MAX_FILENAME);
  emake(pdst, src, off);
  if (src->parent != pdst) {
    eunlock(pdst);
    elock(src->parent);
  }
  eremove(src);
  eunlock(src->parent);
  struct dirent *psrc = src->parent;  // src must not be root, or it won't pass the for-loop test
  src->parent = edup(pdst);
  src->off = off;
  src->valid = 1;
  eunlock(src);

  eput(psrc);
  if (dst) {
    eput(dst);
  }
  eput(pdst);
  eput(src);

  return 0;

fail:
  if (srclock)
    eunlock(src);
  if (dst)
    eput(dst);
  if (pdst)
    eput(pdst);
  if (src)
    eput(src);
  return -1;
}
