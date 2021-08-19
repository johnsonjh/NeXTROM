/*
 * compress.c -- compresses pictures for ROM
 *
 * boot rom video compression and expansion.  Assumes 2 bit graphics, and the
 * compressed runs break at each line, so that a run will never be part of
 * 2 lines.
 *
 * HISTORY
 * 29-May-89  John Seamons (jks) at NeXT
 *	Added sparse compression mode.
 *
 * 11-Aug-88  Joe Becker (jbecker) at NeXT
 *	Created. 
 */

#include <pot/POT.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <nextdev/video.h>

#define MAX_PICTURE_SIZE 256*256

u_char         *expanded;

struct compressed_picture {
    char            type, spare[3];
    int             row_length, num_rows;
};

main(argc, argv)
    int             argc;
    char           *argv[];

{
    int		    readInput();
    int             sizePic();

    u_char         *start, *scan, *input;
    u_int           i;
    int             inSize, outSize, ok;
    struct compressed_picture *pic;
    float           compression, temp;
    char            type;

    start = input = (u_char *) malloc(MAX_PICTURE_SIZE);
    if (input == 0) {
    	fprintf(stderr,"Couldn't malloc space for input buffer\n");
	exit(-1);
    }
    
    pic = (struct compressed_picture *) malloc(5 * MAX_PICTURE_SIZE);
    if (pic == 0) {
    	fprintf(stderr,"Couldn't malloc space for output buffer\n");
	exit(-1);
    }
    
    inSize = readInput(argc, argv,input);
    
    for (i = 0; i < inSize; i++)
	*start++ = ~*start;	/* h/w has 00 = white 11 = black ... */

    fprintf(stderr, "How many pixels in each row? ");
    scanf("%d", &pic->row_length);
    pic->num_rows = (4 * inSize) / pic->row_length;

    type = 'U';
    compression = 1.0;

    pic->type = 'B';
    byteCompressPicture(input, pic);
    outSize = sizePic(pic);
    temp = ((float)inSize) / outSize;
    if (temp > compression) {
	compression = temp;
	type = 'B';
    }
#if 0
    pic->type = 'P';
    pixelCompressPicture(input, pic);
    outSize = sizePic(pic);
    temp = ((float)inSize) / outSize;
    if (temp > compression) {
	compression = temp;
	type = 'P';
    }
#endif
    pic->type = 'S';
    ok = sparseCompressPicture(input, pic);
    outSize = sizePic(pic);
    temp = ((float)inSize) / outSize;
    if (ok && temp > compression) {
	compression = temp;
	type = 'S';
    }
    fprintf(stderr, "Using compression type %c\n", type);
    switch (type) {
    case 'U':
	pic->type = 'U';
	outSize = inSize;
	scan = (u_char *)(pic+1);
	while (outSize--)
	    *scan++ = *input++;
	outSize = inSize + sizeof(struct compressed_picture);
	break;
    case 'B':
	pic->type = 'B';
	byteCompressPicture(input, pic);
	outSize = sizePic(pic);
	break;
    case 'P':
	pic->type = 'P';
	pixelCompressPicture(input, pic);
	outSize = sizePic(pic);
	break;
    case 'S':
	pic->type = 'S';
	sparseCompressPicture(input, pic);
	outSize = sizePic(pic);
	break;
    default:
	fprintf(stderr, "eh?, undefined compression type\n");
	exit(-2);
    }

    fprintf(stderr, "Compression: %f, size: %d bytes\n", compression, outSize);

    start = (u_char *) pic;
    for (i = 0; outSize-- > 0; i++) {
	if ((i % 12) == 0)
		printf ("\n");
	printf ("0x%02x, ", *start++);
    }

#if 0
{
	int fd;
	
	fd = open ("/dev/vid0", 0);
	if (fd < 0) {
		perror ("/dev/vid0");
		exit (-1);
	}
	ioctl (fd, DKIOCGADDR, &expanded);
	drawSparse (440, 176, 100 + 100 * VIDEO_MW, ++pic);
}
#endif

#if 0
	expanded = (u_char *) malloc(256 * 256);
	drawCompressedPixels(0, 0, pic);
	while (inSize-- > 0)
	    if (*expanded++ != *input++) {
		fprintf(stderr, "Expansion Failed\n");
		exit(-3);
	    }
#endif
    exit(0);
}


