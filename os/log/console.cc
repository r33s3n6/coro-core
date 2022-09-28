#include "console.h"
#include "rustsbi/sbi.h"

void consputc(int c)
{
	console_putchar(c);
}

void console_init()
{
	// DO NOTHING
}

int consgetc()
{
	return console_getchar();
}