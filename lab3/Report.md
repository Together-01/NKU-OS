# 实验三：中断与中断处理流程

## 练习1：完善中断处理

**请编程完善trap.c中的中断处理函数trap，在对时钟中断进行处理的部分填写kern/trap/trap.c函数中处理时钟中断的部分，使操作系统每遇到100次时钟中断后，调用print_ticks子程序，向屏幕上打印一行文字”100 ticks”，在打印完10行后调用sbi.h中的shut_down()函数关机。**

**要求完成问题1提出的相关函数实现，提交改进后的源代码包（可以编译执行），并在实验报告中简要说明实现过程和定时器中断中断处理的流程。实现要求的部分代码后，运行整个系统，大约每1秒会输出一次”100 ticks”，输出10行。**

定时器中断中断处理的流程：

在 init.c 中调用函数 `clock_init()` 使能时钟中断、设置下一次时钟中断的时间以及初始化 ticks 记录。 

```c
void clock_init(void) {
    // enable timer interrupt in sie
    set_csr(sie, MIP_STIP);
    // divided by 500 when using Spike(2MHz)
    // divided by 100 when using QEMU(10MHz)
    // timebase = sbi_timebase() / 500;
    clock_set_next_event();

    // initialize time counter 'ticks' to zero
    ticks = 0;

    cprintf("++ setup timer interrupts\n");
}
```

在触发时钟中断时硬件会记录中断的类型到 `scause` 中，`interrupt_handler()` 函数会根据中断的类型跳转到对应的处理代码中。

kern/trap/trap.c 函数中处理时钟中断的代码如下：

```c
case IRQ_S_TIMER:
    clock_set_next_event();
    ticks++;
    if (ticks % TICK_NUM == 0) {
        print_ticks();
    }
    if(ticks / TICK_NUM >= 10) {
        sbi_shutdown();
    }
    break;
```

每次处理时钟中断时内核都要设置下次中断的时间，`ticks` 记录自增，如果触发了100次时钟中断便打印 `100 ticks` ，若打印十次该内容则调用 `sbi_shutdown()` 关机。

## 扩展练习 Challenge1：描述与理解中断流程

**回答：描述ucore中处理中断异常的流程（从异常的产生开始），其中mov a0，sp的目的是什么？SAVE_ALL中寄寄存器保存在栈中的位置是什么确定的？对于任何中断，__alltraps 中都需要保存所有寄存器吗？请说明理由。**

### ucore中处理中断异常的流程

- CPU执行指令时发生异常或外设发出中断请求

- CPU自动保存pc到sepc，根据stvec跳转到__alltraps

- SAVE_ALL宏保存寄存器形成陷阱帧

- move a0, sp将栈指针作为参数传递给trap函数

- trap函数根据scause识别类型并分发给具体处理程序

- RESTORE_ALL从栈中恢复寄存器

- sret指令恢复执行

### mov a0，sp的目的

mov a0，sp 目的是将栈指针作为参数传递给trap函数。此时sp指向内核栈上的一个trapframe，包含所有保存的寄存器值。C函数trap通过这个指针可以访问所有保存的上下文信息，用于异常分析和处理。

### SAVE_ALL中寄存器保存在栈中的位置

由REGBYTES常量和寄存器的偏移确定

### __alltraps 中是否都需要保存所有寄存器

对于任何中断，__alltraps 中有理由需要保存所有寄存器：
1. __alltraps是所有中断的统一入口，无法提前知道具体是哪种中断
2. 不完整保存可能导致某些中断处理时信息丢失
3. 完整上下文保存可支持中断嵌套处理

## Challenge2：理解上下文切换机制
**回答：在trapentry.S中汇编代码 csrw sscratch, sp；csrrw s0, sscratch, x0实现了什么操作，目的是什么？save all里面保存了stval scause这些csr，而在restore all里面却不还原它们？那这样store的意义何在呢？**

```assembly
csrw sscratch, sp；
csrrw s0, sscratch, x0
```
csrw sscratch, sp；这一句是把栈指针sp的值写入`sscratch`寄存器

csrrw s0, sscratch, x0，这一句是先把此时`sscratch`寄存器的值也就是原始栈指针读入`s0`寄存器，然后把 0 写入`sscratch`寄存器

目的就是安全地保存原始SP，将其存入内存，并标记当前状态为内核态。

`save all`里面保存了`stval、scause`这些`csr`而`restore all`不还原，原因是：`stval`和`scause`这两个寄存器是在 trap 发生时根据 trap 类型自动设置的，记录的分别是 trap 相关的地址和 trap 的种类，与恢复后要执行的代码无关。而后续如果出现了新的 trap，硬件会用新的值进行覆盖。

保存这两个寄存器的意义在于为 trap 处理函数提供诊断信息。


## Challenge3：完善异常中断

**编程完善在触发一条非法指令异常和断点异常，在 `kern/trap/trap.c` 的异常处理函数中捕获，并对其进行处理，简单输出异常类型和异常指令触发地址，即 `"Illegal instruction caught at 0x(地址)"`，`"ebreak caught at 0x（地址）"` 与 `"Exception type:Illegal instruction"`，`"Exception type: breakpoint"`。**

### 代码实现：

#### trap.c 改动

**非法指令异常处理：**

```c
case CAUSE_ILLEGAL_INSTRUCTION:
    // 非法指令异常处理
    /* LAB3 CHALLENGE3   YOUR CODE :  2311991 */
    /* (1)输出指令异常类型（ Illegal instruction）
     *(2)输出异常指令地址
     *(3)更新 tf->epc寄存器
     */
    cprintf("Illegal instruction caught at 0x%08x\n", tf->epc);
    cprintf("Exception type:Illegal instruction\n");
    tf->epc += 4; //跳过异常指令
    break;
```
**断点异常处理：**

```c
case CAUSE_BREAKPOINT:
    //断点异常处理
    /* LAB3 CHALLLENGE3   YOUR CODE :  2311991 */
    /* (1)输出指令异常类型（ breakpoint）
     *(2)输出异常指令地址
     *(3)更新 tf->epc寄存器
     */
    cprintf("ebreak caught at 0x%08x\n", tf->epc);
    cprintf("Exception type: breakpoint\n");
    tf->epc += 2; //跳过断点指令
    break;
```
#### init.c 触发异常
```c
cprintf("非法指令异常测试:\n");
asm volatile(".word 0x00000000"); //触发非法指令异常

cprintf("断点异常测试:\n");
asm volatile("ebreak"); //触发断点异常
```
### 测试结果
```
++ setup timer interrupts
非法指令异常测试:
Illegal instruction caught at 0xc02000a8
Exception type:Illegal instruction
断点异常测试:
ebreak caught at 0xc02000b8
Exception type: breakpoint
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
```

## 本实验重要知识点及与OS原理对应
1. 中断/异常处理流程
- 原理：中断机制是OS管理外设、实现并发的基础
- 关系：实验是原理在RISC-V架构上的具体实现

2. 上下文保存与恢复
- 实验：SAVE_ALL/RESTORE_ALL宏保存恢复寄存器
- 原理：进程上下文切换的核心步骤
- 差异：实验只完成寄存器切换，完整调度还需页表等切换

3. 特权级切换
- 实验：ecall从用户态陷入内核态，sret返回
- 原理：CPU特权级保护内核安全

1. 定时器中断
- 实验：处理时钟中断，简单计数
- 差异：实验只有基础计数，原理包含完整调度算法

## OS原理重要但实验未涉及知识点
- 进程管理 - 进程创建、调度、通信

- 虚拟内存 - 缺页处理、页表管理
