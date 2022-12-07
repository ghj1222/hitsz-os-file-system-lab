#ifndef _NEWFS_H_
#define _NEWFS_H_

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>
#include "fcntl.h"
#include "string.h"
#include "fuse.h"
#include <stddef.h>
#include "ddriver.h"
#include "errno.h"
#include "types.h"

#define NEWFS_MAGIC    0xEA114514   /* TODO: Define by yourself */
#define NEWFS_DEFAULT_PERM    0777   /* 全权限打开 */

/******************************************************************************
* SECTION: newfs.c
*******************************************************************************/
void* 			   newfs_init(struct fuse_conn_info *);
void  			   newfs_destroy(void *);
int   			   newfs_mkdir(const char *, mode_t);
int   			   newfs_getattr(const char *, struct stat *);
int   			   newfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
						                struct fuse_file_info *);
int   			   newfs_mknod(const char *, mode_t, dev_t);
int   			   newfs_write(const char *, const char *, size_t, off_t,
					                  struct fuse_file_info *);
int   			   newfs_read(const char *, char *, size_t, off_t,
					                 struct fuse_file_info *);
int   			   newfs_access(const char *, int);
int   			   newfs_unlink(const char *);
int   			   newfs_rmdir(const char *);
int   			   newfs_rename(const char *, const char *);
int   			   newfs_utimens(const char *, const struct timespec tv[2]);
int   			   newfs_truncate(const char *, off_t);
			
int   			   newfs_open(const char *, struct fuse_file_info *);
int   			   newfs_opendir(const char *, struct fuse_file_info *);

// newfs_utils.c
void readdisk(void *p, int st, int len);
void writedisk(void *p, int st, int len);
int getbit(uint8_t *bitmap, int pos);
void setbit(uint8_t *bitmap, int pos, int val);

struct newfs_dentry *new_dentry(char *fname, enum newfs_file_type ftype);
struct newfs_inode *alloc_inode(struct newfs_dentry *dentry);
int alloc_datablk();
void read_inode(struct newfs_inode_d *inode_d, int id);
void write_inode(struct newfs_inode_d *inode_d);
void sync_inode(struct newfs_inode *inode);
struct newfs_inode *get_inode(struct newfs_dentry *dentry, int inode_id);
struct newfs_dentry *get_dentry(struct newfs_inode *inode, int dir_id);
char* get_fname(const char* path);
struct newfs_dentry *lookup(char *path, int *found, int *is_rt);
void printinfo(struct newfs_super_d *super_d);


int newfs_mount(struct custom_options opt);
int newfs_unmount();

#endif  /* _newfs_H_ */