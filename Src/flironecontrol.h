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

//�������� ������� ����������� (�� �����������)
#define ORIGINAL_IMAGE_WIDTH 160
#define ORIGINAL_IMAGE_HEIGHT 120

//��������� ������ �����������
#pragma pack(1) 
typedef struct 
{
 unsigned char MagicByte[4];//���������� �����
 unsigned char Unknow1[4];//����������� ������
 unsigned long FrameSize;//������ ������
 unsigned long ThermalSize;//������ ����� �����������
 unsigned long JpgSize;//������ ����������� � ������
 unsigned long StatusSize;//������ ����� ��������� 	 
 unsigned char Unknow2[4];//����������� ������
} SHeader;
#pragma pack()

//������ ���������� Flir One
typedef struct 
{
 unsigned char ImageBuffer[IMAGE_BUFFER_SIZE];//����� ������ ������ ������ � ������������ 
 unsigned short ThermalImage[ORIGINAL_IMAGE_WIDTH*ORIGINAL_IMAGE_HEIGHT];//����� ��������� �����������
	
 unsigned short ColorMap[256];//�������
 
 unsigned long Head;//������ � ������
 unsigned long Tail;//����� � ������
 unsigned char MagicPos;//������� �������������� ����������� �����
 unsigned long MagicHeadPos;//������� ������, ����� ���������� ������ ���������� ����
	
 SHeader sHeader;//��������� ������
 bool HeaderIsReceived;//��������� ������
} SFlirOneControl;

void FLIR_ONE_Init(void);//�������������
void FLIR_ONE_CalculateCRC(unsigned short *crc,unsigned char byte);//��������� crc
bool FLIR_ONE_CreateImage(unsigned char *buffer,unsigned long size);//������� �����������

#endif
