#include "../include/newfs.h"
#include "../include/types.h"

struct newfs_super sup;


/** 读磁盘。
 * 将 磁盘[st, st+len) 读到 内存[p, p+len), 不需要保证地址对齐。
 * @param p:内存地址, @param st:磁盘地址 @param len:读入长度
 */
void readdisk(void *p, int st, int len)
{
    int s0 = ROUND_DOWN(st, sup.sz_io);
    int sz = ROUND_UP(st+len-s0, sup.sz_io);
    char *tmp = (char*)malloc(sz);
    ddriver_seek(sup.fd, s0, SEEK_SET);
    for (int i = 0; i < sz; i += sup.sz_io)
    {
        ddriver_read(sup.fd, tmp+i, sup.sz_io);
    }
    memcpy(p, tmp+st-s0, len);
    free((void*)tmp);
}

/** 写磁盘。
 * 将 内存[p, p+len) 写入到 磁盘[st, st+len), 不需要保证地址对齐。
 * @param p:内存地址, @param st:磁盘地址 @param len:写长度
 */
void writedisk(void *p, int st, int len)
{
    
    int s0 = ROUND_DOWN(st, sup.sz_io);
    int sz = ROUND_UP(st+len-s0, sup.sz_io);
    char *tmp = malloc(sz);
    zxc("st=%d,len=%d\n", st, len);
    zxc("s0=%d,sz=%d\n", s0, sz);
    zxc("tmp=%p\n", tmp);
    readdisk(tmp, s0, sz);
    memcpy(tmp+st-s0, p, len);
    ddriver_seek(sup.fd, s0, SEEK_SET);
    for (int i = 0; i < sz; i += sup.sz_io)
    {
        ddriver_write(sup.fd, tmp+i, sup.sz_io);
    }
    zxc("now tmp=%p\n", tmp);
    free(tmp);
}

/** 操纵位图 bitmap，返回 pos 的值
 * @param bitmap: 位图, @param pos: 下标(按位编址的)
 * @return 值(0或1)
 */
int getbit(uint8_t *bitmap, int pos)
{
    uint8_t *fuck = bitmap + pos / 8;
    return ((*fuck)>>(pos%8)) & 1;
}

/** 操纵位图 bitmap，将 pos 的值设置为 val
 * @param bitmap: 位图, @param pos: 下标(按位编址的)
 * @param val: 值(0,1)
 */
void setbit(uint8_t *bitmap, int pos, int val)
{
    uint8_t *fuck = bitmap + pos / 8;
    if (val) *fuck = *fuck | (1<<(pos%8));
    else *fuck = *fuck & ~(1<<(pos%8));
}

/** 使用 malloc 分配空的 内存中 dentry
 * 只初始化 fname 和 ftype，inode_id 初始化为 -1，其它为 0。
 * @param fname 文件名, @param ftype 文件类型
 * @return 返回创建的 dentry
 */
struct newfs_dentry *new_dentry(char *fname, enum newfs_file_type ftype)
{
    struct newfs_dentry *ret =
        (struct newfs_dentry *)malloc(sizeof(struct newfs_dentry));
    memset(ret, 0, sizeof(struct newfs_dentry));
    strcpy(ret->fname, fname);
    ret->ftype = ftype;
    ret->inode_id = -1;
    return ret;
}

/** 在位图中找可用的 inode 并分配内存的数据结构。（新的。）
 * @param dentry 需要分配 inode 的 dentry
 * @return 返回分配的 inode。
 */
struct newfs_inode *alloc_inode(struct newfs_dentry *dentry)
{
    struct newfs_inode *inode;
    int found = -1;
    for (int i = 0; i < sup.cnt_inode; i++)
    {
        if (getbit(sup.inode_map, i) == 0)
        {
            found = i;
            break;
        }
    }
    if (found == -1) return 0;
    setbit(sup.inode_map, found, 1);
    inode = malloc(sizeof(struct newfs_inode));
    inode->inode_id = found;
    inode->fa = dentry;
    inode->ftype = dentry->ftype;
    inode->dir_cnt = 0;
    inode->dentrys = 0;
    inode->data = 0;
    inode->size = 0;
    memset(inode->block_pointer, -1, sizeof(inode->block_pointer));
    dentry->inode = inode;
    dentry->inode_id = found;
    return inode;
}

/** 在位图中找可用的 datablk，返回 id。
 * @return 返回分配的 datablk 编号。
 */
int alloc_datablk()
{
    for (int i = 0; i < sup.cnt_datablk; i++)
    {
        if (getbit(sup.datablk_map, i) == 0)
        {
            setbit(sup.datablk_map, i, 1);
            return i;
        }
    }
    return -1;
}

