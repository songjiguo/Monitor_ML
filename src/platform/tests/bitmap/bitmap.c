#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

#define LINUX_TEST

#define WORD_SIZE 32
#include <bitmap.h>
#define SIZE 1024
#define EXTENT 1
#define ITER 1026

struct test_bitmap {
	u32_t bm[SIZE];
};

int main(void)
{
	struct test_bitmap *tbm;
	u32_t *x;
	
	tbm = malloc(sizeof(struct test_bitmap));
	if (!tbm) goto err;

	x = &tbm->bm[0];
	
	bitmap_set_contig(x, 0, SIZE, 1);
	/* bitmap_set_contig(x, 2, 50, 0); */
	print_bitmap(x, SIZE/WORD_SIZE);

	int ret, extent, i;
	extent = EXTENT;
	for (i = 0; i < ITER; i++) {
		ret = bitmap_extent_find_set(x, 0, extent, SIZE);
		printf("%d ",ret);
		/* if (ret < 0) bitmap_set_contig(x, 0, SIZE, 1); */
	}
	printf("\n");
	print_bitmap(x, SIZE/WORD_SIZE);

done:
	return 0;
err:
	printf("fault happened\n");
	goto done;
}
