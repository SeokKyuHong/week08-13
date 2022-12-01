#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"

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

#define VM_TYPE(type) ((type) & 7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	/* Your implementation */

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	/* 유형별 데이터는 공용체에 바인딩됩니다.
	 * 각 함수는 현재 합집합을 자동으로 감지 */
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
	void *kva;
	struct page *page;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
/* 페이지 작업을 위한 함수 테이블.
  * 이것은 C에서 "인터페이스"를 구현하는 한 가지 방법입니다.
  * "method" 테이블을 구조체의 멤버에 넣고 필요할 때마다 호출하십시오. */
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

	//** 프로세스마다 pt을 갖어야 한다. 
	//1. va를 가르키는 포인터
	struct page page_offset;
	//2. pa를 가르키는 포인터 
	struct frame frame_offset;
	// 스왑
	struct page_operations swap;
	//3. r/w 데이터를 저장 해야 할까?(value)
	//4. hash / Hash_elem
	//5. 페이지 폴트 관련 
	
	//1. 추가 페이지 테이블: 데이터 위치(프레임/디스크/스왑), 
	//해당 커널 가상 주소에 대한 포인터, 
	//활성 대 비활성 등 각 페이지에 대한 추가 데이터를 추적하는 프로세스별 데이터 구조입니다.

	//2. 프레임 테이블: 할당/사용 가능한 물리적 프레임을 추적하는 전역 데이터 구조.

	//3. 스왑 테이블: 스왑 슬롯을 추적합니다.

	//4. 파일 매핑 테이블: 어떤 메모리 매핑된 파일이 어떤 페이지에 매핑되는지 추적합니다.	


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

#endif  /* VM_VM_H */
