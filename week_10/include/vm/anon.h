#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

struct anon_page {
    int swap_index;//swap된 데이터 들이 저장된 섹터 구역
    //swap_in/out을 구현 할 때 swap_sec 변수 추가 예정
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
