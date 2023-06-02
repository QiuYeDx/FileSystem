#include "fileop.h"
#include "blockop.h"

/*
*	Create a new empty file with the specific file name.
*
*	return: the function return true only when the new file is successfully
*			created.
*/
bool CreateFile(const char* filename)
{
	//parameter check
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return false;
	}

	if (superBlock.s_num_fblock <= 0 || superBlock.s_num_finode <= 0)
	{
		printf("File creation error: No valid spaces.\n");
		return false;
	}
	//Find new inode number and new block address
	int new_ino = 0;
	unsigned int new_block_addr = -1;
	for (; new_ino < INODE_NUM; new_ino++)
	{
		if (inode_bitmap[new_ino] == 0)
		{
			break;
		}
	}

	for (int i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strcmp(currentDirectory.fileName[i], filename) == 0)
		{
			inode* tmp_file_inode = new inode;
			int tmp_file_ino = currentDirectory.inodeID[i];
			fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
			fread(tmp_file_inode, sizeof(inode), 1, fd);
			if (tmp_file_inode->di_mode == 0) continue;
			else {
				printf("File creation error: File name '%s' has been used.\n", currentDirectory.fileName[i]);
				return false;
			}
		}
	}

	int itemCounter = 0;
	for (int i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strlen(currentDirectory.fileName[i]) > 0)
		{
			itemCounter++;
		}
	}
	if (itemCounter >= DIRECTORY_NUM)
	{
		printf("File creation error: Too many files or directories in current path.\n");
		return false;
	}

	inode ifile_tmp;
	ifile_tmp.i_ino = new_ino;				//Identification
	ifile_tmp.di_number = 1;				//Associations
	ifile_tmp.di_mode = 1;					//1 stands for file
	ifile_tmp.di_size = 0;					//New file is empty
	memset(ifile_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
	ifile_tmp.di_uid = userID;				//Current user id.
	ifile_tmp.di_grp = users.groupID[userID];//Current user group id
	ifile_tmp.permission = MAX_PERMISSION;
	ifile_tmp.icount = 0;
	time_t t = time(0);
	strftime(ifile_tmp.time, sizeof(ifile_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
	ifile_tmp.time[64];
	fseek(fd, INODE_START + new_ino * INODE_SIZE, SEEK_SET);
	fwrite(&ifile_tmp, sizeof(inode), 1, fd);

	inode_bitmap[new_ino] = 1;
	fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	fwrite(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);

	int pos_directory_inode = 0;
	pos_directory_inode = currentDirectory.inodeID[0]; //"."
	inode tmp_directory_inode;
	fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
	fread(&tmp_directory_inode, sizeof(inode), 1, fd);

	for (int i = 2; i < DIRECTORY_NUM; i++)
	{
		if (strlen(currentDirectory.fileName[i]) == 0)
		{
			strcat(currentDirectory.fileName[i], filename);
			currentDirectory.inodeID[i] = new_ino;
			break;
		}
	}
	fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(&currentDirectory, sizeof(directory), 1, fd);

	directory tmp_directory = currentDirectory;
	int tmp_pos_directory_inode = pos_directory_inode;
	while (true)
	{
		tmp_directory_inode.di_number++;
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
		if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
		{
			break;
		}
		tmp_pos_directory_inode = tmp_directory.inodeID[1];		//".."
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fread(&tmp_directory_inode, sizeof(inode), 1, fd);
		fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
		fread(&tmp_directory, sizeof(directory), 1, fd);
	}

	superBlock.s_num_finode--;
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
	printf("Create a file successfully!\n");
	return true;
};

/*
*	Delete a file.
*
*	return: the function returns true only delete the file successfully.
*/
bool DeleteFile(const char* filename)
{
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return false;
	}

	int pos_in_directory = -1, tmp_file_ino;
	inode tmp_file_inode;
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
			printf("Delete error: File not found.\n");
			return false;
		}

		tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(&tmp_file_inode, sizeof(inode), 1, fd);
	} while (tmp_file_inode.di_mode == 0);

	if (userID == tmp_file_inode.di_uid)
	{
		if (!(tmp_file_inode.permission & OWN_X)) {
			printf("Delete error: Access deny.\n");
			return -1;
		}
	}
	else if (users.groupID[userID] == tmp_file_inode.di_grp) {
		if (!(tmp_file_inode.permission & GRP_X)) {
			printf("Delete error: Access deny.\n");
			return -1;
		}
	}
	else {
		if (!(tmp_file_inode.permission & ELSE_X)) {
			printf("Delete error: Access deny.\n");
			return -1;
		}
	}
	if (tmp_file_inode.icount > 0) {
		tmp_file_inode.icount--;
		fseek(fd, INODE_START + tmp_file_inode.i_ino * INODE_SIZE, SEEK_SET);
		fwrite(&tmp_file_inode, sizeof(inode), 1, fd);

		int pos_directory_inode = currentDirectory.inodeID[0];	//"."
		inode tmp_directory_inode;
		fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
		fread(&tmp_directory_inode, sizeof(inode), 1, fd);

		memset(currentDirectory.fileName[pos_in_directory], 0, FILE_NAME_LENGTH);
		currentDirectory.inodeID[pos_in_directory] = -1;
		fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
		fwrite(&currentDirectory, sizeof(directory), 1, fd);

		directory tmp_directory = currentDirectory;
		int tmp_pos_directory_inode = pos_directory_inode;
		while (true)
		{
			tmp_directory_inode.di_number--;
			fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
			fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
			if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
			{
				break;
			}
			tmp_pos_directory_inode = tmp_directory.inodeID[1];		//".."
			fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
			fread(&tmp_directory_inode, sizeof(inode), 1, fd);
			fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
			fread(&tmp_directory, sizeof(directory), 1, fd);
		}
		printf("Delete a file successfully!\n");
		return true;
	}
	int tmp_fill[sizeof(inode)];
	memset(tmp_fill, 0, sizeof(inode));
	fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
	fwrite(&tmp_fill, sizeof(inode), 1, fd);

	inode_bitmap[tmp_file_ino] = 0;
	fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	fwrite(&inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);

	for (int i = 0; i < NADDR - 2; i++)
	{
		if(tmp_file_inode.di_addr[i] != -1)
			recycle_block(tmp_file_inode.di_addr[i]);
		else break;
	}
	if (tmp_file_inode.di_addr[NADDR - 2] != -1) {
		unsigned int f1[128];
		fseek(fd, DATA_START + tmp_file_inode.di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
		fread(f1, sizeof(f1), 1, fd);
		for (int k = 0; k < 128; k++) {
			recycle_block(f1[k]);
		}
		recycle_block(tmp_file_inode.di_addr[NADDR - 2]);
	}

	int pos_directory_inode = currentDirectory.inodeID[0];	//"."
	inode tmp_directory_inode;
	fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
	fread(&tmp_directory_inode, sizeof(inode), 1, fd);

	memset(currentDirectory.fileName[pos_in_directory], 0, FILE_NAME_LENGTH);
	currentDirectory.inodeID[pos_in_directory] = -1;
	fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(&currentDirectory, sizeof(directory), 1, fd);

	directory tmp_directory = currentDirectory;
	int tmp_pos_directory_inode = pos_directory_inode;
	while (true)
	{
		tmp_directory_inode.di_number--;
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
		if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
		{
			break;
		}
		tmp_pos_directory_inode = tmp_directory.inodeID[1];		//".."
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fread(&tmp_directory_inode, sizeof(inode), 1, fd);
		fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
		fread(&tmp_directory, sizeof(directory), 1, fd);
	}

	superBlock.s_num_finode++;
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
	printf("Delete a file successfully!\n");
	return true;
}
/*
*	Open the specific file under current directory.
*
*	return: the function returns a pointer of the file's inode if the file is
*			successfully opened and NULL otherwise.
*/
inode* OpenFile(const char* filename)
{
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return NULL;
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
			printf("Open file error: File not found.\n");
			return NULL;
		}

		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(tmp_file_inode, sizeof(inode), 1, fd);
	} while (tmp_file_inode->di_mode == 0);
	printf("Open a file successfully!\n");
	return tmp_file_inode;
};

