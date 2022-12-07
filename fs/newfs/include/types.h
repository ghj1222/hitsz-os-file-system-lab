#ifndef _TYPES_H_
#define _TYPES_H_

#define MAX_NAME_LEN    128

// Macro functions
#define ROUND_DOWN(x, y) ((x)-(x)%(y))
#define ROUND_UP(x, y)  ROUND_DOWN((x)+(y)-1,y)
#define zxc(format, ...) printf("\t\t\t\t##### fucked_debug[%s]: " format, __func__, ##__VA_ARGS__)

struct custom_options
{
	const char*        device;
};

// 文件类型：普通文件，目录
enum newfs_file_type
{
    NEWFS_REGFILE,
    NEWFS_DIR
};

// 内存中超级块的数据结构
struct newfs_super
{
    int fd; // 文件描述符
    int sz_disk; // 磁盘大小
    int sz_io; // 单次 IO 大小
    int sz_blk; // 单块大小

    int cnt_inode; // inode 数量
    int cnt_datablk; // data block 数量

    int offset_inode_map; // inode_map 区域起点
    int offset_datablk_map; // datablk_map 区域起点
    int offset_inode; // inode区域起点
    int offset_datablk; // data区域起点
    int size_inode_map;
    int size_datablk_map;
    int mounted;
    
    struct newfs_dentry *root_dentry; // 根目录的 dentry
    uint8_t *inode_map;
    uint8_t *datablk_map;
};

// 内存中 inode 数据结构
struct newfs_inode
{
    int inode_id; // 在 inode bitmap 中的地址
    struct newfs_dentry *fa; // 所对应的 dentry
    enum newfs_file_type ftype; // 我是什么
    int dir_cnt; // 子 dentry 数量 (if dir)
    struct newfs_dentry *dentrys; // 所有子 dentry (if dir)
    uint8_t *data; // 实际内容副本 (if regfile)
    int size; // 实际内容大小 (if regfile)
    int block_pointer[6]; // 数据块指针
};

// 内存中 dentry 数据结构
struct newfs_dentry
{
    char fname[MAX_NAME_LEN]; // 文件名
    enum newfs_file_type ftype; // 文件类型
    struct newfs_inode *fa_inode; // 所属 inode(保证为 dir)
    struct newfs_dentry *nxt; // 兄弟
    struct newfs_inode *inode; // 指向的inode
    int inode_id;
};

// 磁盘上 super block 的结构
struct newfs_super_d
{
    uint32_t magic; // magic number
    int cnt_inode; // inode 数量
    int cnt_datablk; // data block 数量

    int offset_inode_map; // inode_map 区域起点
    int offset_datablk_map; // datablk_map 区域起点
    int offset_inode; // inode区域起点
    int offset_datablk; // data区域起点
    int size_inode_map;
    int size_datablk_map;
};

struct newfs_inode_d
{
    int inode_id; // 编号
    int size; // 文件大小
    enum newfs_file_type ftype; // 文件类型
    int dir_cnt; // 目录中内容的数量 (if dir)
    int block_pointer[6]; // 数据块指针
};

struct newfs_dentry_d
{
    char fname[MAX_NAME_LEN]; // 文件名
    enum newfs_file_type ftype; // 文件类型
    int inode_id; // 指向的 inode 编号
   //  int valid; // 是否有效
};

#endif /* _TYPES_H_ */