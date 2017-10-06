#ifndef FLIRONECONTROL_H
#define FLIRONECONTROL_H

#ifndef bool 
#define bool unsigned char
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#define IMAGE_BUFFER_SIZE 45000

//исходные размеры изображения (не перевёрнутое)
#define ORIGINAL_IMAGE_WIDTH 160
#define ORIGINAL_IMAGE_HEIGHT 120

//заголовок данных тепловизора
#pragma pack(1) 
typedef struct 
{
 unsigned char MagicByte[4];//магические байты
 unsigned char Unknow1[4];//неизвестные данные
 unsigned long FrameSize;//размер данных
 unsigned long ThermalSize;//размер кадра тепловизора
 unsigned long JpgSize;//размер изображения с камеры
 unsigned long StatusSize;//размер слова состояния 	 
 unsigned char Unknow2[4];//неизвестные данные
} SHeader;
#pragma pack()

//модуль управления Flir One
typedef struct 
{
 unsigned char ImageBuffer[IMAGE_BUFFER_SIZE];//буфер сборки пакета данных с изображением 
 unsigned short ThermalImage[ORIGINAL_IMAGE_WIDTH*ORIGINAL_IMAGE_HEIGHT];//буфер теплового изображения
	
 unsigned short ColorMap[256];//палитра
 
 unsigned long Head;//голова в буфере
 unsigned long Tail;//хвост в буфере
 unsigned char MagicPos;//позиция анализируемого магического байта
 unsigned long MagicHeadPos;//позиция головы, когда встречился первый магический байт
	
 SHeader sHeader;//заголовок пакета
 bool HeaderIsReceived;//заголовок принят
} SFlirOneControl;

void FLIR_ONE_Init(void);//инициализация
void FLIR_ONE_CalculateCRC(unsigned short *crc,unsigned char byte);//вычислить crc
bool FLIR_ONE_CreateImage(unsigned char *buffer,unsigned long size);//создать изображение

#endif
