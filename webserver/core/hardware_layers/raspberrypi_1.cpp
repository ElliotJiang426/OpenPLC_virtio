//-----------------------------------------------------------------------------
// Copyright 2015 Thiago Alves
//
// Based on the LDmicro software by Jonathan Westhues
// This file is part of the OpenPLC Software Stack.
//
// OpenPLC is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OpenPLC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenPLC.  If not, see <http://www.gnu.org/licenses/>.
//------
//
// This file is the hardware layer for the OpenPLC. If you change the platform
// where it is running, you may only need to change this file. All the I/O
// related stuff is here. Basically it provides functions to read and write
// to the OpenPLC internal buffers in order to update I/O state.
// Thiago Alves, Dec 2015
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <pthread.h>

#include "ladder.h"
#include "custom_layer.h"

#if !defined(ARRAY_SIZE)
    #define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
#endif

#define MAX_INPUT 		4	
#define MAX_OUTPUT 		4
#define MAX_ANALOG_OUT	1

/********************I/O PINS CONFIGURATION*********************
 * A good source for RaspberryPi I/O pins information is:
 * http://pinout.xyz
 *
 * The buffers below works as an internal mask, so that the
 * OpenPLC can access each pin sequentially
****************************************************************/
//inBufferPinMask: pin mask for each input, which
//means what pin is mapped to that OpenPLC input
int inBufferPinMask[MAX_INPUT] = { 0, 1, 2, 3};

//outBufferPinMask: pin mask for each output, which
//means what pin is mapped to that OpenPLC output
int outBufferPinMask[MAX_OUTPUT] =	{ 4, 5, 6, 7};

//analogOutBufferPinMask: pin mask for the analog PWM
//output of the RaspberryPi
int analogOutBufferPinMask[MAX_ANALOG_OUT] = { 8 };

// helper structures & functions
// handle of the gpio_driver
int handle = -1;
// read buffer
char read_buf[MAX_INPUT] = {0, 0, 0, 0};
char write_buf[MAX_OUTPUT] = {0, 0, 0, 0};

static void gpio_read(){
	// read from (virtual) gpio device
    int len = read(handle, read_buf, MAX_INPUT);
	if(len != MAX_INPUT){
		printf("Error: read insufficient bytes\n");
    	return;
	}
}

static void gpio_write(int pin, IEC_BOOL output){
	int len = 0;

	sprintf(write_buf, "0%d%d", pin, output);
	len = write(handle, write_buf, sizeof(write_buf));
}


//-----------------------------------------------------------------------------
// This function is called by the main OpenPLC routine when it is initializing.
// Hardware initialization procedures should be here.
//-----------------------------------------------------------------------------
void initializeHardware()
{
	// wiringPiSetup();
	// //piHiPri(99);

	// //set pins as input
	// for (int i = 0; i < MAX_INPUT; i++)
	// {
	//     if (pinNotPresent(ignored_bool_inputs, ARRAY_SIZE(ignored_bool_inputs), i))
	//     {
	// 	    pinMode(inBufferPinMask[i], INPUT);
	// 	    if (i != 0 && i != 1) //pull down can't be enabled on the first two pins
	// 	    {
	// 		    pullUpDnControl(inBufferPinMask[i], PUD_DOWN); //pull down enabled
	// 	    }
	//     }
	// }

	// //set pins as output
	// for (int i = 0; i < MAX_OUTPUT; i++)
	// {
	//     if (pinNotPresent(ignored_bool_outputs, ARRAY_SIZE(ignored_bool_outputs), i))
	//     	pinMode(outBufferPinMask[i], OUTPUT);
	// }

	// //set PWM pins as output
	// for (int i = 0; i < MAX_ANALOG_OUT; i++)
	// {
	//     if (pinNotPresent(ignored_int_outputs, ARRAY_SIZE(ignored_int_outputs), i))
    // 		pinMode(analogOutBufferPinMask[i], PWM_OUTPUT);
	// }
	// TODO: should support front-end configure the direction of pins
	handle = open("/dev/vgpiochip",O_RDWR);

	if(handle==-1){
    	printf("Error: Can not open the file\n");
    	return;
    }
}

//-----------------------------------------------------------------------------
// This function is called by the main OpenPLC routine when it is finalizing.
// Resource clearing procedures should be here.
//-----------------------------------------------------------------------------
void finalizeHardware()
{
	close(handle);
}

//-----------------------------------------------------------------------------
// This function is called by the OpenPLC in a loop. Here the internal buffers
// must be updated to reflect the actual state of the input pins. The mutex buffer_lock
// must be used to protect access to the buffers on a threaded environment.
//-----------------------------------------------------------------------------
void updateBuffersIn()
{
	pthread_mutex_lock(&bufferLock); //lock mutex

	gpio_read();

	//INPUT
	for (int i = 0; i < MAX_INPUT; i++)
	{
	    if (pinNotPresent(ignored_bool_inputs, ARRAY_SIZE(ignored_bool_inputs), i))
    		if (bool_input[i/8][i%8] != NULL) *bool_input[i/8][i%8] = read_buf[i];
	}

	pthread_mutex_unlock(&bufferLock); //unlock mutex
}

//-----------------------------------------------------------------------------
// This function is called by the OpenPLC in a loop. Here the internal buffers
// must be updated to reflect the actual state of the output pins. The mutex buffer_lock
// must be used to protect access to the buffers on a threaded environment.
//-----------------------------------------------------------------------------
void updateBuffersOut()
{
	pthread_mutex_lock(&bufferLock); //lock mutex

	//OUTPUT
	for (int i = 0; i < MAX_OUTPUT; i++)
	{
	    if (pinNotPresent(ignored_bool_outputs, ARRAY_SIZE(ignored_bool_outputs), i))
    		if (bool_output[i/8][i%8] != NULL) gpio_write(outBufferPinMask[i], *bool_output[i/8][i%8]);
	}

	// TODO: not support analog output yet
	//ANALOG OUT (PWM)
	// for (int i = 0; i < MAX_ANALOG_OUT; i++)
	// {
	//     if (pinNotPresent(ignored_int_outputs, ARRAY_SIZE(ignored_int_outputs), i))
    // 		if (int_output[i] != NULL) pwmWrite(analogOutBufferPinMask[i], (*int_output[i] / 64));
	// }

	pthread_mutex_unlock(&bufferLock); //unlock mutex
}
