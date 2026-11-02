#define __ASSEMBLY__
#ifdef UNDERSCORE
#define U(X) _##X
#else
#define U(X) X
#endif
#include "../zcore.h"
.intel_syntax noprefix
.code32

// Z80 core (x86 ASM) - formatting-only refactor; logic unchanged.

// ----- Build/ABI macros -----
#define T(X)        sub dword ptr [ebp+32],(X)
#define CSEG
#define SIZE32
#define OP32
#define ADDR32
#define callX(X)    call X
#define AINT        .long
#define ASIZE       4

// ----- Memory helpers (ROM/RAM fetch/store) -----
U(priv_zifetchb):
U(priv_zdfetchb):	movzx edx,ax
	cmp dx,0x4000
	jb fetch_rom
	add edx,[ebp+44]
	mov al,[edx-0x4000]
	ret
fetch_rom:	add edx,[ebp+40]
	mov al,[edx]
	ret

U(priv_zifetchw):
U(priv_zdfetchw):	movzx edx,ax
	cmp dx,0x3FFF
	jb fetch2l_rom
	je fetch2l_mix
	cmp dx,0xFFFF
	je fetch2h_mix
	add edx,[ebp+44]
	mov ax,[edx-0x4000]
	ret
fetch2l_rom:	add edx,[ebp+40]
	mov ax,[edx]
	ret
fetch2l_mix:	add edx,[ebp+40]
	mov al,[edx]
	sub edx,[ebp+40]
	add edx,[ebp+44]
	mov ah,[edx+1-0x4000]
	ret
fetch2h_mix:	add edx,[ebp+44]
	mov al,[edx-0x4000]
	sub edx,[ebp+44]
	add edx,[ebp+40]
	mov ah,[edx+1]
	ret

U(priv_zdstoreb):	movzx eax,ax
	cmp ax,0x4000
	jb store_rom
	add eax,[ebp+44]
	mov [eax-0x4000],bl
store_rom:	ret

U(priv_zdstorew):	movzx eax,ax
	cmp ax,0x3FFF
	jb store2l_rom
	je store2l_mix
	cmp ax,0xFFFF
	je store2h_mix
	add eax,[ebp+44]
	mov [eax-0x4000],bx
store2l_rom:	ret
store2l_mix:	add eax,[ebp+44]
	mov [eax+1-0x4000],bh
	ret
store2h_mix:	add eax,[ebp+44]
	mov [eax-0x4000],bl
	ret

// ----- I/O helpers -----
U(z80_input):	test al,0x01
	je 1f
	mov al,0xFF
	ret
1:	lea edx,[ebp+48]
	// EAR bit forced low (no tape input).
	mov al,0xBF
	not ah
2:	test ah,ah
	je in_done
	shl ah,1
	jnc 1f

	shl eax,8
	mov al,[edx+8]
	xchg al,[edx]
	and ah,al
	shr eax,8

1:	inc edx
	jmp 2b
in_done:	ret
U(z80_output):	ret

.extern U(z80_check_interrupted)

// ----- PC/stack/addressing helpers -----
pcfetch:	movzx eax,word ptr [ebp+PCLIND]
	inc word ptr [ebp+PCLIND]
	jmp U(priv_zdfetchb)

pcfetch2:	movzx eax,word ptr [ebp+PCLIND]
	add word ptr [ebp+PCLIND],2
	jmp U(priv_zdfetchw)

pop2:	movzx eax,word ptr [ebp+PIND]
	add word ptr [ebp+PIND],2
	jmp U(priv_zdfetchw)

push2:	sub word ptr [ebp+PIND],2
	movzx eax,word ptr [ebp+PIND]
	jmp U(priv_zdstorew)

hl_idx_addr:	test ebx,ebx
	jne 1f
	movzx eax,word ptr [ebp+LIND]
	ret
1:	callX(pcfetch)
	cbw
	movzx edx,byte ptr CSEG [pairtab1+ebx*4+2]
	mov dx,[ebp+edx]
	add dx,ax
	mov [ebp+VIND],dx
	mov eax,edx
	T(8)
	ret

// ----- Main opcode groups -----
main_00:	mov edx,eax
	and dl,7
	shr eax,3
	jmp CSEG [main_00_table+edx*ASIZE]

main_00_0:	btr eax,2
	jc 1f
	jmp CSEG [main_00_0_table+eax*ASIZE]
1:	mov dl,[ebp+FIND]
	and dl,CSEG [andmasks+eax]
	xor dl,CSEG [xormasks+eax]
	jne z_jr
	inc word ptr [ebp+PCLIND]
	T(7)
	jmp opc_end

z_nop:	T(4)
	jmp opc_end

z_exafaf1:	rol dword ptr [ebp+FIND],16
	T(4)
	jmp opc_end

z_djnz:	dec byte ptr [ebp+BIND]
	jne 1f
	inc word ptr [ebp+PCLIND]
	T(8)
	jmp opc_end
1:	T(1)
z_jr:	callX(pcfetch)
	cbw
	add ax,[ebp+PCLIND]
	mov [ebp+PCLIND],ax
	mov [ebp+VIND],ax
	T(12)
	jmp opc_end

main_00_1:	shr eax,1
	movzx esi,byte ptr CSEG [pairtab1+eax+ebx*4]
	jnc 1f
	movzx edi,byte ptr CSEG [pairtab1+2+ebx*4]
	mov dx,[ebp+edi]
	mov ax,dx
	inc ax
	mov [ebp+VIND],ax
	mov cx,[ebp+esi]
	mov al,[ebp+FIND]
	and al,0xC4
	add dl,cl
	adc dh,ch
	mov [ebp+edi],dx
	lahf
	and ah,0x11
	or al,ah
	and dh,0x28
	or al,dh
	mov [ebp+FIND],al
	T(11)
	jmp opc_end
1:	callX(pcfetch2)
	mov [ebp+esi],ax
	T(10)
	jmp opc_end

main_00_2:	jmp CSEG [main_00_2_table+eax*ASIZE]
z_ld_de_a:	movzx eax,word ptr [ebp+EIND]
	jmp 1f
z_ld_bc_a:	movzx eax,word ptr [ebp+CIND]
1:	mov bl,[ebp+AIND]
	callX(U(priv_zdstoreb))
	mov [ebp+OIND],bl
	mov al,[ebp+CIND]
	inc al
	mov [ebp+VIND],al
	T(7)
	jmp opc_end

z_ld_a_de:	movzx eax,word ptr [ebp+EIND]
	jmp 1f
z_ld_a_bc:	movzx eax,word ptr [ebp+CIND]
1:	callX(U(priv_zdfetchb))
	mov [ebp+AIND],al
	mov ax,[ebp+CIND]
	inc ax
	mov [ebp+VIND],ax
	T(7)
	jmp opc_end

z_ld_ann_hl:	callX(pcfetch2)
	mov [ebp+VIND],ax
	movzx edx,byte ptr CSEG [pairtab1+ebx*4+2]
	mov bx,[ebp+edx]
	callX(U(priv_zdstorew))
	T(16)
	jmp opc_end

z_ld_hl_ann:	callX(pcfetch2)
	mov [ebp+VIND],ax
	callX(U(priv_zdfetchw))
	movzx edx,byte ptr CSEG [pairtab1+ebx*4+2]
	mov [ebp+edx],ax
	T(16)
	jmp opc_end

z_ld_ann_a:	callX(pcfetch2)
	mov bl,[ebp+AIND]
	mov [ebp+OIND],bl
	mov dx,ax
	inc dx
	mov [ebp+VIND],dl
	callX(U(priv_zdstoreb))
	T(13)
	jmp opc_end

z_ld_a_ann:	callX(pcfetch2)
	mov [ebp+VIND],ax
	callX(U(priv_zdfetchb))
	mov [ebp+AIND],al
	T(13)
	jmp opc_end

main_00_3:	T(6)
	shr eax,1
	movzx ebx,byte ptr CSEG [pairtab1+eax+ebx*4]
	jnc 1f
	dec word ptr [ebp+ebx]
	jmp opc_end
1:	inc word ptr [ebp+ebx]
	jmp opc_end

main_00_4:	xor edi,edi
	jmp 1f
main_00_5:	mov edi,0x02
1:	movzx edx,byte ptr CSEG [regindtab+eax+ebx*8]
	test edx,edx
	jne 1f
	call hl_idx_addr
	mov esi,eax
	callX(U(priv_zdfetchb))
	xor edx,edx
	jmp 2f
1:	mov al,[ebp+edx]
2:	test edi,edi
	jne 1f
	inc al
	jmp 2f
1:	dec al
2:	mov cx,di
	bt word ptr [ebp+FIND],0
	lahf
	seto ch
	shl ch,2
	or cl,ch
	and ah,0xD1
	or ah,cl
	mov cl,al
	and cl,0x28
	or ah,cl
	mov [ebp+FIND],ah
	test edx,edx
	je 1f
	mov [ebp+edx],al
	T(4)
	jmp opc_end
1:	mov bl,al
	mov eax,esi
	callX(U(priv_zdstoreb))
	T(11)
	jmp opc_end

main_00_6:	movzx edi,byte ptr CSEG [regindtab+eax+ebx*8]
	test edi,edi
	je 1f
	callX(pcfetch)
	mov [ebp+edi],al
	T(7)
	jmp opc_end
1:	test ebx,ebx
	je 1f
	add dword ptr [ebp+32],3		// Save 3T on ld (hl),n
1:	call hl_idx_addr
	mov edi,eax
	callX(pcfetch)
	mov bl,al
	mov eax,edi
	callX(U(priv_zdstoreb))
	T(10)
	jmp opc_end

main_00_7:	jmp CSEG [main_00_7_table+eax*ASIZE]

z_rlca:	mov ax,[ebp+FIND]
	and al,0xC4
	rol ah,1
	adc al,0
	mov dl,ah
	and dl,0x28
	or al,dl
	mov [ebp+FIND],ax
	T(4)
	jmp opc_end

