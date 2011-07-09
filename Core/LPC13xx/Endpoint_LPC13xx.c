/*
             LUFA Library
     Copyright (C) Dean Camera, 2011.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2011  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

#define  __INCLUDE_FROM_USB_DRIVER
#include "../USBMode.h"

#if defined(USB_CAN_BE_DEVICE)

#include "../Endpoint.h"
#include "../Events.h"

#if !defined(FIXED_CONTROL_ENDPOINT_SIZE)
uint8_t USB_ControlEndpointSize = ENDPOINT_CONTROLEP_DEFAULT_SIZE;
#endif

volatile uint32_t USB_SelectedEndpoint = ENDPOINT_CONTROLEP;
volatile Endpoint_flags_t Endpoint_flags[USB_EP_NUM];

bool Endpoint_ConfigureEndpoint_Prv(const uint8_t Number,
                                    const uint32_t UECFG0Data)
{
	Endpoint_SelectEndpoint(Number);
	Endpoint_EnableEndpoint();


	return Endpoint_IsConfigured();
}

void Endpoint_ClearEndpoints(void)
{
	for (uint8_t EPNum = 0; EPNum < ENDPOINT_TOTAL_ENDPOINTS; EPNum++)
	{
		Endpoint_SelectEndpoint(EPNum);
		Endpoint_DisableEndpoint();
	}
}

void Endpoint_ClearStatusStage(void)
{
	if (USB_ControlRequest.bmRequestType & REQDIR_DEVICETOHOST)
	{
		//while (!(Endpoint_IsOUTReceived()))
		//{
		//	if (USB_DeviceState == DEVICE_STATE_Unattached)
		//	  return;
		//}

		Endpoint_ClearOUT();
	}
	else
	{
		//while (!(Endpoint_IsINReady()))
		//{
		//	if (USB_DeviceState == DEVICE_STATE_Unattached)
		//	  return;
		//}

		Endpoint_ClearIN();
	}
}

#if !defined(CONTROL_ONLY_DEVICE)
uint8_t Endpoint_WaitUntilReady(void)
{
	#if (USB_STREAM_TIMEOUT_MS < 0xFF)
	uint8_t  TimeoutMSRem = USB_STREAM_TIMEOUT_MS;
	#else
	uint16_t TimeoutMSRem = USB_STREAM_TIMEOUT_MS;
	#endif

	uint16_t PreviousFrameNumber = USB_Device_GetFrameNumber();

	for (;;)
	{
		if (Endpoint_GetEndpointDirection() == ENDPOINT_DIR_IN)
		{
			if (Endpoint_IsINReady())
			  return ENDPOINT_READYWAIT_NoError;
		}
		else
		{
			if (Endpoint_IsOUTReceived())
			  return ENDPOINT_READYWAIT_NoError;
		}
		
		uint8_t USB_DeviceState_LCL = USB_DeviceState;

		if (USB_DeviceState_LCL == DEVICE_STATE_Unattached)
		  return ENDPOINT_READYWAIT_DeviceDisconnected;
		else if (USB_DeviceState_LCL == DEVICE_STATE_Suspended)
		  return ENDPOINT_READYWAIT_BusSuspended;
		else if (Endpoint_IsStalled())
		  return ENDPOINT_READYWAIT_EndpointStalled;

		uint16_t CurrentFrameNumber = USB_Device_GetFrameNumber();

		if (CurrentFrameNumber != PreviousFrameNumber)
		{
			PreviousFrameNumber = CurrentFrameNumber;

			if (!(TimeoutMSRem--))
			  return ENDPOINT_READYWAIT_Timeout;
		}
	}
}

uint32_t Endpoint_Read_buf(uint8_t *buf, uint32_t size)
{
	if (Endpoint_flags[USB_SelectedEndpoint].preparedRead){
		Endpoint_prepare_read();
	}
	
	uint32_t cnt;

	do 
	{
		cnt = USB_RXPLEN;
	} while ((cnt & PKT_DV) == 0);
	cnt &= PKT_LNGTH_MASK;

	for (uint32_t i = 0; i<(cnt+3)/4 && i<(size/4); i++) 
	{
		*((uint32_t __attribute__((packed)) *)buf) = USB_RXDATA;
		buf += 4;
	}

	return cnt;
}

void Endpoint_prepare_read(){
	USB_CTRL = ((USB_SelectedEndpoint & 0x0F) << 2) | CTRL_RD_EN;
	__asm("nop"); __asm("nop"); __asm("nop"); __asm("nop"); __asm("nop");
	Endpoint_flags[USB_SelectedEndpoint].preparedRead = 1;
}

void Endpoint_complete_read(){
	USB_CTRL = 0;

	if ((USB_SelectedEndpoint & 0x80) != 0x04) 
	{   /* Iso endpoints are cleared on SOF */
		WriteEndpointCommand(USB_SelectedEndpoint, CMD_CLR_BUF);
	}
	
	Endpoint_flags[USB_SelectedEndpoint].preparedRead = 0;
}

void Endpoint_prepare_write(uint32_t size){
	USB_CTRL = ((USB_SelectedEndpoint & 0x0F) << 2) | CTRL_WR_EN;
	__asm("nop"); __asm("nop"); __asm("nop"); __asm("nop"); __asm("nop");
	USB_TXPLEN = size;
	Endpoint_flags[USB_SelectedEndpoint].preparedWrite = 1;
}

void Endpoint_complete_write(){
	USB_CTRL = 0;
	WriteEndpointCommand(USB_SelectedEndpoint|0x80, CMD_VALID_BUF);
	Endpoint_flags[USB_SelectedEndpoint].preparedWrite = 0;
}

uint32_t Endpoint_write_buf(uint8_t *buf, uint32_t size) 
{
	uint32_t n;

	for (n = 0; n < (size + 3) / 4; n++) 
	{
		USB_TXDATA = *((uint32_t __attribute__((packed)) *)buf);
		buf += 4;
	}
	
	return size;
}

#endif

#endif

