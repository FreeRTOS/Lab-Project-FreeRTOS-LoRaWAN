# 1 "C:\\Workspace\\FreeRTOS-LoRaWAN\\projects\\nordic_nrf52\\SEGGER_THUMB_Startup.s"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "C:\\Workspace\\FreeRTOS-LoRaWAN\\projects\\nordic_nrf52\\SEGGER_THUMB_Startup.s"
# 58 "C:\\Workspace\\FreeRTOS-LoRaWAN\\projects\\nordic_nrf52\\SEGGER_THUMB_Startup.s"
  .syntax unified

  .section .segger.init.__SEGGER_init_lzss, "ax"
  .code 16
  .balign 2
  .global __SEGGER_init_lzss
.thumb_func
__SEGGER_init_lzss:
  ldr r0, [r4] @ destination
  ldr r1, [r4, #4] @ source stream
  adds r4, r4, #8
2:
  ldrb r2, [r1]
  adds r1, r1, #1
  tst r2, r2
  beq 9f @ 0 -> end of table
  cmp r2, #0x80
  bcc 1f @ +ve -> literal run
# 85 "C:\\Workspace\\FreeRTOS-LoRaWAN\\projects\\nordic_nrf52\\SEGGER_THUMB_Startup.s"
3:
  subs r2, r2, #0x80
  beq 10f
  ldrb r3, [r1]
  adds r1, r1, #1
  cmp r3, #0x80
  bcc 5f
  subs r3, r3, #0x80
  lsls r3, r3, #8
  ldrb r5, [r1]
  adds r1, r1, #1
  adds r3, r3, r5
5:
  subs r5, r0, r3
4:
  ldrb r3, [r5]
  strb r3, [r0]
  adds r5, r5, #1
  adds r0, r0, #1
  subs r2, r2, #1
  bne 4b
  b 2b
# 116 "C:\\Workspace\\FreeRTOS-LoRaWAN\\projects\\nordic_nrf52\\SEGGER_THUMB_Startup.s"
1:
  ldrb r3, [r1]
  adds r1, r1, #1
  strb r3, [r0]
  adds r0, r0, #1
  subs r2, r2, #1
  bne 1b
  b 2b
9:
  bx lr
10:
  b 10b

  .section .segger.init.__SEGGER_init_zero, "ax"
  .code 16
  .balign 2
  .global __SEGGER_init_zero
.thumb_func
__SEGGER_init_zero:
  ldr r0, [r4] @ destination
  ldr r1, [r4, #4] @ size
  adds r4, r4, #8
  tst r1, r1
  beq 2f
  movs r2, #0
1:
  strb r2, [r0]
  adds r0, r0, #1
  subs r1, r1, #1
  bne 1b
2:
  bx lr

  .section .segger.init.__SEGGER_init_copy, "ax"
  .code 16
  .balign 2
  .global __SEGGER_init_copy
.thumb_func
__SEGGER_init_copy:
  ldr r0, [r4] @ destination
  ldr r1, [r4, #4] @ source
  ldr r2, [r4, #8] @ size
  adds r4, r4, #12
  tst r2, r2
  beq 2f
1:
  ldrb r3, [r1]
  strb r3, [r0]
  adds r0, r0, #1
  adds r1, r1, #1
  subs r2, r2, #1
  bne 1b
2:
  bx lr

  .section .segger.init.__SEGGER_init_pack, "ax"
  .code 16
  .balign 2
  .global __SEGGER_init_pack
.thumb_func
__SEGGER_init_pack:
  ldr r0, [r4] @ destination
  ldr r1, [r4, #4] @ source stream
  adds r4, r4, #8
1:
  ldrb r2, [r1]
  adds r1, r1, #1
  cmp r2, #0x80
  beq 4f
  bcc 3f
  ldrb r3, [r1] @ byte to replicate
  adds r1, r1, #1
  negs r2, r2
  adds r2, r2, #255
  adds r2, r2, #1
2:
  strb r3, [r0]
  adds r0, r0, #1
  subs r2, r2, #1
  bpl 2b
  b 1b

3: @ 1+n literal bytes
  ldrb r3, [r1]
  strb r3, [r0]
  adds r0, r0, #1
  adds r1, r1, #1
  subs r2, r2, #1
  bpl 3b
  b 1b
4:
  bx lr

  .section .segger.init.__SEGGER_init_zpak, "ax"
  .code 16
  .balign 2
  .global __SEGGER_init_zpak
.thumb_func
__SEGGER_init_zpak:
  ldr r0, [r4] @ destination
  ldr r1, [r4, #4] @ source stream
  ldr r2, [r4, #8] @ size
  adds r4, r4, #12 @ skip table entries
1:
  ldrb r3, [r1] @ get control byte from source stream
  adds r1, r1, #1
  movs r6, #8
2:
  movs r5, #0 @ prepare zero filler
  lsrs r3, r3, #1 @ get byte control flag
  bcs 3f @ carry set -> zero filler
  ldrb r5, [r1] @ get literal byte from source stream
  adds r1, r1, #1
3:
  strb r5, [r0] @ store initialization byte
  adds r0, r0, #1
  subs r2, r2, #1 @ size -= 1
  beq 4f @ exit when destination filled
  subs r6, r6, #1 @ decrement bit count
  bne 2b @ still within this control byte
  b 1b @ get next control byte
4:
  bx lr
# 248 "C:\\Workspace\\FreeRTOS-LoRaWAN\\projects\\nordic_nrf52\\SEGGER_THUMB_Startup.s"
  .global _start
  .extern main
  .global exit
  .weak exit





  .section .init, "ax"
  .code 16
  .align 1
  .thumb_func

_start:
  ldr r0, = __stack_end__
  mov sp, r0
  ldr r4, =__SEGGER_init_table__
1:
  ldr r0, [r4]
  adds r4, r4, #4
  tst r0, r0
  beq 2f
  blx r0
  b 1b
2:


  ldr r0, = __heap_start__
  ldr r1, = __heap_end__
  subs r1, r1, r0
  cmp r1, #8
  blt 1f
  movs r2, #0
  str r2, [r0]
  adds r0, r0, #4
  str r1, [r0]
1:







  ldr r0, =__ctors_start__
  ldr r1, =__ctors_end__
ctor_loop:
  cmp r0, r1
  beq ctor_end
  ldr r2, [r0]
  adds r0, #4
  push {r0-r1}
  blx r2
  pop {r0-r1}
  b ctor_loop
ctor_end:


  movs r0, #0
  mov lr, r0
  mov r12, sp

  .type start, function
start:
# 321 "C:\\Workspace\\FreeRTOS-LoRaWAN\\projects\\nordic_nrf52\\SEGGER_THUMB_Startup.s"
  movs r0, #0
  movs r1, #0

  ldr r2, =main
  blx r2

  .thumb_func
exit:
# 357 "C:\\Workspace\\FreeRTOS-LoRaWAN\\projects\\nordic_nrf52\\SEGGER_THUMB_Startup.s"
exit_loop:
  b exit_loop



.macro HELPER helper_name
  .section .text.\helper_name, "ax", %progbits
  .global \helper_name
  .align 1
  .weak \helper_name
\helper_name:
  .thumb_func
.endm

.macro JUMPTO name







  b \name

.endm

HELPER __aeabi_read_tp
  ldr r0, =__tbss_start__-8
  bx lr
HELPER abort
  b .
HELPER __assert
  b .
HELPER __aeabi_assert
  b .
HELPER __sync_synchronize
  bx lr
HELPER __getchar
  JUMPTO debug_getchar
HELPER __putchar
  JUMPTO debug_putchar
HELPER __open
  JUMPTO debug_fopen
HELPER __close
  JUMPTO debug_fclose
HELPER __write
  mov r3, r0
  mov r0, r1
  movs r1, #1
  JUMPTO debug_fwrite
HELPER __read
  mov r3, r0
  mov r0, r1
  movs r1, #1
  JUMPTO debug_fread
HELPER __seek
  push {r4, lr}
  mov r4, r0
  bl debug_fseek
  cmp r0, #0
  bne 1f
  mov r0, r4
  bl debug_ftell
  pop {r4, pc}
1:
  ldr r0, =-1
  pop {r4, pc}

  .section .bss.__user_locale_name_buffer, "aw", %nobits
  .global __user_locale_name_buffer
  .weak __user_locale_name_buffer
  __user_locale_name_buffer:
  .word 0x0
