/*
 *  HISTORY
 *
 *  90.06.23  John Seamons (jks) at NeXT
 *		Modified to work with 1 Mbit ROMs and the ROM address space on
 *		040 based machine where the upper address bits are non-zero.
 *
 *  89.09.01  Bassanio (blaw) at NeXT
 *		Created
 *
 */


#include <stdio.h>

typedef unsigned long int INT_U4B;	/* integer, unsigned, 4 bytes long */

void        errorexit_usage(char *progName)
#define USAGE "filename"
#define USAGE_EG "mon.srec"
{
                fprintf(stderr, "  %s", progName);
    fprintf(stderr, " %s\n", USAGE);
    fprintf(stderr, "  e.g.  %s", progName);
    fprintf(stderr, " %s\n", USAGE_EG);
    exit(1);
}


void        errorexit_file(char *fileName)
{
                fprintf(stderr, "  Can not open: %s\n", fileName);
    exit(1);
}

void        errorexit_general(char *msg)
{
                fprintf(stderr, msg);
    exit(1);
}

#define MAXLINESIZE 150
#define SHEADER "S3"
#define N_SHEADER 2
/* (32 * 1024 * 4) = 128 KiloBytes */
#define KB128 32768

INT_U4B	buffer[KB128];

main(argc, argv)
    int         argc;
    char      **argv;
{
    char       *progName, *srecFileName;
    char        tempstr[40];
    FILE       *source;
    INT_U4B     *bufptr, value;
    char        linebuf[MAXLINESIZE], *lineptr;
    int         end, nBytes;
    register int i;
    long        dAdr;


    progName = *argv++;
    if (argc != 2)
	errorexit_usage(progName);

    srecFileName = *argv++;
    if ((source = fopen(srecFileName, "r")) == NULL)
	errorexit_file(srecFileName);

    for (i = 0; i < KB128; i++)
	buffer[i] = 0;

    for (linebuf[0] = '\0'; fgets(linebuf, MAXLINESIZE, source) != NULL;) {
	if (strncmp(linebuf, SHEADER, N_SHEADER) == 0) {
	    sscanf(&linebuf[N_SHEADER], "%2X%8X", &nBytes, &dAdr);
	    nBytes -= 5;   /* subtract address & sumcheck bytes */
	    bufptr = &buffer[0];
	    bufptr += (dAdr / 4);
	    lineptr = &linebuf[N_SHEADER + 10];
	    for (i = 0, end = nBytes / 4; i < end; i++) {
		sscanf(lineptr, "%8X", bufptr);
		bufptr++;
		lineptr += 8;
	    }
	}
    }

    {
	INT_U4B     sumCheck;
	unsigned char *bytesptr;

	bytesptr = (unsigned char *)buffer;
	for (i = 0, sumCheck = 0; i < (KB128 * 4); i++)
	    sumCheck += *bytesptr++;
	printf("  Byte-wide hex sum (should match DATA I/O 288): %08X\n", sumCheck);
    }

    {
	INT_U4B     resetPC, crcValue;
	int         bytesCnt;
	unsigned char *bytesptr;

	resetPC = buffer[1] & 0xffffff;		/* ignore upper address bits */
	{
	    bytesCnt = resetPC - 8;
	    bytesptr = (unsigned char *)buffer;

	    crcValue = calculateCRC(bytesptr, bytesCnt);
	    printf("  Ethernet hex CRC: %08X\n", crcValue);
	}
	{
	    bytesCnt = KB128 * 4 - resetPC;
	    bytesptr = (unsigned char *)buffer;

	    bytesptr += resetPC;

	    crcValue = calculateCRC(bytesptr, bytesCnt);
	    printf("  Monitor hex CRC: %08X\n", crcValue);
	}


    }
}

