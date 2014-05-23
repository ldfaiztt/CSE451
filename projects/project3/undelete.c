// Copyright 2013 Elliott Brossard and James Youngquist.
//
// This file is part of cse451-undelete.
//
// cse451-undelete is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// cse451-undelete is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with cse451-undelete.  If not, see <http://www.gnu.org/licenses/>.

#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/dcache.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/namei.h>
#include <linux/security.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/buffer_head.h>
#include <linux/string.h>
#include <asm/bitops.h>
#include "tar_utils.h"
#include "ext2.h"

MODULE_LICENSE("GPL");

//####################################################################
// DEFINES AND MACROS
//####################################################################

// Comment this out and recompile to remove debugging messages
#define DEBUG

#ifdef DEBUG
// This macro can be used to issue debug messages.  Use it exactly like
// printk, except that it adds a '\n' for you at the end.
#define DBG(format, args...) printk("[%s] " format "\n", __func__, ##args)
#else
#define DBG(X)
#endif

// This macro can be used to issue alert messages.  Use it exactly like
// printk, except that it adds a '\n' for you at the end.
#define ALERT(format, args...) printk(KERN_ALERT "ALERT [%s] " format "\n", __func__, ##args)

//####################################################################
// GLOBALS AND DATA STRUCTURES
//####################################################################
static struct ext2_inode *ext2_get_inode(struct super_block *sb, ino_t ino,
					struct buffer_head **p)
{
	struct buffer_head * bh;
	unsigned long block_group;
	unsigned long block;
	unsigned long offset;
	struct ext2_group_desc * gdp;

	*p = NULL;
	if ((ino != EXT2_ROOT_INO && ino < EXT2_FIRST_INO(sb)) ||
	    ino > le32_to_cpu(EXT2_SB(sb)->s_es->s_inodes_count)) {
		/*if (ino != EXT2_ROOT_INO) {
			printk("ROOOOOOOOOOOOT_INO GG\n");
			printk("%d\n", EXT2_ROOT_INO);
			printk("%d\n", (int)ino);
			printk("%d\n", EXT2_FIRST_INO(sb));
		}
		if (ino < EXT2_FIRST_INO(sb)) {
			printk("FIRST INO GG\n");
		}
		if (ino > le32_to_cpu(EXT2_SB(sb)->s_es->s_inodes_count)) {
			printk("Total count GGn\n");
		}
		printk("AAAAAAAAAAAAAAAAAAAAAAAAA\n");*/
		goto Error;
	}
	block_group = (ino - 1) / EXT2_INODES_PER_GROUP(sb);
	gdp = ext2_get_group_desc(sb, block_group, NULL);
	if (!gdp) {
		//printk("BBBBBBBBBBBBBBBBBBBBBBBBBB\n");
		goto Error;
	}
	/*
	 * Figure out the offset within the block group inode table
	 */
	offset = ((ino - 1) % EXT2_INODES_PER_GROUP(sb)) * EXT2_INODE_SIZE(sb);
	block = le32_to_cpu(gdp->bg_inode_table) +
		(offset >> EXT2_BLOCK_SIZE_BITS(sb));
	if (!(bh = sb_bread(sb, block))) {
		//printk("CCCCCCCCCCCCCCCCCCCCCCCCCCC\n");
		goto Error;
	}

	*p = bh;
	offset &= (EXT2_BLOCK_SIZE(sb) - 1);
	return (struct ext2_inode *) (bh->b_data + offset);

Error:
	return NULL;
}

struct ext2_group_desc * ext2_get_group_desc(struct super_block * sb,
					     unsigned int block_group,
					     struct buffer_head ** bh)
{
	unsigned long group_desc;
	unsigned long offset;
	struct ext2_group_desc * desc;
	struct ext2_sb_info *sbi = EXT2_SB(sb);

	if (block_group >= sbi->s_groups_count) {
		return NULL;
	}

	group_desc = block_group >> EXT2_DESC_PER_BLOCK_BITS(sb);
	offset = block_group & (EXT2_DESC_PER_BLOCK(sb) - 1);
	if (!sbi->s_group_desc[group_desc]) {
		return NULL;
	}

	desc = (struct ext2_group_desc *) sbi->s_group_desc[group_desc]->b_data;
	if (bh)
		*bh = sbi->s_group_desc[group_desc];
	return desc + offset;
}
// Holds information for a specific undelete device
struct undelete_dev {
    // This structure holds details about the character device
    struct cdev cdev;

