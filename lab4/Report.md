# 实验4 进程管理

## 练习1

**alloc_proc函数（位于kern/process/proc.c中）负责分配并返回一个新的struct proc_struct结构，用于存储新建立的内核线程的管理信息。ucore需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。**

**请在实验报告中简要说明你的设计实现过程。请回答如下问题：**

- 请说明`proc_struct`中`struct context context`和`struct trapframe *tf`成员变量含义和在本实验中的作用是啥？（提示通过看代码和编程调试可以判断出来）

**代码实现**：
```c
proc->state = PROC_UNINIT;
proc->pid = -1;
proc->runs = 0;
proc->kstack = 0;
proc->need_resched = 0;
proc->parent = NULL;
proc->mm = NULL;
memset(&(proc->context), 0, sizeof(struct context));
proc->tf = NULL;
proc->pgdir = boot_pgdir_pa;
proc->flags = 0;
memset(proc->name, 0, sizeof(proc->name));
```
`alloc_proc`函数的目的是分配一个新的`proc_struct`结构体并进行初始化

**struct context context：**
- **含义**：进程的上下文，保存进程切换时需要保留的寄存器（ra, sp, s0-s11）
- **作用**：进程切换时保存和恢复执行。`switch_to` 函数将当前上下文保存到 `from->context`，从 `to->context` 恢复新进程上下文

**struct trapframe \*tf：**
- **含义**：指向进程的陷阱帧，保存在内核栈顶，包含中断或异常时的完整寄存器状态
- **作用**：
  1. 系统调用时传递参数和返回值（a0-a7）
  2. 进程首次执行时的初始状态（在 `kernel_thread` 中设置）
  3. 中断处理时保存被中断进程的状态


## 练习2

**创建一个内核线程需要分配和设置好很多资源。kernel_thread函数通过调用do_fork函数完成具体内核线程的创建工作。do_kernel函数会调用alloc_proc函数来分配并初始化一个进程控制块，但alloc_proc只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过do_fork实际创建新的内核线程。do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。因此，我们实际需要"fork"的东西就是stack和trapframe。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在kern/process/proc.c中的do_fork函数中的处理过程。它的大致执行步骤包括：**

- 调用alloc_proc，首先获得一块用户信息块。
- 为进程分配一个内核栈。
- 复制原进程的内存管理信息到新进程（但内核线程不必做此事）
- 复制原进程上下文到新进程
- 将新进程添加到进程列表
- 唤醒新进程
- 返回新进程号

**请在实验报告中简要说明你的设计实现过程。请回答如下问题：**

- 请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由。

**代码实现**
```c
//    1. call alloc_proc to allocate a proc_struct
if ((proc = alloc_proc()) == NULL)
{
    goto fork_out;
}

//    2. call setup_kstack to allocate a kernel stack for child process
if ((ret = setup_kstack(proc)) != 0)
{
    goto bad_fork_cleanup_proc;
}

//    3. call copy_mm to dup OR share mm according clone_flag
if ((ret = copy_mm(clone_flags, proc)) != 0)
{
    goto bad_fork_cleanup_kstack;
}

//    4. call copy_thread to setup tf & context in proc_struct
copy_thread(proc, stack, tf);

//    5. insert proc_struct into hash_list && proc_list
proc->pid = get_pid();
list_add(&proc_list, &(proc->list_link));
hash_proc(proc);

//    6. call wakeup_proc to make the new child process RUNNABLE
nr_process++;
wakeup_proc(proc);

//    7. set ret vaule using child proc's pid
ret = proc->pid;
```

**ucore是否做到给每个新fork的线程一个唯一的id？**

**是**。理由：

1. `get_pid()` 函数通过静态变量 `last_pid` 和 `next_safe` 跟踪PID分配
2. 分配时遍历所有进程检查冲突：
   ```c
   while ((le = list_next(le)) != list) {
       if (proc->pid == last_pid) { // 发现冲突
           // 重新搜索可用PID
       }
   }
   ```
3. 使用 `next_safe` 避免每次全表扫描
4. PID范围管理：超过 `MAX_PID` 时回绕到1重新开始

因此ucore能保证每个新fork线程获得唯一PID。


