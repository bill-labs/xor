#ifndef __ZCORE_H
#define __ZCORE_H

#ifndef __ASSEMBLY__

typedef struct z80cpu {
	u8 gpr[32];
} z80cpu_t;

typedef struct z80env {
	z80cpu_t		cpustate;
	i32				work_time;			/* 32 */
	i32				trap;				/* 36 */
	void			*rom;				/* 40 */
	void			*ram;				/* 44 */
	u8				io[16];				/* 48 */
} z80env_t;

#define CPU_ENV					(ZVMENV.cpustate)
#define ZGPR(X)					((X).gpr)
#define ZGPRA					(ZVMENV.cpustate.gpr)
#define Z80_CPU					(ZVMENV.cpustate)
#define Z80_WORKTIME			(ZVMENV.work_time)
#define Z80_FREQSHIFT			(ZVMENV.freq_shift)
#define Z80_START_TIMESTAMP		(ZVMENV.start_timestamp)
#define Z80_WORKTIME_DEADLINE	(ZVMENV.worktime_deadline)
#define ZTRAP					(ZVMENV.trap)
#define ZTRAPVAL				(ZVMENV.trapval)

#endif

#define OIND  	1
#define VIND  	0

#define BIND  	3
#define CIND  	2

#define B1IND  	5
#define C1IND  	4

#define DIND  	9
#define EIND  	8

#define D1IND  	7
#define E1IND  	6

#define HIND  	11
#define LIND  	10

#define H1IND  	13
#define L1IND  	12

#define AIND  	15
#define FIND  	14

#define A1IND  	17
#define F1IND  	16

#define IXHIND	19
#define IXLIND	18

#define IYHIND	21
#define IYLIND	20

#define SIND  	23
#define PIND  	22

#define PCHIND	25
#define PCLIND	24

#define IIND 	26
#define RIND 	27
#define RHIND	28
#define IMIND 	29
#define SFIND 	30
#define IVIND 	31

#define BCIND	(CIND >> 1)
#define DEIND	(EIND >> 1)
#define HLIND	(LIND >> 1)
#define IXIND	(IXLIND >> 1)
#define IYIND	(IYLIND >> 1)
#define SPIND	(PIND >> 1)
#define AFIND	(FIND >> 1)

#define FLAG_S		0x80
#define FLAG_Z		0x40
#define FLAG_5		0x20
#define FLAG_H		0x10
#define FLAG_3		0x08
#define FLAG_PV		0x04
#define FLAG_N		0x02
#define FLAG_C		0x01

#define F35MASK	    0x28

#define	SYSFLAG_INTERRUPTED		0x01
#define	SYSFLAG_IFF1			0x02
#define	SYSFLAG_IFF2			0x04
#define	SYSFLAG_HALTED			0x08
#define	SYSFLAG_PENDING_EI		0x10
#define	SYSFLAG_RETURN	    	0x20
#define	SYSFLAG_RETERR	    	0x40
#define	SYSFLAG_SETFLAGS		0x80

/* O, V, B, C, D, E, H, L, A, F */
#define REG_O(X)        			(ZGPR(X)[OIND])
#define O               			REG_O(CPU_ENV)
#define REG_V(X)        			(ZGPR(X)[VIND])
#define V               			REG_V(CPU_ENV)
#define REG_B(X)        			(ZGPR(X)[BIND])
#define B               			REG_B(CPU_ENV)
#define REG_C(X)        			(ZGPR(X)[CIND])
#define C              			 	REG_C(CPU_ENV)
#define REG_D(X)        			(ZGPR(X)[DIND])
#define D               			REG_D(CPU_ENV)
#define REG_E(X)        			(ZGPR(X)[EIND])
#define E               			REG_E(CPU_ENV)
#define REG_H(X)        			(ZGPR(X)[HIND])
#define H               			REG_H(CPU_ENV)
#define REG_L(X)        			(ZGPR(X)[LIND])
#define L               			REG_L(CPU_ENV)
#define REG_A(X)        			(ZGPR(X)[AIND])
#define A               			REG_A(CPU_ENV)
#define REG_F(X)        			(ZGPR(X)[FIND])
#define F              			 	REG_F(CPU_ENV)

/* IX, IY halves */
#define REG_IXH(X)      			(ZGPR(X)[IXHIND])
#define IXH             			REG_IXH(CPU_ENV)
#define REG_IXL(X)      			(ZGPR(X)[IXLIND])
#define IXL             			REG_IXL(CPU_ENV)
#define REG_IYH(X)      			(ZGPR(X)[IYHIND])
#define IYH             			REG_IYH(CPU_ENV)
#define REG_IYL(X)      			(ZGPR(X)[IYLIND])
#define IYL             			REG_IYL(CPU_ENV)

