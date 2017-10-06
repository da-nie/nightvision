#include "main.h"
#include "stm32f4xx_hal.h"
#include "usb_host.h"
#include "usbh_core.h"
#include "hx8347d.h"
#include "flironecontrol.h"

//размер буфера приема данных
#define RECEIVE_BUFFER_SIZE_0x80 64UL
#define RECEIVE_BUFFER_SIZE_0x81 64UL
#define RECEIVE_BUFFER_SIZE_0x83 64UL
#define RECEIVE_BUFFER_SIZE_0x85 64UL

#define RECEIVE_BLOCK_BUFFER_SIZE (42*1024UL)

extern USBH_HandleTypeDef hUsbHostFS;//дескриптор подключения
extern ApplicationTypeDef Appli_state;//состояние приложения

//индексы конечных точек в массиве DevicePipe
#define PIPE_0x81 0
#define PIPE_0x83 1
#define PIPE_0x85 2
#define PIPE_0x02 3
//количество каналов
#define OPEN_PIPES 4

//список конечных точек
const unsigned char EndPoint[OPEN_PIPES]={0x81,0x83,0x85,0x02}; 

unsigned char ReceiveBuffer_0x80[RECEIVE_BUFFER_SIZE_0x80*8];//буфер приема данных
unsigned char ReceiveBuffer_0x85[RECEIVE_BUFFER_SIZE_0x85*8];//буфер приема данных
unsigned char ReceiveBuffer_0x81[RECEIVE_BUFFER_SIZE_0x81*8];//буфер приема данных
unsigned char ReceiveBuffer_0x83[RECEIVE_BUFFER_SIZE_0x83*8];//буфер приема данных

static unsigned char ReceiveBlockBuffer_Ptr[RECEIVE_BLOCK_BUFFER_SIZE];//буфер приема блока
uint8_t DevicePipe[OPEN_PIPES];//каналы для приема данных
bool PipeIsInit=false;//инициализированы ли каналы
extern uint32_t FrameTickStart;//момент появления кадра

void RCC_Init(void);
void GPIO_Init(void);
void SPI_Init(void);

long USB_HOST_ControlTransfer(USBH_HandleTypeDef *hUsbHost,uint8_t request_type,uint8_t bRequest,uint16_t wValue,uint16_t wIndex,uint8_t *data,uint16_t wLength);//передать управляющее сообщение
unsigned long USB_HOST_Receive(USBH_HandleTypeDef *hUsbHost);//прием данных

long MODE_Led1_On(long state_pos);//включение светодиода 1
long MODE_Led2_On(long state_pos);//включение светодиода 2
long MODE_Led3_On(long state_pos);//включение светодиода 3
long MODE_Led4_On(long state_pos);//включение светодиода 4
long MODE_Led1_Off(long state_pos);//отключение светодиода 1
long MODE_Led2_Off(long state_pos);//отключение светодиода 2
long MODE_Led3_Off(long state_pos);//отключение светодиода 3
long MODE_Led4_Off(long state_pos);//отключение светодиода 4

//----------------------------------------------------------------------------------------------------
//локальный обработчик ошибок
//----------------------------------------------------------------------------------------------------
void ErrorHandler(void)
{
 while(1) 
 {
 }
}
//----------------------------------------------------------------------------------------------------
//глобальный обработчик ошибок
//----------------------------------------------------------------------------------------------------
void _Error_Handler(char *file,int line)
{
 ErrorHandler();
}

//----------------------------------------------------------------------------------------------------
//инициализация GPIO
//----------------------------------------------------------------------------------------------------
void GPIO_Init(void)
{
 //включаем тактирование портов  
 __HAL_RCC_GPIOC_CLK_ENABLE();
 __HAL_RCC_GPIOH_CLK_ENABLE();
 __HAL_RCC_GPIOA_CLK_ENABLE();
 __HAL_RCC_GPIOD_CLK_ENABLE();
  	
 //GPIO_Pin – номера выводов, которые конфигурируются. Пример для нескольких выводов:
 //GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
 //GPIO_Speed – задает скорость для выбранных выводов.
 //GPIO_Mode – задает режим работы выводов. Может принимать следующие значения:

 //настраиваем параметры порта D
 GPIO_InitTypeDef LED_GPIO_Init;
 LED_GPIO_Init.Mode=GPIO_MODE_OUTPUT_PP;
 LED_GPIO_Init.Pull=GPIO_NOPULL;
 LED_GPIO_Init.Pin=GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
 LED_GPIO_Init.Speed=GPIO_SPEED_HIGH;
 HAL_GPIO_Init(GPIOD,&LED_GPIO_Init);
}

