call *%rax
>>>
.bundle_align_mode 4
movq %rax, %r11
.bundle_lock
andq $0xfffffffffffffff0, %r11
callq *%r11
.bundle_unlock
.p2align 4
------
mov (%rsi), %rdi
>>>
.bundle_align_mode 4
mov (%rsi), %rdi
------
mov %rsi, (%rdi)
>>>
.bundle_align_mode 4
mov %rsi, (%rdi)
------
ret
>>>
.bundle_align_mode 4
popq %r11
.bundle_lock
andq $0xfffffffffffffff0, %r11
jmpq *%r11
.bundle_unlock
------
jmp *(%rax)
>>>
.bundle_align_mode 4
movq (%rax), %r11
.bundle_lock
andq $0xfffffffffffffff0, %r11
jmp *%r11
.bundle_unlock
------
callq *(%rax, %rdi, 4)
>>>
.bundle_align_mode 4
movq (%rax, %rdi, 4), %r11
.bundle_lock
andq $0xfffffffffffffff0, %r11
callq *%r11
.bundle_unlock
.p2align 4
