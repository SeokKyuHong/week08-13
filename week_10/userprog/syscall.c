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
#include "user/syscall.h"


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
int add_file_to_fd_table (struct file *file);
void check_address (const uint64_t *addr);
struct file *fd_to_struct_filep (int fd);
void remove_file_from_fd_table(int fd);
int fork_syscall(const char *thread_name, struct intr_frame *f);
int wait_syscall (pid_t pid); 



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
	//유저영역의 주소가 아니거나, 물리주소와 맵핑되어 있는 페이지가 없다면 프로세스를 종료 시킨다.
	if (!(is_user_vaddr(addr)) || pml4_get_page(cur -> pml4, addr) == NULL)
	{
		exit_syscall(-1);
	}
}

int 
add_file_to_fd_table (struct file *file){
	int fd = 2;
	struct thread *t = thread_current();
	while(t->file_descriptor_table[fd] != NULL && fd < MAX_FD_NUM)
	{
		fd++;
	}
	if (fd >= MAX_FD_NUM){
		return -1;
	}
	t->file_descriptor_table[fd] = file;
	return fd;
}

struct
file *fd_to_struct_filep (int fd){
	if (fd < 0 || fd >= MAX_FD_NUM){
		return NULL;
	}
	struct thread *t = thread_current();
	return t -> file_descriptor_table[fd];
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
			
		case SYS_FORK :
			f->R.rax = fork_syscall(f->R.rdi, f);
		break;

		//프로세스 생성
		case SYS_EXEC :
			f->R.rax = exec_syscall(f->R.rdi);
		break;

		case SYS_WAIT :
			f->R.rax = wait_syscall(f->R.rdi);
		break;

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
			seek_syscall (f->R.rdi, f->R.rsi);
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
	t->exit_status = status;	//해당 종료 상태값을 스레드에 저장
	printf("%s: exit(%d)\n", t->name, status); 
	thread_exit ();
}

int 
fork_syscall(const char *thread_name, struct intr_frame *f){
	return process_fork(thread_name, f);
}

// 파일 이름과 파일 사이즈를 인자 값으로 받아 파일을 생성하는 함수.
bool
create_syscall (char *file, unsigned initial_size) {
	check_address(file);
	lock_acquire(&filesys_lock);	//file 관련 함수를 호출 시 동시성 보장을 위해 락을 요청 
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
		return -1;
	}
	strlcpy (fn_copy, file, file_size);

	if (process_exec (fn_copy) == -1){
		return -1;
	}
}

// 써줘
int 
write_syscall (int fd, const void *buffer, unsigned size){
	
	check_address(buffer);
	if (fd == STDIN_FILENO){
		return 0;
	}
	else if (fd == STDOUT_FILENO){	//out 일때
		
		putbuf(buffer, size);	
		
		return size;
	}
	else{
		struct file *write_file = fd_to_struct_filep(fd);
		if (write_file == NULL){
			
			return 0;
		}
		
		lock_acquire(&filesys_lock);
		off_t write_byte = file_write(write_file, buffer, size);
		lock_release(&filesys_lock);
		return write_byte;
	}
}

int
open_syscall (const char *file) {
	check_address(file);
	lock_acquire(&filesys_lock);         
	struct file *open_file = filesys_open(file); //오픈 파일 객체정보를 저장
	/*rox*/
	if (strcmp(thread_current()->name, file) == 0){
		file_deny_write (open_file); 
	}
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
	struct file *fileobj = fd_to_struct_filep(fd);
	if (fileobj == NULL){
		return -1;
	}
	lock_acquire(&filesys_lock);
	off_t write_byte = file_length(fileobj);
	lock_release(&filesys_lock);
	return write_byte;
}

//읽얼꺼야
int
 (int fd, void *buffer, unsigned size) {
	check_address(buffer);
	// check_address(buffer + size -1);

	int read_count;
	struct file *fileobj = fd_to_struct_filep(fd);

	if (fileobj == NULL){
		return -1;
	}

	if (fd == STDOUT_FILENO){
		return -1;
	}
	lock_acquire(&filesys_lock);
	read_count = file_read(fileobj, buffer, size);
	lock_release(&filesys_lock);

	return read_count;
}

//파일 위치 이동
void
seek_syscall (int fd, unsigned position) {
	
	struct file *file = fd_to_struct_filep(fd);

	file_seek(file, position);
}

//열린 파일의 위치를 알려준다. 
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

//파일을 닫고 fd_table도 NULL로 초기화
void
close_syscall (int fd) {
	
	struct file *close_file = fd_to_struct_filep(fd);
	if (close_file == NULL){
		return ;
	}

	lock_acquire(&filesys_lock);
	file_close(close_file);
	lock_release(&filesys_lock);
	remove_file_from_fd_table(fd);
}

int
wait_syscall (pid_t pid) {
	return process_wait(pid);
}

