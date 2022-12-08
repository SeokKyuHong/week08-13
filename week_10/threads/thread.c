#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
/* struct 쓰레드의 'magic' 멤버에 대한 임의의 값.
   스택 오버플로를 감지하는 데 사용됩니다. 상단의 큰 댓글 참조
   자세한 내용은 thread.h를 참조하십시오. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
/* 기본 스레드의 임의 값
    이 값을 수정하지 마십시오. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
/* THREAD_READY 상태의 프로세스, 즉 프로세스 목록
    실행할 준비가 되었지만 실제로 실행되지는 않습니다. */
static struct list ready_list;

/*잠자는 스레드 리스트*/
static struct list sleep_list;

/*sleep_list에서 대기중인 스레드들의 wakeup_tick값 중 최소값을 저장*/
int64_t next_thread_to_awake;

/* Idle thread. */
/* 사용 가능한 thread */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
/* 초기 스레드, init.c:main()을 실행하는 스레드. */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
/* allocate_tid()에 의해 사용되는 lock. */
static struct lock tid_lock;

/* Thread destruction requests */
/* Thread 파괴 요청 */
static struct list destruction_req;

static struct list dona;

/* Statistics. */
/* 상태?통계? */
static long long idle_ticks;    /* # of timer ticks spent idle. 유휴 상태로 보낸 타이머 틱 수*/
static long long kernel_ticks;  /* # of timer ticks in kernel threads. 커널 스레드의 타이머 틱 수 */
static long long user_ticks;    /* # of timer ticks in user programs. 사용자 프로그램의 타이머 틱 수.*/

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. 각 스레드에 제공하는 타이머 틱 수.*/
static unsigned thread_ticks;   /* # of timer ticks since last yield. 마지막 yield 이후 타이머 틱 수.*/

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
/* false(기본값)인 경우 라운드 로빈 스케줄러를 사용합니다.
    true인 경우 multi-level 대기열 스케줄러를 사용합니다.
    커널 명령줄 옵션 "-o mlfqs"에 의해 제어됩니다. */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);
bool more(const struct list_elem *a, const struct list_elem *b, void *aux);
void thread_comp_ready(void);

void thread_comp_dona(void);
void remove_with_lock(struct lock *lock);
void refresh_priority(void);

/* Returns true if T appears to point to a valid thread. */
/* T가 유효한 스레드를 가리키는 것으로 나타나면 true를 반환합니다. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
/* 실행 중인 스레드를 반환합니다.
CPU의 스택 포인터 'rsp'를 읽고 페이지의 시작 부분으로 반올림하십시오. struct thread'는 항상 페이지의 시작 부분에 있고 
스택 포인터는 중간 어딘가에 있기 때문에 현재 스레드를 찾습니다. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
// thread_start에 대한 Global descriptor 테이블.
// gdt는 thread_init 이후에 설정될 것이기 때문에 임시 gdt를 먼저 설정해야 합니다.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
/* 현재 실행 중인 코드를 스레드로 변환하여 스레드 시스템을 초기화합니다. 이것은 일반적으로 작동하지 않으며 이 경우에만 가능합니다. 
loader.S가 스택의 맨 아래를 페이지 경계에 두도록 주의했기 때문입니다.
또한 실행 대기열과 tid 잠금을 초기화합니다.
이 함수를 호출한 후 thread_create()로 스레드를 생성하기 전에 페이지 할당자를 초기화해야 합니다.
이 함수가 완료될 때까지 thread_current()를 호출하는 것은 안전하지 않습니다. */
void
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	/* 커널에 대한 임시 gdt를 다시 로드합니다.(Global Descriptor Table)
	*이 gdt는 사용자 컨텍스트를 포함하지 않습니다.
	* 커널은 gdt_init()에서 사용자 컨텍스트로 gdt를 다시 빌드합니다. */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* Init the globla thread context */
	/* 전역 스레드 컨텍스트 초기화 */
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init (&destruction_req);

	list_init (&sleep_list);

	/* Set up a thread structure for the running thread. */
	/* 실행 중인 스레드에 대한 스레드 구조를 설정합니다. */
	initial_thread = running_thread (); 				//실행중인 스레드의 구조체를 가져옴(스택 포인터 포함)
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();
}

void
thread_sleep(int64_t ticks){
	struct thread *cur = thread_current(); 	//현재 실행중인 스레드
	enum intr_level old_level;				//지금 인트러덥트

	old_level = intr_disable (); 		//interrupt off

	cur -> wakeup_time = ticks;		//깨어나야 할 ticks 저장
	// list_push_back (&sleep_list, &cur->elem);	//슬립 큐 삽입/
	list_insert_ordered (&sleep_list, &cur->elem, more, 0);

	thread_block();					//block하고
	intr_set_level (old_level);			//interrupt on
}