int
readInput(argc, argv,input)
    int             argc;
    char           *argv[];
    u_char	   *input;

{
    int             inSize;
    struct stat     statbuf;
    FILE           *inFile;
    int             c;

    if (argc > 2) {
	fprintf(stderr, "usage: test filename\n", argv[1]);
	return (0);
    }
    if (argc == 2) {

	if (stat(argv[1], &statbuf) < 0) {
	    fprintf(stderr, "Couldn't stat %s\n", argv[1]);
	    exit(-1);
	}
	inSize = statbuf.st_size;
	if (inSize > MAX_PICTURE_SIZE) {
	    fprintf(stderr, "Input picture must have less than %d pixels\n",
		    MAX_PICTURE_SIZE);
	    exit(-1);
	}
	if ((inFile = fopen(argv[1], "r")) == NULL) {
	    exit(-1);
	}
	if (inSize != fread(input, 1, inSize, inFile)) {
	    fprintf(stderr, "Couldn't fread %ld bytes from %s\n", inSize, argv[1]);
	    exit(-2);
	}
    } else {
	inSize = 0;
	while ((c = getchar()) != EOF) {
	    *input++ = c;
	    inSize++;
	    if (inSize > MAX_PICTURE_SIZE) {
		fprintf(stderr, "Input picture must have less than %d pixels\n",
			MAX_PICTURE_SIZE);
		exit(-1);
	    }
	}
    }
    return (inSize);
}


int
sizePic(pic)
    struct compressed_picture *pic;
{
    int             size = sizeof(struct compressed_picture), *scan;

    pic++;
    scan = (int *)pic;
    while (*scan++ != 0)
	size += sizeof(int);
    return (size);
}


/*
 * UGLY, UGLY hacking to squeeze picture into boot rom... boot rom video
 * compression and expansion.  Assumes 2 bit graphics, and the compressed
 * runs break at each line, so that a run will never be part of 2 lines 
 */


struct compressed_group {
    u_char          count[4];
    u_char          value;	/* four 2 bit pixels packed into a byte */
};

/* FIXME: sizeof macro returns word aligned, but ROM data is packed */

#define TRUE_SIZE_OF_GROUP 5

/* FIXME: next 6 lines are only for debugging the expansion */
#undef	P_VIDEOMEM
#undef	VIDEO_W
#undef	VIDEO_UNSEEN
#define P_VIDEOMEM  (int)expanded
#define VIDEO_W  0
#define VIDEO_UNSEEN  -1200

drawCompressedBytes(x0, y0, picture)
    u_int           x0, y0;
    u_char         *picture;
{
    int             pixel;
    int             row_length, num_rows, row, col;
    u_char          count, value;

    picture += sizeof(u_int);	/* ignore type stuff */

/* Find left edge of picture and right edge of screen */

    pixel = x0 + y0 * VIDEO_W;

/* Make sure we don't draw past the visible region */

    row_length = *(u_int *) picture;
    picture += sizeof(u_int);
    if (row_length + x0 > VIDEO_W - VIDEO_UNSEEN) {
	row_length = VIDEO_W - VIDEO_UNSEEN - x0;
	if (row_length <= 0)
	    return (-1);
    }
    num_rows = *(u_int *) picture;
    picture += sizeof(u_int);
    if (num_rows + y0 > VIDEO_H) {
	num_rows = VIDEO_H - y0;
	if (num_rows <= 0)
	    return (-1);
    }
/* draw each row till out of rows... */

    row = col = 0;

    while (row < num_rows) {
	while (col < row_length) {
	    count = *picture++;
	    value = *picture++;
	    storeBytes(&pixel, count, value);
	    if (col + count > row_length) {
		count = row_length - col;
		fprintf(stderr, "Column overrun!!!");
	    }
	    col += count << 2;
	}
	row++;
	col = 0;
    }
    return(0);
}

storeBytes(pixel, count, value)
    register int   *pixel;
    register u_char count, value;
{

    for (; count > 0; count--) {
	*((u_char *) (P_VIDEOMEM + (*pixel >> 2))) = value;
	*pixel += 4;
    }
}

