#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#include "kernel/hash.h"
#include "userprog/process.h"
#endif


/* States in a thread's life cycle. */
/* 스레드의 라이프 사이클에 있는 상태. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About sto be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
/* 스레드 식별자 유형. 원하는 유형으로 재정의할 수 있습니다. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/*project2: sysem call*/
#define FDT_PAGES 3
#define MAX_FD_NUM	(1<<9)
#define STDOUT_FILENO 1
#define STDIN_FILENO 0


/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
* 이것의 결과는 두 가지입니다.
  *
  1. 먼저 `struct thread'가 너무 커지면 안 됩니다.
	그렇다면 커널 스택을 위한 공간이 충분하지 않습니다.
	우리의 기본 `struct thread'는 크기가 몇 바이트에 불과합니다.
	아마도 1kb 미만으로 유지되어야 할 것입니다.
  *
  2. 둘째, 커널 스택이 너무 커지면 안 됩니다.
	스택이 오버플로되면 스레드 상태가 손상됩니다.
	따라서 커널 함수는 비정적 지역 변수로 큰 구조나 배열을 할당해서는 안 됩니다.
	대신 malloc() 또는 palloc_get_page()와 함께 동적 할당을 사용하십시오.
	
* 이러한 문제 중 하나의 첫 번째 증상은 실행 중인 스레드의 'struct 스레드'의'magic' 멤버가 
	THREAD_MAGIC으로 설정되어 있는지 확인하는 thread_current()의 어설션 실패일 것입니다.
	스택 오버플로는 일반적으로 이 값을 변경하여 어설션을 트리거합니다. */
/* 'elem' 멤버는 이중 목적을 가지고 있습니다.
	실행 대기열(thread.c)의 요소이거나 세마포 대기 목록(synch.c)의 요소일 수 있습니다.
	이 두 가지 방법은 상호 배타적이기 때문에 사용할 수 있습니다. 
	준비 상태의 스레드만 실행 큐에 있고 차단 상태의 스레드만 세마포 대기 목록에 있습니다. */

struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier.(고유값) */
	enum thread_status status;          /* Thread state. 상태값*/
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. 우선순위*/
	
	/*잠잔 노드가 일어날 시간*/
	int64_t wakeup_time;				

	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */

	/*donation 관련*/
	int init_priority;
	struct lock *wait_lock;		//대기하고 있는 lock 자료구조
	struct list dona;			//multiple 고려
	struct list_elem dona_elem; //multiple 고려

	/*프로젝트 2*/
	int exit_status;			//스레드 종료 상태 체크 
	struct file **file_descriptor_table;	//
	int fdidx;

	/*프로젝트 2 -- fork 관련*/

	struct semaphore sema_fork;			//포크작업을 위한 sema
	struct intr_frame parent_if;		//부모의 인터럽트 프레임 정보 저장
	struct list child_list;				//자식 리스트
	struct list_elem child_list_elem;

	bool is_waited;						//waited 불렸는지 아닌지(기다리라고 했다면 true)
	struct semaphore sema_wait;			//자식이 끝날때 까지 대기하기 위한 sema
	struct semaphore sema_free;			//process_exit를 하기 전에 자식의 exit_status를 체크하기 위한 sema
	
	/*project 3*/
	// struct hash_elem hash_elem;


#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	/* 스레드가 소유한 전체 가상 메모리에 대한 테이블. */
	struct supplemental_page_table spt;
	void *stack_bottom;
	uintptr_t *rsp_stack;
#endif
	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_sleep(int64_t ticks); 		//실행중인 스레드를 슬립으로 만듬
void thread_awake (int64_t ticks);		//슬립큐에서 캐워야할 스레드를 깨움

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);
bool more(const struct list_elem *a, const struct list_elem *b, void *aux);
// bool lock_more(const struct list_elem *a, const struct list_elem *b, void *aux);

void thread_comp_ready(void);

void dona_priority(void);
void remove_with_lock(struct lock *lock);
void refresh_priority(void);

#endif /* threads/thread.h */