//----------------------------------------------------------------------------------------------------
//инициализация SPI
//----------------------------------------------------------------------------------------------------
void SPI_Init(void)
{
 SPI_HandleTypeDef hspi1;  
  
 hspi1.Instance=SPI1;
 hspi1.Init.Mode=SPI_MODE_MASTER;
 hspi1.Init.Direction=SPI_DIRECTION_2LINES;
 hspi1.Init.DataSize=SPI_DATASIZE_8BIT;
 hspi1.Init.CLKPolarity=SPI_POLARITY_LOW;
 hspi1.Init.CLKPhase=SPI_PHASE_1EDGE;
 hspi1.Init.NSS=SPI_NSS_SOFT;
 hspi1.Init.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_2;
 hspi1.Init.FirstBit=SPI_FIRSTBIT_MSB;
 hspi1.Init.TIMode=SPI_TIMODE_DISABLE;
 hspi1.Init.CRCCalculation=SPI_CRCCALCULATION_DISABLE;
 hspi1.Init.CRCPolynomial=10;
 if (HAL_SPI_Init(&hspi1)!=HAL_OK) ErrorHandler();
}

//----------------------------------------------------------------------------------------------------
//инициализация тактового генератора
//----------------------------------------------------------------------------------------------------
void RCC_Init(void)
{
 RCC_OscInitTypeDef RCC_OscInitStruct;
 RCC_ClkInitTypeDef RCC_ClkInitStruct;
 __HAL_RCC_PWR_CLK_ENABLE();
 __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  
 RCC_OscInitStruct.OscillatorType=RCC_OSCILLATORTYPE_HSE;
 RCC_OscInitStruct.HSEState=RCC_HSE_ON;
 RCC_OscInitStruct.PLL.PLLState=RCC_PLL_ON;
 RCC_OscInitStruct.PLL.PLLSource=RCC_PLLSOURCE_HSE;
 RCC_OscInitStruct.PLL.PLLM=4;
 RCC_OscInitStruct.PLL.PLLN=168;
 RCC_OscInitStruct.PLL.PLLP=RCC_PLLP_DIV2;
 RCC_OscInitStruct.PLL.PLLQ=7;
 if (HAL_RCC_OscConfig(&RCC_OscInitStruct)!=HAL_OK) ErrorHandler();
 
 RCC_ClkInitStruct.ClockType=RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
 RCC_ClkInitStruct.SYSCLKSource=RCC_SYSCLKSOURCE_PLLCLK;
 RCC_ClkInitStruct.AHBCLKDivider=RCC_SYSCLK_DIV1;
 RCC_ClkInitStruct.APB1CLKDivider=RCC_HCLK_DIV4;
 RCC_ClkInitStruct.APB2CLKDivider=RCC_HCLK_DIV2;

 if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct,FLASH_LATENCY_5)!=HAL_OK) ErrorHandler();
 HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);
 HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
 HAL_NVIC_SetPriority(SysTick_IRQn,0,0);
}

//----------------------------------------------------------------------------------------------------
//передать управляющее сообщение
//----------------------------------------------------------------------------------------------------
long USB_HOST_ControlTransfer(USBH_HandleTypeDef *hUsbHost,uint8_t request_type,uint8_t bRequest,uint16_t wValue,uint16_t wIndex,uint8_t *buffer,uint16_t wLength)
{
 hUsbHost->Control.setup.b.bmRequestType=request_type;
 hUsbHost->Control.setup.b.bRequest=bRequest;
 hUsbHost->Control.setup.b.wValue.w=wValue;
 hUsbHost->Control.setup.b.wIndex.w=wIndex;
 hUsbHost->Control.setup.b.wLength.w=wLength;   
 USBH_StatusTypeDef status=USBH_BUSY;
 while(1)
 {
  status=USBH_CtlReq(hUsbHost,buffer,wLength);
  if (status==USBH_BUSY) continue;
  break;
 }
 if (status!=USBH_OK)
 {
  hUsbHost->RequestState=CMD_SEND;	  
  return(-1);
 }
 return(wLength);
}