z_rrca:	mov ax,[ebp+FIND]
	and al,0xC4
	ror ah,1
	adc al,0
	mov dl,ah
	and dl,0x28
	or al,dl
	mov [ebp+FIND],ax
	T(4)
	jmp opc_end

z_rla:	mov ax,[ebp+FIND]
	and al,0xC5
	btr ax,0
	rcl ah,1
	adc al,0
	mov dl,ah
	and dl,0x28
	or al,dl
	mov [ebp+FIND],ax
	T(4)
	jmp opc_end

z_rra:	mov ax,[ebp+FIND]
	and al,0xC5
	btr ax,0
	rcr ah,1
	adc al,0
	mov dl,ah
	and dl,0x28
	or al,dl
	mov [ebp+FIND],ax
	T(4)
	jmp opc_end

z_daa:	mov ax,[ebp+FIND]
	xchg al,ah
	bt ax,9
	jc daa_sub

daa_add:	sahf
	mov cx,ax
	daa
	lahf
	and ah,0xFD
	mov [ebp+AIND],al
	and cl,0x0F
	cmp cl,0x0A
	jae daafin
	and ah,0xEF
	jmp daafin

daa_sub:	sahf
	mov cx,ax
	das
	lahf
	mov [ebp+AIND],al

	or ch,0xEE
	and ah,ch

	cmp cl,0x9A
	jb 1f
	or ah,1

1:	and cl,0x0F
	cmp cl,0x06
	jb daafin
	and ah,0xEF

daafin:	and ah,0xD7
	and al,0x28
	or ah,al
	mov [ebp+FIND],ah
	T(4)
	jmp opc_end

z_cpl:	mov ax,[ebp+FIND]
	not ah
	and al,0xC5
	or al,0x12
	mov dl,ah
	and dl,0x28
	or al,dl
	mov [ebp+FIND],ax
	T(4)
	jmp opc_end

z_scf:	mov ax,[ebp+FIND]
	and ax,0x28C5
	or al,0x01
	or al,ah
	mov [ebp+FIND],al
	T(4)
	jmp opc_end

z_ccf:	mov ax,[ebp+FIND]
	and ax,0x28C5
	or al,ah
	mov dl,al
	and dl,0x01
	xor al,0x01
	shl dl,4
	or al,dl
	mov [ebp+FIND],al
	T(4)
	jmp opc_end

main_01:	mov edx,eax
	mov ecx,eax
	shr edx,3
	and ecx,7
	and edx,7
	movzx esi,byte ptr CSEG [regindtab+ecx+ebx*8]
	movzx edi,byte ptr CSEG [regindtab+edx+ebx*8]
	test edi,edi
	je dst_mem
	test esi,esi
	je src_mem
	mov al,[ebp+esi]
	mov [ebp+edi],al
	T(4)
	jmp opc_end
dst_mem:	test esi,esi
	je opc_halt
	movzx esi,byte ptr CSEG [regindtab+ecx]
	call hl_idx_addr
	mov bl,[ebp+esi]
	callX(U(priv_zdstoreb))
	T(7)
	jmp opc_end
src_mem:	movzx edi,byte ptr CSEG [regindtab+edx]
	call hl_idx_addr
	callX(U(priv_zdfetchb))
	mov [ebp+edi],al
	T(7)
	jmp opc_end

opc_halt:	cmp word ptr [ebp+PCLIND],0x0557
	je halt_trap
// cmp word ptr [ebp+PCLIND],0x34BC
// jne real_halt
	jmp real_halt
halt_trap:	mov dword ptr [ebp+36],0xFFFFFFFD
	jmp trapped
real_halt:	or byte ptr [ebp+SFIND],SYSFLAG_HALTED
	T(4)
do_halt:	mov eax,[ebp+32]
	cmp eax,0
	jle no_more_time
	and eax,3
	je 1f
	sub eax,4
1:	mov [ebp+32],eax
	jmp no_more_time

z_al8_add:	xor ch,ch
	add dl,al
arith_finish:	mov [ebp+AIND],dl
arith_finish1:	lahf
	seto cl
	shl cl,2
	or cl,ch
	and ah,0xD1
	or ah,cl
	and dl,0x28
	or ah,dl
	mov [ebp+FIND],ah
	jmp opc_end

z_al8_adc:	xor ch,ch
	bt word ptr [ebp+FIND],0
	adc dl,al
	jmp arith_finish

z_al8_sub:	mov ch,FLAG_N
	sub dl,al
	jmp arith_finish

z_al8_sbc:	mov ch,FLAG_N
	bt word ptr [ebp+FIND],0
	sbb dl,al
	jmp arith_finish

z_al8_and:	mov cl,FLAG_H
	and dl,al
log_finish:	mov [ebp+AIND],dl
log_finish1:	lahf
	and ah,0xC4
	or ah,cl
	and dl,0x28
	or ah,dl
	mov [ebp+FIND],ah
	jmp opc_end

z_al8_xor:	xor cl,cl
	xor dl,al
	jmp log_finish

z_al8_or:	xor cl,cl
	or dl,al
	jmp log_finish