    // The buffer to which the stream of tar-format data for undeleted files
    // should be written.
    char *buffer;
    
    // The total length of the buffer. Buffers are initialized to the size of
    // blocks in the corresponding file system.
    size_t buffer_len;
    
    // The current read offset. This is the location from which calls to
    // undelete_read should begin reading data.
    size_t buffer_read_offset;
    
    // The current write offset. This is how much the last buffer filling
    // operation wrote up to.  The next read request will read up to this
    // number of bytes.  It is used to simulate a dynamic sized buffer, that
    // is, buffer_write_offset <= buffer_len.
    size_t buffer_write_offset;

    // The super block of the ext2 file system associated with this device.
    struct super_block *super_block;

    // TODO:
		unsigned int group_index;
		unsigned int bitmap_offset;
		int has_written_header;
		struct ext2_inode* curr_inode;
		int inode_id;
		unsigned int level_1_index;
		unsigned int level_2_index;		
		unsigned int level_3_index;
		unsigned int level_4_index;
		
		unsigned long in_written;
		
		int last;
    // - Add whatever state you need to keep track of where in
    //   the undelete operation you are for this file system.
};

static const char kDeviceName[] = "undelete";
static const char kDeviceDir[] = "/dev/undelete";

// A map of minor number to ext2 super block for
// each mounted ext2 file system.
static struct super_block **super_block_map = NULL;
static int num_super_blocks = 0;

// One struct for each mounted ext2 file system, indexed by minor number.
static struct undelete_dev *undelete_devs = NULL;

// For tracking the device nodes
static dev_t undelete_devnode;
static char **undelete_dev_paths = NULL;

// Impose an arbitrary limit on the number of mounted file systems to handle.
static const int kSuperBlockLimit = 10;

// The file operations structure.  This is used to set function pointers for
// all file related operations like open, read, etc.
static struct file_operations undelete_fops;

//####################################################################
// FUNCTIONS
//####################################################################

//====================================================================
// Iterator function for adding an entry to the super block map for
// each ext2 file system.
static void build_super_block_map_iter_fn (struct super_block *sb, void *arg) {
    if (num_super_blocks >= kSuperBlockLimit) {
        DBG("Hit limit of %d file systems", kSuperBlockLimit);
        return;
    }

    // Attempt to obtain a reference to this file system's super block.
    if (!atomic_inc_not_zero(&sb->s_active)) {
        DBG("Couldn't increment s_active for %s", sb->s_id);
        return;
    }

    // Add the super block to the map.
    super_block_map[num_super_blocks++] = sb;
    DBG("Adding mapping %d -> %s", num_super_blocks, sb->s_id);
}

//====================================================================
// Build the map of device minor numbers to super blocks. Returns
// 0 on success and non-zero on failure.
static int build_super_block_map(void) {
    struct file_system_type *ext2_type;

    num_super_blocks = 0;
    super_block_map = kmalloc(kSuperBlockLimit * sizeof(*super_block_map),
                              GFP_KERNEL);
    if (!super_block_map) {
        return -ENOMEM;
    }

    ext2_type = get_fs_type("ext2");
    if (!ext2_type) {
        kfree(super_block_map);
        return -ENODEV;
    }

    iterate_supers_type(ext2_type, build_super_block_map_iter_fn, NULL);
    DBG("Found %d ext2 filesystems", num_super_blocks);
    return 0;
}

//====================================================================
// Create a directory for this device under the given path. Returns
// 0 on success and non-zero on failure.
static int create_dev_dir(const char *dev_dir_path) {
    struct dentry *dentry;
    struct path path;
    int lookup_flags = LOOKUP_DIRECTORY;
    umode_t mode = S_IRUGO | S_IXUGO;
    int err;

    dentry = kern_path_create(AT_FDCWD, dev_dir_path, &path, lookup_flags);
    if (IS_ERR(dentry)) {
        return PTR_ERR(dentry);
    }
    err = security_path_mkdir(&path, dentry, mode);
    if (!err) {
        err = vfs_mkdir(path.dentry->d_inode, dentry, mode);
    }
    done_path_create(&path, dentry);
    return err;
}