//----------------------------------------------------------------------------------------------------
//задать конфигурацию
//----------------------------------------------------------------------------------------------------
long USB_HOST_SetConfiguration(USBH_HandleTypeDef *hUsbHost,uint8_t configuration)
{
 USBH_StatusTypeDef status=USBH_BUSY;  
 while(1)
 {
  status=USBH_SetCfg(hUsbHost,configuration);
  if (status==USBH_BUSY) continue;
  break;
 }
 if (status!=USBH_OK) hUsbHost->RequestState=CMD_SEND;
 return(status);
}

//----------------------------------------------------------------------------------------------------
//прием данных
//----------------------------------------------------------------------------------------------------
unsigned long USB_HOST_Receive(USBH_HandleTypeDef *hUsbHostFS)
{	
 USBH_URBStateTypeDef URB_Status=USBH_URB_IDLE;
 bool receive[3]={false,false,false};	
 unsigned long length=0;
 uint32_t tickstart=HAL_GetTick();
 while(1) 
 {
  if (receive[0]==false)
	{
   USBH_BulkReceiveData(hUsbHostFS,ReceiveBuffer_0x81,RECEIVE_BUFFER_SIZE_0x81,DevicePipe[PIPE_0x81]);
	 receive[0]=true;
	}
  if (receive[1]==false)
	{
   USBH_BulkReceiveData(hUsbHostFS,ReceiveBuffer_0x83,RECEIVE_BUFFER_SIZE_0x83,DevicePipe[PIPE_0x83]);
	 receive[1]=true;
	}
  if (receive[0]==true) 
	{
   URB_Status=USBH_LL_GetURBState(hUsbHostFS,DevicePipe[PIPE_0x81]);		
	 if (URB_Status==USBH_URB_DONE) receive[0]=false;
	}
  if (receive[1]==true) 
	{
   URB_Status=USBH_LL_GetURBState(hUsbHostFS,DevicePipe[PIPE_0x83]);		
	 if (URB_Status==USBH_URB_DONE) receive[1]=false;
	}	
	
	//обработка конечной точки 0x85	
	if (receive[2]==false)
 	{   
	 if (length+RECEIVE_BUFFER_SIZE_0x85>RECEIVE_BLOCK_BUFFER_SIZE) break;	  
   USBH_BulkReceiveData(hUsbHostFS,ReceiveBlockBuffer_Ptr+length,RECEIVE_BUFFER_SIZE_0x85,DevicePipe[PIPE_0x85]);
	 receive[2]=true;
	}
  if (receive[2]==true)
	{
   URB_Status=USBH_LL_GetURBState(hUsbHostFS,DevicePipe[PIPE_0x85]);
	 if (URB_Status==USBH_URB_NOTREADY) break;//принят NAK (завершение передачи)
	 if (URB_Status==USBH_URB_NYET) break;
	 if (URB_Status==USBH_URB_ERROR) break;
	 if (URB_Status==USBH_URB_STALL) break;
	 if (URB_Status==USBH_URB_IDLE)
	 {		
    if (length>0) break;
		if (HAL_GetTick()-tickstart>1) break;
	 }
   if(URB_Status==USBH_URB_DONE)//данные успешно приняты
 	 {    		
    MODE_Led4_On(0);
		receive[2]=false;
    unsigned long len=USBH_LL_GetLastXferSize(hUsbHostFS,DevicePipe[PIPE_0x85]);
		if (len==0) break;
		length+=len;
		if (len<64) break;
	 }
	}
 }
 MODE_Led4_Off(0);
 return(length);
}
//----------------------------------------------------------------------------------------------------
//отправка данных в конечную точку
//----------------------------------------------------------------------------------------------------
void USB_HOST_SendData(USBH_HandleTypeDef *hUsbHost,uint8_t out_pipe,uint8_t *buffer,uint16_t length)
{
 USBH_BulkSendData(hUsbHost,buffer,length,out_pipe,1);
}

