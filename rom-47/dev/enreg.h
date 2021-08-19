/*
 * Register description for the Fujitsu Ethernet Data Link Controller (MB8795)
 * and the Fujitsu Manchester Encoder/Decoder (MB502).
 */

/* The Transmit Status register (Address 0) */

#define EN_TXSTAT_READY		0x80		/* Ready for Packet (Not used)*/
#define EN_TXSTAT_BUSY		0x40		/* Receive Carrier Detect */
#define EN_TXSTAT_TXRECV	0x20		/* Last transmission received */
#define EN_TXSTAT_SHORTED	0x10		/* Possible Coax Short */
#define EN_TXSTAT_UNDERFLOW	0x08		/* Data underflow on Xmit */
					/* A write clears status bit */
#define EN_TXSTAT_COLLERR	0x04		/* Collision detected */
					/* A write clears status bit */
#define EN_TXSTAT_COLLERR16	0x02		/* 16th Collision error */
					/* A write clears status bit */
#define EN_TXSTAT_PARERR	0x01		/* Parity Error in TX Data */
					/* A write clears status bit */

#define EN_TXSTAT_CLEAR		0xff

/* The Transmit Mask register (Address 1) (All RW) */
#define EN_TXMASK_READYIE	0x80		/* Tx Int on packet ready */
#define EN_TXMASK_TXRXIE	0x20		/* Tx Int on transmit recv'd */
#define EN_TXMASK_UNDERFLOWIE	0x08		/* Tx Int on underflow */
#define EN_TXMASK_COLLIE	0x04		/* Tx Int on collision */
#define EN_TXMASK_COLL16IE	0x02		/* Tx Int on 16th collision */
#define EN_TXMASK_PARERRIE	0x01		/* Tx Int on parity error */

/* Receiver Status register (Address 2) */

#define EN_RXSTAT_OK		0x80		/* Packet received is correct */
					/* A write clears status bit */
#define EN_RXSTAT_RESET		0x10		/* Reset packet (not used) */
#define EN_RXSTAT_SHORT		0x08		/* < minimum length */
					/* A write clears status bit */
#define EN_RXSTAT_ALIGNERR	0x04		/* Alignment error */
					/* A write clears status bit */
#define EN_RXSTAT_CRCERR	0x02		/* Bad CRC */
					/* A write clears status bit */
#define EN_RXSTAT_OVERFLOW	0x01		/* Receive FIFO overflow */
					/* A write clears status bit */

#define EN_RXSTAT_CLEAR		0xff	/* Clear all bits (really 9f?) */

/* Receiver Mask register (Address 3) (All RW) */
#define EN_RXMASK_OKIE		0x80		/* Rx Int on packet ok */
#define EN_RXMASK_RESETIE	0x10		/* Rx Int on reset packet */
#define EN_RXMASK_SHORTIE	0x08		/* Rx Int on 
#define EN_RXMASK_ALIGNERRIE	0x04		/* Rx Int on align error */
#define EN_RXMASK_CRCERRIE	0x02		/* Rx Int on CRC error */
#define EN_RXMASK_OVERFLOWIE	0x01		/* Rx Int on overflow error */

/* Transmitter Mode register (Address 4) (All RW) */
#define EN_TXMODE_COLLMASK	0xF0		/* (Read only) collisions */
#define EN_TXMODE_PARIGNORE	0x08		/* Ignore parity (not used)*/
#define EN_TXMODE_TM		0x04		/* not used ??? */
#define EN_TXMODE_NO_LBC	0x02		/* Loop back control disabled */
#define EN_TXMODE_DISCONTENT	0x01		/* Disable contention (Rx carrier) */

/* Receiver Mode register (Address 5) (All RW) */
#define EN_RXMODE_TEST		0x80		/* Must be zero for normal op */
#define EN_RXMODE_ADDRSIZE	0x10		/* Reduces NODE match to 5 chars */
#define EN_RXMODE_SHORTENABLE	0x08		/* Rx packets >= 0x10 bytes */
#define EN_RXMODE_RESETENABLE	0x04		/* One for reset pkt enable */
#define EN_RXMODE_PROMISCUOUS	0x03		/* Accept all packets */
#define EN_RXMODE_NORMAL	0x02		/* Accept Broad/multicasts */
#define EN_RXMODE_LIMITED_MULTI	0x01		/* Accept Broad/limited multicasts */
#define EN_RXMODE_OFF		0x00		/* Accept no packets */

/* Reset Control register */
#define EN_RESET		0x80		/* Generate reset */

struct en_regs {
	char	en_txstat;		/* Tx status */
	char	en_txmask;		/* Tx interrupt condition mask */
	char	en_rxstat;		/* Rx status */
	char	en_rxmask;		/* Rx interrupt condition mask */
	char	en_txmode;		/* Tx control/mode register */
	char	en_rxmode;		/* Rx control/mode register */
	char	en_reset;		/* Device reset */
	char	en_notused[1];		/* Not used */
	char 	en_enetaddr[6];		/* Physical ethernet address */
};

#define ENTX_EOP		0x80000000
#define ENRX_EOP		0x80000000	/* End of packet */
#define ENRX_BOP		0x40000000	/* Start of packet */
