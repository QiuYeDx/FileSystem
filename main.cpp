#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <iostream>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <termios.h>//for getch(): windows <conio.h>;linux <termios.h>;
#include <unistd.h>
#include "struct_def.h"
#include "blockop.h"
#include "fileop.h"
#include "dirop.h"
#include "os.h"
#include "user.h"
using namespace std;

//全局变量
/*
*	模拟文件系统的文件的描述符
*/
FILE* fd = NULL;
/*
*	超级块
*/
filsys superBlock;
/*
*	信息节点位图 元素1代表“used”，0代表“free”
*/
unsigned short inode_bitmap[INODE_NUM];
/*
*	用户信息
*/
userPsw users;
/*
*	当前用户ID
*/
unsigned short userID = ACCOUNT_NUM;
/*
*	当前用户名 在命令行中使用
*/
char userName[USER_NAME_LENGTH + 6];
/*
*	当前目录
*/
directory currentDirectory;
/*
*	当前目录数组
*/
char ab_dir[100][14];
unsigned short dir_pointer;

/*
*	函数声明
*/
int getch(void);//for linux
void CommParser(inode*&);
void Help();
void Sys_start();

/*
*	Rename file/dir.
*/
void Rename(char* filename) {
	printf("File or Dir?(0 stands for file, 1 for dir):");
	int tt;
	scanf("%d", &tt);
	//parameter check
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return;
	}

	int pos_in_directory = -1;
	inode* tmp_file_inode = new inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Not found.\n");
			return;
		}

		//Fetch inode
		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(tmp_file_inode, sizeof(inode), 1, fd);
		//Directory check
	} while (tmp_file_inode->di_mode == tt);

	printf("Please input new file name:");
	char str[15];
	scanf("%s", str);
	str[14] = 0;
	for (int i = 0; i < DIRECTORY_NUM; i++)
	{
		if (currentDirectory.inodeID[i] == tmp_file_inode->i_ino)
		{
			strcpy(currentDirectory.fileName[i], str);
			break;
		}
	}
	//write
	fseek(fd, DATA_START + tmp_file_inode->di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(&currentDirectory, sizeof(directory), 1, fd);
	printf("Rename successfully!\n");
	return;
}

