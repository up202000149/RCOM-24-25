// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define FLAG 0x7E
#define ANSREC 0x03
#define ANSSEN 0x01
#define CTRLSET 0x03
#define CTRLUA 0x07
#define I0 0x00
#define I1 0x80
#define RR0 0xAA
#define RR1 0xAB
#define REJ0 0x54
#define REJ1 0x55
#define DISC 0x0B
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x0B

#define MAX_ALARM_COUNT 3
#define FALSE 0
#define TRUE 1
#define BUF_SIZE 256

int alarmEnabled = FALSE;
int alarmCount = 0;
volatile int STOP = FALSE;
LinkLayerRole role;
int packetCount;
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

enum State { FLAG_REC, A_REC, C_REC, BCC_OK, FLAG_REC_END };
enum ReadState {START, RFLAG_REC, RA_REC, CS_REC, CI_REC, BCCS_OK, BCCI_OK, CONTENT_REC, END};

void processState(enum State *curState, unsigned char currByte) {
    switch (*curState) {
        case FLAG_REC:
            if (currByte == FLAG) *curState = A_REC;
            break;
        case A_REC:
            if (currByte == ANSSEN) *curState = C_REC;
            else *curState = FLAG_REC;
            break;
        case C_REC:
            if (currByte == CTRLSET) *curState = BCC_OK;
            else *curState = FLAG_REC; 
            break;
        case BCC_OK:
            if (currByte == (ANSSEN ^ CTRLSET)) *curState = FLAG_REC_END;
            else *curState = FLAG_REC;
            break;
        case FLAG_REC_END:
            if (currByte == FLAG) {
                printf("Frame received\n");
            }
            *curState = FLAG_REC;
            break;
    }
}



