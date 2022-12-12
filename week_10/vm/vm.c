/* vm.c: Generic interface for virtual memory objects. */
#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "filesys/file.h"
#include "threads/vaddr.h"


struct list frame_table;
struct list_elem *start;
void printf_hash (struct supplemental_page_table *spt);

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr (); 
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);
	start = list_begin(&frame_table);
}

bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();
	
	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);
void spt_destructor(struct hash_elem *e, void *aux);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/* 이니셜라이저로 펜딩 페이지 객체를 생성합니다.
페이지를 생성하고 싶다면 직접 생성하지 마시고 
이 함수나 `vm_alloc_page`를 통해서 만드시면 됩니다. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)
	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* Check wheter the upage is already occupied or not. */
	/* 페이지가 이미 점유되어 있는지 확인 */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		
		/* TODO: Insert the page into the spt. */
		
		//함수 포인터를 사용하여 TYPE에 맞는 페이지 초기화 함수를 사용
		struct page *new_page = (struct page *)malloc (sizeof(struct page));
		typedef bool (*initializeFunc)(struct page*, enum vm_type, void *);
		initializeFunc initializer = NULL;

		switch (VM_TYPE(type)){

			case VM_ANON:
				initializer = anon_initializer;
				break;

			case VM_FILE:
				
				initializer = file_backed_initializer;
				break;
			
			default :
				goto err;
		}

		// 새 페이지를 만들어서 page의 구조체 맴버를 세팅
		uninit_new(new_page, upage, init, type, aux, initializer);
		
		new_page->writable = writable;
		// new_page->vm_type = type;
		// new_page->page_cnt = -1;	// file-mapped page가 아니므로 -1
		
		/* TODO: 페이지를 spt에 삽입합니다. */ 
		// printf("&&&&&&&&&&&&&%p\n", new_page->va);
		return spt_insert_page(spt, new_page);

	}
err:
	PANIC("VM initial fail");
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
/* spt에서 VA를 찾아 페이지로 리턴. 오류가 발생하면 NULL을 반환합니다. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = (struct page *)malloc (sizeof(struct page));
  	struct hash_elem *e;

	/* 해당 va가 속해 있는 페이지 시작 주소를 가지는 page 만든다.*/
  	page->va = pg_round_down(va);

	//인자로 들어온 spt에서 인자로 받은 va와 같은 page를 찾아 elem으로 반환 
  	e = hash_find(&spt->hashs, &page->hash_elem);

	free(page);

	// e가 NULL이 아니면 page 구조체로 반환 
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
/* 유효성 검사와 함께 PAGE를 spt에 삽입합니다. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	
    if(hash_insert(&spt->hashs, &page->hash_elem) == NULL){
        return true;
	}
    return false;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
/* 제거될 구조체 프레임을 가져옵니다. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* FIFO eviction policy */
	struct thread *curr = thread_current();
	struct list_elem *e = start;

	
	for (start = e; start != list_end(&frame_table); start = list_next(start))
	{
		victim = list_entry(start, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va))
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		else
			return victim;
	}

	for (start = list_begin(&frame_table); start != e; start = list_next(start))
	{
		victim = list_entry(start, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va))
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		else
			return victim; 
	} 
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
/* 한 페이지를 제거하고 해당 프레임을 반환합니다.
  * 오류 시 NULL을 반환합니다.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */
	if (victim != NULL){
		swap_out(victim->page);
	}

	return victim; 
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* palloc() 및 프레임 가져오기. 사용 가능한 페이지가 없으면 해당 페이지를 제거하고 반환합니다.
이것은 항상 유효한 주소를 반환합니다.
즉, 사용자 풀 메모리가 가득 찬 경우 이 함수는 사용 가능한 메모리 공간을 얻기 위해 프레임을 제거합니다.*/
struct frame *
vm_get_frame(void)
{
	// struct frame *frame = NULL;
	/* TODO: Fill this function. */
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));

	frame->kva = palloc_get_page(PAL_USER); /* USER POOL에서 커널 가상 주소 공간으로 1page 할당 */

	/* if 프레임이 꽉 차서 할당받을 수 없다면 페이지 교체 실시
	   else 성공했다면 frame 구조체 커널 주소 멤버에 위에서 할당받은 메모리 커널 주소 넣기 */
	if (frame->kva == NULL)
	{
		frame = vm_evict_frame();
		frame->page = NULL;
		return frame;
	}

	/* 새 프레임을 프레임 테이블에 넣어 관리 */
	list_push_back (&frame_table, &frame->frame_elem);  
  
	frame->page = NULL;

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	//스택에 해당하는 ANON 페이지를 UNINIT으로 만들고 SPT에 넣어줌
	//이후 claim해서 물리메모리와 매핑
	
	if (vm_alloc_page(VM_ANON | VM_MARKER_0, addr, 1)){
		vm_claim_page(addr);
		thread_current() -> stack_bottom -= PGSIZE;
	}
}

