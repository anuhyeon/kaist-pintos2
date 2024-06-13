/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

#include "userprog/process.h"
#include "threads/mmu.h"
#include "threads/thread.h"

/*----------------------------------------------------------------------------------*/
unsigned page_hash_func(const struct hash_elem *e, void * aux UNUSED);
bool page_less_func (const struct hash_elem *a,  const struct hash_elem *b, void *aux UNUSED);

void supplemental_page_table_init (struct supplemental_page_table *spt UNUSED);

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
	/*-----------------project 3 vm-------------------*/
	list_init(&frame_table);
	//lock_init(&frame_table);
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

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. 
 * pending 중인 페이지 객체를 초기화하고 생성합니다.
   페이지를 생성하려면 직접 생성하지 말고 이 함수나 vm_alloc_page를 통해 만드세요.
   init과 aux는 첫 page fault가 발생할 때 호출된다.
 * */
/* 페이지를 할당하고 초기화하는 함수입니다. 페이지 타입, 가상 주소, 쓰기 가능 여부,
   초기화 함수, 그리고 추가 데이터를 인자로 받습니다. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	// 인지로 받은 VM_TYPE(type)이 VM_UNINIT이면 프로그램 중단. -> VM_UNINIT타입이면 안됨.
	ASSERT (VM_TYPE(type) != VM_UNINIT) //VM_UNINIT 타입이 아닌지 확인. 이는 페이지가 초기화되지 않은 상태가 아님을 확인.

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) { // va가 현재 spt에 존재하지 않으면, va가 물리주소와 매핑되지 않았으면
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		struct page *p = (struct page *)malloc(sizeof(struct page)); // 페이지 하나 생성 
		bool (*page_initializer)(struct page *, enum vm_type, void *); // 함수 포인터 변수 선언 -> vm_type에 맞는 초기화 함수를 설정해주기 위해서 아래 스위치 문
		if(VM_TYPE(type) == VM_ANON){ // VM_TYPE이 익명 페이지일 경우
			page_initializer = anon_initializer;
		}
		else if(VM_TYPE(type) == VM_FILE){ // VM_TYPE이 파일 기반 페이지일 경우
			page_initializer = file_backed_initializer;
		}
		uninit_new(p, upage, init, type, aux, page_initializer);// uninit_new 함수를 호출해 "uninit" 페이지 구조체를 생성 및 초기화. -> 새롭게 만든 페이지의 상태를 uninit으로 초기화 시켜줌.
		p->writable = writable; // 페이지 구조체의 쓰기 가능 필드를 설정

		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, p); /// 생성된 페이지를 스레드의 보조 페이지 테이블에 삽입
	}
err: // 이미 페이지가 존재하면 에러 처리
	return false;
}

/* Find VA from spt and return page. On error, return NULL. 인자로 받은 va가 spt에 있는지 찾는 함수. 인자로 받은 va가 속해있는 페이지가 해시테이블 spt에 있다면 이를 반환 */
/* 인자로 받은 spt 내에서 va를 키로 전달해서 이를 갖는 page를 리턴한다.)
	hash_find(): hash_elem을 리턴해준다. 이로부터 우리는 해당 page를 찾을 수 있음.
	해당 spt의 hash 테이블 구조체를 인자로 넣는다. 해당 해시 테이블에서 찾아야 하기 때문.
	근데 우리가 받은 건 va 뿐이다. 근데 hash_find()는 hash_elem을 인자로 받아야 하니
	dummy page 하나를 만들고 그것의 가상주소를 va로 만들고 그 다음 이 페이지의 hash_elem을 넣는다.
	*/
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) { 
	struct page *page = NULL;
	/* TODO: Fill this function. */
	page = (struct page*)malloc(sizeof(struct page));// 임시 페이지 구조체 선언
	page->va = pg_round_down(va); // 즉 페이지 오프셋이 0인 가상 페이지의 시작 주소(void*)를 반환하는 함수 -> page_start_address는 페이지의 시작주소를 나타내며, 이것은 메모리 상에서 해당 페이지가 시작하는 위치를 가리킴. 하지만 이 주소 자체가 struct page 구조체의 인스턴스를 나타내는 것은 아니며, 단지 가상 주소 공간에서의 한 지점일 뿐임.
	
	struct hash_elem* e; // 인자로 받은 va와 같은 va를 가지고 있는 페이지의 hash_elem을 찾기 위해 변수 선언.
	e = hash_find(&spt->spt_hash, &page->hash_elem); // page->hash_elem은 사실 쓰레기 값이 들어가있을것임. 하지만 우리는 쓰레기 값 자체를 활용하는게 아니라 쓰레기 값이 들어있는 통(page)를 사용하고 해당 page->va만 활용하기 때문에 상관 없음. -> 즉, 우리는 va 값만 가지고 hash_elem을 찾는것임!
	free(page); // 임시로 페이지 구조체 만든 것이므로 free해줌
	if(e == NULL){ // 해당 요소가 존재하지 않으면
		return NULL;
	}
	//해당 요소가 존재하면 해당 요소를 가지고 해당 페이지의 인스턴스 반환
	return hash_entry(e,struct page, hash_elem);
}

