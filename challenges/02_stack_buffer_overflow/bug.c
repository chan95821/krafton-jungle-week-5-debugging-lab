/*
 *
 */
#include <stdio.h>
#include <stdlib.h>

#define ROWS 14
enum { SIZE = ROWS * (ROWS + 1) / 2  };   /* 0..ROWS-1 행을 담는 정확한 크기 */

/* 행 i, 열 j 의 삼각 인덱스 */
static int tri_index(int i, int j) {
    return i * (i + 1) / 2 + j;
}

/* 파스칼의 삼각형을 tri[] 에 채운다. */
static void build_pascal(int *tri, int rows) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j <= i; j++) { 
            int idx = tri_index(i, j); // tri_index가 최대 rows, rows 인데 size가 rows(rows+1) / 2 까지밖에 없다. -> 그래서 rows + 1 더 크게 해줬다.
            /// -> i <= rows 를 i < rows로 제한함 . SIZE 범위 내
            if (j == 0 || j == i) {
                tri[idx] = 1;                         /* 양 끝은 1 */
            } else {
                int up_left  = tri_index(i - 1, j - 1);
                int up_right = tri_index(i - 1, j);
                tri[idx] = tri[up_left] + tri[up_right];
            }
        }
    }
}

static long row_sum(const int *tri, int i) {
    long sum = 0;
    for (int j = 0; j <= i; j++) sum += tri[tri_index(i, j)];
    return sum;
}

static void print_row(const int *tri, int i) {
    printf("row %2d:", i);
    for (int j = 0; j <= i; j++) printf(" %d", tri[tri_index(i, j)]);
    printf("   (sum=%ld)\n", row_sum(tri, i));
}

int main(void) {
    int tri[SIZE];

    build_pascal(tri, ROWS);          

    for (int i = 0; i < ROWS; i++) print_row(tri, i); 

    printf("SIZE = %d\n", SIZE);

  
    return 0; // stack smash 판단 시점 : 함수가 return 할 때,, 그래서 terminate
    // stack smash
}