/* Handle the fault on write_protected page */
/* write_protected 페이지의 오류 처리 */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	//유저 가상 메모리 안의 페이지가 아니라면 리턴
	// Step 1. Locate the page that faulted in the supplemental page table
	if (is_kernel_vaddr(addr)) {
        return false;
	}

    void *rsp_stack = is_kernel_vaddr(f->rsp) ? thread_current()->rsp_stack : f->rsp;
    if (not_present){
        if (!vm_claim_page(addr)) {
            if (rsp_stack - 8 <= addr && USER_STACK - 0x100000 <= addr && addr <= USER_STACK) {
                vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
                return true;
            }
			// printf("******22222************\n");
            return false;
        }
        else
            return true;
    }
    return false;
    // return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
/* VA에 할당된 페이지를 요청합니다. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	// struct thread *t = thread_current();
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);
	// printf("&&&&&&&&&&&&%p\n",page);
	if (page == NULL){ 
		// printf("*****************#$######\n");
		return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
/* PAGE를 요청하고 mmu를 설정합니다. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	struct thread *t = thread_current();
	/* Set links */
	frame->page = page;
	page->frame = frame;
	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	if (pml4_get_page(t->pml4, page->va) == NULL &&  pml4_set_page (t->pml4, page->va, frame->kva, page->writable)){

		return swap_in(page, frame->kva);
	}
	return false;
}

/* Initialize new supplemental page table */
/* 새로운 추가 페이지 테이블 초기화 
supplemental : 보조*/
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	// struct hash* page_table = malloc(sizeof (struct hash));
	hash_init(&spt->hashs, page_hash, page_less, NULL);
	
	
}

/* Copy supplemental page table from src to dst */
/* src에서 dst로 추가 페이지 테이블 복사 */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {	

	struct hash_iterator i;
    hash_first (&i, &src->hashs);
    while (hash_next (&i)) {	// src의 각각의 페이지를 반복문을 통해 복사
        struct page *parent_page = hash_entry (hash_cur (&i), struct page, hash_elem);   // 현재 해시 테이블의 element 리턴
        enum vm_type type = page_get_type(parent_page);		// 부모 페이지의 type
        void *upage = parent_page->va;						// 부모 페이지의 가상 주소
        bool writable = parent_page->writable;				// 부모 페이지의 쓰기 가능 여부
        vm_initializer *init = parent_page->uninit.init;	// 부모의 초기화되지 않은 페이지들 할당 위해 
        void* aux = parent_page->uninit.aux;

        if (parent_page->uninit.type & VM_MARKER_0) {
            setup_stack(&thread_current()->tf);
        }
        else if(parent_page->operations->type == VM_UNINIT) {	// 부모 타입이 uninit인 경우
            if(!vm_alloc_page_with_initializer(type, upage, writable, init, aux))
                return false;
        }
        else {
            if(!vm_alloc_page(type, upage, writable))
                return false;
            if(!vm_claim_page(upage))
                return false;
        }

        if (parent_page->operations->type != VM_UNINIT) {   //! UNIT이 아닌 모든 페이지(stack 포함)는 부모의 것을 memcpy
            struct page* child_page = spt_find_page(dst, upage);
            memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
        }
    }
	return true;
}


