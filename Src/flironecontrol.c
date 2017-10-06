#include "flironecontrol.h"
#include "hx8347d.h"

static SFlirOneControl sFlirOneControl;
uint32_t FrameTickStart;//������ ��������� �����

//-������� ������------------------------------------------------------------

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//�������� ������� ������
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


//----------------------------------------------------------------------------------------------------
//�������������
//----------------------------------------------------------------------------------------------------
void FLIR_ONE_Init(void)
{ 
 sFlirOneControl.Head=0;
 sFlirOneControl.Tail=0;
 sFlirOneControl.MagicPos=0;
 sFlirOneControl.MagicHeadPos=0;
 sFlirOneControl.HeaderIsReceived=false;
 //�������� �������
 for(long n=0;n<256;n++)
 {	
  unsigned short color=n>>3;
  color<<=6;
	color|=n>>2;
	color<<=5;
	color|=n>>3;
  sFlirOneControl.ColorMap[n]=color;
 }
}

//----------------------------------------------------------------------------------------------------
//��������� crc
//----------------------------------------------------------------------------------------------------
void FLIR_ONE_CalculateCRC(unsigned short *crc,unsigned char byte)
{
 (*crc)^=(byte<<8);
 for(unsigned char n=0;n<8;n++) 
 { 
  if ((*crc)&0x8000) *crc=((*crc)<<1)^0x1021;
                else (*crc)<<=1;
 }
}
//----------------------------------------------------------------------------------------------------
//�������� ���� � ��������� �����
//----------------------------------------------------------------------------------------------------
void FLIR_ONE_PushByte(unsigned char b)
{
 sFlirOneControl.ImageBuffer[sFlirOneControl.Head]=b;
 sFlirOneControl.Head++;	
 if (sFlirOneControl.Head==IMAGE_BUFFER_SIZE) sFlirOneControl.Head=0;
 if (sFlirOneControl.Head==sFlirOneControl.Tail) sFlirOneControl.Tail++;
 if (sFlirOneControl.Tail==IMAGE_BUFFER_SIZE) sFlirOneControl.Tail=0;	
}
//----------------------------------------------------------------------------------------------------
//������� ���� �� ���������� ������
//----------------------------------------------------------------------------------------------------
bool FLIR_ONE_PopByte(unsigned char *b)
{
 if (sFlirOneControl.Head==sFlirOneControl.Tail) return(false);
 *b=sFlirOneControl.ImageBuffer[sFlirOneControl.Tail];
 sFlirOneControl.Tail++;
 if (sFlirOneControl.Tail==IMAGE_BUFFER_SIZE) sFlirOneControl.Tail=0;
 return(true);	
}

//----------------------------------------------------------------------------------------------------
//������� ��� ����� �� ���������� ������
//----------------------------------------------------------------------------------------------------
bool FLIR_ONE_PopShort(unsigned short *s)
{
 if (FLIR_ONE_PopByte((unsigned char*)(s))==false) return(false);
 if (FLIR_ONE_PopByte((unsigned char*)(s)+1)==false) return(false);
 return(true);	
}


//----------------------------------------------------------------------------------------------------
//�������� ������� ���� � ��������� ������
//----------------------------------------------------------------------------------------------------
unsigned long FLIR_ONE_GetBufferSize(void)
{
 if (sFlirOneControl.Head<sFlirOneControl.Tail) return(IMAGE_BUFFER_SIZE-sFlirOneControl.Tail+sFlirOneControl.Head);
 return(sFlirOneControl.Head-sFlirOneControl.Tail);
}