//====================================================================
// Delete the given device directory. Returns 0 on success and
// non-zero on failure.
static int delete_dev_dir(const char *dev_dir_path) {
    int err = 0;
    struct path path;
    unsigned int lookup_flags = LOOKUP_REVAL;

    // Look up the path for this directory.
    if ((err = kern_path(dev_dir_path, lookup_flags, &path)) != 0) {
        return err;
    }

    if (!path.dentry || !path.dentry->d_parent) {
        ALERT("Undelete device directory entry has no parent");
        return -EINVAL;
    }

    if ((err = vfs_rmdir(path.dentry->d_parent->d_inode,
                         path.dentry)) != 0) {
        return err;
    }

    return err;
}

//====================================================================
// Create a node for this device under the given path using the given
// device number. Returns 0 on success and non-zero on failure.
static int create_fs_node(const char *dev_path, dev_t undelete_minor_devnode) {
    struct dentry *dentry;
    struct path path;
    int lookup_flags = 0;
    umode_t mode = S_IRUGO | S_IFCHR;
    int err;

    dentry = kern_path_create(AT_FDCWD, dev_path, &path, lookup_flags);
    if (IS_ERR(dentry)) {
        return PTR_ERR(dentry);
    }
    err = security_path_mknod(&path, dentry, mode, undelete_minor_devnode);
    if (!err) {
        err = vfs_mknod(path.dentry->d_inode, dentry,
                        mode, undelete_minor_devnode);
    }
    done_path_create(&path, dentry);
    return err;
}

//====================================================================
// Delete the node with the given path. Returns 0 on success and
// non-zero on failure.
static int delete_fs_node(const char *dev_path) {
    int err = 0;
    struct path path;
    unsigned int lookup_flags = 0;

    // Look up the path for this directory.
    if ((err = kern_path(dev_path, lookup_flags, &path)) != 0) {
        return err;
    }

    if (!path.dentry->d_parent) {
        ALERT("Undelete device path directory entry has no parent");
        return -EINVAL;
    }

    if ((err = vfs_unlink(path.dentry->d_parent->d_inode,
                          path.dentry)) != 0) {
        return err;
    }

    return err;
}

//====================================================================
// Unregister the character devices, remove their files under
// kDeviceDir, and delete the kDeviceDir directory.
static void unregister_character_devices(void) {
    int err, i;
    for (i = 0; i < num_super_blocks; ++i) {
        if (undelete_dev_paths[i]) {
            if ((err = delete_fs_node(undelete_dev_paths[i])) != 0) {
              ALERT("Error deleting file system node with path %s, err=%d",
                      undelete_dev_paths[i], err);
            }
            kfree(undelete_dev_paths[i]);
        }
        cdev_del(&undelete_devs[i].cdev);
    }
    kfree(undelete_dev_paths);
    if ((err = delete_dev_dir(kDeviceDir)) != 0) {
        ALERT("Error deleting device directory %s, err=%d", kDeviceDir, err);
    }
}

//====================================================================
static int undelete_open(struct inode *inode, struct file *filp) {
    int err = 0;
    struct undelete_dev *dev;   // device information
    int super_block_index;
    size_t block_size;
    DBG("%s called on dev %d, %d", __func__, imajor(inode), iminor(inode));

    // In general, undelete_open should (LDD3 c3p58):
    // 1) Check for device-specific errors
    // 2) Initialized the device
    // 3) update the filp->f_op pointer if necessary to specialized file
    //    operations depending on the type of device that was opened.
    //    e.g. filp->f_op = &undelete_fops;
    // 4) Allocate and fill any data structures to be put in filp->private_data

    super_block_index = iminor(inode);
    if (super_block_index < 0 || super_block_index > num_super_blocks) {
        err = -EINVAL;
        ALERT("Attempted to open super block with index %d / %d, which "
              "is out of range", super_block_index, num_super_blocks);
        goto error_busy;
    }

    // Get the undelete_dev struct that contained the cdev info for this inode,
    // which represents a character device in /dev.
    dev = container_of(inode->i_cdev, struct undelete_dev, cdev);

    // Abort the open if another undelete for this filesystem is already in
    // progress.
    if (dev->buffer) {
        err = -EBUSY;
        ALERT("Attempted to open device %s, which is already running an "
              "undelete operation", super_block_map[super_block_index]->s_id);
        goto error_busy;
    }

    // Save the device reference in the private_data field for convenient
    // future access by functions like undelete_open/release.
    filp->private_data = dev;

    // Allocate a buffer large enough to contain a block of the ext2 file
    // system with which this device is associated.  Also, set our read and
    // write positions for this instance of undelete_open to 0.
    block_size = super_block_map[super_block_index]->s_blocksize;
    dev->buffer = kmalloc(block_size, GFP_KERNEL);
    if (!dev->buffer) {
        err = -ENOMEM;
        ALERT("Error allocating device buffer");
        goto error_init;
    }
    dev->buffer_len = block_size;
    dev->buffer_read_offset = 0;
    dev->buffer_write_offset = 0;
    dev->super_block = super_block_map[super_block_index];

    // TODO:
    // - Lock the file system against writes until undelete_release() is called
		dev->group_index = 0;
		dev->bitmap_offset = 0;
		dev->curr_inode = NULL;
		dev->has_written_header = 0;
		dev->inode_id = 0;
		dev->level_1_index = 0;
		dev->level_2_index = 0;
		dev->level_3_index = 0;
		dev->level_4_index = 0;
		dev->in_written = 0;
		dev->last = 0;
		//Lock the file system
		
    return 0;

    // Unwind allocated resources in the reverse order of their allocation.
    error_init:
    kfree(dev->buffer);
    error_busy:
    return err;
}