/** 读取对应 id 的 inode ，存储到 inode_d 中
 * @param inode_d: 存储位置 @param id: inode 编号
 */
void read_inode(struct newfs_inode_d *inode_d, int id)
{
    readdisk(
        inode_d,
        sup.offset_inode+id*sizeof(struct newfs_inode_d),
        sizeof(struct newfs_inode_d));
}

/** 写一个 inode_d ，编号在结构体中保存。
 * @param inode_d: 写的数据
 */
void write_inode(struct newfs_inode_d *inode_d)
{
    writedisk(
        inode_d,
        sup.offset_inode+inode_d->inode_id*sizeof(struct newfs_inode_d),
        sizeof(struct newfs_inode_d));
}

/**
 * 将一个 inode 下的所有内容同步保存到磁盘中。使用递归。
 * 可以认为只会在unmount 中用到。
 * @param inode: inode
 */
void sync_inode(struct newfs_inode *inode)
{
    int id = inode->inode_id;
    struct newfs_inode_d inode_d = {
        .inode_id = id,
        .size = inode->size,
        .ftype = inode->ftype,
        .dir_cnt = inode->dir_cnt
    };
    memcpy(inode_d.block_pointer, inode->block_pointer, sizeof(inode->block_pointer));
    if (inode->ftype == NEWFS_DIR && inode->dir_cnt > 0)
    {
        
        int tmp_size = inode->dir_cnt * sizeof(struct newfs_dentry_d);
        uint8_t *tmp = malloc(tmp_size);
        struct newfs_dentry_d *dentry_d = (struct newfs_dentry_d *)tmp;
        for (struct newfs_dentry *dentry = inode->dentrys; dentry != 0; dentry = dentry->nxt)
        {
            memcpy(dentry_d->fname, dentry->fname, MAX_NAME_LEN);
            dentry_d->ftype = dentry->ftype;
            dentry_d->inode_id = dentry->inode_id;
            if (dentry->inode != 0)
            {
                sync_inode(dentry->inode);
            }
            dentry_d += sizeof(struct newfs_dentry_d);
        }
        int blkid = 0;
        for (int i = 0; i < tmp_size; i += sup.sz_blk, blkid++)
        {
            int x = inode_d.block_pointer[blkid];
            if (x == -1) { x = alloc_datablk(); inode_d.block_pointer[blkid] = x; }
            int len = tmp_size-i; if (len > sup.sz_blk) len = sup.sz_blk;
            writedisk(tmp + i, sup.offset_datablk + sup.sz_blk * x, len);
        }
        free(tmp);
        
    }
    else if (inode->ftype == NEWFS_REGFILE)
    {
        int blkid = 0;
        for (int i = 0; i < inode->size; i += sup.sz_blk, blkid++)
        {
            int x = inode_d.block_pointer[blkid];
            if (x == -1) { x = alloc_datablk(); inode_d.block_pointer[blkid] = x; }
            int len = inode->size-i; if (len > sup.sz_blk) len = sup.sz_blk;
            writedisk(inode->data + i, sup.offset_datablk + sup.sz_blk * x, len);
        }
    }
    write_inode(&inode_d);
}

/**
 * 给定一个 inode 编号，及其对应的上方 dentry，获取对应 inode 内存结构
 * 注意，需要保证 dentry 没有存储对应 inode，并且本函数不负责更新对应 inode 指针
 * @param dentry: 它对应的 dentry @param inode_id 它的编号
 * @return 它的内存中 inode 结构
 */
struct newfs_inode *get_inode(struct newfs_dentry *dentry, int inode_id)
{
    struct newfs_inode *inode = malloc(sizeof(struct newfs_inode));
    struct newfs_inode_d inode_d;
    read_inode(&inode_d, inode_id);
    inode->inode_id = inode_d.inode_id;
    inode->size = inode_d.size;
    inode->dentrys = 0;
    inode->fa = dentry;
    inode->ftype = inode_d.ftype;
    memcpy(inode->block_pointer, inode_d.block_pointer, sizeof(inode->block_pointer));
    if (inode_d.ftype == NEWFS_DIR)
    {
        inode->dir_cnt = inode_d.dir_cnt;
        if (inode->dir_cnt > 0)
        {
            int tmp_size = inode->dir_cnt * sizeof(struct newfs_dentry_d);
            uint8_t *tmp = malloc(tmp_size);
            int blkid = 0;
            for (int i = 0; i < tmp_size; i += sup.sz_blk, blkid++)
            {
                int x = inode_d.block_pointer[blkid];
                int len = tmp_size-i; if (len > sup.sz_blk) len = sup.sz_blk;
                readdisk(tmp + i, sup.offset_datablk + sup.sz_blk * x, len);
            }
            struct newfs_dentry_d *dentry_d = (struct newfs_dentry_d *)tmp;
            for (int i = 0; i < inode->dir_cnt; i++)
            {
                struct newfs_dentry *sub_dentry = new_dentry(dentry_d->fname, dentry_d->ftype);
                sub_dentry->fa_inode = inode;
                sub_dentry->inode_id = dentry_d->inode_id;
                sub_dentry->nxt = inode->dentrys;
                inode->dentrys = sub_dentry;
                dentry_d += sizeof(struct newfs_dentry_d);
            }
            free(tmp);
        }
    }
    else
    {
        inode->data = malloc(inode->size);
        int blkid = 0;
        for (int i = 0; i < inode->size; i += sup.sz_blk, blkid++)
        {
            int x = inode_d.block_pointer[blkid];
            int len = inode->size-i; if (len > sup.sz_blk) len = sup.sz_blk;
            readdisk(inode->data + i, sup.offset_datablk + sup.sz_blk * x, len);
        }
    }
    return inode;
}

