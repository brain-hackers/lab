#include "the_rookie.c"
// hardware frame buffer size
#define SCREEN_H 480
#define SCREEN_V 640

typedef void (*cache_invalidate_proc)(void *ptr,unsigned int size);
typedef void (*sh_ExitApp_proc)(char *path);

void cpy_rotate(unsigned short *dst,unsigned short *src);

typedef struct{
	void *reserved;
	char *exec_path;
	void *pPresent;
	void *reserved2;
} app_ctx;

 __attribute__((section(".text.init"))) void main(app_ctx *ctx){
	volatile char for_sleep=0;
	sh_ExitApp_proc sh_ExitApp=(sh_ExitApp_proc)0x840e7a48;
	cache_invalidate_proc cache_invalidate=(cache_invalidate_proc)0x600063d4;
	volatile unsigned char *framebuffer = (unsigned char *)0x60a00008;
	cpy_rotate((unsigned short *)framebuffer,(unsigned short*) GIMP_IMAGE_pixel_data);
	cache_invalidate(0x60a00008,0x16c800);
	*(unsigned int *)0x40aa0004 = 1;
	for(int i=0;i<0x10000000;i++){
		for_sleep++;
	}
	sh_ExitApp(ctx->exec_path);
	return;
}

void cpy_rotate(unsigned short *dst,unsigned short *src){
	for (int y = 0; y < 480; y++){
		for (int x = 0; x < 640; x++){
			dst[x * 480 + (479 - y)] = src[y * 640 + x];
		}
	}
}
