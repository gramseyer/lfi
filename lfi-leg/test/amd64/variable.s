mov (%rsi), %rdx
>>>
.bundle_align_mode 4
.bundle_lock
pext %r15, %rsi, %r11
mov (%r14, %r11), %rdx
.bundle_unlock
------
mov (%rsi, %rax), %rdx
>>>
.bundle_align_mode 4
leaq (%rsi, %rax), %r11
.bundle_lock
pext %r15, %r11, %r11
mov (%r14, %r11), %rdx
.bundle_unlock
------
callq *(%rsp)
>>>
.bundle_align_mode 4
movq (%rsp), %r11
.bundle_lock
andq %r15, %r11
andq $0xfffffffffffffff0, %r11
orq %r14, %r11
callq *%r11
.bundle_unlock
.p2align 4
------
imulq $24, -88(%rsp), %rax
imulq $24, -88(%rdi), %rax
>>>
.bundle_align_mode 4
imulq $24, -88(%rsp), %rax
.bundle_lock
movq %rdi, %r11
andq %r15, %r11
imulq $24, -88(%r14, %r11), %rax
.bundle_unlock
------
movq %dh, (%rax)
>>>
.bundle_align_mode 4
movq %rdi, %r11
.bundle_lock
pext %r15, %rax, %rdi
leaq (%r14, %rdi), %rdi
movq %dh, (%rdi)
.bundle_unlock
movq %r11, %rdi
------
rep stosq
>>>
.bundle_align_mode 4
.bundle_lock
pext %r15, %rdi, %rdi
leaq (%r14, %rdi), %rdi
rep stosq
.bundle_unlock
------
movq %rcx, 64(%rdi)
movq %rcx, 8(%rdi)
xorl %ecx, %ecx
>>>
.bundle_align_mode 4
.bundle_lock
movq %rdi, %r11
andq %r15, %r11
movq %rcx, 64(%r14, %r11)
.bundle_unlock
.bundle_lock
movq %rdi, %r11
andq %r15, %r11
movq %rcx, 8(%r14, %r11)
.bundle_unlock
xorl %ecx, %ecx
------
movq %rcx, 64(%rdi)
jne foo
>>>
.bundle_align_mode 4
.bundle_lock
pext %r15, %rdi, %r11
movq %rcx, 64(%r14, %r11)
.bundle_unlock
jne foo
------
mov %rax, (%rdi, %rsi)
addq $8, %rdi
>>>
.bundle_align_mode 4
leaq (%rdi, %rsi), %r11
.bundle_lock
andq %r15, %r11
mov %rax, (%r14, %r11)
.bundle_unlock
addq $8, %rdi
------
mov %rax, (%rdi, %rsi)
cmovns %r11, %rdx
>>>
.bundle_align_mode 4
leaq (%rdi, %rsi), %r11
.bundle_lock
pext %r15, %r11, %r11
mov %rax, (%r14, %r11)
.bundle_unlock
cmovns %r11, %rdx