/* Insert PAGE into spt with validation.  spt에 page 삽입하는 함수*/
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) { // spt에 page_entry 삽입하는 함수 -> 정확히는 spt안의 해시테이블에 삽입하려고하는 page의 hash_elem을 삽입하는 것임.
	int succ = false;
	/* TODO: Fill this function. */
	if(hash_insert(&spt->spt_hash, &page->hash_elem) == NULL){  // hash_insert의 반환 값이 NULL이면 hash_elem이 버킷에 삽입된 것
		succ = true;
	}
	return succ; // 삽입이 되었으면 true를 반환할 것이고 삽입이 안되었으면 false를 반환할 것
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	int succ = false;
	if(hash_delete(&spt->spt_hash, &page->hash_elem) != NULL){ // hash_insert의 반환 값이 NULL이 아니면 삭제하고자 하는 page의 hash_elem이 버킷에서 제거 된 것! -> NULL이면 삭제하고자 하는 page의 hash_elem이 해당 해시테이블에 존재하지 않는 것임.
		vm_dealloc_page (page);
		succ = true;
	}
	return succ;
}

/* Get the struct frame, that will be evicted.메모리에서 뺼 희생자(victim)페이지를 찾고 실제로 물리 frame에서 해당 정보를 지움*/
static struct frame *
vm_get_victim (void) { // 실제로 물리 프레임에서 해당 정보를 지우는 함수.
	struct frame *victim = NULL; // 비어있는 프레임 포인터를 선언, 희생 프레임을 저장할 변수
	/* TODO: The policy for eviction is up to you. */
	struct thread *curr = thread_current(); // 현재 실행 중인 스레드를 가져옴
	struct list_elem *e; // 리스트 순회를 위한 리스트 엘리먼트 포인터를 선언
	// 리스트의 처음부터 현재 엘리먼트까지 다시 순회
	for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
		victim = list_entry(e, struct frame, frame_elem); // 현재 엘리먼트에서 frame 구조체를 가져옴
		if(victim->page == NULL) return victim; // 현재 프레임과 매핑된 페이지가 없는 경우(즉, 페이지가 파괴되었거나 할당해제된 경우) 해당 프레임을 즉시 희생자로 반환
		if (pml4_is_accessed(curr->pml4, victim->page->va)) // 현재 프레임의 페이지가 접근되었는지 확인 -> LRU 페이지 교체 알고리즘의 변형 방식임.
			pml4_set_accessed(curr->pml4, victim->page->va, 0); // 접근되었다면 접근 비트를 0으로 설정
		else
			return victim; // 접근되지 않았다면 해당 프레임을 희생 프레임으로 선택하여 반환
	}
	return victim; // 위의 모든 조건을 만족하지 못했을 경우 마지막으로 선택된 프레임을 반환
}

// /* Get the struct frame, that will be evicted. 
//   페이지 교체를 위해 희생될 물리 프레임을 선정하는 함수
// */
// static struct frame *
// vm_get_victim(void)
// {
// 	struct frame *victim = NULL; // 희생될 프레임을 저장할 포이터 선언 및 초기화
// 	/* TODO: The policy for eviction is up to you. */
// 	struct thread *curr = thread_current();

// 	lock_acquire(&frame_table_lock); // 프레인 테이블에 대한 락을 획득
// 	struct list_elem *start = list_begin(&frame_table); // 프레임 테이블 리스트의 시작부터 순회를 시작
// 	for (start; start != list_end(&frame_table); start = list_next(start))
// 	{
// 		victim = list_entry(start, struct frame, frame_elem); // 현재 순회 중인 리스트 엘리먼트로부터 frame 구조체를 가져옴.
// 		if (victim->page == NULL) // frame에 할당된 페이지가 없는 경우 (page가 destroy된 경우 ) ,현재 프레임에 할당된 페이지가 없는 경우 (페이지가 파괴되었거나 할당 해제된 경우)
// 		{
// 			lock_release(&frame_table_lock); // 프레임 테이블 락을 해제하고 희생자로 선정된 프레임을 반환
// 			return victim;
// 		}
// 		if (pml4_is_accessed(curr->pml4, victim->page->va)) // 현재 프레임의 페이지가 최근에 접근된 경우 접근 비트를 0으로 설정
// 			pml4_set_accessed(curr->pml4, victim->page->va, 0);
// 		else // 프레임이 최근에 접근되지 않은 경우, 이 프레임을 희생자로 선정
// 		{
// 			lock_release(&frame_table_lock); 
// 			return victim;
// 		}
// 	}
// 	// 리스트의 끝까지 적절한 희생자를 찾지 못한 경우, 마지막으로 검토된 프레임을 반환
// 	lock_release(&frame_table_lock);
// 	return victim;
// }