## 练习3

**proc_run用于将指定的进程切换到CPU上运行。它的大致执行步骤包括：**

- 检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。
- 禁用中断。你可以使用/kern/sync/sync.h中定义好的宏local_intr_save(x)和local_intr_restore(x)来实现关、开中断。
- 切换当前进程为要运行的进程。
- 切换页表，以便使用新进程的地址空间。/libs/riscv.h中提供了lsatp(unsigned int pgdir)函数，可实现修改SATP寄存器值的功能。
- 实现上下文切换。/kern/process中已经预先编写好了switch.S，其中定义了switch_to()函数。可实现两个进程的context切换。
- 允许中断。

**请回答如下问题：**

- 在本实验的执行过程中，创建且运行了几个内核线程？

**完成代码编写后，编译并运行代码：make qemu**

**代码实现**：
```c
bool intr_flag;
struct proc_struct *prev = current, *next = proc;
local_intr_save(intr_flag);
{
    current = proc;

    lsatp(next->pgdir);

    switch_to(&(prev->context), &(next->context));
}
local_intr_restore(intr_flag);
```

**创建且运行了几个内核线程？**

在ucore实验执行过程中，创建并运行了**2个内核线程**：

1. **idleproc**（空闲进程）：
   - PID = 0
   - 名称："idle"
   - 系统空闲时运行，不断检查是否需要调度

2. **initproc**（初始进程）：
   - PID = 1  
   - 名称："init"
   - 由`kernel_thread(init_main, "Hello world!!", 0)`创建
   - 执行`init_main`函数，打印提示信息

这两个内核线程在`proc_init()`函数中创建，构成了ucore最初的环境。

**实验结果**
```text
alloc_proc() correct!
++ setup timer interrupts
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
kernel panic at kern/process/proc.c:389:
    process exit!!.
```

## 扩展练习 Challenge

1. 说明语句local_intr_save(intr_flag);....local_intr_restore(intr_flag);是如何实现开关中断的？

2. 深入理解不同分页模式的工作原理（思考题）

    get_pte()函数（位于kern/mm/pmm.c）用于在页表中查找或创建页表项，从而实现对指定线性地址对应的物理页的访问和映射操作。这在操作系统中的分页机制下，是实现虚拟内存与物理内存之间映射关系非常重要的内容。

   - get_pte()函数中有两段形式类似的代码， 结合sv32，sv39，sv48的异同，解释这两段代码为什么如此相像。
   - 目前get_pte()函数将页表项的查找和页表项的分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开？

### 1.开关中断的实现

`local_intr_save(intr_flag)` 和 `local_intr_restore(intr_flag)` 通过 RISC-V 的 CSR 指令实现开关中断：

**执行过程：**

1. **`local_intr_save(intr_flag)`**：
   - 读取 `sstatus` 寄存器检查 `SIE` 位
   - 如果中断原本开启：执行 `csrrc sstatus, SSTATUS_SIE` 关闭中断；`intr_flag = 1` 记录原状态
   - 如果中断原本关闭：`intr_flag = 0`

2. **临界区执行**：中断关闭状态下运行

3. **`local_intr_restore(intr_flag)`**：
   - 如果 `intr_flag = 1`，执行 `csrrs sstatus, SSTATUS_SIE` 重新开启中断
   - 如果 `intr_flag = 0`，保持中断关闭状态

**CSR 指令详解：**

- **`csrrc`** (CSR Read and Clear)：
  ```asm
  csrrc rd, csr, rs1
  ```
  操作：`rd ← csr`, `csr ← csr & ~rs1`
  读取 CSR 到 rd，清除 rs1 指定的位

- **`csrrs`** (CSR Read and Set)：
  ```asm
  csrrs rd, csr, rs1  
  ```
  操作：`rd ← csr`, `csr ← csr | rs1`
  读取 CSR 到 rd，设置 rs1 指定的位


通过原子性的 CSR 操作确保中断状态正确保存和恢复。

### 2.深入理解不同分页模式的工作原理

**两段相似代码的原因**

