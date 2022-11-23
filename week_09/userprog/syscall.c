#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "threads/palloc.h"
#include "filesys/filesys.h"
#include "kernel/stdio.h"
#include "filesys/file.h"


void syscall_entry (void);
void syscall_syscall (struct intr_frame *);
void exit_syscall (int status) NO_RETURN;
bool create_syscall (char *file, unsigned initial_size);
int exec_syscall (char *file);
bool remove_syscall (const char *file);
int write_syscall (int fd, const void *buffer, unsigned size);
int open_syscall (const char *file);
int filesize_syscall (int fd);
int read_syscall (int fd, void *buffer, unsigned size);
void seek_syscall (int fd, unsigned position);
unsigned tell_syscall (int fd);
void close_syscall (int fd);



/* System call.
 *
 * Previously system call services was handled by the interrupt_syscall
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */



/*-------------추가 함수--------------*/

// 유저영역의 주소인지 확인 
void
check_address (const uint64_t *addr)
{
	struct thread *cur = thread_current();
	if (addr == NULL || !(is_user_vaddr(addr)) || pml4_get_page(cur -> pml4, addr) == NULL)
	{
		exit_syscall(-1);
	}
}

int 
add_file_to_fd_table (struct file *file){
	struct thread *t = thread_current();
	struct file **fd_table = t -> file_descriptor_table;
	int fd = t->fdidx; //2부터 시작

	while (t -> file_descriptor_table[fd] != NULL && fd < MAX_FD_NUM){
		fd ++;
	}

	if (fd >= MAX_FD_NUM){
		return -1;
	}
	t -> fdidx = fd;
	fd_table[fd] = file;

	return fd;
}

struct
file *fd_to_struct_filep (int fd){
	if (fd < 0 || fd >= MAX_FD_NUM){
		return NULL;
	}
	struct thread *t = thread_current();
	struct file **fd_table = t -> file_descriptor_table;

	struct file *file = fd_table[fd];
	return file;
}

void
remove_file_from_fd_table(int fd){
	struct thread *t = thread_current();
	if (fd < 0 || fd >= MAX_FD_NUM){
		return;
	}
	t -> file_descriptor_table[fd] = NULL;
}


/*-------------추가 함수 끝--------------*/

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&filesys_lock);

}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	uint64_t syscall_no = f->R.rax;  // 콜 넘버

	// uint64_t a1 = f->R.rdi;		// 파일 네임
	// uint64_t a2 = f->R.rsi;		// v(데이터)
	// uint64_t a3 = f->R.rdx;      // 사이즈
	// uint64_t a4 = f->R.r10;
	// uint64_t a5 = f->R.r8;
	// uint64_t a6 = f->R.r9;
	
	

	switch (f->R.rax) {		// rax is the system call number

		char *fn_copy;
		
		// 핀토드 종료 시스템 콜
		case SYS_HALT : 
			power_off();
		break;

		//프로세스 종료 시스템 콜
		case SYS_EXIT : 
			exit_syscall (f->R.rdi);
		break;
			
		// case SYS_FORK :
		// 	f->R.rdx = fork_syscall(f->R.rdx, f);
		// break;

		//프로세스 생성
		case SYS_EXEC :
			f->R.rax = exec_syscall(f->R.rdi);
		break;

		// case SYS_WAIT :
		// 	f->R.rax = wait_syscall(f->R.rdi);
		// break;

		// 파일 이름과 파일 사이즈를 인자 값으로 받아 파일을 생성하는 함수.
		case SYS_CREATE : 
			f->R.rax = create_syscall(f->R.rdi, f->R.rsi);
		break;

		case SYS_REMOVE :
			f->R.rax = remove_syscall(f->R.rdi);
		break;

		case SYS_OPEN :
			f->R.rax = open_syscall(f->R.rdi);
		break;

		case SYS_FILESIZE :
			
			f->R.rax = filesize_syscall(f->R.rdi);
		break;

		case SYS_READ :
			f->R.rax = read_syscall (f->R.rdi, f->R.rsi, f->R.rdx);
		break;

		case SYS_WRITE :
			
			f->R.rax = write_syscall(f->R.rdi, f->R.rsi, f->R.rdx);	
		break;

		case SYS_SEEK :
			// hong_dump_frame (f);
			seek_syscall (f->R.rdi, f->R.rdx);
		break;

		case SYS_TELL :
			f->R.rax = tell_syscall (f->R.rdi);
		break;

		case SYS_CLOSE :
			close_syscall(f->R.rdi);
		break;

	}
	
}
// printf ("system call!\n");