z_al8_cp:	mov ch,FLAG_N
	cmp dl,al
	mov dl,al
	jmp arith_finish1

main_10:	mov edi,eax
	mov esi,eax
	shr edi,3
	and esi,7
	and edi,7
	movzx esi,byte ptr CSEG [regindtab+esi+ebx*8]
	test esi,esi
	jne 1f
	call hl_idx_addr
	callX(U(priv_zdfetchb))
	T(7)
	jmp 2f
1:	mov al,[ebp+esi]
	T(4)
2:	mov dl,[ebp+AIND]
	jmp CSEG [al8table+edi*ASIZE]

main_11:	mov edx,eax
	and dl,7
	shr eax,3
	and al,7
	jmp CSEG [main_11_table+edx*ASIZE]

main_11_0:	T(1)
	mov dl,[ebp+FIND]
	and dl,CSEG [andmasks+eax]
	xor dl,CSEG [xormasks+eax]
	jne z_ret
	T(4)
	jmp opc_end
z_ret:	callX(pop2)
	mov [ebp+PCLIND],ax
	mov [ebp+VIND],ax
	T(10)
	jmp opc_end

main_11_1:	shr eax,1
	jnc 1f
	jmp CSEG [main_11_1_table+eax*ASIZE]
1:	movzx edi,byte ptr CSEG [pairtab2+eax+ebx*4]
	callX(pop2)
	mov [ebp+edi],ax
	T(10)
	jmp opc_end

z_exx:	rol dword ptr [ebp+CIND],16
	rol dword ptr [ebp+E1IND],16
	rol dword ptr [ebp+LIND],16
	T(4)
	jmp opc_end

z_jp_hl:	movzx ebx,byte ptr CSEG [pairtab2+ebx*4+2]
	mov ax,[ebp+ebx]
	mov [ebp+PCLIND],ax
	T(4)
	jmp opc_end

z_ld_sp_hl:	movzx ebx,byte ptr CSEG [pairtab2+ebx*4+2]
	mov ax,[ebp+ebx]
	mov [ebp+PIND],ax
	T(6)
	jmp opc_end

main_11_2:	mov dl,[ebp+FIND]
	and dl,CSEG [andmasks+eax]
	xor dl,CSEG [xormasks+eax]
	jne z_jp
	add word ptr [ebp+PCLIND],2
	T(10)
	jmp opc_end
z_jp:	callX(pcfetch2)
	mov [ebp+VIND],ax
	mov [ebp+PCLIND],ax
	T(10)
	jmp opc_end

main_11_3:	jmp CSEG [main_11_3_table+eax*ASIZE]

z_bitwise:	test ebx,ebx
	jne 1f
	inc byte ptr [ebp+RIND]
	jmp 2f
1:	call hl_idx_addr
	mov esi,eax
	callX(U(priv_zdfetchb))
	mov edi,eax
	add dword ptr [ebp+32],4
	T(15)
2:	callX(pcfetch)
	movzx eax,al
	mov ecx,eax
	mov edx,eax
	shr ecx,3
	and cl,7
	and dl,7
	movzx edx,byte ptr CSEG [regindtab+edx]
	test edx,edx
	jne 1f
	test ebx,ebx
	jne 2f
	T(15)
	mov ebx,ecx
	mov edi,eax
	movzx esi,word ptr [ebp+LIND]
	mov eax,esi
	callX(U(priv_zdfetchb))
	xor edx,edx					// Restore EDX to zero
	mov ecx,ebx
	inc ebx							// Always non-zero
	jmp 3f
1:	test ebx,ebx
	jne 2f
	movzx edi,byte ptr [ebp+edx]
	T(8)
2:	xchg eax,edi
3:	shr edi,6
	jmp CSEG [bitwise_table+edi*ASIZE]
z_shifts:	jmp CSEG [shifts_table+ecx*ASIZE]
z_rlc:	rol al,1
	jmp bitwise_setflags
z_rrc:	ror al,1
	jmp bitwise_setflags
z_rl:	bt word ptr [ebp+FIND],0
	rcl al,1
	jmp bitwise_setflags
z_rr:	bt word ptr [ebp+FIND],0
	rcr al,1
	jmp bitwise_setflags
z_sla:	shl al,1
	jmp bitwise_setflags
z_sra:	sar al,1
	jmp bitwise_setflags
z_sll:	stc
	rcl al,1
	jmp bitwise_setflags
z_srl:	shr al,1
bitwise_setflags:	setc cl
	test al,al
	lahf
	and ah,0xC4
	or ah,cl
	mov cl,al
	and cl,0x28
	or ah,cl
	mov [ebp+FIND],ah
	jmp bitwise_done
z_bit:	mov dl,al
	mov edi,1
	shl edi,cl
	mov ecx,edi
	and al,cl
	mov ah,[ebp+FIND]
	and ah,0x01
	or ah,0x10
	test al,al
	jne 1f
	or ah,0x44
1:	test al,0x80
	je 2f
	or ah,0x80
2:	test ebx,ebx
	je 3f
	add dword ptr [ebp+32],3
	mov al,[ebp+OIND]
	jmp 4f