//----------------------------------------------------------------------------------------------------
//включение светодиода 1
//----------------------------------------------------------------------------------------------------
long MODE_Led1_On(long state_pos)
{
 HAL_GPIO_WritePin(GPIOD,GPIO_PIN_12,GPIO_PIN_SET);//включаем светодиод
 return(state_pos+1);
}
//----------------------------------------------------------------------------------------------------
//включение светодиода 2
//----------------------------------------------------------------------------------------------------
long MODE_Led2_On(long state_pos)
{
 HAL_GPIO_WritePin(GPIOD,GPIO_PIN_13,GPIO_PIN_SET);//включаем светодиод
 return(state_pos+1);
}
//----------------------------------------------------------------------------------------------------
//включение светодиода 3
//----------------------------------------------------------------------------------------------------
long MODE_Led3_On(long state_pos)
{
 HAL_GPIO_WritePin(GPIOD,GPIO_PIN_14,GPIO_PIN_SET);//включаем светодиод
 return(state_pos+1);
}
//----------------------------------------------------------------------------------------------------
//включение светодиода 4
//----------------------------------------------------------------------------------------------------
long MODE_Led4_On(long state_pos)
{
 HAL_GPIO_WritePin(GPIOD,GPIO_PIN_15,GPIO_PIN_SET);//включаем светодиод
 return(state_pos+1);
}

