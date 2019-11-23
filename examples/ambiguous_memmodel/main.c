#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
/*
 * This contrived example shows ambiguous memory access could happen in klee.
 * Klee uses AddressSpace class to reason about the mapping between arbitrary
 *   symbolic address to allocated memory objects.
 * By symbolic replay, you can see "pointer_v[row][col] = v" triggers various
 *   special memory operation cases.
 */
#define READ0(x) read(0, &(x), sizeof(x))
int main() {
	int rows=4, cols=4;
	int **pointer_v = (int**)malloc(rows*sizeof(int*));
	for (int i=0; i < rows; ++i) {
		pointer_v[i] = (int*)malloc(cols*sizeof(int));
		memset(pointer_v[i], 0, cols*sizeof(int));
	}
	int n;
	READ0(n);
	for (int i=0; i < n; ++i) {
		int row,col,v;
		READ0(row);
		READ0(col);
		READ0(v);
		if (row >= 0 && row < rows &&
		    col >= 0 && col < cols) {
			pointer_v[row][col] = v;
		}
	}
	for (int i=0; i < rows; ++i) {
		for (int j=0; j < cols; ++j) {
			printf("%d ", pointer_v[i][j]);
		}
		putchar('\n');
	}
}
