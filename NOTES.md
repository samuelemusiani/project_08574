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
- cause: This register indicates the event that caused the trap. Its layout is
the following:

        31          30               0
        | Interrupt | Exception code |

    The `31` bit called interrupt indicates if the exeception is an interrupt 
    (1) or a trap (0). The exeption code contains the remaining informations in 
    order to understand what caused the interrupt/trap. All the exemptions code 
    that are useful are reported in the table below. The full list is available 
    at [2].

    | Interrupt | ExCode    | Description                                   |
    | --------- | --------- | --------------------------------------------- |
    | 1         | 3         | Machine software interrupt (Interval Timer)   |
    | 1         | 7         | Machine timer interrupt (CPU Timer)           |
    | 1         | ≥16       | Designed for platform use (Devices)           |
    | 0         | 0         | Instruction address misaligned                |
    | 0         | 1         | Instruction access fault                      |
    | 0         | 2         | Illegal instruction                           |
    | 0         | 3         | Breakpoint                                    |
    | 0         | 4         | Load address misaligned                       |
    | 0         | 5         | Load access fault                             |
    | 0         | 6         | Store address misaligned                      |
    | 0         | 7         | Store access fault                            |
    | 0         | 8         | Environment call from U-mode                  |
    | 0         | 11        | Environment call from M-mode                  |
    | 0         | 12        | Instruction page fault                        |
    | 0         | 13        | Load page fault                               |
    | 0         | 15        | Store page fault                              |
    | 0         | 24        | TLB mod                                       |
    | 0         | 25        | TLB load fault                                |
    | 0         | 26        | TLB store fault                               |
    | 0         | 27        | User TLB load fault                           |
    | 0         | 28        | User TLB store fault                          |
    | 0         | 29-31     | Designed for custom use                       |
    | 0         | 48-63     | Designed for custom use                       |

- status: The `mstatus` register keeps track of and controls of the current and 
past operating state [3] of the processor. Although it's a 32 bit register only 
4 of them are relevant for this project [4]:
    - MIE: It's the bit n. 3 and is used for enabling interrupts [5]. 1 Means
    enabled, 0 means disabled in the current state.
    - MPIE: It's the bit n. 7 [4]. When an exception is raised, the control goes 
    back to the exception handler that is in the nucleus, so the processor 
    operate with all interrupts masked. As a result, the value of MIE is set to 
    0 and the value of MIE before the exception was raised is overwritten. In 
    order to keep memory of that value MPIE is used. In summary **MPIE holds the
    MIE value before an exception is raised**. When control is given back to a
    process (through a `LDST()`), the MPIE value is copied into MIE and becomes
    the actual bit for enabling/disabling interrupts in the current running 
    process.
    - MPP[1:0]: MPP is two bits wide and correspond to the bits 11 and 12 [4].
    These bits hold the mode in witch the process was running before the 
    exception was raised (as for MPIE). The two mode possibles are Machine-mode
    (11) and User-mode (00) (I'm not 100% sure the values are correct). When 
    control is given back to a process (through a `LDST()`), the MPP value 
    becomes the actual mode in which the process is running on.

- pc_epc: This is the program counter.
- mie: // TODO
- gpr[STATE_GPR_LEN]: *General Porpous Registers*, //TODO

# References
[1] https://www.cs.unibo.it/~renzo/doc/umps3/uMPS3princOfOperations.pdf p.24
[2] https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf p.39
[3] https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf p.20
[4] https://www.cs.unibo.it/~renzo/doc/umps3/uMPS3princOfOperations.pdf p.18
[5] https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf p.21