/* Evict one page and return the corresponding frame.
 * Return NULL on error.
 * vm_evict_frame()함수는 page에 달려있는 frame 공간을 디스크로 내리는 swap out을 진행하는 함수이다.
 * swap_out()함수는 매크로로 구현되어있으므로 잘 가져다가 사용하면 됨.
 * 이때 swap_out()은 page_operations 구조체에 들어있는 멤버로 bool type이다.
 */
static struct frame *
vm_evict_frame (void) { // victim페이지를 swap_out함. victim페이지를 물리 프레임에서 지우는 역할을 vm_get_victim()함수가 다해줌.
	struct frame *victim UNUSED = vm_get_victim (); //비우고자 하는 해당 프레임은 victim 임.  이 victim과 연결된 가상 페이지를 swap_out()에 인자로 넣어줌
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out(victim->page);
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.
 * vm_get_frame()함수는 물리 메모리에서 사용 가능한 프레임을 할당하거나 필요에 따라 기존 프레임을 대체(evict)하여
 * 새로운 프레임을 확보하는 역할을 수행한다.
 * 이 함수는 메모리 할당 요펑을 처리 할 때 사용자(pool)메모리 공간이 가득 찼을 경우 페이지를 쫒아내는 기능(eviction)
 * */
/* vm_get_frame함수는 사용자 모드에서 사용할 수 있는 물리 메모리 페이지를 할당하는 함수이다.
 * 망약 할당 가능한 페이지가 없으면, 페이지를 강제로 내보내(evict)서 메모리를 확보한다.
 * 이 함수는 항상 유효한 주소를 반환한다.
 * 1. 새로운 frame 구조체를 할당하고 초기화
 * 2. 사용자 모드 페이지를 할당 받음.
 * 3. 페이지 할당이 실패하면 페이지를 내보내(evict)고 다시 시도.
 * 4. 성공적으로 페이지를 할당받으면 frame_table에 추가
 */
static struct frame *
vm_get_frame (void) { // 새로운 페이지에 프레임을 할당해주기 위해 프레임을 생성하는 함수라고 생각하면 됨.
	struct frame *frame = NULL;
	frame = (struct frame*)malloc(sizeof(struct frame)); // 새로운 frame 구조체를 동적으로 할당
	/* TODO: Fill this function. */
	ASSERT (frame != NULL); // 할당된 프레임이 정상적으로 생성되었는지 ,  NULL이면 프로그램을 중단
	ASSERT (frame->page == NULL); // 연결된 페이지가 현재 NULL로 연결된 페이지가 없는지 확인,  NULL이 아니면 프로그램을 중단
	frame->kva = palloc_get_page(PAL_USER); // 사용자 모드 페이지를 할당받아서 frame->kva에 저장 user pool에서 커널 가상 주소 공간으로 1페이지를 할당 받음.
	if(frame->kva == NULL){ // 페이지 할당에 실패했을 경우
		frame = vm_evict_frame(); // 페이지를 내보내(evict)고 새로운 frame을 반환받음.
		frame->page = NULL; // frame->page 멤버를 null로 초기화
		return frame; // frame 반환
	}
	list_push_back(&frame_table,&frame->frame_elem); // frame_table에 새로운 frame을 추가합니다.
	frame->page = NULL; // frame의 page 멤버를 NULL로 설정
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	vm_alloc_page_with_initializer(VM_ANON,page_round_down(addr),1,NULL,NULL);
	// todo: 스택 크기를 증가시키기 위해 anon page를 하나 이상 할당하여 주어진 주소(addr)가 더 이상 예외 주소(faulted address)가 되지 않도록 합니다.
	// todo: 할당할 때 addr을 PGSIZE로 내림하여 처리
	vm_alloc_page(VM_ANON | VM_MARKER_0, pg_round_down(addr), 1);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
	/* 1. 유저 스택 포인터 가져오는 방법 => 이때 반드시 유저 스택 포인터여야 함! 
	모종의 이유로 인터럽트 프레임 내 rsp 주소가 커널 영역이라면 얘를 갖고 오는 게 아니라 
	thread 내에 우리가 이전에 저장해뒀던 rsp_stack(유저 스택 포인터)를 가져온다.
	그게 아니라 유저 주소를 가리키고 있다면 if(=f)->rsp를 갖고 온다.
	즉, user access인 경우 if(=f)->rsp는 유저 stack을 가리키고
	kernel access인 경우 thread에서 rsp를 가져와야한다.
	*/
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	/* 유저 가상 메모리 안의 페이지가 아니거나 NULL값이면 return false*/
	if (addr == NULL || is_kernel_vaddr(addr)) { 
		return false;
	}
	/* 페이지의 present bit이 0이면(not_present가 1이면)즉, 메모리 상에 존재하지 않으면 프레임에 메모리를 올리고 프레임과 페이지를 매핑시킨다.*/
	if(not_present){ 
		void *rsp = user ? f->rsp : thread_current()->rsp;
		if ((USER_STACK - (1 << 20) <= rsp - 8 && rsp - 8 == addr && addr <= USER_STACK) || (USER_STACK - (1 << 20) <= rsp && rsp <= addr && addr <= USER_STACK)) // 첫번째 경우 어셈블리어 PUSH 사용시 조건 어셈블리어 PUSH는 rsp-8값의 유효성 검사를 먼저 진행.
			vm_stack_growth(addr);
		page = spt_find_page(spt, addr); 
		if (page == NULL)
			return false;
		if (write == 1 && page->writable == 0) // write 불가능한 페이지에 write 요청한 경우
			return false;
		return vm_do_claim_page(page);
	}
	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. 
 * 가상 주소(va)에 대한 페이지를 요구하는 기능을 수행
 *  가상 주소를 물리적 메모리 페이지와 연결하는 역할
*/
bool
vm_claim_page (void *va UNUSED) { // 인자로 받은 va 로 해당 page를 찾아 vm_do_claim_page를 호출함.
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va); // 인자로 받은 va가 속해있는 페이지가 해시테이블 spt에 있다면 해당 페이지를 반환, 없으면 NULL을 반환
	if(page == NULL) return false;
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. 
 * vm_do_claim_page(struct page *page) 함수는 
 특정 페이지 구조체를 가리키는 포인터를 인자로 받아, 
 해당 페이지에 대한 실제 물리 메모리 페이지를 할당하고 
 시스템의 가상 주소 공간에 매핑하는 역할을 수행하는 함수이다.
*/
static bool
vm_do_claim_page (struct page *page) { // 페이지를 물리 메모리에 매핑하고 필요하다면 스왑인을 수행
	struct frame *frame = vm_get_frame (); // 새로운 프레임을 할당 받음.(새로운 프레임 생성) -> 해당 프레임을 가상 페이지와 물리적으로 매핑될 대상임.

	/* Set links 프레임과 페이지의 링크 설정 -> 할당 받은 프레임과 페이지 구조체 간의 연결을 하여 두 구조체가 서로를 참조할 수 있도록 한다.*/
	frame->page = page; // 프레임의 page포인터를 현재 페이지로 설정
	page->frame = frame; // 페이지의 frame 포인터를 현재 프레임으로 설정

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *t = thread_current();
	if(pml4_get_page(t->pml4, page->va) == NULL){ // 주어진 가상 주소 va에 이미 페이지가 매핑되어있는지 확인하고 매핑되어있지 않다면(NULL)이라면 새로운 프레임을 해당 페이지와 매핑
		pml4_set_page(t->pml4, page->va, frame->kva, page->writable); // 가상주소va와 물리주서kva를 매핑하는 역할을 수행
		return swap_in(page, frame->kva); // 매핑이 끝나면 스왑인을 진행
	}
	//실패하면 false 반환
	return false;
}

/* Initialize new supplemental page table spt를 초기화 하는 함수 각 페이지 별로 spt를 만드려면 먼저 초기화 작업을 진행해주어야함.*/
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_hash,page_hash_func,page_less_func,NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
/*----------------------projecr3 func들---------------------*/
unsigned page_hash_func(const struct hash_elem *e, void * aux UNUSED){
	struct page *page = hash_entry(e, struct page, hash_elem); // hash_entry()함수는 해당 hash_elem이 들어있는 page의 주소를 반환. ->  해당 hash_elem을 멤버로 갖고 있는 페이지 반환
	return hash_int(page->va);  //hash_bytes(&page->va,sizeof(page->va)); // hash_bytes()함수는 해시 함수를 이용해 buf 메모리 공간에 있는 정수를 size 만큼 암호화 시킨후 해당 암호화된 해시값(해시테이블 인덱스 값)을 반환
}
/*해시 테이블 내 두 페이지 요소에 대해 페이지의 주소 값을 비교하는 함수 -> 두번째 인자 페이지 주소 > 첫 번째 인자 페이지 주소 인지 check */
bool page_less_func (const struct hash_elem *a,  const struct hash_elem *b, void *aux UNUSED){ // b를 요소로 가진 테이블이 a를 요소로 가진 테이블 보다 크다면 true 반환 
	const struct page *a_page = hash_entry(a,struct page, hash_elem);
	const struct page *b_page = hash_entry(b,struct page, hash_elem);
	return a_page->va < b_page->va;
}



