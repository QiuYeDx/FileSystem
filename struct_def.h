#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <iostream>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
using namespace std;

#ifndef _STRUCT_DEF_H_
#define _STRUCT_DEF_H_

//常量
/*
*	一个文件可以用4个直接地址，1个一级间址，1个二级间址（未用），用在inode结构中的di_addr[]
*/
const unsigned int NADDR = 6;
/*
*	磁盘块的最大容量512B
*/
const unsigned short BLOCK_SIZE = 512;
/*
*	一个文件的最大容量（没考虑二级间址）
*/
const unsigned int FILE_SIZE_MAX = (NADDR - 2) * BLOCK_SIZE + BLOCK_SIZE / sizeof(int) * BLOCK_SIZE;
/*
*	数据磁盘块的最大数量512个
*/
const unsigned short BLOCK_NUM = 512;
/*
*	inode结点的容量128B
*/
const unsigned short INODE_SIZE = 128;
/*
*	inode结点的最大数量256个
*/
const unsigned short INODE_NUM = 256;
/*
*	索引节点链的开始位置，前四个块用于加载程序扇区(本程序中为空)、超级块、索引节点位图和块位图。
*/
const unsigned int INODE_START = 3 * BLOCK_SIZE;
/*
*	数据块的起始位置
*/
const unsigned int DATA_START = INODE_START + INODE_NUM * INODE_SIZE;
/*
*	文件系统用户的最大数量10
*/
const unsigned int ACCOUNT_NUM = 10;
/*
*	一个目录中子文件和子目录的最大数量16
*/
const unsigned int DIRECTORY_NUM = 16;
/*
*	文件名称的最大长度14
*/
const unsigned short FILE_NAME_LENGTH = 14;
/*
*	用户名称最大长度14
*/
const unsigned short USER_NAME_LENGTH = 14;
/*
*	用户密码最大长度14
*/
const unsigned short USER_PASSWORD_LENGTH = 14;
/*
*	文件的最大权限111111111-511
*/
const unsigned short MAX_PERMISSION = 511;
/*
*	owner文件最大权限111000000-448
*/
const unsigned short MAX_OWNER_PERMISSION = 448;
//权限
const unsigned short ELSE_X = 1;
const unsigned short ELSE_W = 1 << 1;
const unsigned short ELSE_R = 1 << 2;
const unsigned short GRP_X = 1 << 3;
const unsigned short GRP_W = 1 << 4;
const unsigned short GRP_R = 1 << 5;
const unsigned short OWN_X = 1 << 6;
const unsigned short OWN_W = 1 << 7;
const unsigned short OWN_R = 1 << 8;
//数据结构
/*
* inode(128B)
*/
struct inode
{
	unsigned int i_ino;			//Identification of the inode.
	unsigned int di_addr[NADDR];//Number of data blocks where the file stored.
	unsigned short di_number;	//Number of associated files.
	unsigned short di_mode;		//0 stand for directory 1 stand for file
	unsigned short icount;		//link number
	unsigned short permission;	//file permission
	unsigned short di_uid;		//File's user id.
	unsigned short di_grp;		//File's group id
	unsigned short di_size;		//File size.
	char time[83];
};
/*
*	超级块
*/
struct filsys
{
	unsigned short s_num_inode;			//Total number of inodes.
	unsigned short s_num_finode;		//Total number of free inodes.
	unsigned short s_size_inode;		//Size of an inode.

	unsigned short s_num_block;			//Total number of blocks.
	unsigned short s_num_fblock;		//Total number of free blocks.
	unsigned short s_size_block;		//Size of a block.

	unsigned int special_stack[50];
	int special_free;
};
/*
*	目录文件
*/
struct directory
{
	char fileName[20][FILE_NAME_LENGTH];
	unsigned int inodeID[DIRECTORY_NUM];
};
/*
*	用户文件
*/
struct userPsw
{
	unsigned short userID[ACCOUNT_NUM];
	char userName[ACCOUNT_NUM][USER_NAME_LENGTH];
	char password[ACCOUNT_NUM][USER_PASSWORD_LENGTH];
	unsigned short groupID[ACCOUNT_NUM];
};

//全局变量
/*
*	模拟文件系统的文件的描述符
*/
extern FILE* fd;
/*
*	超级块
*/
extern filsys superBlock;
/*
*	信息节点位图 元素1代表“used”，0代表“free”
*/
extern unsigned short inode_bitmap[INODE_NUM];
/*
*	用户信息
*/
extern userPsw users;
/*
*	当前用户ID
*/
extern unsigned short userID ;
/*
*	当前用户名 在命令行中使用
*/
extern char userName[USER_NAME_LENGTH + 6];
/*
*	当前目录
*/
extern directory currentDirectory;
/*
*	当前目录数组
*/
extern char ab_dir[100][14];
extern unsigned short dir_pointer;

#endif