#include "fs.h"
#include "disk.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

// Returns the number of dedicated inode blocks given the disk size in blocks
#define NUM_INODE_BLOCKS(disk_size_in_blocks) (1 + (disk_size_in_blocks / 10))

int *bitmap = NULL;

struct fs_superblock {
	int magic;          // Magic bytes
	int nblocks;        // Size of the disk in number of blocks
	int ninodeblocks;   // Number of blocks dedicated to inodes
	int ninodes;        // Number of dedicated inodes
};

struct fs_inode {
	int isvalid;                      // 1 if valid (in use), 0 otherwise
	int size;                         // Size of file in bytes
	int direct[POINTERS_PER_INODE];   // Direct data block numbers (0 if invalid)
	int indirect;                     // Indirect data block number (0 if invalid)
};

union fs_block {
	struct fs_superblock super;               // Superblock
	struct fs_inode inode[INODES_PER_BLOCK];  // Block of inodes
	int pointers[POINTERS_PER_BLOCK];         // Indirect block of direct data block numbers
	char data[DISK_BLOCK_SIZE];               // Data block
};

void inode_load( int inumber, struct fs_inode *inode )
{
	int blocknum = inumber / INODES_PER_BLOCK + 1;

	// load data from disk
	union fs_block tmp_block;
	disk_read(blocknum, tmp_block.data);

	// pick inode
	*inode = tmp_block.inode[inumber % INODES_PER_BLOCK];
}

void inode_save( int inumber, struct fs_inode *inode )
{
	int blocknum = inumber / INODES_PER_BLOCK + 1;

	// load data from disk
	union fs_block tmp_block;
	disk_read(blocknum, tmp_block.data);

	// change picked inode
	tmp_block.inode[inumber % INODES_PER_BLOCK] = *inode;

	// write modified result to disk
	disk_write(blocknum, tmp_block.data);
}

void delete_zeroes(int total, int *with_zero, int *without_zero, int stop ){
	without_zero[0] = 0;
	for (int i = 0; i < total && without_zero[0] < stop; ++i){
		if (with_zero[i]) {
			without_zero[0]++;
			without_zero[without_zero[0]] = with_zero[i];
		}
	}
}

void fs_debug()
{
	union fs_block block;

	disk_read(0,block.data);

	// Print superblock information
	printf("superblock:\n");
	if (block.super.magic == FS_MAGIC) {
		printf("    magic number is valid\n");
	}
	else {
		printf("    magic number is invalid\n");
	}
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);

	// Print inodes information
	struct fs_inode inode;
	for (int i = 0; i < block.super.ninodes; ++i){
		inode_load(i, &inode);
		if (inode.isvalid) {
			printf("inode %d:\n", i);
			printf("    size: %d bytes\n", inode.size);

			// Print direct blocks
			printf("    direct data blocks:");
			for (int j = 0; j < POINTERS_PER_INODE; ++j){
				if (inode.direct[j]) {
					printf(" %d", inode.direct[j]);
				}
			}
			printf("\n");
			
			// Print indirect blocks
			if (inode.indirect){
				printf("    indirect block: %d\n", inode.indirect);

				// read block
				union fs_block pointers;
				disk_read(inode.indirect, pointers.data);

				// print indirect blocks
				printf("    indirect data blocks:");
				for (int j = 0; j < POINTERS_PER_BLOCK; ++j){
					if (pointers.pointers[j]) {
						printf(" %d", pointers.pointers[j]);
					}
				}
				printf("\n");
			}
		}
	}
}

int fs_format()
{	
	int i,j,z;
	int nblocks, num_inode_blocks;
	union fs_block superblock;

	nblocks = disk_size();
	num_inode_blocks = NUM_INODE_BLOCKS(nblocks);

	// Store Data into the Superblock
	superblock.super.magic = FS_MAGIC;
	superblock.super.nblocks = nblocks;
	superblock.super.ninodeblocks = num_inode_blocks;
	superblock.super.ninodes = num_inode_blocks * INODES_PER_BLOCK;
	disk_write(0, superblock.data);

	// Update the InodeBlocks
	for (i = 1; i < num_inode_blocks + 1; i++){

		union fs_block cur_inode_block;

		for (j = 0; j < INODES_PER_BLOCK; j++){
			cur_inode_block.inode[j].isvalid = 0;
			cur_inode_block.inode[j].size = 0;
			cur_inode_block.inode[j].indirect = 0;

			for (z = 0; z < POINTERS_PER_INODE; z++){
				cur_inode_block.inode[j].direct[z] = 0;
			}
		}
		disk_write(i, cur_inode_block.data);
	} 

	return 1;
}

int fs_mount()
{	
	int i,z;
	int nblocks;
	union fs_block superblock;
	
	// Get superblock
	disk_read(0, superblock.data);

	// test magic number
	int magic_number = superblock.super.magic;
	if (magic_number != FS_MAGIC || bitmap != NULL){
		return 0;
	}

	nblocks = superblock.super.nblocks;
	int ninodes = superblock.super.ninodes;
	
	bitmap = (int *)malloc(nblocks * sizeof(int)); // 1 for free, 0 for occupied

	// Update the Bitmap
	bitmap[0] = 0; // occupy superblock
	for (i = 1; i < nblocks; ++i){
		bitmap[i] = 1; // free all data blocks and inode blocks
	}
	struct fs_inode cnode;
	for ( i = 0; i < ninodes; ++i ) {
		// load inode
		inode_load(i, &cnode);

		if ( cnode.isvalid ) {
			bitmap[i] = 0; // occupy the corresponding block
			// check direct pointers
			for (z = 0; z < POINTERS_PER_INODE; z++){
					if (cnode.direct[z]){
						bitmap[cnode.direct[z]] = 0;
					}
			}

			// check indirect pointers
			if (cnode.indirect) {
				// occupy the block of indirect pointers
				bitmap[cnode.indirect] = 0;

				// read pointers
				union fs_block cpointers;
				disk_read(cnode.indirect, cpointers.data);

				for (z = 0; z < POINTERS_PER_BLOCK; ++z) {
					if (cpointers.pointers[z]) {
						bitmap[cpointers.pointers[z]] = 0;
					}
				}
			}
		}
	}
	return 1;
}

int fs_unmount()
{
	// Return 0 if nothing mounted
	if (bitmap == NULL){
		return 0;
	}

	// Deallocate bitmap
	free(bitmap);
	bitmap = NULL;

	return 1;
}

int fs_create()
{
	// Failure if unmounted
	if ( bitmap == NULL ) {
		printf("create(failure): file system not mounted.");
		return -1;
	}
	
	// Load superblock
	union fs_block superblock;
	disk_read(0, superblock.data);

	int inumber = -1;
	struct fs_inode inode;
	// Check if there exists an empty inode block
	for (int i = 1; i < superblock.super.ninodeblocks + 1; ++i) {
		if (bitmap[i]) {
			inumber = (i - 1) * INODES_PER_BLOCK;
			break;
		}
	}
	// Otherwise iterate inode blocks
	if (inumber < 0) {
		for (int i = 0; i < superblock.super.ninodes; ++i) {
			inode_load(i, &inode);
			if (!inode.isvalid) {
				inumber = i;
			}
		}
	}

	// save inode
	if (inumber != -1){
		// initialize inode
		inode.isvalid = 1;
		inode.size = 0;
		for (int i = 0; i < POINTERS_PER_INODE; ++i) {
			inode.direct[i] = 0;
		}
		inode.indirect = 0;
		inode_save(inumber, &inode);
	}
	
	return inumber;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
