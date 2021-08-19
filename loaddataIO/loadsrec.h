#define SWTRACE 1

extern int write(int fd, char *buffer, unsigned nbytes);
extern int read(int fd, char *buffer, unsigned count);
extern int open(char *filename, int mode);
extern int close(int fd);

extern char *strncpy(char *s1, char *s2, int n);
extern char *strcpy(char *to, char *from);
extern int strlen(char *s);
extern char *strcat(char *to, char *from);


extern void char_2h(char *out_str, unsigned int in_char);
extern unsigned char ByteSumcheck(unsigned char *buffer, int length);
extern void InitDataArray(long EtherNet_Adr, unsigned char *DataArray, int *length);
extern void Open_serial_driver(int *fd);
extern int SumCheckEPROM(int fd, char *Expected_sumcheck);
