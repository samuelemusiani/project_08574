# Notes for uRISCV

As the documentation is very fragmented, this is the place to put toghether all
the informations that we have on uRISCV.


## Useful headers files

After installing the uRISV emulator, you should have the following directory:
`/usr/local/include/uriscv/`. In this directory the are all the uRISCV headers
that can be usefull. For example, the <uriscv/liburiscv.h> that implements all 
the low level calls for such as LDST, HALT, WAIT, PANIC, ecc. is in this path.
There are other files that may be useful:
- aout.h
- arch.h
- bios.h
- const.h
- cpu.h
- csr.h
- liburiscv.h
- regdef.h
- types.h

Note that in types is defined `state_t` and `passupvector_t`.


## CPU Registers

In `types.h` is defined the *processor state*. This is used to save the state of
execution of a program when an exception is raised and control is trasnfered 
back to the nucleus. This sections summerizes all the relevant informations that
we have on all the registers of the CPU.

```c
/* Processor state */
typedef struct state {
  unsigned int entry_hi;
  unsigned int cause;
  unsigned int status;
  unsigned int pc_epc;
  unsigned int mie;
  unsigned int gpr[STATE_GPR_LEN];
} state_t;
```
- `entry_hi`: This register contains the current ASID, which is essentially the 
process ID[1]. In our implementation there is only one processor (Processor 0),
so this register should always stay at 0. There are functions that can get the
value and set the value of this register (`getENTRYHI()` and `setENTRYHI()`
but in our case they are probably useless.
- cause: // TODO
- status: // TODO
- pc_epc: Program counter
- mie: // TODO
- gpr[STATE_GPR_LEN]: /TODO

# References
[1]: https://www.cs.unibo.it/~renzo/doc/umps3/uMPS3princOfOperations.pdf p.24