//====================================================================
static ssize_t undelete_read (struct file *filp, char __user *buf,
                             size_t count, loff_t *offp) {
    // Grab our context
    struct undelete_dev *dev = (struct undelete_dev *)filp->private_data;
    struct super_block *super_block_t = dev->super_block;
		struct ext2_sb_info *ext2_super_block_info_t = (struct ext2_sb_info *) super_block_t->s_fs_info;
    struct ext2_super_block *ext2_super_block_t = ext2_super_block_info_t->s_es;
    unsigned int num_of_block_group;

		//Get each group's group desc from the super block
		struct ext2_group_desc *curr_group_desc_t = NULL;
		__le32 curr_inode_bitmap_id;
		struct buffer_head *inode_bitmap;
		unsigned int i;
		//unsigned int num_inode_char;
		//uint32_t data_32_int;
		char *data = NULL;
		//int data_int_representation;
		struct ext2_inode * curr_inode_read = NULL;
		struct buffer_head *curr_data_block;
		unsigned int block_size = 1024 << ext2_super_block_t->s_log_block_size;
		unsigned int block_in_indirect = block_size / 4;
		int * p;
		struct file_properties fp;
		int inode_id;
		int need_padding = 0;
		size_t bytes_to_read;
		struct buffer_head *empty; 
		if (ext2_super_block_t->s_inodes_count % ext2_super_block_t->s_inodes_per_group == 0) {
			num_of_block_group = ext2_super_block_t->s_inodes_count / ext2_super_block_t->s_inodes_per_group;
		} else {
			num_of_block_group = ext2_super_block_t->s_inodes_count / ext2_super_block_t->s_inodes_per_group + 1;
		}
    DBG("%s called on dev %d, %d\n", __func__,
        imajor(filp->f_dentry->d_inode), iminor(filp->f_dentry->d_inode));

    *offp = 0;                  // cuz we can't really seek...
		printk("Now The Fun Part Started!!!\n");
    // TODO:
    // - From undelete_read, scan the file system for deleted inodes, writing
    //   blocks of data to the shared buffer. You will either need to obtain a
    //   reference to the file in which the file system is stored, or else
    //   modify the fs/ext2/* parts of the kernel to support an "undelete"
    //   super operation (see struct super_operations in include/linux/fs.h
    //   and ext2_sops in fs/ext2/super.c) that interprets the file system at
    //   a more abstract level. Note that you will only be able to invoke
    //   kernel functions from this module that have been exposed via the
    //   EXPORT_SYMBOL macro.
    //
    // - For each file that has been deleted, you should:
    //   - Write a tar header for the file (see tar_utils.h)
    //   - Write the data blocks of the file in TAR_BLOCKSIZE increments.
    //   - Pad the final data block to a multiple of TAR_BLOCKSIZE bytes.
    // - Finally, write two empty blocks of size TAR_BLOCKSIZE to indicate the
    //   end of the archive.
    //
    // Note that you will have to keep some state around to determine where
    // in the undelete operation you were during the previous call to
    // undelete_read.
		if (dev->buffer_read_offset == dev->buffer_write_offset) {
			if (dev->last) {
				return 0;
			}
			dev->buffer_read_offset = 0;
			dev->buffer_write_offset = 0;
		} else {
			goto WriteUser;
		}
		
		printk("Finding next inode in the file system\n");
		
		curr_group_desc_t = ext2_get_group_desc(super_block_t, dev->group_index, &empty);
		curr_inode_bitmap_id = curr_group_desc_t->bg_inode_bitmap;
		//process the bitmap and find out if the inode is currently using
		inode_bitmap = sb_bread(super_block_t, curr_inode_bitmap_id);
		data = inode_bitmap->b_data;
		/*printk("group_index: %d\n", dev->group_index);
		printk("bit_map_id: %d\n", curr_inode_bitmap_id);
		for (i = 0; i < 16; i++) {
			printk("%d\n",(int)((1 << i) & *((int *)data)));
		}
		if (inode_bitmap == NULL) {
			printk("Failed to get inode bitmap\n");
			return -1;
		}*/
		/*data_int_representation = 0;
		for (i = 0; i < ext2_super_block_t->s_inodes_count; i += 8) {
			data_int_representation += data[i / 8] << i;
		}*/
		printk("Bitmap initialized! Now checking inode!\n");
		if (!dev->curr_inode) {
			printk("No good inode, we need to find next one\n");
			if (dev->bitmap_offset == ext2_super_block_t->s_inodes_per_group) {
				dev->bitmap_offset = 0;
				dev->group_index ++;
				if (dev->group_index == num_of_block_group) {
					goto End;
				}
			}
			while (1) {
				//printk("While looping!\n");
				int offset = find_next_zero_bit_le(data, ext2_super_block_t->s_inodes_per_group, dev->bitmap_offset);
				if (ext2_super_block_t->s_inodes_per_group > offset) {
					dev->bitmap_offset = offset;
					inode_id = dev->group_index * ext2_super_block_t->s_inodes_per_group + dev->bitmap_offset + 1;
					curr_inode_read = ext2_get_inode(super_block_t, inode_id, &empty);
					printk("group_index: %d\n", dev->group_index);
					printk("num_inode_pergroup_index: %d\n", ext2_super_block_t->s_inodes_per_group);
					printk("bitmap_offset: %d\n", dev->bitmap_offset);
					if (curr_inode_read == NULL) {
						printk("inode_id: %d\n", inode_id);
						printk("SHIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIT\n");
					}
					if (curr_inode_read->i_dtime) {
						dev->curr_inode = curr_inode_read;
						dev->inode_id = inode_id;
						break;
					}
				}
				if (dev->group_index < num_of_block_group - 1) {
					dev->group_index ++;
					curr_group_desc_t = ext2_get_group_desc(super_block_t, dev->group_index, &empty);
					curr_inode_bitmap_id = curr_group_desc_t->bg_inode_bitmap;
					//process the bitmap and find out if the inode is currently using
					inode_bitmap = sb_bread(super_block_t, le32_to_cpu(curr_inode_bitmap_id));
					data = inode_bitmap->b_data;
					if (inode_bitmap == NULL) {
						printk("Failed to get inode bitmap\n");
						return -1;
					}
					/*data_int_representation = 0;
					for (i = 0; i < ext2_super_block_t->s_inodes_per_group; i += 8) {
						data_int_representation += data[i / 8] << i;
					}*/
					dev->bitmap_offset = 0;
				} else {
					//add empty block
					goto End;
					//return 0;
				}
			}
		}
		
		//Now we have the valid inode
		printk("Now we have the valid inode\n");
		/*if (dev->curr_inode == NULL) {
			printk("SHITTTTTTTTTTTTTTTTTT\n");
		}*/
		if (!dev->has_written_header) {
			printk("Now writting head!\n");
			fp.name[0] = 'f';
			fp.name[1] = 'i';
			fp.name[2] = 'l';
			fp.name[3] = 'e';
			fp.name[4] = '-';
			sprintf(fp.name + 5, "%d", dev->inode_id);
			fp.name[10] = '\0';
			fp.mode = (int)(dev->curr_inode->i_mode);
			fp.uid = (int)(dev->curr_inode->i_uid);
			fp.gid = (int)(dev->curr_inode->i_gid);
			fp.size = (size_t)(dev->curr_inode->i_size);
			fp.mtime = (int)(dev->curr_inode->i_mtime);
			fp.uname[0] = 'r';
			fp.uname[1] = 'o';
			fp.uname[2] = 'o';
			fp.uname[3] = 't';
			fp.uname[4] = '\0';
			fp.gname[0] = 'r';
			fp.gname[1] = 'o';
			fp.gname[2] = 'o';
			fp.gname[3] = 't';
			fp.gname[4] = '\0';
			tar_write_header(&fp, dev->buffer, dev->buffer_len, dev->buffer_write_offset);
			dev->has_written_header = 1;
			dev->buffer_write_offset = TAR_BLOCKSIZE;
		} else {
			printk("writting one block !!!\n");
			//Read data block;
			if (dev->level_1_index < EXT2_NDIR_BLOCKS) {
				//Read direct data block;
				printk("Direct!\n");
				curr_data_block = sb_bread(super_block_t, dev->curr_inode->i_block[dev->level_1_index]);
				dev->level_1_index ++;
				if (curr_data_block == NULL) {
					DBG("Failed to read data block");
					return -1;
				}
			} else if (dev->level_1_index == EXT2_IND_BLOCK) {
				//Read indirect data block
				printk("Read single-indirect!\n");
				curr_data_block = sb_bread(super_block_t, dev->curr_inode->i_block[dev->level_1_index]);
				p = (((int *)(curr_data_block->b_data)) + dev->level_2_index);
				curr_data_block = sb_bread(super_block_t, (sector_t)(*p));
				if (dev->level_2_index < block_in_indirect - 1) {
					dev->level_2_index ++;
				} else if (dev->level_1_index < 14){
					dev->level_2_index = 0;
					dev->level_1_index ++;
				} else {
					dev->level_1_index = 0;
					dev->level_2_index = 0;
					dev->level_3_index = 0;
					dev->level_4_index = 0;
					dev->in_written = 0;
					dev->curr_inode = NULL;
					dev->bitmap_offset ++;
				}
			} else if (dev->level_1_index == EXT2_DIND_BLOCK) {
				//Read double-indirect data block
				printk("double-indirect!\n");
				curr_data_block = sb_bread(super_block_t, dev->curr_inode->i_block[dev->level_1_index]);
				p = (((int *)(curr_data_block->b_data)) + dev->level_2_index);
				curr_data_block = sb_bread(super_block_t, (sector_t)(*p));
				p = (((int *)(curr_data_block->b_data)) + dev->level_3_index);
				curr_data_block = sb_bread(super_block_t, (sector_t)(*p));
				if (dev->level_3_index < block_in_indirect - 1) {
					dev->level_3_index ++;
				} else if (dev->level_2_index < block_in_indirect - 1) {
					dev->level_2_index ++;
					dev->level_3_index = 0;
				} else if (dev->level_1_index < EXT2_TIND_BLOCK){
					dev->level_3_index = 0;
					dev->level_2_index = 0;
					dev->level_1_index ++;
				} else {
					dev->level_1_index = 0;
					dev->level_2_index = 0;
					dev->level_3_index = 0;
					dev->level_4_index = 0;
					dev->in_written = 0;
					dev->curr_inode = NULL;
					dev->bitmap_offset ++;
				}
			} else {
				//Read triple-indirect data block
				printk("Triple-indirect!\n");
				curr_data_block = sb_bread(super_block_t, dev->curr_inode->i_block[dev->level_1_index]);
				p = (((int *)(curr_data_block->b_data)) + dev->level_2_index);
				curr_data_block = sb_bread(super_block_t, (sector_t)(*p));
				p = (((int *)(curr_data_block->b_data)) + dev->level_3_index);
				curr_data_block = sb_bread(super_block_t, (sector_t)(*p));
				p = (((int *)(curr_data_block->b_data)) + dev->level_4_index);
				curr_data_block = sb_bread(super_block_t, (sector_t)(*p));
				if (dev->level_4_index < block_in_indirect - 1) {
					dev->level_4_index ++;
				} else if (dev->level_3_index < block_in_indirect - 1) {
					dev->level_3_index ++;
					dev->level_4_index = 0;
				} else if (dev->level_2_index < block_in_indirect - 1) {
					dev->level_2_index ++;
					dev->level_3_index = 0;
					dev->level_4_index = 0;
				} else if (dev->level_1_index < 14){
					dev->level_4_index = 0;
					dev->level_3_index = 0;
					dev->level_2_index = 0;
					dev->level_1_index ++;
				} else {
					dev->level_1_index = 0;
					dev->level_2_index = 0;
					dev->level_3_index = 0;
					dev->level_4_index = 0;
					dev->in_written = 0;
					dev->curr_inode = NULL;
					dev->bitmap_offset ++;
				}
			}
			
			printk("This is where we do the memcp\n");
			memcpy(dev->buffer, curr_data_block->b_data, block_size);
			printk("Memcpy Done!\n");
			dev->buffer_write_offset = block_size;
			if (dev->curr_inode->i_size <= dev->in_written + block_size) {
				//reach end of the file
				printk("We need to add padding!!!\n");
				need_padding = TAR_BLOCKSIZE - dev->curr_inode->i_size % TAR_BLOCKSIZE;
				printk("Figure out how many paddings.\n");
				if (need_padding == TAR_BLOCKSIZE) {
					need_padding = 0;
				}
				
				for (i = block_size - need_padding; i < block_size; i ++) {
					dev->buffer[i] = 0;
				}
				dev->buffer_write_offset = dev->curr_inode->i_size - dev->in_written + need_padding;
				dev->level_1_index = 0;
				dev->level_2_index = 0;
				dev->level_3_index = 0;
				dev->level_4_index = 0;
				dev->in_written = 0;
				dev->curr_inode = NULL;
				dev->bitmap_offset ++;
			} else {
				dev->in_written += block_size;
			}
		}
		printk("Done copying, prepare to user writing!\n");
		goto WriteUser;

End:
		printk("Entering end of the file: Adding empty blook\n");
		for (i = 0; i < 2 * TAR_BLOCKSIZE; i++) {
			dev->buffer[i] = 0;
		}
		dev->buffer_write_offset = 2 * TAR_BLOCKSIZE;
		dev->last = 1;
		goto WriteUser;
		
		
WriteUser:
		printk("Copy to User\n");
    // Check that user memory is valid to write to and perform the write
    if (unlikely(!access_ok(VERIFY_WRITE, buf, count))) {
        return -EFAULT;
    }

    // Writes could cause this process to sleep if memory pages need to be
    // swapped in.  Thus, undelete_read needs to be reentrant.  We also make
    // sure that all the bytes were actually written.
    bytes_to_read = dev->buffer_write_offset - dev->buffer_read_offset;
    if (count < bytes_to_read) {
      bytes_to_read = count;
    }
    if (copy_to_user(buf, &dev->buffer[dev->buffer_read_offset], bytes_to_read) != 0) {
        ALERT("Couldn't finish copy_to_user.");
        return -EFAULT;
    } else {
        dev->buffer_read_offset += bytes_to_read;
    }
    return bytes_to_read;
}