drawSparse(row_length, num_rows, left, data)
	int             row_length, num_rows, left;
	u_char		*data;

{
	u_char          count, value;
	u_int		row, col;
	int		pixel = left, tabsize, i;
	u_char		values[256], runlen[256];
	
	tabsize = *data++;
	for (i = 0; i < tabsize; i++)
		values[i] = *data++,  runlen[i] = *data++;

	/* draw each row till out of rows... */
	row = col = 0;

	while (row < num_rows) {
		while (col < row_length) {
			i = *data++;
			count = runlen[i];
			value = values[i];
			storeBytes(&pixel, count, value);
			col += count<<2;
		}
		row++;
		col = 0;
		pixel = left += VIDEO_MW;
	}
	return(0);
}

u_char
countByteRun(data, pixel, length, runValue)
    register u_int   *pixel;
    register u_char *data, *runValue;
    register int *length;
{

    register u_char count = 0;

    *runValue = data[*pixel >> 2];
    while (*length > 0) {
	if (*runValue ^ data[*pixel >> 2])
	    break;
	count++;
	*length -= 4;
	*pixel += 4;
	if (count == 127)
	    break;
    }
    return (count);
}



byteCompressPicture(data, pic)
    u_char         *data, *pic;
{

/*
 * ASSUMES that pic points to enough space to hold the compressed picture;
 * allocating the size of the raw picture is probably good enough; worst
 * case is compressed picture 5 times larger than raw if every pixel is
 * different...  
 *
 */

    u_char          runValue;
    int		    length, row_length;
    u_int           row, pixel, num_rows;

    pic += 4;			/* skip type bytes */

    row_length = *(int *) pic;
    pic += sizeof(u_int);

    num_rows = *(u_int *) pic;
    pic += sizeof(u_int);

    row = pixel = 0;

    while (row < num_rows) {

	length = row_length;

	while (length > 0) {
	    *pic++ = countByteRun(data, &pixel, &length, &runValue);
	    *pic++ = runValue;
	}
	row++;
    }

    for (length = 20; length-- > 0;*pic++ = 0);		/* terminate list */
}

sparseCompressPicture(data, pic)
    u_char         *data, *pic;
{

/*
 * ASSUMES that pic points to enough space to hold the compressed picture;
 * allocating the size of the raw picture is probably good enough; worst
 * case is compressed picture 5 times larger than raw if every pixel is
 * different...  
 *
 */

    u_char          runValue;
    int		    length;
    u_int           row, pixel, row_length, num_rows;
    u_int	    run, i, j, x;
    u_short	    bucket[256][256];

    bzero (bucket, sizeof (bucket));
    pic += 4;			/* skip type bytes */

    row_length = *(u_int *) pic;
    pic += sizeof(u_int);

    num_rows = *(u_int *) pic;
    pic += sizeof(u_int);

    row = pixel = 0;
    while (row < num_rows) {

	length = row_length;

	while (length > 0) {
	    run = countByteRun(data, &pixel, &length, &runValue);
	    bucket[runValue][run]++;
	}
	row++;
    }
    x = 0;
    for (i = 0; i < 256; i++)
    	for (j = 0; j < 256; j++)
		if (bucket[i][j])
			bucket[i][j] = x++ | 0x0100;
    fprintf (stderr, "sparse compression table size %d\n", x);
    if (x > 256)
	return (0);
    *pic++ = x;
    for (i = 0; i < 256; i++)
    	for (j = 0; j < 256; j++)
		if (bucket[i][j])
			*pic++ = i,  *pic++ = j;

    row = pixel = 0;
    while (row < num_rows) {

	length = row_length;

	while (length > 0) {
	    run = countByteRun(data, &pixel, &length, &runValue);
	    *pic++ = bucket[runValue][run] & 0xff;
	}
	row++;
    }

    for (length = 20; length-- > 0;*pic++ = 0);		/* terminate list */
    return (1);
}


drawCompressedPixels(x0, y0, picture)
    int             x0, y0;
    struct compressed_picture *picture;
{
    int             pixel;
    int             row_length, num_rows, row, col, element;
    u_char          count, value;
    struct compressed_group *group = (struct compressed_group *) (picture + 1);

/* Find left edge of picture and right edge of screen */

