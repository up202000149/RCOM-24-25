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

#define FALSE 0
#define TRUE 1
#define BUF_SIZE 256

int alarmEnabled = FALSE;
int alarmCount = 0;
volatile int STOP = FALSE;
LinkLayerRole role;
int packetCount;
int retransmissions, timeouts, max_tries, max_timeout;
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

enum State { FLAG_REC, A_REC, C_REC, BCC_OK, FLAG_REC_END };
enum ReadState {START, RFLAG_REC, RA_REC, CS_REC, CI_REC, BCCS, BCCI, CONTENT_REC, END};

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

    (void)signal(SIGALRM, alarmHandler);

    role = connectionParameters.role;
    max_tries = connectionParameters.nRetransmissions;
    max_timeout = connectionParameters.timeout;

    packetCount = 0;
    retransmissions = 0;
    timeouts = 0;

    unsigned char buf[257] = {0}; 

    if(role == LlTx){
        buf[0] = FLAG;
        buf[1] = ANSSEN;
        buf[2] = CTRLSET;
        buf[3] = ANSSEN ^ CTRLSET;
        buf[4] = FLAG;

        writeBytes(buf, 5); //FIXME: warning: pointer targets in passing argument 1 of ‘writeBytes’ differ in signedness

        alarm(max_timeout);

        int bytes = 0;

        for (int i = 0; i < 5; i++) {
            bytes += readByte(&buf[i]);
        }
        printf("%d bytes read\n", bytes);
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
    
    (void)signal(SIGALRM, alarmHandler);
       
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
    
        unsigned char res[6];
        unsigned char cur;
        int bytesRead = 0;
        alarmCount = 0;
        enum ReadState state = START;

        while (alarmCount < max_tries) {
            written = writeBytes(buf4, buf4Size);
            sleep(1);
            if (written < 0) {
                return -1;
            }
            
            if (alarmEnabled == FALSE){
                alarm(max_timeout);
                alarmEnabled = TRUE;
            }    
            bytesRead = 0;

            while (bytesRead < 5) {
                readByte(&cur);

                switch (state)
                {
                case START:
                    if(cur == FLAG){
                        state = RFLAG_REC;
                        bytesRead = 1;
                        res[0] = cur;
                    }
                    break;
                case RFLAG_REC:
                    if(cur == ANSREC || cur == ANSSEN){
                        state = RA_REC;
                        bytesRead = 2;
                        res[1] = cur;
                    }else if(cur == FLAG){
                    }else{
                        state = START;
                    }
                    break;
                case RA_REC:
                    if(cur == RR0 || cur == RR1 || cur == REJ0 || cur == REJ1){
                        state = CS_REC;
                        bytesRead = 3;
                        res[2] = cur;
                    }else if(cur == FLAG){
                        state = FLAG_REC;
                    }else{
                        state = START;
                    }
                    break;
                case CS_REC:
                    if(cur == res[1] ^ res[2]){
                        state = BCCS;
                        bytesRead = 4;
                        res[3] = cur;
                    }else if(cur == FLAG){
                        state = FLAG_REC;
                    }else{
                        state = START;
                    }
                    break;
                case BCCS:
                    if(cur == FLAG){
                        state = END;
                        bytesRead = 5;
                        res[4] = cur;    
                    }else{
                        state = START;
                    }
                    break;
                case END:
                    break;
                }
                
            }
            
            if ((res[0] == FLAG) && (res[1] == ANSREC) && ((res[2] == RR0) || (res[2] == RR1)) && (res[3] == (res[1] ^ res[2])) && (res[4] == FLAG)) {
                printf("Received RR\n");
                alarm(0);
                return written;
                
            }else if ((res[0] == FLAG) && (res[1] == ANSREC) && ((res[2] == REJ0) || (res[2] == REJ1)) && (res[3] == (res[1] ^ res[2])) && (res[4] == FLAG)) {
                printf("Received REJ\n");
                alarmCount++;
                break;
                
            }else {
                printf("Response not received, retrying...\n");
                alarmCount++;
                retransmissions++;
            }

        } if (alarmCount >= max_tries) {
            printf("Max retry limit reached. Exiting...\n");
            timeouts++;
            return -1; 
        } 

    return written;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{  
    enum ReadState state = START;
    int bytes = 0, aux = 1;
    unsigned char cur;
    unsigned char buf[8000], res[6];

    res[0] = FLAG;
    res[4] = FLAG;
    res[5] = '\n'; 

    //------------ receive packets --------------

    while (state != END)
    {
        readByte(&cur);
        
        switch (state)
        {
        case START:
            if(cur == FLAG){
                state = RFLAG_REC;
                bytes = 1;
                buf[0] = cur;
            }
            break;
        case RFLAG_REC:
            if(cur == ANSREC || cur == ANSSEN){
                state = RA_REC;
                bytes = 2;
                buf[1] = cur;
            }else if(cur == FLAG){
            }else{
                state = START;
            }
            break;
        case RA_REC:
            if(cur == I0 || cur == I1){
                state = CI_REC;
                bytes = 3;
                buf[2] = cur;
            }else if(cur == C_SET || cur == C_UA || cur == C_DISC){
                state = CS_REC;
                bytes = 3;
                buf[2] = cur;
            }else if(cur == FLAG){
                state = FLAG_REC;
            }else{
                state = START;
            }
            break;
        case CS_REC:
            if(cur == buf[1] ^ buf[2]){
                state = BCCS;
                bytes = 4;
                buf[3] = cur;
            }else if(cur == FLAG){
                state = FLAG_REC;
            }else{
                state = START;
            }
            break;
        case CI_REC:
            if(cur == buf[1] ^ buf[2]){
                state = BCCI;
                bytes = 4;
                buf[3] = cur;
            }else if(cur == FLAG){
                state = FLAG_REC;
            }else{
                state = START;
            }
            break;
        case BCCS:
            if(cur == FLAG){
                state = END;
                bytes = 5;
                buf[4] = cur;    
            }else{
                state = START;
            }
            break;
        case BCCI:
            if(cur == FLAG){
                state = END;
                bytes = 5;
                buf[4] = cur;    
            }else{
                state = CONTENT_REC;
                bytes = 5;
                buf[4] = cur;
            }
            break;
        case CONTENT_REC:
            if(cur == FLAG){
                state = END;
                buf[bytes++] = cur;
            }else{
                state = CONTENT_REC;
                buf[bytes++] = cur;
            }
            break;
        case END:
            break;
        }
    
    }
        
    //------------ make response -------------

    res[1] = buf[1];
    if(buf[2] == CTRLSET){
        res[2] = CTRLUA;
        res[3] = res[1] ^ res[2];
        return writeBytes(res, 6);
    }else if(buf[2] == C_DISC){
        res[2] = C_DISC;
        res[3] = res[1] ^ res[2];
        printf("DISC response sent.\n");
        return writeBytes(res, 6);
    }else if(buf[2] == CTRLUA){
        return -1;
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
            if(buf2[i] == 0x7D){
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
            for(int i = 0; i < buf3Size - 1; i++){
                packet[i] = buf3[i];
            }

            if(buf[2] == I0){ res[2] = RR1; }else{ res[2] = RR0; }
            res[3] = res[1] ^ res[2];
            
            writeBytes(res, 6);
            return buf3Size-1;
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
    unsigned char disc[5] = {FLAG, ANSREC, C_DISC, ANSSEN ^ C_DISC, FLAG};
    unsigned char UA[5] = {FLAG, ANSREC, C_UA, ANSREC ^ C_UA, FLAG};
    unsigned char buf[5];
    unsigned char cur;
    int tries = 0;
    int bytesRead = 0;
    enum ReadState state = START;
    
    if (role == LlTx) {

        (void)signal(SIGALRM, alarmHandler);
        alarmCount = 0;

        if (writeBytes(disc, 5) < 0) {
            return -1;
        }
        sleep(1);

        while (alarmCount < max_timeout) {
            
            if (alarmEnabled == FALSE){
                alarm(3);
                alarmEnabled = TRUE;
            }
            
            bytesRead = 0;
            state = START;

            while (bytesRead < 5) {
                readByte(&cur);

                switch (state)
                {
                case START:
                    if(cur == FLAG){
                        state = RFLAG_REC;
                        bytesRead = 1;
                        buf[0] = cur;
                    }
                    break;
                case RFLAG_REC:
                    if(cur == ANSREC || cur == ANSSEN){
                        state = RA_REC;
                        bytesRead = 2;
                        buf[1] = cur;
                    }else if(cur == FLAG){
                    }else{
                        state = START;
                    }
                    break;
                case RA_REC:
                    if(cur == C_DISC || cur == C_UA){
                        state = CS_REC;
                        bytesRead = 3;
                        buf[2] = cur;
                    }else if(cur == FLAG){
                        state = FLAG_REC;
                    }else{
                        state = START;
                    }
                    break;
                case CS_REC:
                    if(cur == buf[1] ^ buf[2]){
                        state = BCCS;
                        bytesRead = 4;
                        buf[3] = cur;
                    }else if(cur == FLAG){
                        state = FLAG_REC;
                    }else{
                        state = START;
                    }
                    break;
                case BCCS:
                    if(cur == FLAG){
                        state = END;
                        bytesRead = 5;
                        buf[4] = cur;    
                    }else{
                        state = START;
                    }
                    break;
                case END:
                    break;
                } 
            }

            if (buf[0] == FLAG && buf[1] == ANSREC && buf[2] == C_DISC && (buf[3] == (buf[1] ^ buf[2])) && buf[4] == FLAG) {
                printf("Received DISC\n");
                alarm(0);
                if (writeBytes(UA, 5) < 0) {
                    return -1;
                }
                printf("UA response sent\n");
                break;
                
            } else {
                printf("DISC not received, retrying...\n");
                retransmissions++;
                tries++;
                if(tries >= max_tries){
                    printf("Max retry limit reached. Exiting...\n");
                    break;
                }
                if (writeBytes(disc, 5) < 0) {
                    return -1;
                }
                sleep(1);
            }

        } if (alarmCount >= max_timeout) {
            printf("Max time limit reached. Exiting...\n");
            timeouts++;
            if(showStatistics){
                printf("\nNumber of frames: %d\n", packetCount);   
                printf("Number of retransmissions: %d\n", retransmissions);   
                printf("Number of timeouts: %d\n", timeouts);   
            }
            return -1; 
        }

    } else { //llrx
        if (closeSerialPort() < 0) {
            return -1;
        }
        return 1;
    }

    if(showStatistics){
        printf("\nNumber of frames: %d\n", packetCount);   
        printf("Number of retransmissions: %d\n", retransmissions);   
        printf("Number of timeouts: %d\n", timeouts);   
    }

    return closeSerialPort();
}
