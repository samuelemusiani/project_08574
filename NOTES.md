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
value and set the value of this register (`getENTRYHI()` and `setENTRYHI()`)
but in our case they are probably useless.
- cause: This register indicates the event that caused the trap. Its layout is
the following:

               31              30-0
        |  Interrupt  |  Exception code  |

    The `31` bit called interrupt indicates if the exeception is an interrupt 
    (1) or a trap (0). The exeption code contains the remaining informations in 
    order to understand what caused the interrupt/trap. All the exemptions code 
    that are useful are reported in the table below. The full list is available 
    at [2].

    | Interrupt | ExCode    | Description                                   |
    | --------- | --------- | --------------------------------------------- |
    | 1         | 3         | Machine software interrupt (Interval Timer)   |
    | 1         | 7         | Machine timer interrupt (CPU Timer)           |
    | 1         | 11        | Machine external interrupt                    |
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

    ^ The interrupt with ExCode 11 is not present in the Rovelli's thesis, but
    I believe it's important since in the `mie` registers is present the `11` bit.

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

              31-13   12-11   10-8     7      6-4     3     2-0
            |       |  MPP  |      |  MPIE  |     |  MIE  |     |

- pc_epc: This is the program counter.
- mie: This register holds the information about which interrupts are enabled. 
Interrupt cause number `i` corresponds to the i-th bit in both `mip` and `mie`.
The causes are in the tables ABOVE (not below). In Particular the first 15 bits 
of the `mie` register are as follows:

              15-12     11     10-8     7      6-4     3      2-0
            |       |  MEIE  |  0   |  MTIE  |   0  |  MSIE  |  0  |

    Reading the interrupts ExCode and the binded description I believe that:
    - MSIE: Stands for "Machine Software Interrupts Enabled" (Interval Timer)
    - MTIE: Stands for "Machine Timer Interrupts Enabled" (CPU Timer)
    - MEIE: Stands for "Machine External Interrupts Enabled" (Devices)

    Although in the Rovelli's thesis the following tables is showed [6] I don't 
    think that we can enable interrupts only for a specific device. All I/O
    interrupts should be enabled with the MEIE bit set to 1.


    | Interrupt | LineDescription           |
    | --------- | ------------------------- |
    | 16        | Inter Processor Interrupt |
    | 17        | Disk Device               |
    | 18        | Flash Device              |
    | 19        | Ethernet Device           |
    | 20        | Printer Device            |
    | 21        | Terminal Device           |

    The `mip` register holds the pending bits of all interrupts. This register
    is not writable and can be retrieved with the `getMIP()` function provided
    in `<uriscv/liburiscv.h>`. The following are the first 15 bits of the `mip`
    register:

              15-12     11     10-8     7      6-4     3      2-0
            |       |  MEIP  |  0   |  MTIP  |   0  |  MSIP  |  0  |

    The name of the bits are not specified, but I think they mean:
    - MSIP: Stands for "Machine Software Interrupts Pending" (Interval Timer)
    - MTIP: Stands for "Machine Timer Interrupts Pending" (CPU Timer)
    - MEIP: Stands for "Machine External Interrupts Pending" (Devices)

    MEIP does not work. If an external device sends an interrupt it will be 1 
    corresponding bit based on the table above.

- gpr[STATE_GPR_LEN]: The `GPRs` are the *General Purpose Registers*. There are
32 GPRs (STATE_GPR_LEN) and not all of them are useful. In <uriscv/types.h> 
there are some macros that allow a more human way of addressing these registers.
For example, in order to address the register number 0, if `s` is the processor
state, one can simply user the following code: `s->p_s.reg_zero`. This because
there is a macro like the following in the `types.h` header: 
`#define reg_zero gpr[0]`. 

    The useful registers are:
    - reg_sp: Stack pointer of the current processore state
    - reg_a0, reg_a4: Syscall registers

### <uriscv/liburiscv.h> register functions

The following are all the functions that allow register maniplation and reads
for uRISCV.

```c
// TLB specific
unsigned int getINDEX(void);
unsigned int getRANDOM(void);
unsigned int getENTRYLO(void);
unsigned int getBADVADDR(void);
unsigned int getENTRYHI(void);

unsigned int getSTATUS(void);   // Get the status register
unsigned int getCAUSE(void);    // Get the cause register
unsigned int getMIE(void);      // Get the mie register (not the bit)
unsigned int getMIP(void);      // Get the mip register
unsigned int getEPC(void);      // Address to return after an exception is handled 
unsigned int getPRID(void);     // A read-only processor ID register [7]
unsigned int getTIMER(void);    // Processor Local Timer (PLT) [7][8]

// TLB specific
unsigned int setINDEX(unsigned int index);
unsigned int setENTRYLO(unsigned int entry);
unsigned int setENTRYHI(unsigned int entry);

unsigned int setSTATUS(unsigned int entry); // Set status register
unsigned int setCAUSE(unsigned int cause);  // Set status cause (what for?)
unsigned int setMIE(unsigned int mie);      // Set MIE register
unsigned int setTIMER(unsigned int timer);  // Processor Local Timer (PLT)
```


# Timers