void
thread_awake (int64_t ticks){		// 현재 시간
	/*sleep_list의 모든 entry순회*/
  struct list_elem *e = list_begin (&sleep_list); 	//sleep_list의 시작 스레드 포인터를 리스트요소로 할당

  while (e != list_end (&sleep_list)){				//sleep_list의 시작과 끝이 같지 않다면 반복문 시작(같을때까지)
    struct thread *t = list_entry (e, struct thread, elem); 	//e(sleep_list 시작)의 포인터를 thread t 구조체 포인터로 할당 
    if (t->wakeup_time <= ticks){	// 스레드의 일어난 시간이 지금 시간보다 작거나 같으면
      e = list_remove (e);	// sleep list 에서 제거 
      thread_unblock (t);	// 스레드 unblock
	  thread_comp_ready();
    }
    else 
      e = list_next (e);
  }
}

/*
less 함수는 첫번째(기준요소)가 두번째(비교대상)보다 작을때 true
list_insert 는 기준 요소를 비교대상 앞으로 넣기 때문에 
첫번째(기준요소)가 두번째(비교대상)보다 클때 true를 뱉는 함수를 만들면 된다.*/
bool
more(const struct list_elem *a, const struct list_elem *b, void *aux){
	return list_entry (a, struct thread, elem) -> priority
			> list_entry (b, struct thread, elem) -> priority;
}
bool
more_exit(const struct list_elem *a, const struct list_elem *b, void *aux){
	return list_entry (a, struct thread, elem) -> exit_status
			> list_entry (b, struct thread, elem) -> exit_status;
}


/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
/* 인터럽트를 활성화하여 선점 스레드 스케줄링을 시작합니다.
   또한 유휴 스레드를 생성합니다. */