//----------------------------------------------------------------------------------------------------
//отключение светодиода 1
//----------------------------------------------------------------------------------------------------
long MODE_Led1_Off(long state_pos)
{
 HAL_GPIO_WritePin(GPIOD,GPIO_PIN_12,GPIO_PIN_RESET);//включаем светодиод
 return(state_pos+1);
}
//----------------------------------------------------------------------------------------------------
//отключение светодиода 2
//----------------------------------------------------------------------------------------------------
long MODE_Led2_Off(long state_pos)
{
 HAL_GPIO_WritePin(GPIOD,GPIO_PIN_13,GPIO_PIN_RESET);//включаем светодиод
 return(state_pos+1);
}
//----------------------------------------------------------------------------------------------------
//отключение светодиода 3
//----------------------------------------------------------------------------------------------------
long MODE_Led3_Off(long state_pos)
{
 HAL_GPIO_WritePin(GPIOD,GPIO_PIN_14,GPIO_PIN_RESET);//включаем светодиод
 return(state_pos+1);
}
//----------------------------------------------------------------------------------------------------
//отключение светодиода 4
//----------------------------------------------------------------------------------------------------
long MODE_Led4_Off(long state_pos)
{
 HAL_GPIO_WritePin(GPIOD,GPIO_PIN_15,GPIO_PIN_RESET);//включаем светодиод
 return(state_pos+1);
}
//----------------------------------------------------------------------------------------------------
//установка конфигурации
//----------------------------------------------------------------------------------------------------
long MODE_SetConfiguration(USBH_HandleTypeDef *hUsbHostFS,long state_pos)
{
 if (USB_HOST_SetConfiguration(hUsbHostFS,3)==USBH_OK) 
 {
  USBH_SetInterface(hUsbHostFS,0,0);
  USBH_SetInterface(hUsbHostFS,1,0);
  USBH_SetInterface(hUsbHostFS,2,0);
	return(state_pos+1);
 }	
 return(state_pos);
}
//----------------------------------------------------------------------------------------------------
//установка конфигурации
//----------------------------------------------------------------------------------------------------
long MODE_PrintDeviceParam(USBH_HandleTypeDef *hUsbHostFS,long state_pos)
{
 char str[255];
 HX8347D_Clear();
 USBH_DeviceTypeDef *device_ptr=&hUsbHostFS->device;//устройство
 USBH_DevDescTypeDef *devdesc_ptr=&device_ptr->DevDesc;//дескриптор устройства
 USBH_CfgDescTypeDef *cfgdesc_ptr=&(device_ptr->CfgDesc);//дескриптор конфигурации
    
 sprintf(str,"КОНФИГУРАЦИЙ:%i",devdesc_ptr->bNumConfigurations);
 HX8347D_Print(str,HX8347D_YELLOW);
 sprintf(str,"VID:%04x PID:%04x",devdesc_ptr->idVendor,devdesc_ptr->idProduct);
 HX8347D_Print(str,HX8347D_YELLOW);
 HAL_Delay(1000);
 HX8347D_Clear();
 //выводим интерфейсы
 long interface_amount=cfgdesc_ptr->bNumInterfaces;
 for(long n=0;n<10+0*interface_amount;n++)
 {
  USBH_InterfaceDescTypeDef *interfacedesc_ptr=&(cfgdesc_ptr->Itf_Desc[n]);//дескриптор интерфейса
  long ep_amount=interfacedesc_ptr->bNumEndpoints;
  sprintf(str,"ИНТ.:%i ТЧК.:%i",interfacedesc_ptr->bInterfaceNumber,ep_amount);
  HX8347D_Print(str,HX8347D_YELLOW);
  sprintf(str,"АЛЬТ.НАСТР.:%i",interfacedesc_ptr->bAlternateSetting);
  HX8347D_Print(str,HX8347D_YELLOW);
  for(long m=0;m<ep_amount;m++)
  {
   USBH_EpDescTypeDef *epdesc_ptr=&(interfacedesc_ptr->Ep_Desc[m]);//дескриптор конечной точки
   long addr=epdesc_ptr->bEndpointAddress;   
   long size=epdesc_ptr->wMaxPacketSize;
   long attr=epdesc_ptr->bmAttributes;
   sprintf(str,"ТЧК:%02xh РЗМ:%i ТИП:%02x",addr,size,attr);
   HX8347D_Print(str,HX8347D_YELLOW);
  }
  HAL_Delay(1000);
  HX8347D_Clear();
 }
 return(state_pos+1);
}
//----------------------------------------------------------------------------------------------------
//ожидание
//----------------------------------------------------------------------------------------------------
long MODE_Wait(long state_pos)
{
 return(state_pos+1);
}
//----------------------------------------------------------------------------------------------------
//запрос производителя
//----------------------------------------------------------------------------------------------------
long MODE_GetManufacturer(USBH_HandleTypeDef *hUsbHostFS,long state_pos)
{
 USBH_StatusTypeDef status;
 status=USBH_Get_StringDesc(hUsbHostFS,hUsbHostFS->device.DevDesc.iManufacturer,ReceiveBuffer_0x80,64);
 if (status==USBH_OK) 
 {
  HX8347D_Print((char*)ReceiveBuffer_0x80,HX8347D_YELLOW);
  return(state_pos+1);
 }
 return(state_pos);
}
//----------------------------------------------------------------------------------------------------
//запрос модели
//----------------------------------------------------------------------------------------------------
long MODE_GetProduct(USBH_HandleTypeDef *hUsbHostFS,long state_pos)
{
 USBH_StatusTypeDef status;
 status=USBH_Get_StringDesc(hUsbHostFS,hUsbHostFS->device.DevDesc.iProduct,ReceiveBuffer_0x80,64);
 if (status==USBH_OK) 
 {
  HX8347D_Print((char*)ReceiveBuffer_0x80,HX8347D_YELLOW);
  return(state_pos+1);
 }
 return(state_pos);
}
//----------------------------------------------------------------------------------------------------
//инициализация каналов
//----------------------------------------------------------------------------------------------------
long MODE_InitPipe(USBH_HandleTypeDef *hUsbHostFS,long state_pos)
{
 USBH_StatusTypeDef status;
 if (PipeIsInit==true)
 {    
  for(long n=0;n<OPEN_PIPES;n++)
  {
   USBH_ClosePipe(hUsbHostFS,DevicePipe[n]);
   USBH_FreePipe(hUsbHostFS,DevicePipe[n]);
  }   
 }
 PipeIsInit=false;    
 //создаем каналы для конечных точек
 bool ok[OPEN_PIPES];
 for(long n=0;n<OPEN_PIPES;n++) ok[n]=false;
 bool error=false;
 for(long n=0;n<OPEN_PIPES;n++)
 {
  DevicePipe[n]=USBH_AllocPipe(hUsbHostFS,EndPoint[n]);
  //status=USBH_OpenPipe(hUsbHostFS,DevicePipe[n],EndPoint[n],hUsbHostFS->device.address,hUsbHostFS->device.speed,USB_EP_TYPE_BULK,USBH_MAX_DATA_BUFFER);
  status=USBH_OpenPipe(hUsbHostFS,DevicePipe[n],EndPoint[n],hUsbHostFS->device.address,hUsbHostFS->device.speed,USB_EP_TYPE_BULK,64);
 
  if (status==USBH_OK) ok[n]=true;
                  else error=true;
 }
 if (error==true)
 {
  for(long n=0;n<OPEN_PIPES;n++)
  {
   if  (ok[n]==true)  USBH_ClosePipe(hUsbHostFS,DevicePipe[n]);
   USBH_FreePipe(hUsbHostFS,DevicePipe[n]);
  }
  return(state_pos);
 }
 PipeIsInit=true;
 char str[255];
 for(long n=0;n<OPEN_PIPES;n++)
 {
  sprintf(str,"Pipe:0x%02x=0x%02x",EndPoint[n],DevicePipe[n]);
  HX8347D_Print(str,HX8347D_YELLOW);       
 }
 return(state_pos+1);
}  
//----------------------------------------------------------------------------------------------------
//остановка интерфейса 1
//----------------------------------------------------------------------------------------------------
long MODE_ControlTransferStopI1(USBH_HandleTypeDef *hUsbHostFS,long state_pos)
{
 long interface=1;
 long alternate=0;	
 if (USBH_SetInterface(hUsbHostFS,interface,alternate)==USBH_OK) return(state_pos+1);
 return(state_pos);	
}
//----------------------------------------------------------------------------------------------------
//остановка интерфейса 2
//----------------------------------------------------------------------------------------------------
long MODE_ControlTransferStopI2(USBH_HandleTypeDef *hUsbHostFS,long state_pos)
{
 long interface=2;
 long alternate=0;	
 if (USBH_SetInterface(hUsbHostFS,interface,alternate)==USBH_OK) return(state_pos+1);
 return(state_pos);	
}
//----------------------------------------------------------------------------------------------------
//запуск интерфейса 1
//----------------------------------------------------------------------------------------------------
long MODE_ControlTransferStartI1(USBH_HandleTypeDef *hUsbHostFS,long state_pos)
{
 long interface=1;
 long alternate=1;	
 if (USBH_SetInterface(hUsbHostFS,interface,alternate)==USBH_OK) return(state_pos+1);
 return(state_pos);	
}
//----------------------------------------------------------------------------------------------------
//запуск интерфейса 2
//----------------------------------------------------------------------------------------------------
long MODE_ControlTransferStartI2(USBH_HandleTypeDef *hUsbHostFS,long state_pos)
{
 long interface=2;
 long alternate=1;	
 if (USBH_SetInterface(hUsbHostFS,interface,alternate)==USBH_OK) return(state_pos+1);
 return(state_pos);	
}
//----------------------------------------------------------------------------------------------------
//пауза
//----------------------------------------------------------------------------------------------------
long MODE_Pause(long state_pos)
{
 HAL_Delay(100);
 return(state_pos+1);	
}
//----------------------------------------------------------------------------------------------------
//отключить шторку
//----------------------------------------------------------------------------------------------------
long MODE_DisableShuttering(USBH_HandleTypeDef *hUsbHostFS,long state_pos)
{
 unsigned char startCommand[16]={0xcc, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x57, 0x64, 0x17, 0x8b};
 USB_HOST_SendData(hUsbHostFS,DevicePipe[PIPE_0x02],startCommand,16);
 HAL_Delay(2);
 char str[]="{\"type\":\"setOption\",\"data\":{\"option\":\"autoFFC\",\"value\":false}}\0";
 USB_HOST_SendData(hUsbHostFS,DevicePipe[PIPE_0x02],(uint8_t*)(str),strlen(str)+1);
 HAL_Delay(2);
 return(state_pos+1);	
}
//----------------------------------------------------------------------------------------------------
//включить шторку
//----------------------------------------------------------------------------------------------------
long MODE_EnableShuttering(USBH_HandleTypeDef *hUsbHostFS,long state_pos)
{
 unsigned char startCommand[16]={0xcc, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0xB9, 0xCB, 0xA2, 0x99};
 USB_HOST_SendData(hUsbHostFS,DevicePipe[PIPE_0x02],startCommand,16);
 HAL_Delay(2);
 char str[]="{\"type\":\"setOption\",\"data\":{\"option\":\"doFFC\",\"value\":true}}\0";
 USB_HOST_SendData(hUsbHostFS,DevicePipe[PIPE_0x02],(uint8_t*)(str),strlen(str)+1);
 HAL_Delay(2);
 return(state_pos+1);	
}
//----------------------------------------------------------------------------------------------------
//прием данных
//----------------------------------------------------------------------------------------------------
long MODE_Receive(USBH_HandleTypeDef *hUsbHostFS,long state_pos)
{
 unsigned long length=USB_HOST_Receive(hUsbHostFS);
 if (length>0) FLIR_ONE_CreateImage(ReceiveBlockBuffer_Ptr,length);
 return(state_pos);
}

