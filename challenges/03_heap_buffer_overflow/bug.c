/*
 * Challenge 03 — Heap Buffer Overflow (심화: 동적 배열 성장 버그)
 *
 * [시나리오]
 *   자동 성장하는 정수 동적 배열 IntList (init/ensure/push/sum). 용량이 부족하면
 *   list_ensure() 가 용량을 2배로 늘리고 realloc 한다. 이 리스트로 큰 수열을
 *   만들어 합을 구한다.
 *
 * [기대 동작]
 *   0..N-1 을 100 으로 나눈 나머지를 리스트에 넣고, 길이·용량·합을 출력한 뒤 정상 종료.
 *
 * [증상]
 *   list_ensure() 가 새 용량(newcap)을 계산해 l->cap 에는 반영하지만,
 *   정작 realloc 은 "옛 용량(l->cap)" 으로 호출한다. 즉 논리 용량(cap)은 커지는데
 *   실제 버퍼는 한 세대 뒤처져, push 가 실제 버퍼 밖으로 계속 쓴다.
 *   힙 경계를 넘어 쓰면서 힙 메타데이터가 깨지거나(→ 이후 realloc/free 에서 SIGABRT)
 *   매핑되지 않은 페이지까지 밀고 나가 SIGSEGV. 크래시는 push 의 대입 지점 또는
 *   다음 realloc 에서 나지만, 원인은 ensure 의 realloc 인자다.
 *
 * [gdb 로 잡기]
 *   make gdb NAME=03_heap_buffer_overflow
 *   (gdb) run                         → 크래시(SIGSEGV) 또는 abort
 *   (gdb) bt                          → list_push 의 l->data[l->len]=x 또는 realloc 내부
 *   (gdb) frame N ; print *l           → cap 은 큰데 실제 버퍼는 그보다 작음(불일치)
 *   (gdb) print l->len  / print l->cap → len 이 실제 확보량을 넘어섰는지 확인
 *   (gdb) break list_ensure           → newcap 과 realloc 에 넘기는 크기를 대조
 *
 * [printf(로그)로 잡기]
 *   ensure 에서 (old cap, newcap, realloc 에 넘기는 크기) 를 함께 찍어 불일치를 본다:
 *     fprintf(stderr, "ensure old=%zu new=%zu realloc_bytes=%zu\n",
 *             l->cap, newcap, l->cap * sizeof(int));
 *   → newcap 과 realloc 크기가 다르면 그게 원인.
 *   (stdout 은 버퍼링되니 stderr 로 찍어야 크래시 직전 로그가 남는다)
 *
 * TODO: realloc 은 반드시 "새 용량(newcap)" 으로 호출하고, l->cap 갱신과 순서를 맞춰야 한다.
 *       (성장 로직은 '용량 필드'와 '실제 확보량'이 항상 같도록 유지해야 한다)
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    int *data;
    /* [Thinking Point]
     * 개수/크기를 담는 len, cap 을 왜 int 가 아니라 size_t 로 선언할까?
     *   tip 1. size_t 는 "이 플랫폼에서 표현 가능한 가장 큰 객체 크기"를 담도록 만든
     *          부호 없는(unsigned) 정수 타입이다. malloc/sizeof/strlen 의 타입도 size_t 다.
     *   tip 2. int 는 보통 32비트라 약 21억(2^31-1)에서 넘치고, 음수도 가능하다.
     *          원소가 그보다 많아지거나 cap*sizeof(int) 계산이 커지면 int 는 오버플로된다.
     *   생각해보기: 크기를 int 로 두면 어떤 버그가 생길 수 있을까?
     */
    size_t len;
    size_t cap;
} IntList;

static void list_init(IntList *l)
{
    l->cap = 8;
    l->len = 0;
    l->data = malloc(l->cap * sizeof(int));
    if (!l->data)
    {
        perror("malloc");
        exit(1);
    }
}

static void list_ensure(IntList *l, size_t need) 
{
    if (need <= l->cap)
        return;

    size_t newcap = l->cap ? l->cap * 2 : 8; // lcap이 16이라서  newcap이 32,,
    while (newcap < need)
        newcap *= 2;
/*
      realloc은 새 크기에 맞춰 메모리를 할당할 때 성능 최적화를 위해 두 가지 방식으로 동작합니다.
      동일 위치 확장 (In-place Allocation): 기존 메모리 블록의 뒤쪽에 연속된 여유 공간이 충분하다면,
      주소를 바꾸지 않고 기존 주소 그대로 크기만 늘립니다. 성능상 가장 이상적입니다.
      새로운 위치 이동 (New Allocation): 뒤쪽에 연속된 공간이 부족하면, 
      새로운 메모리 공간을 찾아 전체를 새로 할당합니다. 그 후 기존 데이터를 새 공간으로 자동 복사하고, 
      기존 메모리는 자동으로 해제(free)합니다.
      */
    int *p = realloc(l->data, newcap * sizeof(int)); // -> l-cap 이 아니라, newcap으로 바꿨다.

    // 1.용량이 작게 됐다면 realloc이 아니라 대입에서 문제 생기지 않았을지?

    // realloc 문제 나는 경우 : 할당 범위 외 침범시 - 메타데이터 덧씌워짐,, 무결성 오류, : 침범시 감지하지 않음.
    // realloc이나 free 때 발생함.
    // 넘겨준 시작 주소가 정확하지 않은 경우 
    // size가 오류 있는 경우 
    // free 된 상태인데 realloc 시도 경우 
    // heap 외 주소를 전달한 경우 


    // -> break list_ensure로 가장 처음 realloc 확인함. - 그 전 realloc이 8일 때 발생, 
    // 2. 아니면 그 전 realloc에서 문제 됐을수도 
    // realloc시에도 lcap이었으므로 사이즈가 늘지 않음
    // 그런데 l->cap 변수만 newcap으로 업데이트 되었으므로 실제 힙 크기와 불일치.
    // ensure 전까지 어떤 alloc도 없고 대입만 하므로 오류 없었다.
    if (!p)
    {
        perror("realloc");
        free(l->data);
        exit(1);
    } // crash시 p는 이미 주소 있다.
      
    l->data = p;
    l->cap = newcap;
}

static void list_push(IntList *l, int x)
{
    if (l->len == l->cap)
        list_ensure(l, l->cap + 1); // len이 cap에 닿았을 때 - len ++ 위해 reallc?
    l->data[l->len++] = x;
}

static long long list_sum(const IntList *l)
{
    long long s = 0;
    for (size_t i = 0; i < l->len; i++)
        s += l->data[i];
    return s;
}

static void list_free(IntList *l)
{
    free(l->data);
    l->data = NULL;
    l->len = l->cap = 0;
}

int main(void)
{
    IntList l;
    list_init(&l);

    const int N = 2000000;
    for (int i = 0; i < N; i++)
    { // i가 16일 때  list_ensure (l=0x7fffffffdde0, need=17) at challenges/03_heap_buffer_overflow/bug.c:68
        list_push(&l, i % 100);
    }

    printf("len=%zu cap=%zu sum=%lld\n", l.len, l.cap, list_sum(&l));
    list_free(&l);
    return 0;
}