//----------------------------------------------------------------------------------------------------
//������� �����������
//----------------------------------------------------------------------------------------------------
bool FLIR_ONE_CreateImage(unsigned char *buffer,unsigned long size)
{
 unsigned long n;
 long x;
 long y;
 
 //���� � �������� ������ ����������� 4 ����������� �����, �� �������� ������ ������
 static unsigned char magic_byte[4]={0xEF,0xBE,0x00,0x00};  
 //��������� �����
 for(n=0;n<size;n++)
 {
  unsigned char b=buffer[n];	
	if (b==magic_byte[sFlirOneControl.MagicPos])
	{
   if (sFlirOneControl.MagicPos==0) sFlirOneControl.MagicHeadPos=sFlirOneControl.Head;		
   sFlirOneControl.MagicPos++;
   if (sFlirOneControl.MagicPos==4)
	 {
    sFlirOneControl.MagicPos=0;
		sFlirOneControl.HeaderIsReceived=false;
    sFlirOneControl.Tail=sFlirOneControl.MagicHeadPos;//��������� ����� � ������
	 }
	}
	else sFlirOneControl.MagicPos=0;
	FLIR_ONE_PushByte(b);
 }
 //������, ������� ������ � ������
 long length=FLIR_ONE_GetBufferSize(); 
 //������� ���������, ���, ����� �� ��������� ������ � ������
 if (length<sizeof(SHeader)) return(false);//��������� �� ������ 
 if (sFlirOneControl.HeaderIsReceived==false)//�������� ���������
 {
  unsigned char *sheader_ptr=(unsigned char*)(&sFlirOneControl.sHeader); 
  unsigned long tail=sFlirOneControl.Tail;
  for(n=0;n<sizeof(SHeader);n++,sheader_ptr++)
  {
   if (sFlirOneControl.Head==tail) return(false);//������ ������-�� �� �������
   *sheader_ptr=sFlirOneControl.ImageBuffer[tail];
   tail++;
   if (tail==IMAGE_BUFFER_SIZE) tail=0;  	 
  }
  if (sFlirOneControl.sHeader.MagicByte[0]!=magic_byte[0]) return(false);
  if (sFlirOneControl.sHeader.MagicByte[1]!=magic_byte[1]) return(false); 
  if (sFlirOneControl.sHeader.MagicByte[2]!=magic_byte[2]) return(false); 
  if (sFlirOneControl.sHeader.MagicByte[3]!=magic_byte[3]) return(false);  
	sFlirOneControl.HeaderIsReceived=true;
 }
 if (sFlirOneControl.sHeader.ThermalSize+sizeof(SHeader)>length) return(false);//���� ��� �� ������
 //���� ������, ���������� ��������� �����
 unsigned char b; 
 for(n=0;n<sizeof(SHeader);n++) FLIR_ONE_PopByte(&b); 
 
 //���� ������
 //������ �������� ����������� (164 ����� � ������)
 long min=0x10000;
 long max=0x0; 
 bool enabled=true;
 
 for(y=0;y<ORIGINAL_IMAGE_HEIGHT;y++) 
 {  
  unsigned short crc=0;
  unsigned short id;
  unsigned short crc_package;
  //������ ���������
  FLIR_ONE_PopShort(&id);
  id&=0x0FFF;
  FLIR_ONE_CalculateCRC(&crc,(id>>8)&0xFF);
  FLIR_ONE_CalculateCRC(&crc,id&0xFF);
  FLIR_ONE_PopShort(&crc_package);
  FLIR_ONE_CalculateCRC(&crc,0);
  FLIR_ONE_CalculateCRC(&crc,0);
  for(x=0;x<ORIGINAL_IMAGE_WIDTH/2;x++)
  {   
   unsigned short value;
   FLIR_ONE_PopShort(&value);		
		
   unsigned long offset=y+(ORIGINAL_IMAGE_WIDTH-1-x)*ORIGINAL_IMAGE_HEIGHT;
   sFlirOneControl.ThermalImage[offset]=value;
   //������� CRC
   FLIR_ONE_CalculateCRC(&crc,(value>>8)&0xff);
   FLIR_ONE_CalculateCRC(&crc,value&0xff);
   if (value<min) min=value;
   if (value>max) max=value;
  }
  if (crc!=crc_package)//CRC �� �������
  {
   enabled=false;
   break;
  }
 
  crc=0;
  //������ ���������
  FLIR_ONE_PopShort(&id);
  id&=0x0FFF;
  FLIR_ONE_CalculateCRC(&crc,(id>>8)&0xFF);
  FLIR_ONE_CalculateCRC(&crc,id&0xFF);
	FLIR_ONE_PopShort(&crc_package);
  FLIR_ONE_CalculateCRC(&crc,0);
  FLIR_ONE_CalculateCRC(&crc,0);
  for(x=ORIGINAL_IMAGE_WIDTH/2;x<ORIGINAL_IMAGE_WIDTH;x++)
  {   
   unsigned short value;
   FLIR_ONE_PopShort(&value);		
   unsigned long offset=y+(ORIGINAL_IMAGE_WIDTH-1-x)*ORIGINAL_IMAGE_HEIGHT;   
   sFlirOneControl.ThermalImage[offset]=value;
   //������� CRC
   FLIR_ONE_CalculateCRC(&crc,(value>>8)&0xff);
   FLIR_ONE_CalculateCRC(&crc,value&0xff);
   if (value<min) min=value;
   if (value>max) max=value;
  }
  if (crc!=crc_package)//CRC �� �������
  {
   enabled=false;
   break;
  }
 }
 
 //�������� ������ ������
 sFlirOneControl.Head=0;
 sFlirOneControl.Tail=0;
 sFlirOneControl.MagicPos=0;
 sFlirOneControl.MagicHeadPos=0; 
 
 if (enabled==true)
 {
  FrameTickStart=HAL_GetTick();
  //������ ������������ �����������
  long delta=max-min;
  if (delta==0) delta=1;	 
  for(x=0;x<ORIGINAL_IMAGE_WIDTH;x++)
  {
   long offset=x*ORIGINAL_IMAGE_HEIGHT;		
   for(y=0;y<ORIGINAL_IMAGE_HEIGHT;y++,offset++)
   {
    long value=sFlirOneControl.ThermalImage[offset];
    value-=min;
    value=(value*255)/delta;
    if (value>255) value=255;
		if (value<0) value=0;
    sFlirOneControl.ThermalImage[offset]=sFlirOneControl.ColorMap[value];
   }
  }
	//������� ������������ ����������� � ��������� ����� � ��������
  HX8347D_SetWindow(0,0,HX8347D_WIDTH-1,HX8347D_HEIGHT-1);
  for(x=0;x<ORIGINAL_IMAGE_WIDTH;x++)//������� 320 �����
  {
   for(long n=0;n<2;n++)
   {		
    long offset=x*ORIGINAL_IMAGE_HEIGHT;
    for(y=0;y<ORIGINAL_IMAGE_HEIGHT;y++,offset++)//������� 240 �����
    {
     unsigned short color=sFlirOneControl.ThermalImage[offset];		 
     HX8347D_Write16(color);
     HX8347D_Write16(color);
    }
	 }
  } 
 }
 return(true);
}