//----------------------------------------------------------------------------------------------------
//получить интерфейс (FlirOne не поддерживает эту функциюдля ненулевого интерфейса)
//----------------------------------------------------------------------------------------------------
long MODE_GetInterface(USBH_HandleTypeDef *hUsbHostFS,long interface,long state_pos)
{
 char str[255];	 
 long ret=USB_HOST_ControlTransfer(hUsbHostFS,USB_D2H|USB_REQ_RECIPIENT_INTERFACE|USB_REQ_TYPE_STANDARD,USB_REQ_GET_INTERFACE,0,interface,ReceiveBuffer_0x80,1);
 if (ret>=0)
 {
  HX8347D_Clear();
	sprintf(str,"ИНТЕРФЕЙС%i: %02x",interface,ReceiveBuffer_0x80[0]);
  HX8347D_Print(str,HX8347D_YELLOW);
	HAL_Delay(3000);
  return(state_pos+1);
 }
 return(state_pos+1);	
}
//----------------------------------------------------------------------------------------------------
//получить статус конечной точки
//----------------------------------------------------------------------------------------------------
long MODE_GetStatusEP(USBH_HandleTypeDef *hUsbHostFS,long ep,long state_pos)
{
 char str[255];	
 long ret=USB_HOST_ControlTransfer(hUsbHostFS,USB_D2H|USB_REQ_RECIPIENT_ENDPOINT|USB_REQ_TYPE_STANDARD,USB_REQ_GET_STATUS,0,ep,ReceiveBuffer_0x80,2);
 if (ret>=0)
 {
  HX8347D_Clear();
	sprintf(str,"СТАТУС EP[%02x]:%02x %02x",ep,ReceiveBuffer_0x80[0],ReceiveBuffer_0x80[1]);
  HX8347D_Print(str,HX8347D_YELLOW);
	HAL_Delay(3000);
  return(state_pos+1);
 }
 return(state_pos+1);	
}