3:	mov al,dl
4:	and al,0x28
	or ah,al
	mov [ebp+FIND],ah
	jmp opc_end
z_res:	btr eax,ecx
	jmp bitwise_done
z_set:	bts eax,ecx
bitwise_done:	test edx,edx
	je 1f
	mov [ebp+edx],al
1:	test ebx,ebx
	je opc_end
	mov bl,al
	mov eax,esi
	callX(U(priv_zdstoreb))
	jmp opc_end

z_outa:	callX(pcfetch)
	movzx eax,al
	mov dl,[ebp+AIND]
	mov ah,dl
	mov [ebp+OIND],dl
	mov cl,al
	inc cl
	mov [ebp+VIND],cl
	callX(U(z80_output))
	T(11)
	jmp opc_end

z_ina:	callX(pcfetch)
	movzx eax,al
	mov ah,[ebp+AIND]
	mov dx,ax
	inc dx
	mov [ebp+VIND],dx
	callX(U(z80_input))
	mov [ebp+AIND],al
	T(11)
	jmp opc_end

z_ex_sp_hl:	mov si,[ebp+PIND]
	mov ax,si
	callX(U(priv_zdfetchw))
	mov [ebp+VIND],ax
	movzx ebx,byte ptr CSEG [pairtab2+ebx*4+2]
	xchg ax,[ebp+ebx]
	mov bx,ax
	mov ax,si
	callX(U(priv_zdstorew))
	T(19)
	jmp opc_end

z_ex_de_hl:	rol dword ptr [ebp+EIND],16
	T(4)
	jmp opc_end

z_di:	and byte ptr [ebp+SFIND],~(SYSFLAG_IFF1 | SYSFLAG_IFF2)
	T(4)
	jmp opc_end

z_ei:	or byte ptr [ebp+SFIND],(SYSFLAG_IFF1 | SYSFLAG_IFF2 | SYSFLAG_PENDING_EI)
	mov dword ptr [ebp+36],0xFFFFFFFE
	T(4)
	jmp opc_end

main_11_4:	mov dl,[ebp+FIND]
	and dl,CSEG [andmasks+eax]
	xor dl,CSEG [xormasks+eax]
	jne z_call
	add word ptr [ebp+PCLIND],2
	T(10)
	jmp opc_end
z_call:	callX(pcfetch2)
	mov [ebp+VIND],ax
	xchg ax,[ebp+PCLIND]
	mov bx,ax
	callX(push2)
	T(17)
	jmp opc_end

main_11_5:	shr eax,1
	jnc 1f
	jmp CSEG [main_11_5_table+eax*ASIZE]
1:	movzx ebx,byte ptr CSEG [pairtab2+eax+ebx*4]
	mov bx,[ebp+ebx]
	callX(push2)
	T(11)
	jmp opc_end

z_221:	mov ebx,1
	T(4)
	jmp prefixed_op

z_253:	mov ebx,2
	T(4)
	jmp prefixed_op

z_237:	inc byte ptr [ebp+RIND]
	callX(pcfetch)
	movzx eax,al
	mov edx,eax
	shr edx,6
	jmp CSEG [extra_opc_table+edx*ASIZE]

z_nop2:	T(8)
	jmp opc_end

extra_01:	mov edx,eax
	and dl,7
	shr eax,3
	and al,7
	jmp CSEG [extra_01_table+edx*ASIZE]

extra_01_0:	movzx edi,byte ptr CSEG [regindtab+eax]
	mov ax,[ebp+CIND]
	mov dx,ax
	inc dx
	mov [ebp+VIND],dx
	callX(U(z80_input))
	test edi,edi
	je 1f
	mov [ebp+edi],al
1:
	test al,al
	bt word ptr [ebp+FIND],0
	lahf
	and ah,0xC5
	and al,0x28
	or ah,al
	mov [ebp+FIND],ah
	T(12)
	jmp opc_end

extra_01_1:	movzx esi,byte ptr CSEG [regindtab+eax]
	mov ax,[ebp+CIND]
	mov dx,ax
	inc dx
	mov [ebp+VIND],dx
	test esi,esi
	jne 1f
	xor dl,dl
	jmp 2f
1:	mov dl,[ebp+esi]
2:	callX(U(z80_output))
	T(12)
	jmp opc_end

extra_01_2:	shr eax,1
	movzx esi,byte ptr CSEG [pairtab1+eax]
	mov bx,[ebp+LIND]
	mov dx,[ebp+esi]
	jnc 1f
	mov ax,bx
	inc ax
	mov [ebp+VIND],ax
	xor ch,ch
	bt word ptr [ebp+FIND],0
	adc bl,dl
	adc bh,dh
	jmp 2f
1:	mov [ebp+VIND],bx
	mov ch,FLAG_N
	bt word ptr [ebp+FIND],0
	sbb bl,dl
	sbb bh,dh