/*
*	Append a string "content" to the specific file.
*
*	return: the function returns the number of bytes it has writen or -1 when
*			some error occur.
*/
int Write(inode& ifile, const char* content)
{
	if (content == NULL)
	{
		printf("Error: No content.\n");
		return -1;
	}
	if (userID == ifile.di_uid)
	{
		if (!(ifile.permission & OWN_W)) {
			printf("Write error: Access deny.\n");
			return -1;
		}
	}
	else if (users.groupID[userID] == ifile.di_grp) {
		if (!(ifile.permission & GRP_W)) {
			printf("Write error: Access deny.\n");
			return -1;
		}
	}
	else {
		if (!(ifile.permission & ELSE_W)) {
			printf("Write error: Access deny.\n");
			return -1;
		}
	}

	int len_content = strlen(content);
	unsigned int new_file_length = len_content + ifile.di_size;
	if (new_file_length >= FILE_SIZE_MAX)
	{
		printf("Write error: File over length.\n");
		return -1;
	}

	unsigned int block_num;
	if (ifile.di_addr[0] == -1)block_num = -1;
	else
	{
		for (int i = 0; i < NADDR - 2; i++)
		{
			if (ifile.di_addr[i] != -1)
				block_num = ifile.di_addr[i];
			else break;
		}
		int f1[128];
		fseek(fd, DATA_START + ifile.di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
		int num;
		if (ifile.di_size%BLOCK_SIZE == 0)
			num = ifile.di_size / BLOCK_SIZE;
		else num = ifile.di_size / BLOCK_SIZE + 1;
		if (num > 4 && num <=132)
		{
			fseek(fd, DATA_START + ifile.di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
			fread(f1, sizeof(f1), 1, fd);
			block_num = f1[num - 4];
			printf("%d\n", block_num);
		}

	}
	int free_space_firstBlock = BLOCK_SIZE - ifile.di_size % BLOCK_SIZE;
	unsigned int num_block_needed;
	if (len_content - free_space_firstBlock > 0)
	{
		num_block_needed = (len_content - free_space_firstBlock) / BLOCK_SIZE + 1;
	}
	else
	{
		num_block_needed = 0;
	}
	if (num_block_needed > superBlock.s_num_fblock)
	{
		printf("Write error: No enough space available.\n");
		return -1;
	}

	if (ifile.di_addr[0] == -1)
	{
		find_free_block(block_num);
		ifile.di_addr[0] = block_num;
		fseek(fd, DATA_START + block_num * BLOCK_SIZE, SEEK_SET);
	}
	else
		fseek(fd, DATA_START + block_num * BLOCK_SIZE + ifile.di_size % BLOCK_SIZE, SEEK_SET);
	char data[BLOCK_SIZE];
	if (num_block_needed == 0)
	{
		fwrite(content, len_content, 1, fd);
		fseek(fd, DATA_START + block_num * BLOCK_SIZE, SEEK_SET);
		fread(data, sizeof(data), 1, fd);
		ifile.di_size += len_content;
	}
	else
	{
		fwrite(content, free_space_firstBlock, 1, fd);
		fseek(fd, DATA_START + block_num * BLOCK_SIZE, SEEK_SET);
		fread(data, sizeof(data), 1, fd);
		ifile.di_size += free_space_firstBlock;
	}

	char write_buf[BLOCK_SIZE];
	unsigned int new_block_addr = -1;
	unsigned int content_write_pos = free_space_firstBlock;
	if ((len_content + ifile.di_size) / BLOCK_SIZE + ((len_content + ifile.di_size) % BLOCK_SIZE == 0 ? 0 : 1) <= NADDR - 2) {
		for (int i = 0; i < num_block_needed; i++)
		{
			find_free_block(new_block_addr);
			if (new_block_addr == -1)return -1;
			for (int j = 0; j < NADDR - 2; j++)
			{
				if (ifile.di_addr[j] == -1)
				{
					ifile.di_addr[j] = new_block_addr;
					break;
				}
			}
			memset(write_buf, 0, BLOCK_SIZE);
			unsigned int tmp_counter = 0;
			for (; tmp_counter < BLOCK_SIZE; tmp_counter++)
			{
				if (content[content_write_pos + tmp_counter] == '\0')
					break;
				write_buf[tmp_counter] = content[content_write_pos + tmp_counter];
			}
			content_write_pos += tmp_counter;
			fseek(fd, DATA_START + new_block_addr * BLOCK_SIZE, SEEK_SET);
			fwrite(write_buf, tmp_counter, 1, fd);
			fseek(fd, DATA_START + new_block_addr * BLOCK_SIZE, SEEK_SET);
			fread(data, sizeof(data), 1, fd);
			ifile.di_size += tmp_counter;
		}
	}
	else if ((len_content+ifile.di_size)/BLOCK_SIZE+((len_content + ifile.di_size) % BLOCK_SIZE == 0 ? 0 : 1)> NADDR - 2) {
		for (int i = 0; i < NADDR - 2; i++)
		{
			if (ifile.di_addr[i] != -1)continue;

			memset(write_buf, 0, BLOCK_SIZE);
			new_block_addr = -1;

			find_free_block(new_block_addr);
			if (new_block_addr == -1)return -1;
			ifile.di_addr[i] = new_block_addr;
			unsigned int tmp_counter = 0;
			for (; tmp_counter < BLOCK_SIZE; tmp_counter++)
			{
				if (content[content_write_pos + tmp_counter] == '\0') {
					break;
				}
				write_buf[tmp_counter] = content[content_write_pos + tmp_counter];
			}
			content_write_pos += tmp_counter;
			fseek(fd, DATA_START + new_block_addr * BLOCK_SIZE, SEEK_SET);
			fwrite(write_buf, tmp_counter, 1, fd);

			ifile.di_size += tmp_counter;
		}
		int cnt = 0;
		unsigned int f1[BLOCK_SIZE / sizeof(unsigned int)] = { 0 };

		new_block_addr = -1;
		find_free_block(new_block_addr);
		if (new_block_addr == -1)return -1;
		ifile.di_addr[NADDR - 2] = new_block_addr;
		for (int i = 0;i < BLOCK_SIZE / sizeof(unsigned int);i++)
		{
			new_block_addr = -1;
			find_free_block(new_block_addr);
			if (new_block_addr == -1)return -1;
			else
				f1[i] = new_block_addr;
		}
		fseek(fd, DATA_START + ifile.di_addr[4] * BLOCK_SIZE, SEEK_SET);
		fwrite(f1, sizeof(f1), 1, fd);
		bool flag = 0;
		for (int j = 0; j < BLOCK_SIZE / sizeof(int); j++) {
			fseek(fd, DATA_START + f1[j] * BLOCK_SIZE, SEEK_SET);
			unsigned int tmp_counter = 0;
			for (; tmp_counter < BLOCK_SIZE; tmp_counter++)
			{
				if (content[content_write_pos + tmp_counter] == '\0') {
					flag = 1;
					break;
				}
				write_buf[tmp_counter] = content[content_write_pos + tmp_counter];
			}
			content_write_pos += tmp_counter;
			fwrite(write_buf, tmp_counter, 1, fd);
			ifile.di_size += tmp_counter;
			if (flag == 1) break;
		}
	}
	time_t t = time(0);
	strftime(ifile.time, sizeof(ifile.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
	ifile.time[64] = 0;
	fseek(fd, INODE_START + ifile.i_ino * INODE_SIZE, SEEK_SET);
	fwrite(&ifile, sizeof(inode), 1, fd);

	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(superBlock), 1, fd);

	return len_content;
};
/*
*	Print the string "content" in the specific file.
*
*	return: none
*
*/
void PrintFile(inode& ifile)
{
	if (userID == ifile.di_uid)
	{
		if (!(ifile.permission & OWN_R)) {
			printf("Read error: Access deny.\n");
			return;
		}
	}
	else if (users.groupID[userID] == ifile.di_grp) {
		if (!(ifile.permission & GRP_R)) {
			printf("Read error: Access deny.\n");
			return;
		}
	}
	else {
		if (!(ifile.permission & ELSE_R)) {
			printf("Read error: Access deny.\n");
			return;
		}
	}
	int block_num = ifile.di_size / BLOCK_SIZE + 1;
	int print_line_num = 0;		
	char stack[BLOCK_SIZE];
	if (block_num <= NADDR - 2)
	{
		for (int i = 0; i < block_num; i++)
		{
			fseek(fd, DATA_START + ifile.di_addr[i] * BLOCK_SIZE, SEEK_SET);
			fread(stack, sizeof(stack), 1, fd);
			for (int j = 0; j < BLOCK_SIZE; j++)
			{
				if (stack[j] == '\0')break;
				if (j % 16 == 0)
				{
					printf("\n");
					printf("%d\t", ++print_line_num);
				}
				printf("%c", stack[j]);
			}
		}
	}
	else if (block_num > NADDR - 2) {
		for (int i = 0; i < NADDR - 2; i++)
		{
			fseek(fd, DATA_START + ifile.di_addr[i] * BLOCK_SIZE, SEEK_SET);
			fread(stack, sizeof(stack), 1, fd);
			for (int j = 0; j < BLOCK_SIZE; j++)
			{
				if (stack[j] == '\0')break;
				if (j % 16 == 0)
				{
					printf("\n");
					printf("%d\t", ++print_line_num);
				}
				printf("%c", stack[j]);
			}
		}
		unsigned int f1[BLOCK_SIZE / sizeof(unsigned int)] = { 0 };
		fseek(fd, DATA_START + ifile.di_addr[NADDR - 2] * BLOCK_SIZE, SEEK_SET);
		fread(f1, sizeof(f1), 1, fd);
		for (int i = 0; i < block_num - (NADDR - 2); i++) {
			fseek(fd, DATA_START + f1[i] * BLOCK_SIZE, SEEK_SET);
			fread(stack, sizeof(stack), 1, fd);
			for (int j = 0; j < BLOCK_SIZE; j++)
			{
				if (stack[j] == '\0')break;
				if (j % 16 == 0)
				{
					printf("\n");
					printf("%d\t", ++print_line_num);
				}
				printf("%c", stack[j]);
			}
		}
	}
	printf("\n\n\n");
};