//----------------------------------------------------------------------------------------------------
//обработка USB
//----------------------------------------------------------------------------------------------------
bool USB_HOST_Processing(void) 
{ 
 //состояния программы
 typedef enum
 {
  MODE_WAIT,
  MODE_PRINT_DEVICE_PARAM,
  MODE_GET_MANUFACTURER,
  MODE_GET_PRODUCT,
  MODE_INIT_PIPE,
  MODE_SET_CONFIGURATION,
	MODE_GET_INTERFACE_0,
	MODE_GET_INTERFACE_1,
	MODE_GET_INTERFACE_2,
	MODE_GET_STATUS_EP_0x81,
	MODE_GET_STATUS_EP_0x83,
	MODE_GET_STATUS_EP_0x85,
  MODE_CONTROL_TRANSFER_STOP_I2,
  MODE_CONTROL_TRANSFER_STOP_I1,
  MODE_CONTROL_TRANSFER_START_I1,
  MODE_CONTROL_TRANSFER_START_I2, 
  MODE_DISABLE_SHUTTERING,
  MODE_ENABLE_SHUTTERING,
	MODE_LED_1_ON,
	MODE_LED_1_OFF,
	MODE_LED_2_ON,
	MODE_LED_2_OFF,
	MODE_LED_3_ON,
	MODE_LED_3_OFF,
	MODE_LED_4_ON,
	MODE_LED_4_OFF,
  MODE_RECEIVE,
  MODE_PAUSE,
  MODE_STOP
 } MODE;
 static long state_pos=0;
 //последовательность инициализации
 static MODE state[]=
 {
  MODE_WAIT,	  	 
  MODE_SET_CONFIGURATION,
  MODE_LED_2_ON,	 		
	MODE_INIT_PIPE,
  MODE_CONTROL_TRANSFER_STOP_I1,
  MODE_CONTROL_TRANSFER_STOP_I2,
  MODE_CONTROL_TRANSFER_START_I1,
  MODE_CONTROL_TRANSFER_START_I2,	
  MODE_LED_3_ON,
  MODE_RECEIVE,
  MODE_STOP
 }; 
 
 if (HAL_GetTick()-FrameTickStart>3*1000) return(false);

 if (hUsbHostFS.device.is_connected==0 && state_pos!=0)//устройство отключено
 {
  MODE_Led2_Off(0);
  MODE_Led3_Off(0);
  MODE_Led4_Off(0);
	 
  state_pos=0;
  PipeIsInit=false;	  
 }
 if (hUsbHostFS.device.is_connected==0) MODE_Led1_Off(0);
                                   else MODE_Led1_On(0);
  
 if (hUsbHostFS.gState==HOST_CHECK_CLASS)//определение класса устройства
 {
  MODE mode=state[state_pos];
  if (mode==MODE_LED_1_ON) state_pos=MODE_Led1_On(state_pos);
  if (mode==MODE_LED_2_ON) state_pos=MODE_Led2_On(state_pos);
  if (mode==MODE_LED_3_ON) state_pos=MODE_Led3_On(state_pos);
  if (mode==MODE_LED_4_ON) state_pos=MODE_Led4_On(state_pos);
	 
  if (mode==MODE_LED_1_OFF) state_pos=MODE_Led1_Off(state_pos);
  if (mode==MODE_LED_2_OFF) state_pos=MODE_Led2_Off(state_pos);
  if (mode==MODE_LED_3_OFF) state_pos=MODE_Led3_Off(state_pos);
  if (mode==MODE_LED_4_OFF) state_pos=MODE_Led4_Off(state_pos);
	
 	if (mode==MODE_SET_CONFIGURATION) state_pos=MODE_SetConfiguration(&hUsbHostFS,state_pos);
  if (mode==MODE_PRINT_DEVICE_PARAM) state_pos=MODE_PrintDeviceParam(&hUsbHostFS,state_pos);
	 
	if (mode==MODE_GET_STATUS_EP_0x85) state_pos=MODE_GetStatusEP(&hUsbHostFS,0x85,state_pos);
	if (mode==MODE_GET_STATUS_EP_0x83) state_pos=MODE_GetStatusEP(&hUsbHostFS,0x83,state_pos);
	if (mode==MODE_GET_STATUS_EP_0x81) state_pos=MODE_GetStatusEP(&hUsbHostFS,0x81,state_pos);	 
	 
  if (mode==MODE_WAIT) state_pos=MODE_Wait(state_pos);
	if (mode==MODE_GET_MANUFACTURER) state_pos=MODE_GetManufacturer(&hUsbHostFS,state_pos);
	if (mode==MODE_GET_PRODUCT) state_pos=MODE_GetProduct(&hUsbHostFS,state_pos);
	if (mode==MODE_INIT_PIPE) state_pos=MODE_InitPipe(&hUsbHostFS,state_pos);
	if (mode==MODE_CONTROL_TRANSFER_STOP_I1) state_pos=MODE_ControlTransferStopI1(&hUsbHostFS,state_pos);
  if (mode==MODE_CONTROL_TRANSFER_STOP_I2) state_pos=MODE_ControlTransferStopI2(&hUsbHostFS,state_pos);
	if (mode==MODE_CONTROL_TRANSFER_START_I1) state_pos=MODE_ControlTransferStartI1(&hUsbHostFS,state_pos);
  if (mode==MODE_CONTROL_TRANSFER_START_I2) state_pos=MODE_ControlTransferStartI2(&hUsbHostFS,state_pos);
  if (mode==MODE_PAUSE) state_pos=MODE_Pause(state_pos);
  if (mode==MODE_DISABLE_SHUTTERING) state_pos=MODE_DisableShuttering(&hUsbHostFS,state_pos); 
  if (mode==MODE_ENABLE_SHUTTERING) state_pos=MODE_EnableShuttering(&hUsbHostFS,state_pos); 
  if (mode==MODE_RECEIVE) 
	{
	 state_pos=MODE_Receive(&hUsbHostFS,state_pos);
	}	
  if (mode==MODE_GET_INTERFACE_0) state_pos=MODE_GetInterface(&hUsbHostFS,0,state_pos);
  if (mode==MODE_GET_INTERFACE_1) state_pos=MODE_GetInterface(&hUsbHostFS,1,state_pos);
  if (mode==MODE_GET_INTERFACE_2) state_pos=MODE_GetInterface(&hUsbHostFS,2,state_pos);
 }
 return(true);
}

//----------------------------------------------------------------------------------------------------
//главная функция программы
//----------------------------------------------------------------------------------------------------
int main(void)
{
 	
 HAL_Init();
 RCC_Init();
 GPIO_Init();
 SPI_Init();
 MX_USB_HOST_Init();
 HX8347D_Init();
 HX8347D_Clear();	 
 FLIR_ONE_Init(); 
 FrameTickStart=HAL_GetTick();
 while (1)
 {
  MX_USB_HOST_Process();
  if (USB_HOST_Processing()==false) SCB->AIRCR=0x05FA0004;
 }
}



