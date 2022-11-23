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


void syscall_entry (void);
void syscall_syscall (struct intr_frame *);
void halt_syscall (void) NO_RETURN;
void exit_syscall (int status) NO_RETURN;
bool create_syscall (char *file, unsigned initial_size);
int exec_syscall (char *file);
bool remove_syscall (const char *file);
int write (int fd, const void *buffer, unsigned size);


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
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	uint64_t syscall_no = f->R.rax;  // 콜 넘버

	// uint64_t a1 = f->R.rdi;		// c(개수?)
	// uint64_t a2 = f->R.rsi;		// v(데이터)
	// uint64_t a3 = f->R.rdx;     
	// uint64_t a4 = f->R.r10;
	// uint64_t a5 = f->R.r8;
	// uint64_t a6 = f->R.r9;
	
	

	switch (syscall_no) {		// rax is the system call number

		char *fn_copy;
		
		// 핀토드 종료 시스템 콜
		case SYS_HALT : 
		halt_syscall();
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
			if (exec_syscall (f->R.rdi) == -1)
			{
				exit_syscall(-1);
			}
		break;

		// case SYS_WAIT :
		// break;

		// 파일 이름과 파일 사이즈를 인자 값으로 받아 파일을 생성하는 함수.
		case SYS_CREATE : 
			check_address(f->R.rdi);
			f->R.rax = create_syscall(f->R.rdi, f->R.rsi);
		break;

		case SYS_REMOVE :
			check_address(f->R.rdi);
			f->R.rax = remove_syscall(f->R.rdi);
		break;

		// case SYS_OPEN :
		// 	hong_dump_frame (f);
		// break;

		// case SYS_FILESIZE :
		// break;

		// case SYS_READ :
		// break;

		case SYS_WRITE :
			check_address(f->R.rsi);
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);	
		break;

		// case SYS_SEEK :
		// break;

		// case SYS_TELL :
		// break;

		// case SYS_CLOSE :
		// break;

	}
	
}
// printf ("system call!\n");


// pintos 종료 시스템 콜
void halt_syscall(void) {
	power_off();
}

// 프로세스 종료 시스템 콜
void
exit_syscall (int status) {
	struct thread *t = thread_current();
	t->exit_status = status;
	printf("%s: exit(%d)\n", thread_name(), status); 
	thread_exit ();
}

// 파일 이름과 파일 사이즈를 인자 값으로 받아 파일을 생성하는 함수.
bool
create_syscall (char *file, unsigned initial_size) {
	// 파일이름과 사이즈를 받아 파일을 생성해 주는 함수 
	bool return_valeu = filesys_create(file, initial_size);
	return return_valeu;
}

bool
remove_syscall (const char *file) {
	bool return_valeu = filesys_remove(file);
	return return_valeu;
}

// 현재 프로세스를 cmd_line에서 지정된 인수를 전달하여 이름이 지정된 실행 파일로 변경
int
exec_syscall (char *file) {
	check_address(file);

	int file_size = strlen(file)+1;
	char *fn_copy = palloc_get_page(PAL_ZERO); // 파일 네임 카피
	if (fn_copy == NULL) 
	{
		exit_syscall (-1);
	}
	strlcpy (fn_copy, file, file_size);

	if (process_exec (fn_copy) == -1) 
		return -1;
	
	NOT_REACHED();
	return 0;
}

int 
write (int fd, const void *buffer, unsigned size){
	if(fd == 1){
		putbuf(buffer, size);
		return size;
	}
}
