#ifndef __LIB_SYSCALL_NR_H
#define __LIB_SYSCALL_NR_H

/* System call numbers. */
enum {
	/* Projects 2 and later. */
	SYS_HALT,                   /* 운영 체제를 중지합니다. *//* Halt the operating system. */
	SYS_EXIT,                   /* 이 프로세스를 종료합니다. *//* Terminate this process. */
	SYS_FORK,                   /* 현재 프로세스를 복제합니다. *//* Clone current process. */
	SYS_EXEC,                   /* 현재 프로세스를 전환합니다. *//* Switch current process. */
	SYS_WAIT,                   /* 자식 프로세스가 죽을 때까지 기다립니다. *//* Wait for a child process to die. */
	SYS_CREATE,                 /* 파일을 생성합니다. *//* Create a file. */
	SYS_REMOVE,                 /* Delete a file. */
	SYS_OPEN,                   /* Open a file. */
	SYS_FILESIZE,               /* 파일의 크기를 얻는다. *//* Obtain a file's size. */
	SYS_READ,                   /* 파일에서 읽습니다. *//* Read from a file. */
	SYS_WRITE,                  /* Write to a file. */
	SYS_SEEK,                   /* 파일의 위치를 변경합니다. *//* Change position in a file. */
	SYS_TELL,                   /* 파일의 현재 위치를 보고합니다. *//* Report current position in a file. */
	SYS_CLOSE,                  /* Close a file. */

	/* Project 3 and optionally project 4. */
	SYS_MMAP,                   /* Map a file into memory. */
	SYS_MUNMAP,                 /* Remove a memory mapping. */

	/* Project 4 only. */
	SYS_CHDIR,                  /* Change the current directory. */
	SYS_MKDIR,                  /* Create a directory. */
	SYS_READDIR,                /* Reads a directory entry. */
	SYS_ISDIR,                  /* Tests if a fd represents a directory. */
	SYS_INUMBER,                /* Returns the inode number for a fd. */
	SYS_SYMLINK,                /* Returns the inode number for a fd. */

	/* Extra for Project 2 */
	SYS_DUP2,                   /* Duplicate the file descriptor */

	SYS_MOUNT,
	SYS_UMOUNT,
};

#endif /* lib/syscall-nr.h */