/* SP halves */
#define REG_S(X)        			(ZGPR(X)[SIND])
#define S               			REG_S(CPU_ENV)
#define REG_P(X)        			(ZGPR(X)[PIND])
#define P               			REG_P(CPU_ENV)

/* PC halves */
#define REG_PCH(X)      			(ZGPR(X)[PCHIND])
#define PCH             			REG_PCH(CPU_ENV)
#define REG_PCL(X)      			(ZGPR(X)[PCLIND])
#define PCL             			REG_PCL(CPU_ENV)

/* Altermative GPR set */
#define REG_B1(X)       			(ZGPR(X)[B1IND])
#define B1              			REG_B1(CPU_ENV)
#define REG_C1(X)       			(ZGPR(X)[C1IND])
#define C1              			REG_C1(CPU_ENV)
#define REG_D1(X)       			(ZGPR(X)[D1IND])
#define D1              			REG_D1(CPU_ENV)
#define REG_E1(X)       			(ZGPR(X)[E1IND])
#define E1              			REG_E1(CPU_ENV)
#define REG_H1(X)       			(ZGPR(X)[H1IND])
#define H1              			REG_H1(CPU_ENV)
#define REG_L1(X)       			(ZGPR(X)[L1IND])
#define L1              			REG_L1(CPU_ENV)
#define REG_A1(X)       			(ZGPR(X)[A1IND])
#define A1              			REG_A1(CPU_ENV)
#define REG_F1(X)       			(ZGPR(X)[F1IND])
#define F1              			REG_F1(CPU_ENV)

/* Register pairs */
#define PAIR(Z,X)       			(*(u16 *)(ZGPR(Z) + (X)))

#define REG_OV(X)       			PAIR(X,VIND)
#define OV              			REG_OV(CPU_ENV)
#define REG_BC(X)       			PAIR(X,CIND)
#define BC              			REG_BC(CPU_ENV)
#define REG_DE(X)       			PAIR(X,EIND)
#define DE              			REG_DE(CPU_ENV)
#define REG_HL(X)       			PAIR(X,LIND)
#define HL              			REG_HL(CPU_ENV)
#define REG_AF(X)       			PAIR(X,FIND)
#define AF              			REG_AF(CPU_ENV)

#define REG_BC1(X)      			PAIR(X,C1IND)
#define BC1             			REG_BC1(CPU_ENV)
#define REG_DE1(X)      			PAIR(X,E1IND)
#define DE1             			REG_DE1(CPU_ENV)
#define REG_HL1(X)      			PAIR(X,L1IND)
#define HL1             			REG_HL1(CPU_ENV)
#define REG_AF1(X)      			PAIR(X,F1IND)
#define AF1             			REG_AF1(CPU_ENV)

#define REG_SP(X)       			PAIR(X,PIND)
#define SP              			REG_SP(CPU_ENV)
#define REG_IX(X)       			PAIR(X,IXLIND)
#define IX              			REG_IX(CPU_ENV)
#define REG_IY(X)       			PAIR(X,IYLIND)
#define IY              			REG_IY(CPU_ENV)
#define REG_PC(X)       			PAIR(X,PCLIND)
#define PC              			REG_PC(CPU_ENV)

#define I               			(ZGPRA[IIND])
#define _R              			(ZGPRA[RIND])
#define RL              			(_R & 0x7F)
#define RH              			(ZGPRA[RHIND])
#define R               			(RL | RH)
#define IM              			(ZGPRA[IMIND])
#define VECTOR          			(ZGPRA[IVIND])

#define SF              			ZGPRA[SFIND]
#define IFF1            			(SF & SYSFLAG_IFF1)
#define IFF2            			(SF & SYSFLAG_IFF2)
#define IFF2To1()       			SF = (SF & ~(SYSFLAG_IFF1 | SYSFLAG_IFF2)) | ((SF >> 1) & SYSFLAG_IFF1)
#define IFFClear()      			SF &= ~(SYSFLAG_IFF1 | SYSFLAG_IFF2)
#define IFFSet()        			SF |= (SYSFLAG_IFF1 | SYSFLAG_IFF2)
#define HALTED          			(SF & SYSFLAG_HALTED)
#define INTERRUPTED					(SF & SYSFLAG_INTERRUPTED)
// #define set_interrupted()		{ SF |= SYSFLAG_INTERRUPTED; }
// #define clear_interrupted()	{ SF &= ~SYSFLAG_INTERRUPTED; }

#endif
