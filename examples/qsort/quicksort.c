#include <klee/klee.h>

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>

int partition(int *a, int l, int r) {
	int rand_key = (l+r)/2;
	int key = a[rand_key];
	a[rand_key]=a[l];
	a[l]=key;
	int head=l, tail=r;
	while ( head < tail ) {
		while ( head < tail && a[tail] >= key ) --tail;
		a[head]=a[tail];
		while ( head < tail && a[head] <= key ) ++head;
		a[tail]=a[head];
	}
	a[head]=key;
	return head;
}

void quicksort(int *a, int l, int r) {
	if ( l < r ) {
		int mid=partition(a, l ,r);
		quicksort(a,l, mid-1);
		quicksort(a,mid+1, r);
	}
}

#define SIZE 100
int main(){
	int *a=malloc(SIZE*sizeof(a[0]));
	for (unsigned int i=0; i < SIZE; ++i) {
		scanf("%d", a+i);
	}
#ifdef KLEE_SYMBOLIC
	klee_make_symbolic(a, SIZE*sizeof(a[0]), "input");
#endif
	quicksort(a,0,SIZE-1);
	for ( int j=0; j < SIZE-1; ++j ) {
		klee_assert(a[j] < a[j+1]);
	}
	return 0;
}
