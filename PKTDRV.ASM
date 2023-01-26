; Packet Driver
;
                .286
                .MODEL  SMALL, C
                .CODE
MAX_DRV_NAME    EQU     12

;
Signature       DB      'PKT DRVR', 0
SignLEN         EQU     $ - Signature

PacketDriver    LABEL   DWORD
_IP             DW      0
_CS             DW      0
;
pktdrv_error    DB      0
;
                PUBLIC  initial_pktdrv
initial_pktdrv  PROC    NEAR USES ds es bx cx dx di si

                cld
                mov     ax, cs
                mov     ds, ax

                mov     ax, 3520h

init_next:      int     21h             ; ES:BX
                mov     di, bx
                add     di, 3
                mov     si, OFFSET Signature
                mov     cx, SignLEN
        rep     cmpsb
                jz      init_found

                inc     al
                jnz     init_next

                mov     ax, 0
                jmp     SHORT init_exit

init_found:     push    ax
                mov     ah, 35h
                int     21h
                mov     WORD PTR [_IP], bx
                mov     WORD PTR [_CS], es
                pop     ax
                xor     ah, ah
init_exit:
                ret

initial_pktdrv  ENDP


                PUBLIC  driver_info
driver_info     PROC    USES ds es bx cx dx si di, handle: WORD, ver: DWORD, iclass: DWORD, itype: DWORD, inum: DWORD, dname: DWORD
                mov     bx, [handle]
                mov     ax, 01FFh
                pushf
                cli
                call    DWORD PTR cs:[PacketDriver]
                mov     BYTE PTR cs:[pktdrv_error], dh
                jc      drvinfo_error
                les     di, [ver]
                mov     WORD PTR es:[di], bx
                xor     bx, bx
                mov     bl, ch
                les     di, [iclass]
                mov     WORD PTR es:[di], bx
                les     di, [itype]
                mov     WORD PTR es:[di], dx
                mov     bl, cl
                les     di, [inum]
                mov     WORD PTR es:[di], bx
                les     di, [dname]
                mov     cx, MAX_DRV_NAME
                mov     dx, ax
                cld
drv_name_copy:  lodsb
                stosb
                or      al, al
                jz      drv_name_fin
                loop    drv_name_copy
                xor     al, al
                stosb
drv_name_fin:   mov     ax, dx
                xor     ah, ah
                jmp     SHORT drvinfo_exit
drvinfo_error:  xor     ax, ax
drvinfo_exit:   ret
driver_info     ENDP


;    access_type()
;    int access_type(int if_class, int if_type, int if_number,
;                    char far *tp, unsigned typelen, int (far *receiver)());
;
                PUBLIC  access_type
access_type     PROC    NEAR USES ds es bx cx dx si di, iclass, itype, inum: WORD, tp: DWORD, tlen: WORD, addr: DWORD
                mov     ax, [iclass]
                mov     bx, [itype]
                mov     dx, [inum]
                mov     cx, [tlen]
                les     di, [addr]
                lds     si, [tp]
                mov     ah, 2
                pushf
                cli
                call    DWORD PTR cs:[PacketDriver]
                jnc     access_ok
                mov     BYTE PTR cs:[pktdrv_error], dh
                xor     ax, ax
access_ok:      ret
access_type     ENDP

                PUBLIC  release_type
release_type    PROC    NEAR USES bx dx, handle: WORD
                mov     bx, [handle]
                mov     ah, 3
                pushf
                cli
                call    DWORD PTR cs:[PacketDriver]
                mov     BYTE PTR cs:[pktdrv_error], dh
                mov     ax, 1
                sbb     ax, 0
                ret
release_type    ENDP

                PUBLIC  send_pkt
send_pkt        PROC    NEAR USES ds si cx dx, buffer: DWORD, leng: WORD
                mov     cx, [leng]
                lds     si, [buffer]
                mov     ah, 4
                pushf
                cli
                call    DWORD PTR cs:[PacketDriver]
                mov     BYTE PTR cs:[pktdrv_error], dh
                mov     ax, 1
                sbb     ax, 0
                ret
send_pkt        ENDP

                PUBLIC  terminate
terminate       PROC    NEAR USES bx dx, handle: WORD
                mov     bx, [handle]
                mov     ah, 05
                pushf
                cli
                call    DWORD PTR cs:[PacketDriver]
                mov     BYTE PTR cs:[pktdrv_error], dh
                mov     ax, 1
                sbb     ax, 0
                ret
terminate       ENDP

                PUBLIC  get_address
