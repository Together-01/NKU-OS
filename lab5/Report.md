# lab5


## 练习1: 加载应用程序并执行（需要编码）

**do_execve**函数调用`load_icode`（位于kern/process/proc.c中）来加载并解析一个处于内存中的ELF执行文件格式的应用程序。你需要补充`load_icode`的第6步，建立相应的用户内存空间来放置应用程序的代码段、数据段等，且要设置好`proc_struct`结构中的成员变量trapframe中的内容，确保在执行此进程后，能够从应用程序设定的起始执行地址开始执行。需设置正确的trapframe内容。

**请在实验报告中简要说明你的设计实现过程。**

请简要描述这个用户态进程被ucore选择占用CPU执行（RUNNING态）到具体执行应用程序第一条指令的整个经过。

---

代码实现
```c
tf->gpr.sp = USTACKTOP;
tf->epc = elf->e_entry;
tf->status = (sstatus & ~SSTATUS_SPP) | SSTATUS_SPIE;
```
**用户态进程被ucore选择占用CPU执行（RUNNING态）到具体执行应用程序第一条指令的整个经过：**
- 在`init_main`中调用`kernel_thread`创建新进程`user_main`
- user_main函数调用宏KERNEL_EXECVE
- KERNEL_EXECVE调用kernel_execve，发生断点异常
- 执行trap函数，执行到CAUSE_BREAKPOINT处,调用syscall
- syscall根据系统调用号，执行sys_exec，调用do_execve
- do_execve调用load_icode，加载程序文件
- 加载完毕，返回至`__alltraps`的末尾，执行__trapret后的内容，直到sret，退出内核态，回到用户态，执行加载的文件。

## 练习2: 父进程复制自己的内存空间给子进程（需要编码）
创建子进程的函数`do_fork`在执行中将拷贝当前进程（即父进程）的用户内存地址空间中的合法内容到新进程中（子进程），完成内存资源的复制。具体是通过`copy_range`函数（位于kern/mm/pmm.c中）实现的，请补充`copy_range`的实现，确保能够正确执行。

**请在实验报告中简要说明你的设计实现过程。**

- 如何设计实现`Copy on Write`机制？给出概要设计，鼓励给出详细设计。
```text
Copy-on-write（简称COW）的基本概念是指如果有多个使用者对一个资源A（比如内存块）进行读操作，则每个使用者只需获得一个指向同一个资源A的指针，就可以该资源了。若某使用者需要对这个资源A进行写操作，系统会对该资源进行拷贝操作，从而使得该“写操作”使用者获得一个该资源A的“私有”拷贝—资源B，可对资源B进行写操作。该“写操作”使用者对资源B的改变对于其他的使用者而言是不可见的，因为其他使用者看到的还是资源A。
```

代码实现
```c
uintptr_t src_kvaddr = page2kva(page);
uintptr_t dst_kvaddr = page2kva(npage);
memcpy((void *)dst_kvaddr, (void *)src_kvaddr, PGSIZE);
ret = page_insert(to, npage, start, perm);
```

- page2kva(page)：获取父页面物理地址对应的内核虚拟地址（源地址）

- page2kva(npage)：获取新分配页面的内核虚拟地址（目标地址）

- memcpy(...)：复制整个页面内容（4KB）

- page_insert(...)：将新页面插入子进程页表，替换原来的只读映射


**Copy on Write 机制实现概要**

* **基本思想**：
  多个进程最初共享同一份只读资源；当某个进程尝试写该资源时，系统才为其复制一份私有副本。

* **共享阶段**：
  创建进程（如 `fork`）时，不复制内存页，而是让父子进程共享物理页，并将页表项设为**只读 + COW 标记**。

* **写触发机制**：
  对共享页的写操作会触发**写保护异常（page fault）**。

* **复制处理**：
  内核在异常处理中：

  * 分配新物理页并复制原内容；
  * 更新写进程页表，使其指向新页并允许写；
  * 其他进程仍指向原页，不受影响。

