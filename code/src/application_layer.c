// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define START 0x01
#define END 0x03
#define DATA 0x02

#define SIZE 0x00
#define NAME 0x01
#define BUF_SIZE 512

int fileSize(FILE *f){
    int size;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    printf("%d\n", size);

    return size;
}

int sizeConvert(unsigned char *sizeArray, uint32_t size){
    int num = 0;

    while (size)
    {
        sizeArray[num++] = size & 0xFF;
        size = size >> 8;
    }

    return num;
}

int getData(unsigned char *data, FILE *f){
    int cur, size = 0;
    for(int i = 0; i < 512; i++){
        cur = fgetc(f);
        if(cur != EOF){
            data[i] = cur;
            size++;
        }else{
            break;
        }
    }

    return size;
}

int saveData(unsigned char *data, int size, FILE *f){
    for(int i = 4; i < size; i++){
        fputc(data[i], f);
    }

    return size - 4;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    FILE *f;
    
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
    if(parameters.role == LlTx){
        f = fopen(filename, "r");
        int fsize = fileSize(f);
        unsigned char convSize[4]; //will be saved backwards because im lazy
        uint8_t fsize_size = sizeConvert(convSize, fsize);

        unsigned char start[7];
        start[0] = START;
        start[1] = SIZE;
        start[2] = fsize_size;
        int cnt = 0;

        for(int i = fsize_size - 1;i >= 0; i--){
            start[3 + cnt++] = convSize[i];
        }
        
        printf("%d bytes written\n", llwrite(start, 3 + fsize_size));
        
        uint16_t size;
        unsigned char data[BUF_SIZE];
        uint8_t count = 0;
        unsigned char packet[4 + BUF_SIZE];
        while (!feof(f))
        {   
            size = getData(data, f);

            packet[0] = DATA;
            packet[1] = count++;
            packet[2] = size >> 8;
            packet[3] = size & 0xFF;

            for(int i = 0; i < size; i++){
                packet[4 + i] = data[i];
            }

            printf("%d bytes written\n", llwrite(packet, size + 4));
        }
        start[0] = END;
        
        printf("%d bytes written\n", llwrite(start, 3 + fsize_size));
        
    }else{
        f = fopen(filename, "w");
        printf("enter switch\n");
        int fileSize, fileSize2 = 0, packetSize;
        unsigned char r_packet[4 + BUF_SIZE];

        while (TRUE)
        {
            
            packetSize = llread(&r_packet);
            fileSize2 += packetSize;
            printf("Read %d bytes\n", packetSize);
            
            printf("\ncode 0x%02X\n", r_packet[0]);
            for(int i = 0; i < packetSize; i++){
                //printf("0x%02X\n", r_packet[i]);
            }
            
            if(r_packet[0] == START){
                fileSize = r_packet[2] * 256 + r_packet[3];
            }else if(r_packet[0] == END){
                printf("%d %d", fileSize, fileSize2);
                break;
            }else if(r_packet[0] == DATA){
                saveData(r_packet, packetSize, f);
                printf("\nsuccess\n");
                
            }
        }
    }

   fclose(f);
   llclose(0);
}