void
thread_start (void) {
	/* Create the idle thread. */
	/* idle 스레드를 생성합니다. 스타트*/
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	/*이순간 idle thread 생성, 동시에 idle함수 실행*/
	thread_create ("idle", PRI_MIN, idle, &idle_started);

	/* Start preemptive thread scheduling. */
	/* 선점형 스레드 스케줄링을 시작합니다. */
	intr_enable ();

	/* Wait for the idle thread to initialize idle_thread. */
	/* 유휴 스레드가 유휴 스레드를 초기화할 때까지 기다립니다. */
	sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
/* 각 타이머 틱에서 타이머 인터럽트 핸들러에 의해 호출됩니다.
    따라서 이 함수는 외부 인터럽트 컨텍스트에서 실행됩니다. */
void
thread_tick (void) {
	struct thread *t = thread_current ();

	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	/* ticks가 TIMAE_SLICE 보다 커지는 순간  intr_yield_on_return ()실행
	이 인터럽트는 결과적으로 thread_yield()를 실행 시킨다.*/
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
/* 주어진 이니셜로 NAME이라는 이름의 새 커널 스레드를 생성합니다.
    PRIORITY는 AUX를 인수로 전달하는 FUNCTION을 실행하고 이를 준비 대기열에 추가합니다. 
	새 스레드의 스레드 식별자를 반환하거나 생성에 실패하면 TID_ERROR를 반환합니다.

    thread_start()가 호출된 경우 새 스레드는 thread_create()가 반환되기 전에 예약될 수 있습니다.
	thread_create()가 반환되기 전에 종료될 수도 있습니다. 반대로 원래 스레드는 새 스레드가 예약되기 전에 일정 시간 동안 실행될 수 있습니다.
	순서를 확인해야 하는 경우 세마포어 또는 다른 형태의 동기화를 사용하십시오.

    제공된 코드는 새 스레드의 '우선순위' 멤버를 PRIORITY로 설정하지만 실제 우선순위 스케줄링은 구현되지 않습니다.
   
	우선순위 스케줄링은 문제 1-3의 목표입니다. */
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* Allocate thread. */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/*project 2_ system call*/
	t -> file_descriptor_table = palloc_get_multiple(PAL_ZERO, FDT_PAGES);
	if (t->file_descriptor_table == NULL){
		return TID_ERROR;
	}
	t -> fdidx = 2;	//0은 stdin, 1은 stsout에 이미 할당 함
	t -> file_descriptor_table[0] = 1;	//stdin 자리 : 1 
	t -> file_descriptor_table[1] = 2;	//stdout 자리 : 2

	struct thread *curr = thread_current();
	list_push_back(&curr -> child_list, &t->child_list_elem); 
	// list_insert_ordered (&curr -> child_list, &t->child_list_elem, more_exit, 0);

	/* Call the kernel_thread if it scheduled.
		* Note) rdi is 1st argument, and rsi is 2nd argument. */
	/* 예약된 경우 kernel_thread를 호출합니다.
		rdi는 첫 번째 인수, rsi는 두 번째 인수입니다. */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock (t);

	thread_comp_ready();

	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
/* 현재 스레드를 절전 모드로 전환합니다. thread_unblock()에 의해 깨어날 때까지 다시 예약되지 않습니다.
	이 함수는 인터럽트를 끈 상태에서 호출해야 합니다. 일반적으로 synch.h에서 동기화 프리미티브 중 하나를 사용하는 것이 더 좋습니다. */
void
thread_block (void) {
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule ();
	
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
/* 차단된 스레드 T를 실행 준비 상태로 전환합니다.
T가 차단되지 않은 경우 오류입니다. (thread_yield()를 사용하여 실행 중인 스레드를 준비합니다.)
이 함수는 실행 중인 스레드를 선점하지 않습니다.
이것은 중요할 수 있습니다. 호출자가 인터럽트 자체를 비활성화한 경우 스레드를 원자적으로 차단 해제하고 다른 데이터를 업데이트할 수 있을 것으로 예상할 수 있습니다. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);
	list_insert_ordered (&ready_list, &t->elem, more, 0);
	t->status = THREAD_READY;
	
	intr_set_level (old_level);
	
}

/* Returns the name of the running thread. */
/* 실행 중인 스레드의 이름을 반환합니다. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
/* 실행 중인 스레드를 반환합니다.
    이것은 running_thread()와 몇 가지 온전성 검사입니다.
    자세한 내용은 thread.h 상단의 큰 주석을 참조하십시오. */
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();
	
	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* 현재 스레드의 스케줄을 취소하고 소멸시킵니다. 
호출자에게 절대 반환하지 않습니다. */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
/* CPU를 양보합니다. 현재 스레드는 휴면 상태가 아니며 스케줄러의 변덕에 따라 즉시 다시 예약될 수 있습니다. */
void
thread_yield (void) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	ASSERT (!intr_context ());

	old_level = intr_disable ();					//intr off
	if (curr != idle_thread)						//놀고 있면 
		list_insert_ordered (&ready_list, &curr->elem, more, 0);
	do_schedule (THREAD_READY);
	intr_set_level (old_level);						//intr on
}

/*지금 실행중인 스레드의 우선순위와 ready_list 맨 앞에 스레드우선순위를 비교하여 y*/
void
thread_comp_ready() {
	struct thread *curr = thread_current();
	int e = (list_entry(list_begin (&ready_list), struct thread , elem) -> priority);

	if (curr -> priority < e && thread_current() != idle_thread){ 
		thread_yield();
	}

}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) {
	struct thread *curr = thread_current();
	curr->init_priority = new_priority;

	refresh_priority();

	thread_comp_ready();
	
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) {
	/* TODO: Your implementation goes here */
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* Let someone else run. */
		intr_disable ();
		thread_block ();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
/*커널 스레드의 동작
thread_func *function : kernal이 실행할 함수를 가르킴
void *aux : 보조 파라미터로 동기화를 위한 세마포 등
+-----------------------kernel_thread-------------------+
|						inir_enable()					|
|														|
|	+--------------------function(aux)---------------+	|
|	|				idles thread(main thread)		 |	|
|	|												 |	|
|	|					thread C					 |	|
|	|	thread A									 |	|
|	|							thread B			 |	|
|	|												 |	|
|	+------------------------------------------------+	|
|						thread_exit()					|
+-------------------------------------------------------+
*/
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* The scheduler runs with interrupts off. */
	function (aux);       /*thread가 종료 될때까지 실행되는 main함수*/
	thread_exit ();       /* If function() returns, kill the thread. */
}


/* Does basic initialization of T as a blocked thread named
   NAME. */
/* NAME이라는 이름의 block된 스레드로 T의 기본 초기화를 수행합니다. */
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);	
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);
	t->priority = priority;
	t->magic = THREAD_MAGIC;
	
	/*priority donation 관련 자료구조 초기화*/
	t->init_priority = priority;
	t->wait_lock = NULL;
	list_init(&t->dona);

	/*project 2 
	sema는 다운(0)하여 초기화*/
	t->exit_status = 0;
	list_init(&t->child_list);
	sema_init(&t->sema_fork, 0);

	t->is_waited = false;
	sema_init(&t->sema_wait, 0);
	sema_init(&t->sema_free, 0);



}

