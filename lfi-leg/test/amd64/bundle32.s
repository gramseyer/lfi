call *%rax
>>>
.bundle_align_mode 5
movq %rax, %r15
.p2align 5
.bundle_lock
andl $0xffffffe0, %r15d
orq %r14, %r15
callq *%r15
.bundle_unlock
.p2align 5
------
call *32+output(%rip)
>>>
.bundle_align_mode 5
movq 32+output(%rip), %r15
.p2align 5
.bundle_lock
andl $0xffffffe0, %r15d
orq %r14, %r15
callq *%r15
.bundle_unlock
.p2align 5
------
jmp *(%rax)
>>>
.bundle_align_mode 5
movq %gs:(%eax), %r15
.bundle_lock
andl $0xffffffe0, %r15d
orq %r14, %r15
jmp *%r15
.bundle_unlock
------
callq *(%rax, %rdi, 4)
>>>
.bundle_align_mode 5
movq %gs:(%eax, %edi, 4), %r15
.p2align 5
.bundle_lock
andl $0xffffffe0, %r15d
orq %r14, %r15
callq *%r15
.bundle_unlock
.p2align 5
------
notrack jmp *%rax
>>>
.bundle_align_mode 5
.bundle_lock
andl $0xffffffe0, %eax
orq %r14, %rax
notrack jmp *%rax
.bundle_unlock
------
callq *__cxa_finalize@GOTPCREL(%rip)
>>>
.bundle_align_mode 5
movq __cxa_finalize@GOTPCREL(%rip), %r15
.p2align 5
.bundle_lock
andl $0xffffffe0, %r15d
orq %r14, %r15
callq *%r15
.bundle_unlock
.p2align 5
------
callq foo
>>>
.bundle_align_mode 5
callq foo
.p2align 5
------
ret
>>>
.bundle_align_mode 5
popq %r15
.bundle_lock
andl $0xffffffe0, %r15d
orq %r14, %r15
jmpq *%r15
.bundle_unlock