2:	mov [ebp+LIND],bx
	seto cl
	lahf
	shl cl,2
	or cl,ch
	or bl,bh
	sete ch
	shl ch,6
	and ah,0x91
	or ah,cl
	or ah,ch
	and bh,0x28
	or ah,bh
	mov [ebp+FIND],ah
	T(15)
	jmp opc_end

extra_01_3:	shr eax,1
	movzx edi,byte ptr CSEG [pairtab1+eax]
	jnc 1f
	callX(pcfetch2)
	mov dx,ax
	inc dx
	mov [ebp+VIND],dx
	movzx eax,ax
	callX(U(priv_zdfetchw))
	mov [ebp+edi],ax
	jmp 2f
1:	callX(pcfetch2)
	mov dx,ax
	inc dx
	mov [ebp+VIND],dx
	movzx eax,ax
	mov bx,[ebp+edi]
	callX(U(priv_zdstorew))
2:	T(20)
	jmp opc_end

extra_01_4:	xor dl,dl
	mov al,[ebp+AIND]
	T(8)
	jmp z_al8_sub

extra_01_5:	callX(pop2)
	mov [ebp+PCLIND],ax
	mov [ebp+VIND],ax
	mov al,[ebp+SFIND]
	mov ah,al
	shr ah,1
	and ah,2
	and al,0xF9
	or al,ah
	mov [ebp+SFIND],al
	T(14)
	jmp opc_end

extra_01_6:	mov [ebp+IMIND],al
	T(8)
	jmp opc_end

extra_01_7:	jmp CSEG [extra_01_7_table+eax*ASIZE]
z_ld_i_a:	mov al,[ebp+AIND]
	mov [ebp+IIND],al
	T(9)
	jmp opc_end

z_ld_r_a:	mov al,[ebp+AIND]
	mov ah,al
	and ax,0x807F
	mov [ebp+RIND],ax
	T(9)
	jmp opc_end

z_ld_a_i:	mov al,[ebp+IIND]
	jmp 1f
z_ld_a_r:	mov ax,[ebp+RIND]
	and ax,0x807F
	or al,ah
1:	mov [ebp+AIND],al
	test al,al
	bt word ptr [ebp+FIND],0
	lahf
	and ah,0xC1
	and al,0x28
	or al,ah
	mov ah,[ebp+SFIND]
	and ah,0x04
	or al,ah
	mov [ebp+FIND],al
	T(9)
	jmp opc_end

z_rld:	movzx esi,word ptr [ebp+LIND]
	mov ax,si
	inc ax
	mov [ebp+VIND],ax
	mov eax,esi
	callX(U(priv_zdfetchb))
	mov ah,[ebp+AIND]
	rol ah,4
	rol ax,4
	jmp rxdfin
z_rrd:	movzx esi,word ptr [ebp+LIND]
	mov ax,si
	inc ax
	mov [ebp+VIND],ax
	mov eax,esi
	callX(U(priv_zdfetchb))
	mov ah,[ebp+AIND]
	ror ax,4
	rol ah,4
rxdfin:	mov [ebp+AIND],ah
	movzx edi,ah
	mov bl,al
	mov eax,esi
	callX(U(priv_zdstoreb))
	mov eax,edi
	test al,al
	lahf
	and ah,0xC4
	mov bl,[ebp+FIND]
	and bl,1
	or bl,ah
	and al,0x28
	or al,bl
	mov [ebp+FIND],al
	T(18)
	jmp opc_end

extra_10:	mov edx,eax
	and dl,7
	shr eax,3
	mov bx,1
	xor edi,edi
	shr eax,1
	jnc 1f
	neg bx
1:	shr eax,1
	rcl edi,1
	shr eax,1
	jnc z_nop2
	jmp CSEG [strop_table+edx*ASIZE]

z_ldx:	movzx eax,word ptr [ebp+LIND]
	callX(U(priv_zdfetchb))
	movzx esi,al
	shl esi,16
	mov si,bx
	mov bl,al
	movzx eax,word ptr [ebp+EIND]
	callX(U(priv_zdstoreb))
	mov bx,si
	shr esi,16
	add word ptr [ebp+LIND],bx
	add word ptr [ebp+EIND],bx
	mov eax,esi
	mov dl,[ebp+FIND]
	and dl,0xC1
	add al,[ebp+AIND]
	mov ah,al
	shl ah,4
	and ax,0x2008
	or al,ah
	or dl,al
	dec word ptr [ebp+CIND]
	je 1f
	or dl,0x04
	test edi,edi
	je 1f
	mov ax,[ebp+PCLIND]
	dec ax
	mov [ebp+VIND],ax
	sub word ptr [ebp+PCLIND],2
	T(5)
1:	mov [ebp+FIND],dl
	T(16)
	jmp opc_end

z_cpx:	movzx eax, word ptr [ebp+LIND]
	callX(U(priv_zdfetchb))
	add word ptr [ebp+LIND],bx
	add word ptr [ebp+VIND],bx
	mov dl,[ebp+AIND]
	cmp dl,al
	bt word ptr [ebp+FIND],0
	lahf
	bt ax,12
	sbb dl,al
	mov dh,dl
	shl dh,4
	and dx,0x2008
	and ah,0xD1
	or dl,ah
	or dl,dh
	or dl,2
	dec word ptr [ebp+CIND]
	je 1f
	or dl,0x04
	test edi,edi
	je 1f
	bt dx,6
	jc 1f
	mov ax,[ebp+PCLIND]
	dec ax
	mov [ebp+VIND],ax
	sub word ptr [ebp+PCLIND],2
	T(5)