/** 返回第 dir_id 个目录项
 * @param inode: inode, dir_id 索引编号
 * @return 返回对应的dentry
 */
struct newfs_dentry *get_dentry(struct newfs_inode *inode, int dir_id)
{
    for (struct newfs_dentry *dentry = inode->dentrys; dentry != 0; dentry = dentry->nxt)
    {
        if (dir_id == 0) return dentry;
        dir_id--;
    }
    return 0;
}

char* get_fname(const char* path)
{
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}

/** 给出路径，寻找对应 dentry
 * @param path: 路径, @param found: 是否找到 @param is_rt 是否为根
 * @return dentry
 */
struct newfs_dentry *lookup(char *path, int *found, int *is_rt)
{
    // 是根目录
    if (strcmp(path, "/") == 0)
    {
        *found = 1;
        *is_rt = 1;
        return sup.root_dentry;
    } *is_rt = 0;
    char *path1 = malloc(strlen(path));
    strcpy(path1, path);
    int totdepth = 0, depth = 0;
    for (char *p = path; *p != 0; p++)
        if (*p == '/') totdepth++;
    char *fname = strtok(path1, "/");
    struct newfs_dentry *dentry = sup.root_dentry;
    struct newfs_inode *inode;
    struct newfs_dentry *ret;
    while (fname)
    {
        depth += 1;
        if (dentry->inode == 0)
            dentry->inode = get_inode(dentry, dentry->inode_id);
        inode = dentry->inode;
        if (inode->ftype == NEWFS_REGFILE && depth < totdepth)
        {
            zxc("%s is not a dir\n", fname);
            ret = dentry;
            *found = 0;
            break;
        }
        if (inode->ftype == NEWFS_DIR)
        {
            dentry = inode->dentrys;
            int fucked = 0;
            while (dentry)
            {
                if (strcmp(fname, dentry->fname) == 0)
                {
                    fucked = 1; break;
                }
                dentry = dentry->nxt;
            }
            if (fucked == 0)
            {
                *found = 0;
                zxc("%s not found\n", fname);
                ret = inode->fa;
                break;
            }
            if (fucked == 1 && depth == totdepth)
            {
                *found = 1;
                ret = dentry;
                break;
            }
        }
        fname = strtok(0, "/");
    }
    if (ret->inode == 0) ret->inode = get_inode(ret, ret->inode_id);
    free(path1);
    return ret;
}


/** 打印超级块的调试信息。
 * @param super_d: 超级块。
 */
void printinfo(struct newfs_super_d *super_d)
{
    zxc("magic=%08x\n", super_d->magic);
    zxc("inode cnt=%d\n", super_d->cnt_inode);
    zxc("datablk cnt=%d\n", super_d->cnt_datablk);
    zxc("offset inode map=%d\n", super_d->offset_inode_map);
    zxc("offset datablk map=%d\n", super_d->offset_datablk_map);
    zxc("offset inode=%d\n", super_d->offset_inode);
    zxc("offset datablk=%d\n", super_d->offset_datablk);
    zxc("size inode map=%d\n", super_d->size_inode_map);
    zxc("size datablk map=%d\n", super_d->size_datablk_map);
}

int newfs_mount(struct custom_options opt)
{
    int init = 0;
    if (sup.mounted == 1) return 0;
    struct newfs_super_d super_d;
    // 打开磁盘
    int fd = ddriver_open(opt.device);
    if (fd < 0) return fd;
    sup.fd = fd;
    // 获取磁盘大小和单次 IO 大小
    ddriver_ioctl(sup.fd, IOC_REQ_DEVICE_SIZE, &sup.sz_disk);
    ddriver_ioctl(sup.fd, IOC_REQ_DEVICE_IO_SZ, &sup.sz_io);
    sup.sz_blk = 1024;
    // 创建根的 单独的dentry
    struct newfs_dentry *root_dentry = new_dentry("/", NEWFS_DIR);
    // 读取超级块
    readdisk(&super_d, 0, sizeof(super_d));
    // 格式化磁盘
    if (super_d.magic != NEWFS_MAGIC)
    {
        super_d.magic = NEWFS_MAGIC;
        init = 1;
        int filecnt = sup.sz_disk / (16*1024); // 估算文件数
        super_d.cnt_inode = filecnt; // inode 实际数量
        int datablk_max_cnt = ROUND_UP(sup.sz_disk, sup.sz_blk) / sup.sz_blk; // datablk 估算数量
        // 分配 offset
        // inode map
        super_d.offset_inode_map = sup.sz_blk;
        int inode_map_byte = ROUND_UP(super_d.cnt_inode, 8) / 8;
        super_d.size_inode_map = ROUND_UP(inode_map_byte, sup.sz_blk);
        // datablk_map
        super_d.offset_datablk_map = super_d.offset_inode_map + super_d.size_inode_map;
        int datablk_map_byte = ROUND_UP(datablk_max_cnt, 8) / 8;
        super_d.size_datablk_map = ROUND_UP(datablk_map_byte, sup.sz_blk);
        // inode
        super_d.offset_inode = super_d.offset_datablk_map + super_d.size_datablk_map;
        int inode_byte = sizeof(struct newfs_inode_d) * super_d.cnt_inode;
        int inode_size = ROUND_UP(inode_byte, sup.sz_blk);
        // datablk
        super_d.offset_datablk = super_d.offset_inode + inode_size;
        int real_datablk_cnt = (sup.sz_disk - super_d.offset_datablk) / sup.sz_blk;
        super_d.cnt_datablk = real_datablk_cnt;
    }
    printinfo(&super_d);

    // 超级块内容复制到内存
    sup.cnt_inode = super_d.cnt_inode;
    sup.cnt_datablk = super_d.cnt_datablk;
    sup.offset_inode_map = super_d.offset_inode_map;
    sup.offset_datablk_map = super_d.offset_datablk_map;
    sup.offset_inode = super_d.offset_inode;
    sup.offset_datablk = super_d.offset_datablk;
    sup.size_inode_map = super_d.size_inode_map;
    sup.size_datablk_map = super_d.size_datablk_map;
    sup.mounted = 1;
    sup.inode_map = malloc(super_d.size_inode_map);
    sup.datablk_map = malloc(super_d.size_datablk_map);
    if (init)
    {
        memset(sup.inode_map, 0, super_d.size_inode_map);
        memset(sup.datablk_map, 0, super_d.size_datablk_map);
    }
    else
    {
        readdisk(sup.inode_map, super_d.offset_inode_map, super_d.size_inode_map);
        readdisk(sup.datablk_map, super_d.offset_datablk_map, super_d.size_datablk_map);
    }

    if (init)
    {
         // generate root inode
        struct newfs_inode *root_inode = alloc_inode(root_dentry);
        sync_inode(root_inode);
        free(root_inode);
        root_dentry->inode = 0;
    }
    if (root_dentry->inode == 0)
    {
        root_dentry->inode = get_inode(root_dentry, 0);
        root_dentry->inode_id = 0;
    }
    sup.root_dentry = root_dentry;
    return 0;
}

// 卸载
int newfs_unmount()
{
    if (sup.mounted == 0) return 0;
    zxc("rewrite datas and inodes\n");
    sync_inode(sup.root_dentry->inode);
    // 回写超级块
    zxc("rewrite super block\n");
    struct newfs_super_d super_d = {
        .magic = NEWFS_MAGIC,
        .cnt_inode = sup.cnt_inode,
        .cnt_datablk = sup.cnt_datablk,
        .offset_inode_map = sup.offset_inode_map,
        .offset_datablk_map = sup.offset_datablk_map,
        .offset_inode = sup.offset_inode,
        .offset_datablk = sup.offset_datablk,
        .size_inode_map = sup.size_inode_map,
        .size_datablk_map = sup.size_datablk_map
    };
    writedisk(&super_d, 0, sizeof(struct newfs_super_d));
    zxc("rewrite inode bitmap\n");
    // 回写 bitmap
    writedisk(sup.inode_map, super_d.offset_inode_map, super_d.size_inode_map);
    zxc("rewrite datablk bitmap\n");
    writedisk(sup.datablk_map, super_d.offset_datablk_map, super_d.size_datablk_map);
    // 释放空间
    free(sup.inode_map);
    free(sup.datablk_map);
    // 关闭设备
    ddriver_close(sup.fd);
    return 0;
}