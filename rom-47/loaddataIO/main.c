/**********************************************************************
  
loaddataIO: 	a utility to down load ASCII file in srec format to DATA
		I/O 288 eprom programmer.  It is designed to replace the
		IBM PC used to do the job.
		
  
  
   
  
	---------- CHANGE HISTORY ----------
	
11/07/88  blaw  
    New!
08/15/90  blaw
    Changed to use 1Mbit part instead.
    Also it will work with the new DataIO 288 firmware.
09/25/90  blaw
    Fixed a bug in execmd which causes data I/O to hang.
    Don't understand why did it work before.
    Added 19200 bauds support.
  
***********************************************************************/

#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sgtty.h>
#include <sys/ttydev.h>

#define SWTRACE 1

static int     execmd(int fd, char cmd[], char *commentstr, int readtillend)
{
#define MAXBUFSIZE 80

    int     wrbytcnt;
    int     failcode;
    int     bytesRead;
    char    myreadbuf[MAXBUFSIZE];

    wrbytcnt = strlen(cmd);
    strcpy(myreadbuf, "");

    if (SWTRACE)
	printf("%s\n\tcommand code:%s\n", commentstr, cmd);
    failcode = write(fd, cmd, wrbytcnt);
    if (failcode < 0)
	printf("write failcode: %d\n", failcode);

    bytesRead = read(fd, myreadbuf, MAXBUFSIZE);
    
    while (myreadbuf[bytesRead-1] == '\000')
        bytesRead--;
    if (myreadbuf[bytesRead-1] == '\n')
        bytesRead--;
	
    myreadbuf[bytesRead] = '\0';
    if (SWTRACE)
	printf("\trespond:%s\n", myreadbuf);

    while (readtillend)
	switch (myreadbuf[bytesRead - 1]) {
	case '>':
	case 'F':
	case '?':
	    readtillend = 0;
	    break;
	default:
	    bytesRead = read(fd, myreadbuf, MAXBUFSIZE);
	    myreadbuf[bytesRead] = '\0';
	    printf(myreadbuf);
	};

    if (myreadbuf[bytesRead - 1] == 'F')
	return (-1);
    else
	return (bytesRead);
}



static void    Open_serial_driver(int *fd, int speedcode)
{
#define serial_driver "/dev/ttya"
    struct sgttyb mysgttyb;
    short int old_sg_flags;

    *fd = open(serial_driver, O_RDWR);
    ioctl(*fd, TIOCGETP, &mysgttyb);
    old_sg_flags = mysgttyb.sg_flags;
    mysgttyb.sg_flags = mysgttyb.sg_flags & ~ECHO;

    mysgttyb.sg_ispeed = speedcode;
    mysgttyb.sg_ospeed = speedcode;

    ioctl(*fd, TIOCSETP, &mysgttyb);

    execmd(*fd, "\033", "Escape code to wake up programmer", 1);
    execmd(*fd, "H\r", "Syn computer & programmer", 1);
    execmd(*fd, "00^\r", "zero entire RAM", 1);
/*
    execmd(*fd, "082A\r", "select data control & format code", 1);
    execmd(*fd, "[\r", "get family and pinout code", 1);
*/
}


static void    Dump_data(fd, source)
    int     fd;
    FILE   *source;
{
#define MAXLINESIZE 80

    int     wrbytcnt;
    int     failcode;
    char    linebuf[MAXLINESIZE];
    int     bytesRead;
    int     i=0;

    execmd(fd, "087A\r", "Input data format code", 1);
    printf("Loading file to the programmer, will take awhile...\n");
    strcpy(linebuf, "I\r");
    failcode = write(fd, linebuf, strlen(linebuf));
    for (linebuf[0] = '\0'; fgets(linebuf, MAXLINESIZE, source) != NULL;) {
	if (i++ == 10) {
	    i=0;
	    fprintf(stdout, ".");
	    fflush(stdout);
	    }
	failcode = write(fd, linebuf, strlen(linebuf));
    }
    printf("\n");
    
    bytesRead = read(fd, linebuf, MAXBUFSIZE);
    linebuf[bytesRead] = '\0';
    if (SWTRACE)
	printf("\trespond:%s\n", linebuf);
    
}

static void    Program_EPROM(fd)
    int     fd;
{
    execmd(fd, "12B0CB@\r", "Set family/pinout code for TI 27C010 part", 1);
    execmd(fd, "S\r", "SumCheck, wait...", 1);
    execmd(fd, "P\r", "Program the EPROM", 1);
}

main(int argc, char **argv)
{
    char   *progName, *srecFileName;
    FILE   *source;
    int     fd;
    int     speedcode=B9600;

    progName = *argv++;
    switch (argc) {
    case 2:
	srecFileName = *argv++;
	break;
    case 3:
	srecFileName = *argv++;
	if (*argv[0] == 'f') {
	    speedcode = EXTA;
	    printf("Baud rate set to: 19200\n");
	    break;
	}
    default:
	fprintf(stderr, "This utility program a TI27C010 1Mbit EPROM given a srec file\n");
	fprintf(stderr, "Usage:\n\t%s srecFileName [f]\n", progName);
	fprintf(stderr, "\tf -- optinal fast speed (19200 bauds)\n");
	fprintf(stderr, "\t     must manually set the Data I/O to match this speed\n");	
	fprintf(stderr, "Note:\n");
	fprintf(stderr, "\tPut Data I/O 288 in computer control mode\n");
	fprintf(stderr, "\tMake sure cable is connected to /dev/ttya port\n");
	fprintf(stderr, "\tRemember to latch the EPROM(s)\n");	
	exit(1);
    }
    
    
    if (SWTRACE)
	fprintf(stdout, "srecFileName: %s\n", srecFileName);
    if ((source = fopen(srecFileName, "r")) == NULL) {
	fprintf(stderr, "Can not open file: %s \n", srecFileName);
	exit(1);
    }
    Open_serial_driver(&fd, speedcode);
    Dump_data(fd, source);
    Program_EPROM(fd);
}