void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        return -1;
    }

    //(void)signal(SIGALRM, alarmHandler);

    role = connectionParameters.role;
    packetCount = 0;

    unsigned char buf[BUF_SIZE + 1] = {0}; 

    if(role == LlTx){
        buf[0] = FLAG;
        buf[1] = ANSSEN;
        buf[2] = CTRLSET;
        buf[3] = ANSSEN ^ CTRLSET;
        buf[4] = FLAG;

        writeBytes(buf, 5); //FIXME: warning: pointer targets in passing argument 1 of ‘writeBytes’ differ in signedness

        alarm(3);

        int bytes = 0;

        for (int i = 0; i < 5; i++) {
            bytes += readByte(&buf[i]); //FIXME: warning: pointer targets in passing argument 1 of ‘readBytes’ differ in signedness
            printf("%d bytes read\n", bytes);
        }

        if ((bytes == 5) && (buf[0] == FLAG) && (buf[1] == ANSREC) && (buf[2] == CTRLUA) && (buf[3] == (ANSREC ^ CTRLUA)) && (buf[4] == FLAG)) {
            printf("Success\n");
            alarm(0);
        }
        else {
            printf("Error\n");
        }
    } else {
        enum State curState = FLAG_REC;

        while (!STOP) {
            int bytes = readByte(&buf[0]); //FIXME: warning: pointer targets in passing argument 1 of ‘readBytes’ differ in signedness

            if (bytes > 0) {
                processState(&curState, buf[0]);

                if (curState == FLAG_REC_END) {
                    printf("sending UA\n");
                    unsigned char response[5] = {FLAG, ANSREC, CTRLUA, ANSREC ^ CTRLUA, FLAG};
                    writeBytes(response, 5); //FIXME: warning: pointer targets in passing argument 1 of ‘writeBytes’ differ in signedness
                    printf("UA frame sent\n");
                    STOP = TRUE;
                }
            }
        }
    }

    return 1;        
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{   
    uint8_t bcc2 = 0xFF;
    int n, written;

        n = packetCount++ % 2;
        
        //-----------buf2------------
        
        unsigned char buf2[bufSize + 1];
            bcc2 = buf[0];
            buf2[0] = buf[0];
        for(int i = 1; i < bufSize; i++){
            bcc2 = bcc2 ^ buf[i];
            buf2[i] = buf[i];
        }
    
        buf2[bufSize] = bcc2;
        int buf2Size = bufSize + 1;
        
        //-----------buf3------------
        unsigned char buf3[buf2Size + bufSize - 4];
        int stuffed = 0;

        for(int i = 0; i < buf2Size; i++){
            switch (buf2[i])
            {
            case 0x7E:
                buf3[i + stuffed++] = 0x7D;
                buf3[i + stuffed] = 0x5E;
                break;
            case 0x7D:
                buf3[i + stuffed++] = 0x7D;
                buf3[i + stuffed] = 0x5D;
                break;
            default:
                buf3[i + stuffed] = buf2[i];
                break;
            }
        }

        int buf3Size = buf2Size + stuffed;
        
        //-----------buf4------------

        int buf4Size = buf3Size + 5;
        unsigned char buf4[buf4Size];
        buf4[0] = FLAG;
        buf4[1] = ANSREC;
    
        if(!n){
            buf4[2] = I0;
        }else{
            buf4[2] = I1;
        }
    
        buf4[3] = buf4[1] ^ buf4[2];

        for(int i = 0; i < buf3Size; i++){
            buf4[4 + i] = buf3[i];
        }

        buf4[buf4Size - 1] = FLAG;
    
        //---------------------------
        /*while (alarmCount < 4) {
            
            if (alarmEnabled == FALSE) {
                    alarm(3); // Set alarm to be triggered in 3s
                    alarmEnabled = TRUE;
                }
        }*/
       for(int i = 0; i < buf4Size; i++){
        printf("0x%02X\n", buf4[i]);
       }
        written = writeBytes(buf4, buf4Size);
    

    // TODO: receive cycle
    

    return written;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{  

    int bytes = 0, aux = 1;
    unsigned char cur;
    unsigned char buf[1000], res[6];

    res[0] = FLAG;
    res[4] = FLAG;
    res[5] = '\n'; 

    bytes += readByte(&cur);
    while (cur != FLAG || aux)
    {   
        aux = 0;
        buf[bytes - 1] = cur;
        bytes += readByte(&cur);
    }
    buf[bytes - 1] = cur;
    
    if((buf[1] ^ buf[2]) != buf[3]){
        //reject
    }

    res[1] = buf[1];

    if(buf[2] == CTRLSET){
        res[2] = CTRLUA;
        res[3] = res[1] ^ res[2];
        return writeBytes(res, 6);
    }else if(buf[2] == DISC){
        res[2] = DISC;
        res[3] = res[1] ^ res[2];
        return writeBytes(res, 6);
    }else if(buf[2] == CTRLUA){
    
    }else if(buf[2] == I0 || buf[2] == I1){
        int buf2Size = bytes - 5;
        unsigned char buf2[buf2Size];

        for(int i = 4; i < bytes - 1; i++){
            buf2[i - 4] = buf[i];
        }

        int buf3Size;
        unsigned char buf3[buf2Size];
        int removed = 0, flag = 0;
        
        for(int i = 0; i < buf2Size; i++){
            if(buf2[i - removed] == 0x7D){
                flag = 1;
                removed++;
            }else if((buf2[i] == 0x5E) && flag){
                buf3[i - removed] = 0x7E;
                flag = 0;
            }else if((buf2[i] == 0x5D) && flag){
                buf3[i - removed] = 0x7D;
                flag = 0;
            }else{
                buf3[i - removed] = buf2[i]; 
            }
        }

        buf3Size = buf2Size - removed;
        uint8_t bcc2 = buf3[0];

        for(int i = 1; i < buf3Size - 1; i++){
            bcc2 = bcc2 ^ buf3[i];
        }

        if(bcc2 == buf3[buf3Size - 1]){
            printf("\nlink\n");
            for(int i = 0; i < buf3Size; i++){
                packet[i] = buf3[i];
                printf("0x%02X\n", packet[i]);
            }

            if(buf[2] == I0){ res[2] = RR1; }else{ res[2] = RR0; }
            res[3] = res[1] ^ res[2];
            
            writeBytes(res, 6);
            return buf3Size;
        }else{
            if(buf[2] == I0){ res[2] = REJ0; }else{ res[2] = REJ1; }
            res[3] = res[1] ^ res[2];
            writeBytes(res, 6);

            return 0;
        }
    }
    
    return 0;
}



////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    unsigned char disc[5] = {FLAG, ANSSEN, C_DISC, ANSSEN ^ C_DISC, FLAG};
    unsigned char UA[5] = {FLAG, ANSREC, C_UA, ANSREC ^ C_UA, FLAG};
    unsigned char buf[5] = {0};
    int bytesRead = 0;

    if (role == LlTx) {

        alarmCount = 0;

        while (alarmCount < MAX_ALARM_COUNT) {
            if (writeBytes(disc, 5) < 0) { //FIXME: warning: pointer targets in passing argument 1 of ‘writeBytes’ differ in signedness
                return -1;
            }

            alarm(3);     
            bytesRead = 0;

            while (bytesRead < 5) {
                bytesRead += readByte(&buf[bytesRead]); //FIXME: warning: pointer targets in passing argument 1 of ‘readBytes’ differ in signedness
            }

            if (buf[0] == FLAG && buf[1] == ANSREC && buf[2] == C_DISC && (buf[3] == (ANSSEN ^ C_DISC)) && buf[4] == FLAG) {
                printf("Received DISC\n");
                alarm(0);
                if (writeBytes(UA, 5) < 0) { //FIXME: warning: pointer targets in passing argument 1 of ‘writeBytes’ differ in signedness
                    return -1;
                }
                printf("UA response sent\n");
                break;
                
            } else {
                printf("DISC not received, retrying...\n");
                alarmCount++;
            }

        } if (alarmCount >= MAX_ALARM_COUNT) {
            printf("Max retry limit reached. Exiting...\n");
            return -1; 
        }

    } else { //llrx
        if (closeSerialPort() < 0) {
            return -1;
        }
        return 1;
    }

    /*lltx
        send disc
        await response
        send ua
        close
    llrx
        close
    */

    int clstat = closeSerialPort();
    return clstat;
}
