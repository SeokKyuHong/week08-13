#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include "kernel/hash.h"
#include "userprog/syscall.h"
// #include "userprog/process.h"
// #include "threads/thread.h"
#include "userprog/syscall.h"

enum vm_type {
	VM_UNINIT = 0,	/* page not initialized */
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,	/* 파일과 관련이 없는 페이지, 일명 익명 페이지 */
	/* page that realated to the file */
	VM_FILE = 2,	/* 파일과 관련된 페이지 */
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,	/* 프로젝트 4에 대한 페이지 캐시를 보유하는 페이지 */

	/* Bit flags to store state */
	/* 상태를 저장하기 위한 비트 플래그 */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	/* 저장 정보를 위한 보조 비트 플래그 마커. 
	값이 int에 맞을 때까지 더 많은 마커를 추가할 수 있습니다. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;
struct list frame_table;
struct list_elem *start;


#define VM_TYPE(type) ((type) & 7)
#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page {
	const struct page_operations *operations;//페이지 관련 연산종류
	void *va;              /* Address in terms of user space */
	struct frame *frame;   //물리 메모리 프레임 주소

	/* Your implementation */
	struct hash_elem hash_elem;
	bool writable;
	size_t page_cnt; 
	enum vm_type vm_type;

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	//해당 페이지의 유형
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache;
#endif
	};
};

/* The representation of "frame" */
struct frame {
	void *kva;	//커널 가상 주소를 가르키는 포인터
	struct page *page;	//프레임과 맵핑되는 프로세스 유저의 가상주소 페이지
	struct list_elem frame_elem; //추가
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */

//페이지의 연산 종류 
//해당 페이지가 어떤 타입을 가지고 있느냐에 따라 해당 연산을 실제로
//수행하는 함수들이 달라진다. 
//함수 포인터를 활용하여 각 페이지 유형에 맞는 연산 수행
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. 
 * 현재 프로세스의 메모리 공간 표현.*/
struct supplemental_page_table {
	//뭐가 필요할까
	struct hash hashs;
	//** 프로세스마다 pt을 갖어야 한다. 
	//1. va를 가르키는 포인터
	struct page *page_table;
	struct frame *frame_table;

};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);
static struct frame *vm_evict_frame(void);
bool delete_page (struct hash *pages, struct page *p);
unsigned page_hash (const struct hash_elem *e, void *aux);
bool page_less (const struct hash_elem *a, 
	const struct hash_elem *b, void *aux);
static void vm_stack_growth (void *addr UNUSED);

#endif  /* VM_VM_H */