get_address     PROC    NEAR USES es di bx cx dx, handle: WORD, buffer: DWORD, leng: WORD
                mov     bx, [handle]
                mov     cx, [leng]
                les     di, [buffer]
                mov     ah, 06
                pushf
                cli
                call    DWORD PTR cs:[PacketDriver]
                mov     BYTE PTR cs:[pktdrv_error], dh
                mov     ax, 1
                sbb     ax, 0
                ret
get_address     ENDP

                PUBLIC  reset_interface
reset_interface PROC    NEAR USES bx dx, handle: WORD
                mov     bx, [handle]
                mov     ah, 07
                pushf
                cli
                call    DWORD PTR cs:[PacketDriver]
                mov     BYTE PTR cs:[pktdrv_error], dh
                mov     ax, 1
                sbb     ax, 0
                ret
reset_interface ENDP

                PUBLIC  set_multicast_list
set_multicast_list PROC NEAR USES es di cx, addrlist: DWORD, len: WORD
                les     di, [addrlist]
                mov     cx, [len]
                mov     ah, 16h
                pushf
                cli
                call    DWORD PTR cs:[PacketDriver]
                mov     BYTE PTR cs:[pktdrv_error], dh
                mov     ax, 1
                sbb     ax, 0
                ret
set_multicast_list ENDP

                PUBLIC  get_multicast_list
get_multicast_list PROC NEAR USES ds es si di cx, addrlist: DWORD, len: DWORD
                mov     ah, 17h
                pushf
                cli
                call    DWORD PTR cs:[PacketDriver]
                mov     BYTE PTR cs:[pktdrv_error], dh

                pushf
                jc      get_multicast_list_exit
                push    es
                pop     ds
                mov     si, di
                les     di, [addrlist]
                push    cx
                cld
        rep     movsb
                pop     ax
                lds     si, [len]
                mov     WORD PTR ds:[si], ax

get_multicast_list_exit:
                popf
                mov     ax, 1
                sbb     ax, 0
                ret
get_multicast_list ENDP

                PUBLIC  set_rcv_mode
set_rcv_mode    PROC    NEAR USES bx cx dx, handle, mode: WORD
                mov     bx, [handle]
                mov     cx, [mode]
                mov     ah, 14h
                pushf
                cli
                call    DWORD PTR cs:[PacketDriver]
                mov     BYTE PTR cs:[pktdrv_error], dh
                mov     ax, 1
                sbb     ax, 0
                ret
set_rcv_mode    ENDP

                PUBLIC  get_rcv_mode
get_rcv_mode    PROC    NEAR USES ds bx dx si, handle: WORD, mode: DWORD
                mov     bx, [handle]
                lds     si, [mode]
                mov     ah, 15h
                pushf
                cli
                call    DWORD PTR cs:[PacketDriver]
                mov     BYTE PTR cs:[pktdrv_error], dh
                mov     WORD PTR ds:[si], ax
                xor     ax, ax
                add     ax, WORD PTR ds:[si]
                ret
get_rcv_mode    ENDP


                PUBLIC  get_pktdrv_err
get_pktdrv_err  PROC    NEAR
                mov     al, BYTE PTR cs:[pktdrv_error]
                xor     ah, ah
                ret
get_pktdrv_err  ENDP



;; ----------------------------------------------------------------------

                PUBLIC  farmemcpy
farmemcpy       PROC    NEAR USES ds es cx di si, tar: DWORD, src: DWORD, len: WORD
                les     di, [tar]
                mov     ax, di
                shr     ax, 4
                and     di, 0Fh
                mov     cx, es
                add     ax, cx
                mov     es, ax
                lds     si, [src]
                mov     ax, si
                shr     ax, 4
                and     si, 0Fh
                mov     cx, ds
                add     ax, cx
                mov     ds, ax
                mov     cx, [len]
        rep     movsb
                ret
farmemcpy       ENDP

                PUBLIC  farmemcmp
farmemcmp       PROC    NEAR USES ds es cx di si, tar: DWORD, src: DWORD, len: WORD
                les     di, [tar]
                mov     ax, di
                shr     ax, 4
                and     di, 0Fh
                mov     cx, es
                add     ax, cx
                mov     es, ax
                lds     si, [src]
                mov     ax, si
                shr     ax, 4
                and     si, 0Fh
                mov     cx, ds
                add     ax, cx
                mov     ds, ax
                mov     cx, [len]
        rep     cmpsb
                mov     ax, 0
                jz      fmcmp_exit
                inc     ax
fmcmp_exit:     ret
farmemcmp       ENDP

                END
