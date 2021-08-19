/*
 * Definition file for video ROM pcode opcodes.
 */
#ifndef _KM_PCODE_H_
#define _KM_PCODE_H_	1

/* Assorted stuff to read the slot address space and NBIC. */
#define KM_SLOT_ID_REG(slot)	(0xF0FFFFF0 | ((slot) << 24))
#define KM_SLOT_CONSOLE_BITS	0xC0000000		/* ID_VALID | CONSOLE_DEV */

#define KM_SIGNATURE_ADDR	0xF0FFFFE0
#define KM_SIGNATURE_BITS	0xA5000000
#define KM_SIGNATURE_MASK	0xFF000000
	
#define KM_READ_SLOT_BYTE(slot, addr) ((*((int*)((addr)|((slot)<<24))) >> 24) & 0xFF)

#define KM_BYTE_LANE_ID_ADDR	0xF0FFFFD8
#define KM_BYTE_LANE_ID(slot) KM_READ_SLOT_BYTE(slot, KM_BYTE_LANE_ID_ADDR)

#define KM_PCODE_ADDR3		0xF0FFFFB8	/* Abs address of pcode module start */
#define KM_PCODE_ADDR2		0xF0FFFFC0
#define KM_PCODE_ADDR1		0xF0FFFFC8
#define KM_PCODE_ADDR0		0xF0FFFFD0

#define KM_CONVERT_ADDR(slot,addr) \
	((addr)|((slot) << ((((addr) & 0xFF000000)==0xF0000000)?24:28)))

#if defined(MONITOR)
#define KM_ABS_ADDR(addr) \
	((addr)|(mg->km_coni.slot_num << ((((addr) & 0xFF000000)==0xF0000000)?24:28)))
#else
#define KM_ABS_ADDR(addr) \
	((addr)|(km_coni.slot_num << ((((addr) & 0xFF000000)==0xF0000000)?24:28)))
#endif

/* Indices into p-code data structures. */
#define	KMPC_SIZE		0	/* Zeroth word is size of p-code module */
#define KMPC_NUM_FBS		1	/* 1st word is number of FBs on board. */
#define KMPC_TABLE_START	2	/* Start of 1st FB description. */
#define KMPC_INIT_OFF		0	/* Init func ptr within FB description. */
#define KMPC_UNBLANK_SCREEN_OFF	1	/* Unblank_scr func ptr in FB description. */
#define KMPC_START_ACCESS_OFF	2	/* start_access func ptr in FB description. */
#define KMPC_END_ACCESS_OFF	3	/* end_access func ptr in FB description. */
#define KMPC_FB_LAYOUT_OFF	4	/* FB layout ptr within FB description. */
#define KMPC_TABLE_INCR		(KMPC_FB_LAYOUT_OFF+1)	/* Size of 1 FB description. */




/* Offsets in slot address space to extensions */
#define ExtendedROMSignature_OFFSET	0x00FFFFE0
#define ByteLaneID_OFFSET		0x00FFFFD8
#define PcodeAddrbyte0_OFFSET		0x00FFFFD0
#define PcodeAddrByte1_OFFSET		0x00FFFFC8
#define PcodeAddrByte2_OFFSET		0x00FFFFC0
#define PcodeAddrByte3_OFFSET		0x00FFFFB8

/* Valid byte lane ID values. Code must be in the MSB only or all lanes */
#define ID_MSB64		0x8
#define ID_MSB32		0x4
#define ID_ALL			0x1

/* p-machine handle for slot information */
#define int32			int	/* Type for 32 bit integers. */
	
/* P-machine description */
#define P_NREGS			8	/* 8 machine registers */

/* Definitions of masks and shifts for instruction contents. */
#define OPCODE_SHIFT	26
#define OPCODE_MASK	0x3f
#define INSTR_IMMED_BIT	0x80000000	/* MSB of opcode field */
#define OPCODE_IMMED_BIT	(INSTR_IMMED_BIT >> OPCODE_SHIFT)
#define FIELD1_SHIFT	21
#define FIELD2_SHIFT	16
#define FIELD3_SHIFT	11
#define FIELD_MASK	0x1f
#define REG_MASK	0x7
#define BRANCH_MASK	0x3ffffff	/* Branch dest PC and STOREV count */
/*
 * The following macros are intended to extract the appropriate
 * bitfields from the instruction in a reasonably portable manner.
 */