There are two timers:
- PLT: Is the Processor Local Timer and should be used to support process 
scheduling. It should be loaded with a value of 5ms when a process is dispached.
To enable interrupts from the PLT the MTIE bit is used in the `mie` register.
To set the PLT one should user the setTIMER function provided by 
 the `<uriscv/liburiscv.h>` library.

- Interval Timer: This timer is used to implement Speudo-clock ticks. The 
default value should be 100ms.
To enable interrupts from the Interval timer the MSIE bit is used in the `mie` 
register.
To set the Interval Timer there is a macro called `LDIT(T)` that can be used. 
It's defined in the `<uriscv/const.h>` header.


There is also a clock called TOD (Time-of-Day) that keeps track of how many cpu
cycles are executed (?). The only documentation I found was here: [9]. The 
specifications provided however mentions the TOD clock as it's well suited for
measuring intervals length. It's initialized at 0 at boot time and can't be 
written. It does not generate interrupts. In order to get its value there is
the `STCK(T)` macro.


# Memory

All addresses are 32 bit

The address space, both physical and logical, is divided into equal sized units
of 4 KB each. Hence an address has two components; a 20-bit Frame Number
(physical) or Page Number (logical), and a 12-bit Offset into the page.

                                31-12          11-0
                        | Frame/Page Number | Offset |


- BIOS region: 0x00000000 - 0x20000000.
- Installed RAM: 2-512 * 4Kb frames 0x20000000 - RAMTOP
- Bus error: RAMTOP - 0xFFFFFFFF

Since RAMTOP will fall between 0x2000.8000 to 0x2020.0000, RAMTOP is an 
address in kseg1. We have RAMTOP equal to 0x2010.0000

- Bios (kseg0): 0x00000000 - 0x20000000.
Access to this region is limited to kernel-mode only. User-mode access to kseg0 
will always raise an *Address Error Program Trap exception*. Access to any 
undefined or inaccessible portions of the BIOS region will raise a Bus Error 
Program Trap exception.[10]

- kseg1 (0x2000.0000 - 0x4000.0000): This 0.5 GB section is designed to hold the 
kernel/OS. Access to kseg1 is limited to kernel-mode only. User-mode access will 
raise an Address Error Program Trap exception.

All addresses in kseg1 below RAMTOP are exempt from virtual address
translation: these logical kseg1 addresses are also their physical address.
Addresses in kseg1 above RAMTOP can only be accessible through virtual
address translation.

- kseg2 (0x4000.0000 - 0x8000.0000): This 1 GB section is for use when
implementing a sophisticated operating system. Access to kseg2 is limited to 
kernel-mode only. User-mode access will raise an Address Error Program Trap 
exception.

- kuseg (0x8000.0000 - 0xFFFF.FFFF): This 2 GB section is for user programs. 
Access to kuseg is possible from both the kernel-mode and user-mode processor 
setting.
The kuseg of one process is differentiated from another process’s kuseg by
a unique 6-bit process identifier called the Address Space Identifier - ASID.
The ASID is contained in the EntryHi register (EntryHi.ASID), which is
part of a processor state.

The TLB Floor Address is an address below which address translation is disabled 
and the address is considered a physical address.

TLB EntryHi:

                          31-12                11-6     5-0
                | Virtual page number (VPN) |  ASID  |       |

TLB EntryLo:

                          31-12           11  10  9   8   7-0
                | Physical Frame Number | N | D | V | G |     |

- VPN: his is simply the higher order 20-bits of a logical address.
- ASID: The Address Space Identifier, a.k.a. process ID for this VPN.
- PFN: The physical frame number where the VPN for the specified ASID can be 
found in RAM.
- N: Not used
- D: Dirty bit: This bit is used to implement memory protection mechanisms.
When set to zero (off) a write access to a location in the physical frame will
cause a TLB-Modification exception to be raised. All Page Table entries (and 
therefore all TLB entries) should be marked as dirty.
- V: Valid bit: If set to 1 (on), this TLB entry is considered valid. A valid
entry is one where the PFN actually holds the ASID/virtual page number
pairing. If 0 (off), the indicated ASID/virtual page number pairing is not
actually in the PFN and any access to this page will cause a TLB-Invalid
exception to be raised. In practical terms, a TLB-Invalid exception is what
is generically called a “page fault.
- G: Not needed


# References
[1] https://www.cs.unibo.it/~renzo/doc/umps3/uMPS3princOfOperations.pdf p.24
[2] https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf p.39
[3] https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf p.20
[4] "Porting of the μMPS3 Educational Emulator to RISC-V", Gianmarie Rovelli p.18
[5] https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf p.21
[6] "Porting of the μMPS3 Educational Emulator to RISC-V", Gianmarie Rovelli p.23
[7] https://www.cs.unibo.it/~renzo/doc/umps3/uMPS3princOfOperations.pdf p.23
[8] https://www.cs.unibo.it/~renzo/doc/umps3/uMPS3princOfOperations.pdf p.75
[8] https://www.cs.unibo.it/~renzo/doc/umps3/uMPS3princOfOperations.pdf p.37
[10] https://www.cs.unibo.it/~renzo/doc/umps3/uMPS3princOfOperations.pdf p.61
