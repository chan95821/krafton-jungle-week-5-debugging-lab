#include <stdlib.h>
#include <stdio.h>

int main(void)
{
    char *p = malloc(24);
    printf("%p\n", (void *)p);

    getchar(); // 여기서 GDB로 관찰
    free(p);
}