#define OPCODE( i )	(((i)>>OPCODE_SHIFT) & OPCODE_MASK)
#define IMMEDIATE_FOLLOWS_OPCODE( op )	((op) & INSTR_IMMED_BIT)

/* And, for the assembler, a way to set the opcode field. */
#define SET_OPCODE( inst, opcode )	\
	(inst) = ((inst) & \
	~(OPCODE_MASK<<OPCODE_SHIFT))|(((opcode)&OPCODE_MASK)<<OPCODE_SHIFT)

/* Extract the full width of each encoded source/dest field */
#define FIELD1( i )	(((i)>>FIELD1_SHIFT) & FIELD_MASK)
#define FIELD2( i )	(((i)>>FIELD2_SHIFT) & FIELD_MASK)
#define FIELD3( i )	(((i)>>FIELD3_SHIFT) & FIELD_MASK)

/* Set the full width of each encoded field (assembler support) */
#define SET_FIELD1( inst, val)	\
	(inst)=((inst) &		\
	~(FIELD_MASK<<FIELD1_SHIFT))|(((val)&FIELD_MASK)<<FIELD1_SHIFT)
#define SET_FIELD2( inst, val)	\
	(inst)=((inst) &		\
	~(FIELD_MASK<<FIELD2_SHIFT))|(((val)&FIELD_MASK)<<FIELD2_SHIFT)
#define SET_FIELD3( inst, val)	\
	(inst)=((inst) &		\
	~(FIELD_MASK<<FIELD3_SHIFT))|(((val)&FIELD_MASK)<<FIELD3_SHIFT)

/* Extract the register field width of each encoded source/dest field. */
#define REG1( i )	(((i)>>FIELD1_SHIFT) & REG_MASK)
#define REG2( i )	(((i)>>FIELD2_SHIFT) & REG_MASK)
#define REG3( i )	(((i)>>FIELD3_SHIFT) & REG_MASK)

/* Set the register field of each encoded field. */
#define SET_REG1( inst, val)	\
	(inst)=((inst) &		\
	~(FIELD_MASK<<FIELD1_SHIFT))|(((val)&REG_MASK)<<FIELD1_SHIFT)
#define SET_REG2( inst, val)	\
	(inst)=((inst) &		\
	~(FIELD_MASK<<FIELD2_SHIFT))|(((val)&REG_MASK)<<FIELD2_SHIFT)
#define SET_REG3( inst, val)	\
	(inst)=((inst) &		\
	~(FIELD_MASK<<FIELD3_SHIFT))|(((val)&REG_MASK)<<FIELD3_SHIFT)


/* Extract the branch destination field from an instruction */
#define BRANCH_DEST( i )	((i) & BRANCH_MASK)

/* Set the branch destination field of an instruction */
#define SET_BRANCH_DEST( inst, val ) \
	(inst) = ((inst)& ~BRANCH_MASK) | ((val) & BRANCH_MASK)
/*
 * Each opcode name is followed by letters encoding the source(s) and
 * destination.  The encoding is as follows:
 *
 *	C	constant
 *	A	address
 *	I	indirect address (in register)
 */
#define LOAD_CR		32
#define LOAD_AR		33
#define	LOAD_IR		1
#define STORE_RA	34
#define _NOT_USED1	35
#define STORE_RI	2
#define STOREV		3
#define	ADD_CRR		36
#define ADD_RRR		4
#define SUB_RCR		37
#define SUB_CRR		38
#define	SUB_RRR		5
#define	AND_CRR		39
#define	AND_RRR		7
#define	OR_CRR		40
#define	OR_RRR		8
#define	XOR_CRR		41
#define	XOR_RRR		9
#define	ASL_RCR		10
#define ASR_RCR		11
#define	MOVE_RR		12
#define	TEST_R		6
#define	BR		13
#define BPOS		14
#define BNEG		15
#define	BZERO		16
#define BNPOS		17
#define BNNEG		18
#define BNZERO		19
#define END		0

/* Pseudo-ops */
#define	LABEL_T		300
#define TARGET_T	301
#define CONST_T		302
#define ADDR_T		303
#define REG_T		304


#endif /* _PCODE_H_ */