1:	mov [ebp+FIND],dl
	T(16)
	jmp opc_end

z_inx:	movzx eax,word ptr [ebp+CIND]
	callX(U(z80_input))
	mov dx,[ebp+CIND]
	add dx,bx
	mov [ebp+VIND],dx
	movzx esi,al
	shl esi,16
	mov si,bx
	mov bl,al
	movzx eax,word ptr [ebp+LIND]
	callX(U(priv_zdstoreb))
	mov bx,si
	shr esi,16
	add word ptr [ebp+LIND],bx
	mov cl,[ebp+BIND]
	dec cl
	mov dl,cl
	mov [ebp+BIND],cl
	lahf
	and ah,0xC0
	and dl,0x28
	or ah,dl
	mov edx,esi
	test dl,0x80
	je 1f
	or ah,0x02
1:	mov al,[ebp+CIND]
	add al,bl
	add al,dl
	jnc 1f
	or ah,0x11
1:	mov dl,al
	and dl,7
	xor dl,cl
	jnp 1f
	or ah,0x04
1:	test edi,edi
	je 1f
	test cl,cl
	je 1f
	sub word ptr [ebp+PCLIND],2
	T(5)
1:	mov [ebp+FIND],ah
	T(16)
	jmp opc_end

z_outx:	movzx eax,word ptr [ebp+LIND]
	callX(U(priv_zdfetchb))
	movzx esi,al
	mov edx,esi
	movzx eax,word ptr [ebp+CIND]
	callX(U(z80_output))
	mov dx,[ebp+CIND]
	add dx,bx
	mov [ebp+VIND],dx
	add word ptr [ebp+LIND],bx
	mov cl,[ebp+BIND]
	dec cl
	mov dl,cl
	mov [ebp+BIND],cl
	lahf
	and ah,0xC0
	and dl,0x28
	or ah,dl
	mov edx,esi
	test dl,0x80
	je 1f
	or ah,0x02
1:	mov al,[ebp+LIND]
	add al,dl
	jnc 1f
	or ah,0x11
1:	mov dl,al
	and dl,7
	xor dl,cl
	jnp 1f
	or ah,0x04
1:	test edi,edi
	je 1f
	test cl,cl
	je 1f
	sub word ptr [ebp+PCLIND],2
	T(5)
1:	mov [ebp+FIND],ah
	T(16)
	jmp opc_end

// AROP n
main_11_6:	mov edi,eax
	callX(pcfetch)
	mov dl,[ebp+AIND]
	T(7)
	jmp CSEG [al8table+edi*ASIZE]

// RST n
main_11_7:	shl ax,3
	xchg ax,[ebp+PCLIND]
	mov bx,ax
	callX(push2)
	T(11)
	jmp opc_end

// ----- Execution loop -----
.globl U(zcore_run)
U(zcore_run):	push esi
	push edi
	push ebp
	push ebx
	mov ebp,eax

	mov dword ptr [ebp+36],0

	mov al,[ebp+SFIND]
	test al,SYSFLAG_INTERRUPTED
	je 1f
	and byte ptr [ebp+SFIND],~SYSFLAG_INTERRUPTED
	test al,SYSFLAG_IFF1
	jne raise_interrupt
1:	test al,SYSFLAG_HALTED
	jne do_halt

exec_loop:	xor ebx,ebx
prefixed_op:	movzx eax,word ptr [ebp+PCLIND]
	callX(U(priv_zifetchb))
	cmp dword ptr [ebp+36],0
	jne trapped

	movzx eax,al
	inc word ptr [ebp+PCLIND]
do_single_op:	inc byte ptr [ebp+RIND]
	mov edx,eax
	shr edx,6
	jmp CSEG [main_opc_table+edx*ASIZE]

opc_end:	cmp dword ptr [ebp+36],0
	jne trapped
	mov eax,[ebp+32]
	cmp eax,0
	jle no_more_time
	jmp exec_loop

raise_interrupt:	inc byte ptr [ebp+RIND]
	and byte ptr [ebp+SFIND],~(SYSFLAG_INTERRUPTED | SYSFLAG_IFF1 | SYSFLAG_IFF2 | SYSFLAG_HALTED)
	movzx eax,byte ptr [ebp+IMIND]
	and al,3
	jmp CSEG [intr_mode_table+eax*ASIZE]
raise_im0:	// movzx eax,byte ptr [ebp+IVIND]
	mov eax,0xFF
	jmp do_single_op
raise_im1:	mov eax,0xFF
	jmp do_single_op
raise_im2:	mov bx,[ebp+PCLIND]
	callX(push2)
	movzx eax,byte ptr [ebp+IVIND]
	mov ah,[ebp+IIND]
	callX(U(priv_zdfetchw))
	mov [ebp+PCLIND],ax
	mov [ebp+VIND],ax
	T(19)
	jmp exec_loop