    pixel = x0 + y0 * VIDEO_W;


/* Make sure we don't draw past the visible region */

    row_length = picture->row_length;
    if (row_length + x0 > VIDEO_W - VIDEO_UNSEEN) {
	row_length = VIDEO_W - VIDEO_UNSEEN - x0;
	if (row_length <= 0)
	    return (-1);
    }
    num_rows = picture->num_rows;
    if (num_rows + y0 > VIDEO_H) {
	num_rows = VIDEO_H - y0;
	if (num_rows <= 0)
	    return (-1);
    }
/* draw each row till out of rows... */

    row = col = element = 0;

    while (row < num_rows) {
	while (col < row_length) {
	    value = (group->value >> (6 - 2 * element)) & 3;
	    count = group->count[element++];
	    if (element > 3) {
		element = 0;
		group++;
	    }
	    drawPixels(&pixel, count, value);
	    if (col + count > row_length) {
		count = row_length - col;
		fprintf(stderr, "Column overrun!!!");
	    }
	    col += count;
	}
    /* FIXME: following line is commented out for debug only */
    /* pixel = left += VIDEO_W; */
	row++;
	col = 0;
    }
    return(0);
}



drawPixels(pixel, count, value)
    register int   *pixel;
    register u_char count, value;
{
/*
 * Sleazy routine to expand a compressed picture; should 
 *
 * 1) draw the 0-3 pixels to a byte boundary in VRAM 2) draw a byte of values
 * till <4 pixels remain 3) draw the 0-3 remaining pixels... 
 *
 * I'll fix all that once we get the basics working... 
 *
 * Routine uses the fact that if there are 4 pixels per byte, pixel/4 is byte
 * offset from start of VRAM, and the 2 bit field starts 2*(pixel %4) bits
 * right from the byte boundary. 
 *
 * Don't forget that pixel INCLUDES NON_VISIBLE PIXELS. 
 */

    register u_char *adr, offset;
    register u_int  orValue = value << 8;

    for (; count > 0; count--) {
	adr = (u_char *) (P_VIDEOMEM + (*pixel >> 2));
	offset = (1 + (*pixel & 0x3)) << 1;
	*adr = (((*adr << offset) & ~0x300) | orValue) >> offset;
	++*pixel;
    }
}



u_char
countPixelRun(data, pixel, length, runValue)
    register u_int   *pixel;
    register u_char *data, *runValue;
    register int *length;
{

#define	PIXEL_VALUE(DATA,PIXEL) (u_char)(DATA[PIXEL>>2]>>((3-(PIXEL&0X3))<<1)&0x3)

    register u_char count = 0;

    *runValue = PIXEL_VALUE(data, *pixel);
    while (*length > 0) {
	if (*runValue ^ PIXEL_VALUE(data, *pixel))
	    break;
	count++;
	--*length;
	++*pixel;
	if (count == 255)
	    break;
    }
    return (count);
#undef PIXEL_VALUE
}



pixelCompressPicture(data, pic)
    u_char         *data;
    struct compressed_picture *pic;
{

/*
 * ASSUMES that pic points to enough space to hold the compressed picture;
 * allocating the size of the raw picture is probably good enough; worst
 * case is compressed picture 5 times larger than raw if every pixel is
 * different...  
 *
 */

    u_char          runValue;
    int		    length;
    u_int           row, pixel, element;
    struct compressed_group *group = (struct compressed_group *) (pic + 1);
    char	    *scan;

    element = row = pixel = 0;

    while (row < pic->num_rows) {

	length = pic->row_length;

	while (length > 0) {

	    group->count[element] = countPixelRun(data, &pixel, &length, &runValue);
	    group->value = (group->value << 2) | runValue;

	    if (++element > 3) {
		element = 0;
		group++;
	    }
	}

	row++;
    }

    while (element != 0) {	/* zero pad list */
	group->count[element++] = 0;
	group->value = (group->value << 2) | 0;
	if (element > 3) {
	    group++;
	    break;
	}
    }

    scan = (char *)group;
    for (length = 20; length-- > 0;*scan++ = 0);		/* terminate list */
}

