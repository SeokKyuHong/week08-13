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


void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void halt (void) NO_RETURN;
void exit (int status) NO_RETURN;


/* System call.
 *
 * Previously system call services was handled by the interrupt handler
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
	int syscall_no = f->R.rax;  // 파일 네임
	uint64_t a1 = f->R.rdi;		// c(개수)
	uint64_t a2 = f->R.rsi;		// v(데이터)
	uint64_t a3 = f->R.rdx;     //
	uint64_t a4 = f->R.r10;
	uint64_t a5 = f->R.r8;
	uint64_t a6 = f->R.r9;
	
	// printf("야%p 뭐%p 야%p \n", (char*)a1, (char*)a2, (char*)a3);

	switch (syscall_no) {		// rax is the system call number
		
		case SYS_HALT : 
		halt();
		break;

		case SYS_EXIT : 
		exit (a1);
		break;
			
		// case SYS_FORK :
		// a3 = fork(a3, f);
		// break;

		// case SYS_EXEC :
		// break;

		// case SYS_WAIT :
		// break;

		// case SYS_CREATE :
		// break;

		// case SYS_REMOVE :
		// break;

		// case SYS_OPEN :
		// break;

		// case SYS_FILESIZE :
		// break;

		// case SYS_READ :
		// break;

		case SYS_WRITE :
		printf("%s", (char*)a2);
		break;

		// case SYS_SEEK :
		// break;

		// case SYS_TELL :
		// break;

		// case SYS_CLOSE :
		// break;

	}


	// printf ("system call!\n");


	
}

// pintos 종료 시스템 콜
void halt(void) {
	power_off();
}

void
exit (int status) {
	struct thread *t = thread_current();
	t->exit_status = status;
	printf("%s: exit(%d)\n", thread_name(), status); 
	thread_exit ();
}