// 프로세스 종료 시스템 콜
void
exit_syscall (int status) {
	struct thread *t = thread_current();
	t->exit_status = status;
	printf("%s: exit(%d)\n", t->name, status); 
	thread_exit ();
}

// 파일 이름과 파일 사이즈를 인자 값으로 받아 파일을 생성하는 함수.
bool
create_syscall (char *file, unsigned initial_size) {
	// 파일이름과 사이즈를 받아 파일을 생성해 주는 함수 
	check_address(file);
	lock_acquire(&filesys_lock);
	bool return_value = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return return_value;
}

bool
remove_syscall (const char *file) {
	check_address(file);
	lock_acquire(&filesys_lock);
	bool return_value = filesys_remove(file);
	lock_release(&filesys_lock);
	return return_value;
}

// 현재 프로세스를 cmd_line에서 지정된 인수를 전달하여 이름이 지정된 실행 파일로 변경
int
exec_syscall (char *file) {
	check_address(file);

	int file_size = strlen(file)+1;
	char *fn_copy = palloc_get_page(PAL_ZERO); // 파일 네임 카피
	if (fn_copy == NULL) {
		exit_syscall (-1);
	}
	strlcpy (fn_copy, file, file_size);

	if (process_exec (fn_copy) == -1){
		return -1;
	}
	
	NOT_REACHED();
	return 0;
}

int 
write_syscall (int fd, const void *buffer, unsigned size){
	check_address(buffer);
	struct file *fileobj = fd_to_struct_filep(fd);
	int read_count;
	if (fd == STDOUT_FILENO){
		putbuf(buffer, size);
		read_count = size;
	}
	else if (fd == STDIN_FILENO){
		return -1;
	}
	else {
		lock_acquire(&filesys_lock);
		read_count = file_write(fileobj, buffer, size);
		lock_release(&filesys_lock);
	}
}

int
open_syscall (const char *file) {
	check_address(file);
	lock_acquire(&filesys_lock);
	struct file *open_file = filesys_open(file); //오픈 파일 객체정보를 저장
	lock_release(&filesys_lock);
	
	if(open_file == NULL){
		return -1;
	}

	int fd = add_file_to_fd_table(open_file); // 만들어진 파일을 스레드 안에 fd테이블에 저장
	
	if (fd == -1){				//열수 없는 파일이면 
		file_close (open_file);
	}
	return fd;
}

int
filesize_syscall (int fd) {
	check_address(fd);
	struct file *fileobj = fd_to_struct_filep(fd);
	if (fileobj == NULL){
		return -1;
	}
	file_length(fileobj);
}

int
read_syscall (int fd, void *buffer, unsigned size) {
	check_address(buffer);
	check_address(buffer + size -1);
	unsigned char *buf = buffer;
	int read_count;

	struct file *fileobj = fd_to_struct_filep(fd);

	if (fileobj == NULL){
		return -1;
	}

	//stdin일때
	if (fd == STDIN_FILENO){
		char key;
		for (int read_count = 0; read_count < size; read_count++){
			key = input_getc();
			*buf++ = key;
			if (key == '\0'){
				break;
			}
		}
	}
	else if (fd == STDOUT_FILENO){
		return -1;
	}
	else {
		lock_acquire(&filesys_lock);
		read_count = file_read(fileobj, buffer, size);
		lock_release(&filesys_lock);
	}
	return read_count;
}

void
seek_syscall (int fd, unsigned position) {
	if (fd < 2){
		return;
	}
	struct file *file = fd_to_struct_filep(fd);
	check_address(file);
	if (file == NULL){
		return;
	}
	file_seek(file, position);
}

unsigned
tell_syscall (int fd) {
	if (fd < 2){
		return;
	}
	struct file *file = fd_to_struct_filep(fd);
	check_address(file);
	if (file == NULL){
		return;
	}
	return file_tell(fd);
}

void
close_syscall (int fd) {
	struct file *close_file = fd_to_struct_filep(fd);
	if (close_file == NULL){
		return;
	}

	lock_acquire(&filesys_lock);
	file_close(close_file);
	lock_release(&filesys_lock);
	remove_file_from_fd_table(close_file);
}