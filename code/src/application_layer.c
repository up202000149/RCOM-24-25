// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <stdint.h>

#define START 0x01
#define END 0x03
#define DATA 0x02

#define SIZE 0x00
#define NAME 0x01
#define BUF_SIZE 65535

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer parameters;
    strcpy(parameters.serialPort, serialPort);
    if(!strcmp(role,"rx")){
        parameters.role = LlRx;
    }else if(!strcmp(role,"tx")){
        parameters.role = LlTx;
    }
    parameters.baudRate = baudRate;
    parameters.nRetransmissions = nTries;
    parameters.timeout = timeout;
    
    llopen(parameters);

    switch (parameters.role)
    {
    case LlTx:
        int fsize; // TODO: create function get file size
        uint8_t fsize_size; // TODO: definitly wrong, needs to convert from int to several uint8_t

        unsigned char start[4] = {START, SIZE, fsize_size, fsize};
        unsigned char end[4] = {END, SIZE, fsize_size, fsize};
        
        llwrite(start, sizeof(start));
        
        uint16_t size;
        unsigned char data[BUF_SIZE] = {0};
        uint8_t count = 0;
        unsigned char packet[4 + BUF_SIZE] = {0};
        //TODO: open file, read to packet buf size
        while (FALSE) //TODO: add actual condition
        {   
            //TODO: define packet size and data
            packet[0] = DATA;
            packet[1] = count++;
            packet[2] = size >> 8;
            packet[3] = size & 0xFF;

            for(int i = 0; i < size; i++){
                packet[4 + i] = data[i];
            }

            llwrite(packet, size + 4);

            memset(packet, 0, BUF_SIZE + 4);
        }
        
        llwrite(end, sizeof(end));
        
        break;
    
    case LlRx:
        int endFlag = 0;
        unsigned char r_packet[4 + BUF_SIZE] = {0};
        while (!endFlag)
        {
            llread(r_packet);
            if(r_packet[0] == END){
                
            }
        }
        
        
        break;
    }
    /* TODO: 
    open connection --
    if write
        llwrite start --
        open filename
            create packets
            llwrite
        llwrite end --
    if read
        llread
    close connection
    */
   llclose(0);
}