/* Free the resource hold by the supplemental page table */
/* 보조 페이지 테이블이 보유하고 있는 리소스 해제 */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	/* 스레드가 보유한 모든 supplemental_page_table을 삭제하고 
	수정된 모든 내용을 스토리지에 다시 기록합니다. */

	struct hash_iterator i;
	hash_first(&i, &spt->hashs);
	while(hash_next(&i))
	{
		struct page *page = hash_entry(hash_cur(&i), struct page, hash_elem);
		if(page->operations->type == VM_FILE)
		{
			do_munmap(page->va);
		}
	} 
	
	hash_clear(&spt->hashs, spt_destructor);
}

//hash_elem이 포함된 페이지를 free
void 
spt_destructor(struct hash_elem *e, void *aux) 
{
	const struct page *p = hash_entry(e, struct page, hash_elem);
	free(p);
}

static bool
setup_stack (struct intr_frame *if_) {
	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */
	/* TODO: stack_bottom에 스택을 매핑하고 즉시 페이지를 요청합니다.
	* TODO: 성공하면 그에 따라 rsp를 설정합니다.
	* TODO: 페이지가 스택임을 표시해야 합니다. */	

	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);
	if (vm_alloc_page(VM_ANON | VM_MARKER_0, stack_bottom, 1)) {
		success = vm_claim_page(stack_bottom);

		if (success){
			
			if_->rsp = USER_STACK;
			thread_current()->stack_bottom = stack_bottom;
		}
	}

	// uint8_t *kpage;
	// bool success = false;
	// void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);
	// kpage = palloc_get_page(PAL_USER | PAL_ZERO);
	// if (kpage != NULL)
	// {
	// 	success = install_page(((uint8_t *)USER_STACK) - PGSIZE, kpage, true);
	// 	if (success){
	// 		if_->rsp = USER_STACK;
	// 		thread_current()->stack_bottom = stack_bottom;
	// 	}
	// 	else
	// 		palloc_free_page(kpage); 
	// }
		
	return success;
}

// hash_elem를 넣으면 해당 페이지의 va에서 해시값을 추출 
unsigned 
page_hash (const struct hash_elem *e, void *aux){
	const struct page *p = hash_entry (e, struct page, hash_elem);
	
	return hash_bytes(&p->va, sizeof(p->va));
}

//a와 b의 va를 비교해 b가 크면 참을 뱉음
bool 
page_less (const struct hash_elem *a, 
	const struct hash_elem *b, void *aux){
	
	const struct page *p_a = hash_entry (a, struct page, hash_elem);
	const struct page *p_b = hash_entry (b, struct page, hash_elem);
	return p_a->va < p_b->va;
}

bool 
delete_page (struct hash *pages, struct page *p) {
	if (hash_delete(pages, &p->hash_elem))
		return false;
	else
		return true;
}

void
printf_hash (struct supplemental_page_table *spt) {
  struct hash *h = &spt->hashs;
  struct hash_iterator i;
  hash_first (&i, h);
  printf ("===== hash 순회시작 =====\n");
  while (hash_next (&i)) {
    struct page *p = hash_entry (hash_cur (&i), struct page, hash_elem);
    if (p->frame == NULL) {
      printf ("va: %X, writable : %X\n", p->va, p->writable);
    } else {
      printf ("va: %X, kva : %X, writable : %X\n", p->va, p->frame->kva,
              p->writable);
    }
  }
  printf ("===== hash 순회종료 =====\n");
}