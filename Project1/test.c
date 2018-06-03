#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
	char *a;
	char b[] = "abcdeijwero";
	a = (char *)malloc(strlen(b) + 1);
	strcpy(a, b);
	printf("%s\n", a);



	return 0;
}