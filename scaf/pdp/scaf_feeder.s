#     Pure Data Packet - scaf feeder routine.
#    Copyright (c) by Tom Schouten <tom@zwizwa.be>
# 
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
# 
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
# 
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

# for dup

#include "../compiler/scafmacro.s"


# *rg is only used for returning the stack pointer
# the 4 bit counter is using registers mm4-mm7 now
# long long scaf_feeder(void *tos, void *rg, *void() ca_rule, void *env)
.globl scaf_feeder
.type scaf_feeder, @function

#if defined(__i386__)        
scaf_feeder:
	pushl %ebp
	movl %esp, %ebp
	push %esi
	push %edi
	
	movl 20(%ebp), %edi	# load env ptr
	movl 8(%ebp), %esi	# load TOS2 ptr
	movl 16(%ebp), %eax	# address of ca routine
	pcmpeqw %mm3, %mm3	# load 1 reg

	call *%eax		# TOS = 32x2 cell result
	dup			# push %mm0 to memory
	movl (%esi), %eax
	movl 4(%esi), %edx
	lea 16(%esi), %esi	# discard stack
	movl %esi, (%edi)	# store for stack underflow check

	emms
	pop %edi
	pop %esi
	leave
	ret
#else
#if defined(__x86_64__)
scaf_feeder:
        movq %rdi, %rsi          # arg0: tos  (arg1 is ignoreD)
        movq %rdx, %rax          # arg2: ca_rule
        movq %rcx, %rdi          # arg3: 

	pcmpeqw %mm3, %mm3	# load 1 reg

	call *%rax		# TOS = 32x2 cell result
	dup			# push %mm0 to memory
	movq (%rsi), %rax       # | pop
        lea 16(%rsi), %rsi	# |

	movq %rsi, (%rdi)	# store for stack underflow check

	emms
	ret
#else
#error Needs __i386__ or __x84_64__
#endif
#endif
