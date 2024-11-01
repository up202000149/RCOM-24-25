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

#define MAX_ALARM_COUNT 3
#define FALSE 0
#define TRUE 1
#define BUF_SIZE 256

int alarmEnabled = FALSE;
int alarmCount = 0;
volatile int STOP = FALSE;

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

enum State { FLAG_REC, A_REC, C_REC, BCC_OK, FLAG_REC_END };
enum ReadState {START, FLAG_REC, A_REC, CS_REC, CI_REC, BCCS_OK, BCCI_OK, CONTENT_REC, STOP};

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
    alarmCount++;
    if (alarmCount < MAX_ALARM_COUNT) {
        buf[0] = FLAG;
        buf[1] = ANSSEN;
        buf[2] = CTRLSET;
        buf[3] = ANSSEN ^ CTRLSET;
        buf[4] = FLAG;

        writeBytes(buf, 5);

        alarm(3);       
    } else {
        printf("Max retry limit reached. Exiting...\n");
        exit(1); 
    }
}

int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        return -1;
    }

    unsigned char buf[BUF_SIZE + 1] = {0}; 

    if(connectionParameters.role==LlTx){
        buf[0] = FLAG;
        buf[1] = ANSSEN;
        buf[2] = CTRLSET;
        buf[3] = ANSSEN ^ CTRLSET;
        buf[4] = FLAG;

        writeBytes(buf, 5);

        alarm(3);

        int bytes = 0;

        for (int i = 0; i < 5; i++) {
            bytes += readByte(&buf[i]);
            printf("%d bytes read\n", bytes);
        }

        if (bytes = 5 && buf[0] = FLAG && buf[1] = ANSREC && buf[2] = CTRLUA && buf[3] = ANSREC ^ CTRLUA && buf[4] = FLAG) {
            printf("Success\n");
            alarm(0);
            break;
        }
        else {
            printf("Error\n");
        }
    }

    else {
        enum State curState = FLAG_REC;

        while (!STOP) {
            int bytes = readByte(&buf[0]); 

            if (bytes > 0) {
                processState(&curState, buf[0]);

                if (curState == FLAG_REC_END) {
                    printf("sending UA\n");
                    unsigned char response[5] = {FLAG, ANSREC, CTRLUA, ANSREC ^ CTRLUA, FLAG};
                    writeBytes(response, 5);
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
    int n;

    if(buf[0] == 2){
        n = buf[1] % 2; //TODO: make work for other control fields
        
        //-----------buf2------------
        
        unsigned char buf2[bufSize + 1];
    
        for(int i = 0; i < bufSize; i++){
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
    
        writeBytes(buf4, buf4Size);
        
        readByte
    }

    // TODO: receive cycle
    

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{  
    enum ReadState state = START;
    int bytes = 0;
    unsigned char buf[], res[5];//TODO: needs buf size
    uint8_t cur;

    while (state != STOP)
    {   
        if(0 > readByte(cur)) break;
        
        switch (state)
        {
        case START:
            if(cur == FLAG){
                state = FLAG_REC;
                buf[bytes++] = cur;
            }
            break;
        case FLAG_REC:
            if(cur == (ANSREC || ANSSEN)){
                state = A_REC;
                buf[bytes++] = cur;
            }
            else if(cur == FLAG){
                state = FLAG_REC;
                bytes = 1;
            }
            else{
                state = START;
                bytes = 0;
            }
            break;
        case A_REC:
            if(cur == (CTRLSET || CTRLUA || DISC)){
                state = CS_REC;
                buf[bytes++] = cur;
            }
            else if(cur == (I0 || I1)){
                state = CI_REC;
                buf[bytes++] = cur;
            }
            else if(cur == FLAG){
                state = FLAG_REC;
                bytes = 1;
            }
            else{
                state = START;
                bytes = 0;
            }
            break;
        case CS_REC:
            if(cur == buf[bytes - 1] ^ buf[bytes - 2]){
                state = BCCS_OK;
                buf[bytes++] = cur;
            }
            else{
                printf("bcc1 rejected");
                state = START;
                bytes = 0;
            }
            break;
        case CI_REC:
            if(cur == buf[bytes - 1] ^ buf[bytes - 2]){
                state = BCCI_OK;
                buf[bytes++] = cur;
            }
            else{
                printf("bcc1 rejected");
                state = START;
                bytes = 0;
            }
            break;
        case BCCS_OK:
            if(cur == FLAG){
                state = STOP;
                buf[bytes] = cur;
            }
            else{
                state = START;
                bytes = 0;
            }
            break;
        case BCCI_OK:
            if(cur == FLAG){
                state = FLAG_REC;
                bytes = 1;
            }
            else{
                state = CONTENT_REC;
                buf[bytes++] = cur;
            }
            break;
        case CONTENT_REC:
            if(cur == FLAG){
                state = STOP;
                buf[bytes] = cur; 
            }
            else{
                state = CONTENT_REC;
                buf[bytes++] = cur;
            }
            break;
        }
    }
    
    if(buf[2] == (I0 || I1)){
        int buf2Size = bytes - 4;
        unsigned char buf2[buf2Size];

        for(int i = 0; i < buf2Size; i++){
            buf2[i] = buf[i + 4];
        }

        int buf3Size;
        unsigned char buf3[buf2Size];
        int removed = 0, flag = 0;

        for(int i = 0; i < buf2Size; i++){
            if(buf2[i] == 0x7D){
                flag = 1;
                removed++;
            }else if(buf2[i] == 0x5E){
                buf3[i - removed] = 0x7E;
            }else if(buf2[i] == 0x5D){
                buf3[i - removed] = 0x7D;
            }else{
                buf3[i - removed] = buf2[i]; 
            }
        }

        buf3Size = buf2Size - removed;
        uint8_t bcc2 = 0xFF;
        
        for(int i = 0; i < buf2Size - 1; i++){
            bcc2 = bcc2 ^ buf3[i];
        }
    
        res[0] = FLAG;
        res[1] = ANSREC;
        res[4] = FLAG;

        if(bcc2 == buf3[buf3Size]){
            for(int i = 0; i < buf2Size; i++){
                packet[i] = buf3Size[i];
            }

            if(buf[2] == I0){ res[2] = RR1; }else{ res[2] = RR0; }
            res[3] = res[1] ^ res[2];
            
            
        }else{
            if(buf[2] == I0){ res[2] = REJ0; }else{ res[2] = REJ1; }
            res[3] = res[1] ^ res[2];
        }
        
        writeBytes(res, 5);
    }else if(buf[2] == DISC){ // TODO: send disc message
        writeBytes(buf, 5);
    }else if(buf[2] == CTRLSET){

    }else if(buf[2] == CTRLUA){ // TODO: quit read cycle
        
    }

    return 0;
}



////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    /*
    lltx
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