/*wait_loct 구조체를 돌면서 run중인 스레드의 priority값으로 모두 바꿔*/
void
dona_priority(void){
	struct thread *cur_t = thread_current();

	for (int depth = 0; depth < 8; depth++){
		if (!cur_t -> wait_lock){
			break;
		}
		struct thread *t = cur_t -> wait_lock->holder;
		t -> priority = cur_t -> priority;
		cur_t = t;  
	}
}

/*lock 해지 시 donations 리스트에서 해당 elem 삭제*/
void 
remove_with_lock(struct lock *lock){
	struct thread *t = thread_current();
	struct list_elem *temp;
	for (temp =list_begin (&t->dona); list_end (&t->dona) != temp; temp = list_next (temp)){
		struct thread *t_t = list_entry(temp, struct thread, dona_elem);
		if (t_t->wait_lock == lock){
			list_remove (&t_t->dona_elem);
		}
	}
}

/*스레드 우선순위가 변경 되었을때 dona 를 고려하여 우선순위를 다시 걸정하는 함수를 작성한다.*/
void
refresh_priority(void){
	struct thread *t = thread_current();

	//현재 스레드의 우선순위를 기부받기 전의 우선순위로 변경
	t -> priority = t -> init_priority;

	// 가장 우선순위가 높은 dona리스트의 스레드와 현재 스레드의 우선순위를 비교하여 높은 값을 현재 스레드의 우선순위로 설정한다.
	if (!list_empty(&t->dona)){

		list_sort(&t->dona, more, 0);
		
		struct thread *temp = list_entry (list_front (&t->dona), struct thread, dona_elem);
		
		if (temp ->priority > t->priority){
			t->priority = temp ->priority;
		}
	}
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
/*ready queue에서 다음에 실행될 스레드를 골라 return*/
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
/* Use iretq를 사용하여 스레드를 시작합니다. */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* 새 스레드의 페이지 테이블을 활성화하여 스레드를 전환하고 이전 스레드가 죽으면 소멸합니다.

    이 함수를 호출할 때 PREV 스레드에서 방금 전환했고 새 스레드는 이미 실행 중이며 인터럽트는 여전히 비활성화되어 있습니다.

    스레드 전환이 완료될 때까지 printf()를 호출하는 것은 안전하지 않습니다. 실제로는 printf()가 함수 끝에 추가되어야 함을 의미합니다. */
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile (
			/* Store registers that will be used. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* Fetch input once */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // Saved rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // Saved rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // Saved rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // read the current rip.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
/* 새로운 프로세스를 스케쥴러 한다. 
진입 시 인터럽트가 꺼져 있어야 합니다.
이 함수는 현재 스레드의 상태를 상태로 수정한 다음 실행할 다른 스레드를 찾아 전환합니다.
schedule()에서 printf()를 호출하는 것은 안전하지 않습니다. */
static void
do_schedule(int status) {									//intr off 상태에서 들어와 status(4가지중 하나)를 받아온다. 
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (thread_current()->status == THREAD_RUNNING);
	while (!list_empty (&destruction_req)) {				//dying 리스트에 있으면 돌아!(list_empty는 리스트가 비어있으면 true를 반환 하기떄문에)
		struct thread *victim =								
			list_entry (list_pop_front (&destruction_req), struct thread, elem); //dying리스트의 맨 앞을 꺼내오고
		palloc_free_page(victim);							//꺼내온 thread를 삭제
	}
	thread_current ()->status = status;						//받아온 상태값으로 thread 생성
	schedule ();											//새로 스캐즇
}

static void
schedule (void) {
	struct thread *curr = running_thread (); 		// 실행 중 스래드 curr
	struct thread *next = next_thread_to_run ();	// 다음 실행될 스래드 next

	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (curr->status != THREAD_RUNNING);
	ASSERT (is_thread (next));
	/* Mark us as running. */
	
	next->status = THREAD_RUNNING;					//다음 스래드의 status를 running 상태로

	/* Start new time slice. */
	thread_ticks = 0;								//ticks 초기화

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate (next);
#endif

	if (curr != next) {	// 
		/* 우리가 전환한 쓰레드가 dying 면, 그 struct 쓰레드를 파괴한다. 
		이것은 thread_exit()가 스스로 깔개를 빼내지 않도록 늦게 일어나야 합니다.
		페이지가 현재 스택에서 사용 중이기 때문에 여기에서 페이지 여유 요청을 큐에 넣습니다.
		실제 소멸 논리는 schedule() 시작 부분에서 호출됩니다. */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* 스레드를 전환하기 전에 먼저 현재 실행 중인 정보를 저장합니다. */
		thread_launch (next);			//페이지 생성
	}
}

/* 새 스레드에 사용할 tid를 반환합니다. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}
