/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */

#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops, // 아래 코드들이 다 UNINIT 구조체 내부로 바뀜
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* Initalize the page on first fault 
  아래 함수는 페이지 폴트가 처음 발생했을 때, 초기화되지 않은 페이지를 초기화하는 역할을 함.
*/
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit; // uninit_page 구조체를 통해 페이지 정보에 접근

	/* Fetch first, page_initialize may overwrite the values */
	 /* 초기화 함수와 추가 데이터를 가져옴. 이 값들은 페이지 초기화에 사용될 수 있음.
       페이지 초기화 함수가 이 값들을 덮어쓸 수 있기 때문에, 먼저 값을 복사해둠. */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	/* TODO: You may need to fix this function. */
	/* 페이지 초기화를 수행합니다. uninit->page_initializer 함수는 페이지 타입과 가상 주소를
       기반으로 페이지를 물리적 메모리에 초기화합니다. 이 함수는 초기화 성공 여부를 bool로 반환합니다.
       그리고 init 함수가 존재할 경우, 이 함수도 호출하여 추가적인 사용자 정의 초기화를 수행합니다.
       init 함수가 없다면 true를 반환하여 초기화가 성공적이라고 간주합니다. */
	return uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true);
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit UNUSED = &page->uninit;
	/* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
}