/*
*	Link a file.
*/
bool ln(char* filename)
{

	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return false;
	}

	int pos_in_directory = -1;
	inode* tmp_file_inode = new inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Not found.\n");
			return false;
		}

		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(tmp_file_inode, sizeof(inode), 1, fd);
	} while (tmp_file_inode->di_mode == 0);

	if (userID == tmp_file_inode->di_uid)
	{
		if (!(tmp_file_inode->permission & OWN_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	else if (users.groupID[userID] == tmp_file_inode->di_grp) {
		if (!(tmp_file_inode->permission & GRP_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	else {
		if (!(tmp_file_inode->permission & ELSE_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	//get absolute path
	char absolute[1024];
	int path_pos = 0;
	printf("Input absolute path(end with '#'):");
	scanf("%s", absolute);
	//get directory inode
	char dirname[14];
	int pos_dir = 0;
	bool root = false;
	inode dir_inode;
	directory cur_dir;
	int i;
	for (i = 0; i < 5; i++)
	{
		dirname[i] = absolute[i];
	}
	dirname[i] = 0;
	if (strcmp("root/", dirname) != 0)
	{
		printf("path error!\n");
		return false;
	}
	fseek(fd, INODE_START, SEEK_SET);
	fread(&dir_inode, sizeof(inode), 1, fd);
	for (int i = 5; absolute[i] != '#'; i++)
	{
		if (absolute[i] == '/')
		{
			dirname[pos_dir++] = 0;
			pos_dir = 0;
			fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
			fread(&cur_dir, sizeof(directory), 1, fd);
			int i;
			for (i = 0; i < DIRECTORY_NUM; i++)
			{
				if (strcmp(cur_dir.fileName[i], dirname) == 0)
				{
					fseek(fd, INODE_START + cur_dir.inodeID[i] * INODE_SIZE, SEEK_SET);
					fread(&dir_inode, sizeof(inode), 1, fd);
					if (dir_inode.di_mode == 0)break;
				}
			}
			if (i == DIRECTORY_NUM)
			{
				printf("path error!\n");
				return false;
			}
		}
		else
			dirname[pos_dir++] = absolute[i];
	}
	//update this directory
	fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fread(&cur_dir, sizeof(directory), 1, fd);
	for (i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strlen(cur_dir.fileName[i]) == 0)
		{
			strcat(cur_dir.fileName[i], filename);
			cur_dir.inodeID[i] = tmp_file_inode->i_ino;
			break;
		}
	}
	if (i == DIRECTORY_NUM)
	{
		printf("No value items!\n");
		return false;
	}
	fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(&cur_dir, sizeof(directory), 1, fd);
	dir_inode.di_number++;
	tmp_file_inode->icount++;
	fseek(fd, INODE_START + tmp_file_inode->i_ino*INODE_SIZE, SEEK_SET);
	fwrite(tmp_file_inode, sizeof(inode), 1, fd);

	printf("Link a file successfully!\n");
	return true;
}

/*
*	File copy.
*/
bool Copy(char* filename, inode*& currentInode) {
	currentInode = OpenFile(filename);
	int block_num = currentInode->di_size / BLOCK_SIZE + 1;
	char stack[BLOCK_SIZE];
	char str[100000];
	int cnt = 0;
	if (block_num <= NADDR - 2)
	{
		for (int i = 0; i < block_num; i++)
		{
			if (currentInode->di_addr[i] == -1) break;
			fseek(fd, DATA_START + currentInode->di_addr[i] * BLOCK_SIZE, SEEK_SET);
			fread(stack, sizeof(stack), 1, fd);
			for (int j = 0; j < BLOCK_SIZE; j++)
			{
				if (stack[j] == '\0') {
					str[cnt] = 0;
					break;
				}
				str[cnt++] = stack[j];
			}
		}
	}
	else if (block_num > NADDR - 2) {
		//direct addressing
		for (int i = 0; i < NADDR - 2; i++)
		{
			fseek(fd, DATA_START + currentInode->di_addr[i] * BLOCK_SIZE, SEEK_SET);
			fread(stack, sizeof(stack), 1, fd);
			for (int j = 0; j < BLOCK_SIZE; j++)
			{
				if (stack[j] == '\0') {
					str[cnt] = 0;
					break;
				}
				str[cnt++] = stack[j];
			}
		}

		//first indirect addressing
		unsigned int f1[BLOCK_SIZE / sizeof(unsigned int)] = { 0 };
		fseek(fd, DATA_START + currentInode->di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
		fread(f1, sizeof(f1), 1, fd);
		for (int i = 0; i < block_num - (NADDR - 2); i++) {
			fseek(fd, DATA_START + f1[i] * BLOCK_SIZE, SEEK_SET);
			fread(stack, sizeof(stack), 1, fd);
			for (int j = 0; j < BLOCK_SIZE; j++)
			{
				if (stack[j] == '\0') {
					str[cnt] = 0;
					break;
				}
				str[cnt++] = stack[j];
			}
		}
	}

	int pos_in_directory = -1;
	inode* tmp_file_inode = new inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Not found.\n");
			return false;
		}

		//Fetch inode
		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(tmp_file_inode, sizeof(inode), 1, fd);
		//Directory check
	} while (tmp_file_inode->di_mode == 0);

	//Access check

	if (userID == tmp_file_inode->di_uid)
	{
		if (!(tmp_file_inode->permission & OWN_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	else if (users.groupID[userID] == tmp_file_inode->di_grp) {
		if (!(tmp_file_inode->permission & GRP_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	else {
		if (!(tmp_file_inode->permission & ELSE_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	//get absolute path
	char absolute[1024];
	int path_pos = 0;
	printf("Input absolute path(end with '#'):");
	scanf("%s", absolute);
	//get directory inode
	char dirname[14];
	int pos_dir = 0;
	bool root = false;
	inode dir_inode;
	directory cur_dir;
	int i;
	for (i = 0; i < 5; i++)
	{
		dirname[i] = absolute[i];
	}
	dirname[i] = 0;
	if (strcmp("root/", dirname) != 0)
	{
		printf("path error!\n");
		return false;
	}
	fseek(fd, INODE_START, SEEK_SET);
	fread(&dir_inode, sizeof(inode), 1, fd);
	for (int i = 5; absolute[i] != '#'; i++)
	{
		if (absolute[i] == '/')
		{
			dirname[pos_dir++] = 0;
			pos_dir = 0;
			fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
			fread(&cur_dir, sizeof(directory), 1, fd);
			int i;
			for (i = 0; i < DIRECTORY_NUM; i++)
			{
				if (strcmp(cur_dir.fileName[i], dirname) == 0)
				{
					fseek(fd, INODE_START + cur_dir.inodeID[i] * INODE_SIZE, SEEK_SET);
					fread(&dir_inode, sizeof(inode), 1, fd);
					if (dir_inode.di_mode == 0)break;
				}
			}
			if (i == DIRECTORY_NUM)
			{
				printf("path error!\n");
				return false;
			}
		}
		else
			dirname[pos_dir++] = absolute[i];
	}
	//update this directory
	fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fread(&cur_dir, sizeof(directory), 1, fd);
	for (i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strlen(cur_dir.fileName[i]) == 0)
		{
			strcat(cur_dir.fileName[i], filename);
			int new_ino = 0;
			for (; new_ino < INODE_NUM; new_ino++)
			{
				if (inode_bitmap[new_ino] == 0)
				{
					break;
				}
			}
			inode ifile_tmp;
			ifile_tmp.i_ino = new_ino;				//Identification
			ifile_tmp.icount = 0;
			ifile_tmp.di_uid = tmp_file_inode->di_uid;
			ifile_tmp.di_grp = tmp_file_inode->di_grp;
			ifile_tmp.di_mode = tmp_file_inode->di_mode;
			memset(ifile_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
			ifile_tmp.di_size = 0;
			ifile_tmp.permission = tmp_file_inode->permission;
			time_t t = time(0);
			strftime(ifile_tmp.time, sizeof(ifile_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
			cur_dir.inodeID[i] = new_ino;
			Write(ifile_tmp, str);
			//Update bitmaps
			inode_bitmap[new_ino] = 1;
			fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
			fwrite(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);
			superBlock.s_num_finode--;
			fseek(fd, BLOCK_SIZE, SEEK_SET);
			fwrite(&superBlock, sizeof(filsys), 1, fd);
			break;
		}
	}
	if (i == DIRECTORY_NUM)
	{
		printf("No value iterms!\n");
		return false;
	}
	fseek(fd, DATA_START + dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(&cur_dir, sizeof(directory), 1, fd);
	dir_inode.di_number++;
	fseek(fd, INODE_START + tmp_file_inode->i_ino*INODE_SIZE, SEEK_SET);
	fwrite(tmp_file_inode, sizeof(inode), 1, fd);
	printf("Copy a file successfully!\n");
	return true;
}

int main()
{
	memset(ab_dir, 0, sizeof(ab_dir));
	dir_pointer = 0;
	//Check file system has been formatted or not.
	FILE* fs_test = fopen("f265.os", "r");
	if (fs_test == NULL)
	{
		printf("File system not found... Format file system first...\n\n");
		Format();
	}
	Sys_start();
	strcpy(ab_dir[dir_pointer], "root");
	dir_pointer++;
	//First log in
	char tmp_userName[USER_NAME_LENGTH];
	char tmp_userPassword[USER_PASSWORD_LENGTH * 5];

	do {
		memset(tmp_userName, 0, USER_NAME_LENGTH);
		memset(tmp_userPassword, 0, USER_PASSWORD_LENGTH * 5);

		printf("User log in\n\n");
		printf("User name:\t");
		scanf("%s", tmp_userName);
		printf("Password:\t");
		char c;
		scanf("%c", &c);
		int i = 0;
		while (1) {
			char ch;
			ch = getch();
			if (ch == '\b') {
				if (i != 0) {
					printf("\b \b");
					i--;
				}
				else {
					tmp_userPassword[i] = '\0';
				}
			}
			else if (ch == '\n') {//windows '\r';linux '\n';
				tmp_userPassword[i] = '\0';
				printf("\n\n");
				break;
			}
			else {
				putchar('*');
				tmp_userPassword[i++] = ch;
			}
		}
		//scanf("%s", tmp_userPassword);
	} while (Login(tmp_userName, tmp_userPassword) != true);

	//User's mode of file system
	inode* currentInode = new inode;

	CommParser(currentInode);

	return 0;
}

int getch(void)///for linux
{
    int ch;
    struct termios tm, tm_old;
    tcgetattr(STDIN_FILENO, &tm);
    tm_old = tm;
    tm.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &tm);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &tm_old);
	if(ch == 127) ch = 8;
    return ch;
}
//system start
void Sys_start() {
	//Install file system
	Mount();
	printf("****************************************************************\n");
	printf("*                                                              *\n");
	printf("*     **************     **************     **************     *\n");
	printf("*     **************     **************     **************     *\n");
	printf("*                ***     ***                ***                *\n");
	printf("*                ***     ***                ***                *\n");
	printf("*     **************     **************     **************     *\n");
	printf("*     **************     **************     **************     *\n");
	printf("*     ***                ***        ***                ***     *\n");
	printf("*     ***                ***        ***                ***     *\n");
	printf("*     **************     **************     **************     *\n");
	printf("*     **************     **************     **************     *\n");
	printf("*                                                              *\n");
	printf("****************************************************************\n");
}
/*
*	Recieve commands from console and call functions accordingly.
*
*	para cuurentInode: a global inode used for 'open' and 'write'
*/
void CommParser(inode*& currentInode)
{
	char para1[11];
	char para2[1024];
	bool flag = false;
	//Recieve commands
	while (true)
	{
		unsigned int f1[BLOCK_SIZE / sizeof(unsigned int)] = { 0 };
		fseek(fd, DATA_START + 8 * BLOCK_SIZE, SEEK_SET);
		fread(f1, sizeof(f1), 1, fd);
		memset(para1, 0, 11);
		memset(para2, 0, 1024);

		printf("%s>", userName);
		scanf("%s", para1);
		para1[10] = 0;			//security protection

		//Choose function
		//Print current directory
		if (strcmp("ls", para1) == 0)
		{
			flag = false;
			List();
		}
		else if (strcmp("cp", para1) == 0) {
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection
			Copy(para2, currentInode);
		}
		else if (strcmp("mv", para1) == 0) {
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection
			Rename(para2);
		}
		else if (strcmp("pwd", para1) == 0) {
			flag = false;
			Ab_dir();
		}
		else if (strcmp("passwd", para1) == 0) {
			flag = false;
			Passwd();
		}
		else if (strcmp("chmod", para1) == 0) {
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			Chmod(para2);
		}
		else if (strcmp("chown", para1) == 0) {
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			Chown(para2);
		}
		else if (strcmp("chgrp", para1) == 0) {
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			Chgrp(para2);
		}
		else if (strcmp("info", para1) == 0) {
			printf("System Info:\nTotal Block:%d\nFree Block:%d\nTotal Inode:%d\nFree Inode:%d\n\n", superBlock.s_num_block, superBlock.s_num_fblock, superBlock.s_num_inode, superBlock.s_num_finode);
			for (int i = 0; i < 50; i++)
			{
				if (i>superBlock.special_free)printf("-1\t");
				else printf("%d\t", superBlock.special_stack[i]);
				if (i % 10 == 9)printf("\n");
			}
			printf("\n\n");
		}
		//Create file
		else if (strcmp("create", para1) == 0)
		{
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			CreateFile(para2);
		}
		//Delete file
		else if (strcmp("rm", para1) == 0)
		{
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			DeleteFile(para2);
		}
		//Open file
		else if (strcmp("open", para1) == 0){
			if (flag == false) {
				flag = true;
				scanf("%s", para2);
				para2[1023] = 0;	//security protection
				currentInode = OpenFile(para2);
			}
			else printf("You have opened a file!\n");
		}
		//Write file
		else if (strcmp("write", para1) == 0 && flag){
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			Write(*currentInode, para2);
		}
		//Read file
		else if (strcmp("cat", para1) == 0 && flag) {
			PrintFile(*currentInode);
		}
		//Close file
		else if (strcmp("close", para1) == 0) {
			if (flag == true) {
				printf("Close successfully!\n");
				flag = false;
			}
			else {
				printf("You have never opened a file!\n");
			}
		}
		//Open a directory
		else if (strcmp("cd", para1) == 0){
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			OpenDir(para2);
		}
		//Create dirctory
		else if (strcmp("mkdir", para1) == 0){
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			MakeDir(para2);
		}
		//Delete directory
		else if (strcmp("rmdir", para1) == 0){
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			RemoveDir(para2);
		}
		//Log out
		else if (strcmp("logout", para1) == 0){
			flag = false;
			Logout();
			char tmp_userName[USER_NAME_LENGTH], tmp_userPassword[USER_PASSWORD_LENGTH * 5];
			do {
				memset(tmp_userName, 0, USER_NAME_LENGTH);
				memset(tmp_userPassword, 0, USER_PASSWORD_LENGTH * 5);

				printf("User log in\n\n");
				printf("User name:\t");
				scanf("%s", tmp_userName);
				printf("Password:\t");
				char c;
				scanf("%c", &c);
				int i = 0;
				while (1) {
					char ch;
					ch = getch();
					if (ch == '\b') {
						if (i != 0) {
							printf("\b \b");
							i--;
						}
						else {
							tmp_userPassword[i] = '\0';
						}
					}
					else if (ch == '\n') {//windows '\r';linux '\n';
						tmp_userPassword[i] = '\0';
						printf("\n\n");
						break;
					}
					else {
						putchar('*');
						tmp_userPassword[i++] = ch;
					}
				}

				//scanf("%s", tmp_userPassword);
			} while (Login(tmp_userName, tmp_userPassword) != true);

			//User's mode of file system
			inode* currentInode = new inode;
		}
		else if (strcmp("ln", para1) == 0)
		{
			flag = false;
			scanf("%s", para2);
			para2[1023] = 0;	//security protection

			ln(para2);
		}
		//Log in
		else if (strcmp("su", para1) == 0){
			Logout();
			flag = false;
			char para3[USER_PASSWORD_LENGTH * 5];
			memset(para3, 0, USER_PASSWORD_LENGTH + 1);

			scanf("%s", para2);
			para2[1023] = 0;	//security protection
			//scanf("%s", para3);
			printf("password: ");
			char c;
			scanf("%c", &c);
			int i = 0;
			while (1) {
				char ch;
				ch = getch();
				if (ch == '\b') {
					if (i != 0) {
						printf("\b \b");
						i--;
					}
				}
				else if (ch == '\n') {//windows '\r';linux '\n';
					para3[i] = '\0';
					printf("\n\n");
					break;
				}
				else {
					putchar('*');
					para3[i++] = ch;
				}
			}
			para3[USER_PASSWORD_LENGTH] = 0;	//security protection

			Login(para2, para3);
		}
		else if (strcmp("Muser", para1) == 0) {
			flag = false;
			User_management();
		}
		//Exit
		else if (strcmp("exit", para1) == 0){
			flag = false;
			break;
		}
		//Error or help
		else{
			flag = false;
			Help();
		}
	}
};

/*
*	Print all commands help information when 'help' is
*	called or input error occurs.
*/
void Help(){
	printf("System command:\n");
	printf("\t01.Exit system....................................(exit)\n");
	printf("\t02.Show help information..........................(help)\n");
	printf("\t03.Show current directory..........................(pwd)\n");
	printf("\t04.File list of current directory...................(ls)\n");
	printf("\t05.Enter the specified directory..........(cd + dirname)\n");
	printf("\t06.Return last directory.........................(cd ..)\n");
	printf("\t07.Create a new directory..............(mkdir + dirname)\n");
	printf("\t08.Delete the directory................(rmdir + dirname)\n");
	printf("\t09.Create a new file....................(create + fname)\n");
	printf("\t10.Open a file............................(open + fname)\n");
	printf("\t11.Close a file..................................(close)\n");
	printf("\t12.Read the file...................................(cat)\n");
	printf("\t13.Write the file....................(write + <content>)\n");
	printf("\t14.Copy a file..............................(cp + fname)\n");
	printf("\t15.Delete a file............................(rm + fname)\n");
	printf("\t16.System information view........................(info)\n");
	printf("\t17.Close the current user.......................(logout)\n");
	printf("\t18.Change the current user...............(su + username)\n");
	printf("\t19.Change the mode of a file.............(chmod + fname)\n");
	printf("\t20.Change the user of a file.............(chown + fname)\n");
	printf("\t21.Change the group of a file............(chgrp + fname)\n");
	printf("\t22.Rename a file............................(mv + fname)\n");
	printf("\t23.link.....................................(ln + fname)\n");
	printf("\t24.Change password..............................(passwd)\n");
	printf("\t25.User Management Menu..........................(Muser)\n");
};