no_more_time:	xor eax,eax
	dec eax
	jmp exit_run
trapped:	xor eax,eax
	xchg eax,[ebp+36]

exit_run:	pop ebx
	pop ebp
	pop edi
	pop esi
	OP32
	ret

// ----- Opcode dispatch tables and constants -----
.data
.p2align 2
.globl zx_shared_state
zx_shared_state:
mt_table:	.long 0
int_state:	.long 0
screen_top:	.long 0

main_opc_table:	AINT main_00
	AINT main_01
	AINT main_10
	AINT main_11

main_00_table:	AINT main_00_0
	AINT main_00_1
	AINT main_00_2
	AINT main_00_3
	AINT main_00_4
	AINT main_00_5
	AINT main_00_6
	AINT main_00_7

main_00_0_table:	AINT z_nop
	AINT z_exafaf1
	AINT z_djnz
	AINT z_jr

main_00_2_table:	AINT z_ld_bc_a
	AINT z_ld_a_bc
	AINT z_ld_de_a
	AINT z_ld_a_de
	AINT z_ld_ann_hl
	AINT z_ld_hl_ann
	AINT z_ld_ann_a
	AINT z_ld_a_ann

main_00_7_table:	AINT z_rlca
	AINT z_rrca
	AINT z_rla
	AINT z_rra
	AINT z_daa
	AINT z_cpl
	AINT z_scf
	AINT z_ccf

al8table:	AINT z_al8_add
	AINT z_al8_adc
	AINT z_al8_sub
	AINT z_al8_sbc
	AINT z_al8_and
	AINT z_al8_xor
	AINT z_al8_or
	AINT z_al8_cp

main_11_table:	AINT main_11_0
	AINT main_11_1
	AINT main_11_2
	AINT main_11_3
	AINT main_11_4
	AINT main_11_5
	AINT main_11_6
	AINT main_11_7

main_11_1_table:	AINT z_ret
	AINT z_exx
	AINT z_jp_hl
	AINT z_ld_sp_hl

main_11_3_table:	AINT z_jp
	AINT z_bitwise
	AINT z_outa
	AINT z_ina
	AINT z_ex_sp_hl
	AINT z_ex_de_hl
	AINT z_di
	AINT z_ei

main_11_5_table:	AINT z_call
	AINT z_221
	AINT z_237
	AINT z_253

bitwise_table:	AINT z_shifts
	AINT z_bit
	AINT z_res
	AINT z_set

shifts_table:	AINT z_rlc
	AINT z_rrc
	AINT z_rl
	AINT z_rr
	AINT z_sla
	AINT z_sra
	AINT z_sll
	AINT z_srl

extra_opc_table:	AINT z_nop2
	AINT extra_01
	AINT extra_10
	AINT z_nop2

extra_01_table:	AINT extra_01_0
	AINT extra_01_1
	AINT extra_01_2
	AINT extra_01_3
	AINT extra_01_4
	AINT extra_01_5
	AINT extra_01_6
	AINT extra_01_7

extra_01_7_table:	AINT z_ld_i_a
	AINT z_ld_r_a
	AINT z_ld_a_i
	AINT z_ld_a_r
	AINT z_rrd
	AINT z_rld
	AINT z_nop2
	AINT z_nop2

strop_table:	AINT z_ldx
	AINT z_cpx
	AINT z_inx
	AINT z_outx
	AINT z_nop2
	AINT z_nop2
	AINT z_nop2
	AINT z_nop2

intr_mode_table:	AINT raise_im0
	AINT raise_im1
	AINT raise_im1
	AINT raise_im2

regindtab:	.byte BIND
.byte CIND
.byte DIND
.byte EIND
.byte HIND
.byte LIND
.byte 0
.byte AIND

.byte BIND
.byte CIND
.byte DIND
.byte EIND
.byte IXHIND
.byte IXLIND
.byte 0
.byte AIND

.byte BIND
.byte CIND
.byte DIND
.byte EIND
.byte IYHIND
.byte IYLIND
.byte 0
.byte AIND

pairtab1:	.byte CIND
.byte EIND
.byte LIND
.byte PIND

.byte CIND
.byte EIND
.byte IXLIND
.byte PIND

.byte CIND
.byte EIND
.byte IYLIND
.byte PIND

pairtab2:	.byte CIND
.byte EIND
.byte LIND
.byte FIND

.byte CIND
.byte EIND
.byte IXLIND
.byte FIND

.byte CIND
.byte EIND
.byte IYLIND
.byte FIND

xormasks:	.byte FLAG_Z
.byte 0
.byte FLAG_C
.byte 0
.byte FLAG_PV
.byte 0
.byte FLAG_S
.byte 0

andmasks:	.byte FLAG_Z
.byte FLAG_Z
.byte FLAG_C
.byte FLAG_C
.byte FLAG_PV
.byte FLAG_PV
.byte FLAG_S
.byte FLAG_S