* **结果**：
  实现“读时共享、写时复制”，在保证隔离性的同时减少内存拷贝、提高性能。


### 练习3: 阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现，以及系统调用的实现（不需要编码）

**请在实验报告中简要说明你对 fork/exec/wait/exit函数的分析。并回答如下问题：**

- 请分析fork/exec/wait/exit的执行流程。重点关注哪些操作是在用户态完成，哪些是在内核态完成？内核态与用户态程序是如何交错执行的？内核态执行结果是如何返回给用户程序的？
- 请给出ucore中一个用户态进程的执行状态生命周期图（包执行状态，执行状态之间的变换关系，以及产生变换的事件或函数调用）。（字符方式画即可）
  
执行：make grade。如果所显示的应用程序检测都输出ok，则基本正确。（使用的是qemu-4.1.1）

**fork/exec/wait/exit执行流程**

- **fork()**
  - **用户态**：调用`sys_fork()` → `syscall(SYS_fork)` → `ecall`
  - **内核态**：`do_fork()`复制进程→设置子进程`tf->gpr.a0=0`→父进程返回子进程PID
  - **返回**：通过`trapframe.a0`返回结果

- **exec()**
  - **用户态**：调用`sys_exec()` → `ecall`
  - **内核态**：`do_execve()`释放旧内存→`load_icode()`加载新程序→修改`trapframe.epc=新入口`
  - **返回**：**不返回**，直接`sret`到新程序入口

- **wait()**
  - **用户态**：调用`sys_wait()` → `ecall`
  - **内核态**：`do_wait()`检查子进程状态→若无僵尸子进程则**睡眠**(`schedule()`)→被唤醒后返回退出码
  - **返回**：通过`trapframe.a0`返回状态，通过指针参数返回退出码

- **exit()**
  - **用户态**：调用`sys_exit()` → `ecall`
  - **内核态**：`do_exit()`释放资源→设置僵尸状态→唤醒父进程→`schedule()`永不返回
  - **返回**：**不返回**，进程终止



**用户态→内核态**

**触发**：`ecall`系统调用或中断  
**硬件**：CPU自动保存`sepc`、`scause`、设置`sstatus.SPP=0`  
**软件**：`__alltraps`保存所有寄存器到trapframe，调用C处理函数

**内核态处理**
`trap()` → `exception_handler()` → `syscall()`或中断处理

**内核态→用户态**  

**恢复**：`__trapret`从trapframe恢复寄存器  
**关键**：根据`sstatus.SPP`设置`sscratch`（返回用户态时=内核栈指针）  
**返回**：`sret`指令恢复特权级，跳转`sepc`

**核心机制**
- **trapframe**：保存/恢复完整CPU状态
- **sscratch**：用户态时存内核栈指针，内核态时为0
- **SPP位**：标识来源特权级（0=用户，1=内核）
- **sret**：原子完成特权级切换+跳转


**用户态进程生命周期图**

```
     创建
       ↓
   PROC_UNINIT
       ↓ (kernel_thread/wakeup_proc)
   PROC_RUNNABLE
       ↓ (schedule()选中)
   PROC_RUNNING
     ↙       ↘
(时间片用完)   (等待资源)
     ↓           ↓
PROC_RUNNABLE  PROC_SLEEPING
                    ↓ (资源就绪/wakeup_proc)
                PROC_RUNNABLE
                    ↓
                PROC_RUNNING
                    ↓ (do_exit)
                PROC_ZOMBIE
                    ↓ (父进程wait回收)
                进程销毁
```

**状态变换事件**：
- `UNINIT→RUNNABLE`：`wakeup_proc()`
- `RUNNABLE→RUNNING`：`schedule()`选中
- `RUNNING→RUNNABLE`：时间片用完/yield()
- `RUNNING→SLEEPING`：`wait()`/等待资源
- `SLEEPING→RUNNABLE`：资源就绪/`wakeup_proc()`
- `RUNNING→ZOMBIE`：`do_exit()`
- `ZOMBIE→销毁`：父进程`wait()`回收