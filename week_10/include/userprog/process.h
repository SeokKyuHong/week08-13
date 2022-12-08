#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

typedef int tid_t;
typedef int32_t off_t;

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);
static bool install_page (void *upage, void *kpage, bool writable);
bool lazy_load_segment (struct page *page, void *aux);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable);
static bool setup_stack (struct intr_frame *if_);
struct file* process_get_file(int fd);

//lazy load를 위한 정보 구조체
//이 안에 해당 페이지에 대응되는 파일의 정보들이 들어가있다. 
struct container {
    struct file *file;
    off_t offset;           //해당 파일의 오프셋
    size_t page_read_bytes; //읽어올 파일의 데이터 크기
    size_t page_zero_bytes; //읽어올 파일의 데이터 크기
};

#endif /* userprog/process.h */