//====================================================================
static int undelete_release (struct inode *inode, struct file *filp) {
    // Grab our context
    struct undelete_dev *dev = (struct undelete_dev *)filp->private_data;
    DBG("%s called on dev %d, %d\n", __func__, imajor(inode), iminor(inode));

    // Clean up the memory associated with the device.
    kfree(dev->buffer);
    dev->buffer = 0;
    dev->buffer_len = 0;
    dev->buffer_read_offset = 0;
    dev->buffer_write_offset = 0;

    return 0;
}

//====================================================================
static int __init undelete_init(void) {
    int i, err;

    DBG("Loading module undelete");

    // STEP 1
    // Scan for all ext2 filesystems the kernel knows about
    if ((err = build_super_block_map()) != 0) {
        ALERT("Error building super block map, err=%d", err);
        goto error_sbmap;
    }

    if (num_super_blocks == 0) {
        DBG("No ext2 filesystems found");
        goto exit_success;
    }

    // STEP 2
    // Allocate a dynamic device number
    if ((err = alloc_chrdev_region(&undelete_devnode, 0,
                                   num_super_blocks, kDeviceName)) != 0) {
        ALERT("Error allocating a dynamic device number, err=%d", err);
        printk("%x\n", undelete_devnode);
        goto error_alloc;
    }
    DBG("Allocated %d chrdevs, major=%d, first minor=%d", num_super_blocks,
        MAJOR(undelete_devnode), MINOR(undelete_devnode));

    // STEP 3
    // Initialize our file operations from functions defined in this module
    undelete_fops.owner   = THIS_MODULE;
    undelete_fops.open    = undelete_open;
    undelete_fops.read    = undelete_read;
    undelete_fops.release = undelete_release;

    // STEP 4
    // Allocate and initialize per-EXT2 file system device information
    undelete_devs = kmalloc(num_super_blocks * sizeof(struct undelete_dev),
                            GFP_KERNEL);
    if (!undelete_devs) {
        err = -ENOMEM;
        ALERT("Error allocating character devices");
        goto error_udevs;
    }

    undelete_dev_paths = kcalloc(num_super_blocks, sizeof(*undelete_dev_paths),
                                 GFP_KERNEL);
    if (!undelete_dev_paths) {
        err = -ENOMEM;
        ALERT("Error allocating character device path names");
        goto error_cdev_paths;
    }

    // Create a /dev/undelete directory and a character device file
    // under it each mounted file system.
    if ((err = create_dev_dir(kDeviceDir)) != 0) {
        ALERT("Error creating undelete directory %s, err=%d", kDeviceDir, err);
        goto error_cdev;
    }

    // STEP 5
    // Initialize our own struct cdev and register a character device with the
    // kernel for each EXT2 fs we found above.  As soon as cdev_add returns,
    // each device is live.  So make sure everything that needs to be
    // initialized has been!
    for (i = 0; i < num_super_blocks; ++i) {
        size_t dev_path_len;
        const char *fs_name = super_block_map[i]->s_id;
        dev_t undelete_minor_devnode = MKDEV(MAJOR(undelete_devnode), i);

        cdev_init(&undelete_devs[i].cdev, &undelete_fops);
        undelete_devs[i].buffer = NULL;
        undelete_devs[i].cdev.owner = THIS_MODULE;
        undelete_devs[i].cdev.ops = &undelete_fops;
        if ((err = cdev_add(&undelete_devs[i].cdev, undelete_minor_devnode, 1)) != 0) {
            ALERT("Error adding undelete_cdev, err=%d", err);
            goto error_cdev_add;
        }

        // Allocate space for a device path of the form "kDeviceDir/fs_name\0"
        dev_path_len = sizeof(kDeviceDir) + 1 + strlen(fs_name) + 1;
        undelete_dev_paths[i] = kmalloc(dev_path_len, GFP_KERNEL);
        if (!undelete_dev_paths[i]) {
          err = -ENOMEM;
          ALERT("Error allocating character device path");
          goto error_cdev_add;
        }
        if (snprintf(undelete_dev_paths[i], dev_path_len,
                     "%s/%s", kDeviceDir, fs_name) >= dev_path_len) {
            err = -EINVAL;
            ALERT("Unable to write device path '%s/%s' to buffer of length %ju",
                  kDeviceDir, fs_name, dev_path_len);
            goto error_cdev_add;
        }
        if ((err = create_fs_node(undelete_dev_paths[i],
                                  undelete_minor_devnode)) != 0) {
          ALERT("Error making a node for file system %s under %s, err=%d",
                fs_name, undelete_dev_paths[i], err);
          goto error_cdev_add;
        }
        DBG("Created a node for file system %s under %s", fs_name,
            undelete_dev_paths[i]);
    }

    exit_success:
    return 0;

    // Unwind allocated resources in the reverse order of their allocation.
    error_cdev_add:
    for (i = 0; i < num_super_blocks; ++i) {
        if (undelete_dev_paths[i]) {
            delete_fs_node(undelete_dev_paths[i]);
            kfree(undelete_dev_paths[i]);
        }
        cdev_del(&undelete_devs[i].cdev);
    }
    kfree(undelete_dev_paths);
    delete_dev_dir(kDeviceDir);
    error_cdev_paths:
    kfree(undelete_dev_paths);
    error_cdev:
    kfree(undelete_devs);
    error_udevs:
    unregister_chrdev_region(undelete_devnode, num_super_blocks);
    error_alloc:
    kfree(super_block_map);
    error_sbmap:
    return err;
}

//====================================================================
static void __exit undelete_exit(void) {
    int i;

    DBG("Unloading module undelete");

    // Release references to super blocks.
    for (i = 0; i < num_super_blocks; ++i) {
        if (!atomic_add_unless(&super_block_map[i]->s_active, -1, 1)) {
            return;
        }
    }

    // Free the super block map.
    if (super_block_map) {
        kfree(super_block_map);
    }

    // Unregister our character devices
    unregister_character_devices();

    // Make sure to free allocated device numbers
    unregister_chrdev_region(undelete_devnode, num_super_blocks);
}

module_init(undelete_init);
module_exit(undelete_exit);
