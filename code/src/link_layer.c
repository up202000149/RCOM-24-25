// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        return -1;
    }
    unsigned char buf[BUF_SIZE + 1] = {0};
    
    switch (connectionParameters.role)
    {
    case LlRx:
        readByte(buf);
        printf("var = 0x%02X\n", buf[0]);
        printf("var = 0x%02X\n", buf[1]);
        printf("var = 0x%02X\n", buf[2]);
        printf("var = 0x%02X\n", buf[3]);
        printf("var = 0x%02X\n", buf[4]);
        buf[2] = UA;
        buf[3] = buf[1] ^ buf[2];

        writeBytes(buf, BUF_SIZE);
        break;
    case LlTx:
        buf[0] = FLAG;
        buf[1] = ADDR_SEN;
        buf[2] = SET;
        buf[3] = ADDR_SEN ^ SET;
        buf[4] = FLAG;

        writeBytes(buf, BUF_SIZE);
        readByte(buf);
        printf("var = 0x%02X\n", buf[0]);
        printf("var = 0x%02X\n", buf[1]);
        printf("var = 0x%02X\n", buf[2]);
        printf("var = 0x%02X\n", buf[3]);
        printf("var = 0x%02X\n", buf[4]);
        break;
    }
    /* TODO:
        
    */

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    /* TODO:
    generate bcc2
    implement n+1 stuffing
    create i frame
    send frame
    enter read cycle
    */

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    /* TODO:
    
    */

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    /* TODO:
    */

    int clstat = closeSerialPort();
    return clstat;
}