在 RISC-V 架构中，页表采用多级树形结构。`get_pte()` 函数中，有两段几乎相同的代码块，分别处理了 **第一级页目录项（pdep1）** 和 **第二级页目录项（pdep0）**。这是因为在 **sv39** 模式下，虚拟地址被划分为多个索引字段，依次用于查找不同级别的页表项。

#### 地址划分（sv39）：
- **虚拟地址**：`[38:0]`
- **PDX1(la)**：第 30~38 位（第 1 级索引）
- **PDX0(la)**：第 21~29 位（第 2 级索引）
- **PTX(la)**：第 12~20 位（第 3 级索引）

#### 代码逻辑：
- 第一段：根据 `PDX1(la)` 查找第一级页目录项 `pdep1`，若不存在且 `create` 为真，则分配一个页作为下一级页表。
- 第二段：根据 `PDX0(la)` 在上一级页表指向的页中查找第二级页目录项 `pdep0`，同样支持按需分配。

这两段代码之所以相似，是因为**页表的每一级结构都是递归的**：每一级都是一个页表项数组，每一项要么指向下一级页表的物理页，要么是最终页表项（指向物理页）。因此，每一级的查找和分配逻辑是相同的，只是使用的索引字段和所在层级不同。

> 如果是 **sv48**，则会有**三段**相似代码；如果是 **sv32**，则只有**一段**（因为只有两级）。


**是否应该将查找和分配功能拆开？**

目前 `get_pte()` 将 **查找页表项** 和 **分配页表页** 两个功能耦合在一起。这种写法有其优缺点：

**优点**：
- **使用方便**：调用者无需关心页表是否存在，函数会自动按需分配。
- **减少代码重复**：避免调用者在外部重复检查并分配页表页。

**缺点**：
- **功能不单一**：违反了“单一职责原则”，一个函数承担了多个职责。
- **错误处理复杂**：函数可能因分配失败返回 `NULL`，但调用者难以区分是“不存在”还是“分配失败”。
- **性能不确定**：隐藏的分配行为可能导致意外的性能开销。

从操作系统的代码维护角度来看，我认为分开这两个功能更好。

## 本实验中重要的知识点及与OS原理的对应关系

**1. 进程控制块（PCB）与进程管理**
- **实验知识点**：`struct proc_struct` 结构体，包含进程状态、PID、上下文、页表等管理信息
- **OS原理知识点**：进程控制块的概念和作用
- **理解**：实验中的 `proc_struct` 就是原理中的PCB具体实现，两者都用于保存进程的完整状态信息。差异在于实验中的实现针对RISC-V架构，包含了架构特定的上下文和陷阱帧。

**2. 进程上下文切换**
- **实验知识点**：`switch_to` 函数通过保存/恢复寄存器实现上下文切换
- **OS原理知识点**：进程切换的上下文保存与恢复机制
- **理解**：实验通过汇编代码 `switch.S` 具体实现了原理中描述的上下文切换过程，核心都是保存当前进程状态、加载新进程状态。实验更具体地展示了RISC-V架构下需要保存哪些寄存器。

**3. 进程创建与fork机制**
- **实验知识点**：`do_fork` 函数复制父进程资源创建子进程
- **OS原理知识点**：进程创建和fork操作原理
- **理解**：实验实现了原理中的fork语义，但内核线程的fork简化了内存空间复制（`copy_mm`），体现了理论到实践的适配。

**4. 进程调度与状态转换**
- **实验知识点**：`wakeup_proc` 将进程设为RUNNABLE，`proc_run` 执行进程切换
- **OS原理知识点**：进程状态模型和调度算法
- **理解**：实验实现了状态转换的基本框架，但调度算法相对简单，主要演示了RUNNABLE状态的维护。

## OS原理中重要但实验未涉及的知识点

**1. 完整的进程调度算法**
- 原理中的多级反馈队列、公平共享调度等复杂算法在实验中未实现

**2. 进程间通信（IPC）机制**
- 信号、管道、消息队列、共享内存等IPC机制在基础实验中未涉及

**3. 虚拟内存管理的完整实现**
- 缺页处理、页面置换算法（LRU、Clock等）、写时复制等高级特性

**4. 多处理器调度与同步**
- SMP环境下的负载均衡、处理器亲和性、自旋锁优化等


