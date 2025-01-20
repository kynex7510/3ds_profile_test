#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

void initStuff() {
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
}

void busyLoop() {
	int num = 0x10000000;
	volatile int* p = &num;
	while (*p)
		*p = *p - 1;
}

void finalizeStuff() {
	gfxExit();
}

void mainLoop() {
	while (aptMainLoop()) {
		gspWaitForVBlank();
		gfxSwapBuffers();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break;
	}
}

int main(int argc, char* argv[]) {
	initStuff();
	printf("Preparing...\n");
	busyLoop();
	printf("Hello, world!\n");
	mainLoop();
	finalizeStuff();
	return 0;
}
