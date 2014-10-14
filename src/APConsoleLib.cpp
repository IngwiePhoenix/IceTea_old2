/* APConsoleLib.c
 * Library for portable (win & linux) console functions with 16 colors
 * Copyright (C) 2008 Anton Pirogov
 */
#include "APConsoleLib.h"

#ifdef WIN32    /* Windows version */

void clearConsole(void)
{
	COORD coordScreen = {0, 0}; /* upper left corner */
	DWORD cCharsWritten;
	DWORD dwConSize;
	HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hCon, &csbi);
	dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
	/* fill with spaces */
	FillConsoleOutputCharacter(hCon, TEXT(' '), dwConSize, coordScreen, &cCharsWritten);
	GetConsoleScreenBufferInfo(hCon, &csbi);
	FillConsoleOutputAttribute(hCon, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
	/* cursor to upper left corner */
	SetConsoleCursorPosition(hCon, coordScreen);
}

void consoleGotoXY(short x, short y)
{
	COORD coord = {x, y};
	HANDLE StdOutHwnd = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleCursorPosition(StdOutHwnd, coord);
}

void setConsoleColor(short clr)
{
	HANDLE StdOutHwnd = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD color;

	switch (clr) {
	case BLACK: color = 0;
		break;
	case RED: color = 4;
		break;
	case GREEN: color = 2;
		break;
	case DARK_YELLOW: color = 6;
		break;
	case BLUE: color = 1;
		break;
	case PURPLE: color = 5;
		break;
	case CYAN: color = 3;
		break;
	case GRAY: color = 7;
		break;
	case DARK_GRAY: color = 8;
		break;
	case LIGHT_RED: color = 12;
		break;
	case LIGHT_GREEN: color = 10;
		break;
	case YELLOW: color = 14;
		break;
	case LIGHT_BLUE: color = 9;
		break;
	case LIGHT_PURPLE: color = 13;
		break;
	case LIGHT_CYAN: color = 11;
		break;
	case WHITE: color = 15;
		break;
	}

	SetConsoleTextAttribute(StdOutHwnd, color);
}

void setConsoleTitle(char* title)
{
	SetConsoleTitle(title);
}

void setConsoleSize(short x, short y)
{
	HANDLE wHnd = GetStdHandle(STD_OUTPUT_HANDLE);
	SMALL_RECT windowSize = {0, 0, x - 1, y - 1};
	SetConsoleWindowInfo(wHnd, TRUE, &windowSize);
	COORD bufferSize = {x, y};
	SetConsoleScreenBufferSize(wHnd, bufferSize);
}

void getConsoleSize(short *x, short *y)
{
	HANDLE hOut;
	CONSOLE_SCREEN_BUFFER_INFO SBInfo;

	hOut = GetStdHandle(STD_OUTPUT_HANDLE);

	GetConsoleScreenBufferInfo(hOut, &SBInfo);

	*x = SBInfo.dwSize.X;
	*y = SBInfo.dwSize.Y;
}

#else   /* Linux version */

int getch(void)
{
	static int ch = -1, fd = 0;
	struct termios neu, alt;
	fd = fileno(stdin);
	tcgetattr(fd, &alt);
	neu = alt;
	neu.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(fd, TCSANOW, &neu);
	ch = getchar();
	tcsetattr(fd, TCSANOW, &alt);
	return ch;
}

int kbhit(void)
{
	struct termios term, oterm;
	int fd = 0;
	int c = 0;
	tcgetattr(fd, &oterm);
	memcpy(&term, &oterm, sizeof (term));
	term.c_lflag = term.c_lflag & (!ICANON);
	term.c_cc[VMIN] = 0;
	term.c_cc[VTIME] = 1;
	tcsetattr(fd, TCSANOW, &term);
	c = getchar();
	tcsetattr(fd, TCSANOW, &oterm);
	if (c != -1)
		ungetc(c, stdin);
	return ((c != -1) ? 1 : 0);
}

void clearConsole(void)
{
	/*char a[80];*/
	printf("\033[2J"); /* Clear the entire screen.		*/
	printf("\033[0;0f"); /* Move cursor to the top left hand corner */
}

void consoleGotoXY(short x, short y)
{
	printf("\033[%i;%if", y, x);
}

void setConsoleColor(short clr)
{
	switch (clr) {
	case BLACK: printf("\033[0;30m");
		break;
	case RED: printf("\033[0;31m");
		break;
	case GREEN: printf("\033[0;32m");
		break;
	case DARK_YELLOW: printf("\033[0;33m");
		break;
	case BLUE: printf("\033[0;34m");
		break;
	case PURPLE: printf("\033[0;35m");
		break;
	case CYAN: printf("\033[0;36m");
		break;
	case GRAY: printf("\033[0;37m");
		break;
	case DARK_GRAY: printf("\033[1;30m");
		break;
	case LIGHT_RED: printf("\033[1;31m");
		break;
	case LIGHT_GREEN: printf("\033[1;32m");
		break;
	case YELLOW: printf("\033[1;33m");
		break;
	case LIGHT_BLUE: printf("\033[1;34m");
		break;
	case LIGHT_PURPLE: printf("\033[1;35m");
		break;
	case LIGHT_CYAN: printf("\033[1;36m");
		break;
	case WHITE: printf("\033[1;37m");
		break;
	}
}

void setConsoleTitle(char* title)
{
	printf("\033]0;%s\007", title);
}

void setConsoleSize(short xsize, short ysize)
{
	char rcmd[32];
	sprintf(rcmd, "resize -s %i %i > /dev/null", ysize, xsize);
	system(rcmd);
}

void getConsoleSize(short *xsize, short *ysize)
{
	FILE *pipe = popen("stty size", "r");
	fscanf(pipe, "%hi%hi", ysize, xsize);
	pclose(pipe);
}

#endif
