bits 32
section .text
        ;multiboot spec
        align 4
        dd 0x1BADB002              	
        dd 0x00                    	
        dd - (0x1BADB002 + 0x00)  

global start
global keyboard_handler
global read_port
global write_port
global load_idt
global asm_toMain
global timer_handler

extern main
extern kmain 		
extern keyCall
extern nextPtr
extern c_toMain
extern timer

read_port:
	mov edx, [esp + 4]		
	in al, dx	
	ret

write_port:
	mov   edx, [esp + 4]    
	mov   al, [esp + 4 + 4]  
	out   dx, al  
	ret

load_idt:
	mov edx, [esp + 4]
	lidt [edx]
	sti 				
	ret

keyboard_handler:
	push ebp
	push esp
	call keyCall
	pop edx
	pop ebp
	mov esp, edx	
	iretd

timer_handler:
    call timer
    iretd

asm_toMain:
	push ebp
	push esp
	call c_toMain
	pop edx
	pop ebp
	mov esp, edx
	pop edx
	pop ebx
	pop ebx
	push edx
	ret
	
start:
	cli 				
	mov esp, stack_space
	push ebp
	push esp
	call main
	pop edx
	pop ebp
	mov esp, edx
	ret
	hlt 				

section .bss
resb 8192; 
stack_space: