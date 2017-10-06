#ifndef ST7735S_H
#define ST7735S_H

#include "stm32f4xx_hal.h"

//цвета
#define ST7735S_RED  	0xf800
#define ST7735S_GREEN	0x07e0
#define ST7735S_BLUE 	0x001f
#define ST7735S_WHITE	0xffff
#define ST7735S_BLACK	0x0000
#define ST7735S_YELLOW  0xFFE0
#define ST7735S_GRAY0   0xEF7D   	
#define ST7735S_GRAY1   0x8410      	
#define ST7735S_GRAY2   0x4208

#define FONT_HEIGHT 14
#define FONT_WIDTH 8
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 128

#ifndef bool 
#define bool unsigned char
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

void ST7735S_Init(void);//инициализация дисплея
void ST7735S_SetArea(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2);//задать область вывода графики
void ST7735S_OutColor(unsigned short color);//передать цвет точки
void ST7735S_PutPixel(unsigned char x, unsigned char y, unsigned short color);//вывести один пиксель
void ST7735S_PutSymbol(long x,long y,char symbol,unsigned short color);//вывод символа в позицию
void ST7735S_PutString(long x,long y,const char *string,unsigned short color);//вывод строчки в позицию
void ST7735S_Print(const char *string,unsigned short color);//вывести текст в текущую позицию
void ST7735S_Clear(void);//сбросить текущую позицию
#endif

