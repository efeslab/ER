#include <stdio.h>
#include <unistd.h>
/*
 * This tool is to convert ascii integer to binary integer.
 * Since klee can not handle symbolic scanf, we should use read wrapper instead of scanf.
 * That limits the flexibility of input format.
 * Normally you should read integer from stdin (fd=0) directly.
 * You can use this tool to generate integer sequence in binary format from
 *   human-readable plain text.
 *
 */
int main() {
	int n;
	while (scanf("%d", &n) != EOF) {
		write(1, &n, sizeof(n));
	}
}
