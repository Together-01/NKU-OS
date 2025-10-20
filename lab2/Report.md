# OS lab2

## 练习1：理解first-fit 连续物理内存分配算法（思考题）

first-fit 连续物理内存分配算法作为物理内存分配一个很基础的方法，需要同学们理解它的实现过程。请大家仔细阅读实验手册的教程并结合 `kern/mm/default_pmm.c` 中的相关代码，认真分析 `default_init`，`default_init_memmap`，`default_alloc_pages`，`default_free_pages` 等相关函数，并描述程序在进行物理内存分配的过程以及各个函数的作用。 请在实验报告中简要说明你的设计实现过程。请回答如下问题：

- 你的 first fit 算法是否有进一步的改进空间？

### 各个函数的作用：

1. **default_init**：初始化空闲链表和计数器
2. **default_init_memmap**：初始化内存块，设置头页，按地址排序插入链表
3. **default_alloc_pages**：first fit 搜索，分割块，返回分配页指针
4. **default_free_pages**：释放页面，按序插入链表，合并前后相邻块

### 程序进行物理分配的过程：

**初始化阶段**：首先建立空的空闲链表，空闲页计数器清零。将物理页初始化为空闲块并按地址顺序插入链表。

**分配请求**：从表头开始遍历找到第一个大小不小于 n 的空闲块，将其从链表移除。分割出大小为 n 的块，若块的大小大于 n，分割后剩余部分重新插入链表。最后更新空闲页计数器，返回分配块首地址。

**释放回收阶段**：初始化待释放页的状态，按地址顺序插入空闲链表。检查是否能够向前或向后合并。

### 该 first fit 算法的改进空间：

1. 当前的 first fit 搜索算法是线性的，可以采用平衡树
2. 长期分配将导致链表前端产生小碎片，可采取一定的分割策略如设置最小分割阈值
3. 每次搜索均从头开始，可记录上次分配位置

---

## 练习2：实现 Best-Fit 连续物理内存分配算法（需要编程）

在完成练习一后，参考 `kern/mm/default_pmm.c` 对 First Fit 算法的实现，编程实现 Best Fit 页面分配算法，算法的时空复杂度不做要求，能通过测试即可。 请在实验报告中简要说明你的设计实现过程，阐述代码是如何对物理内存进行分配和释放，并回答如下问题：

- 你的 Best-Fit 算法是否有进一步的改进空间？

参考 first fit 算法，该算法仅仅为在链表中寻找第一个大小大于 n 的块用于分配，一旦找到就直接使用。我们实现的 best fit 则是通过遍历链表来找到最小的那个大于 n 的块。

代码中除搜索逻辑与 first fit 不同外，其他四处需要补全代码位置的逻辑均一样。

具体为：

```c
while ((le = list_next(le)) != &free_list) {
    struct Page *p = le2page(le, page_link);
    if (p->property >= n && p->property < min_size) {
        page = p;
        min_size = p->property;
        if(min_size == n) {
            break; // 如果找到最优解，直接退出循环
        }
    }
}
```
通过测试
``` 
mbdim@Mbdims:~/inWSL/OS/NKU-OS/lab2$ make grade
>>>>>>>>>>> here_make>>>>>>>>>>>
gmake[1]: Entering directory '/home/mbdim/inWSL/OS/NKU-OS/lab2'
+ cc kern/init/entry.S
+ cc kern/init/init.c
+ cc kern/libs/stdio.c
+ cc kern/debug/panic.c
+ cc kern/driver/console.c
+ cc kern/driver/dtb.c
+ cc kern/mm/best_fit_pmm.c
+ cc kern/mm/default_pmm.c
+ cc kern/mm/pmm.c
+ cc libs/printfmt.c
+ cc libs/readline.c
+ cc libs/sbi.c
+ cc libs/string.c
+ ld bin/kernel
riscv64-unknown-elf-objcopy bin/kernel --strip-all -O binary bin/ucore.img
gmake[1]: Leaving directory '/home/mbdim/inWSL/OS/NKU-OS/lab2'
>>>>>>>>>>> here_make>>>>>>>>>>>
<<<<<<<<<<<<<<<<< here_run_qemu <<<<<<<<<<<<<<<<<<<
try to run qemu
qemu pid=22602
<<<<<<<<<<<<<<<<< here_run_check <<<<<<<<<<<<<<<<<<<
-check physical_memory_map_information: OK
-check_best_fit: OK
Total Score: 25/25
```
#### 扩展练习Challenge：buddy system（伙伴系统）分配算法（需要编程）

Buddy System算法把系统中的可用存储空间划分为存储块(Block)来进行管理, 每个存储块的大小必须是2的n次幂(Pow(2, n)), 即1, 2, 4, 8, 16, 32, 64, 128...

 -  参考[伙伴分配器的一个极简实现](http://coolshell.cn/articles/10427.html)， 在ucore中实现buddy system分配算法，要求有比较充分的测试用例说明实现的正确性，需要有设计文档。
 
伙伴系统分配器的整体思想是，通过一个数组形式的完全二叉树来监控管理内存，二叉树的节点用于标记相应内存块的使用状态，高层节点对应大的块，低层节点对应小的块，在分配和释放中我们就通过这些节点的标记属性来进行块的分离合并。

数据结构如下：
```c
typedef struct {
    size_t size;           // 管理的总页数   
    size_t *longest;       // 状态数组
    struct Page *base;     // 页基址
    unsigned int nr_free;  // 当前空闲页数
} buddy_tree_t;
```

**系统初始化**

简单起见，本实验中longest使用静态数组，方便初始化，需要设置合理的数组大小，同时还要通过向下取2的幂次保证物理页数能被伙伴系统管理。

```c
#define MAX_BUDDY_PAGES 16384
static size_t buddy_longest_static[2 * MAX_BUDDY_PAGES - 1];

size_t real_size = floor_power_of_2(n);
```

对longest数组初始化操作为：根节点为管理的总页数，之后子节点都是父节点页数的一半。
```c
size_t total_nodes = 2 * real_size - 1;
size_t node_size = real_size;
size_t next_boundary = 1;

for (size_t i = 0; i < total_nodes; i++) {
    buddy_tree.longest[i] = node_size;
    if (i + 1 == next_boundary) {
        node_size >>= 1;
        next_boundary = next_boundary * 2 + 1;
    }
}
```

**分配物理页**

首先需要对请求分配的页数向上取2的幂，对要分配的页数进行初步判断后再通过如下方式遍历树搜索合适的节点：

```c
size_t node_size;
size_t idx = 0;
for(node_size = buddy_tree.size; node_size != npages; node_size /= 2 ) {
    if (buddy_tree.longest[LEFT_CHILD(idx)] >= npages)
        idx = LEFT_CHILD(idx);
    else
        idx = RIGHT_CHILD(idx);
}
```

最后需要从搜索到的节点开始，自底向上更新父节点的状态：

```c
buddy_tree.longest[idx] = 0;
while (idx) {
    idx = PARENT(idx);
    buddy_tree.longest[idx] =MAX(buddy_tree.longest[LEFT_CHILD(idx)], buddy_tree.longest[RIGHT_CHILD(idx)]);
}
```
由于伙伴系统可分配页划分的限制，父节点时需要更新为子节点中的最大值。

**释放物理页**

首先要计算到待释放的实际页数，也就是向上取2的幂，再通过页偏移计算出对应的longest数组元素的下标。

```c
size_t npages = next_power_of_2(n);
size_t node_size = npages;
size_t offset = base - buddy_tree.base;
size_t idx = (offset + buddy_tree.size) / node_size - 1;
```

最后更新longest数组。

```c
buddy_tree.longest[idx] = node_size;
while (idx) {
    idx = PARENT(idx);
    node_size *= 2;

    size_t left_longest = buddy_tree.longest[LEFT_CHILD(idx)];
    size_t right_longest = buddy_tree.longest[RIGHT_CHILD(idx)];
    if (left_longest + right_longest == node_size)
        buddy_tree.longest[idx] = node_size;
    else
        buddy_tree.longest[idx] = MAX(left_longest, right_longest);
}
```
与分配页有所不同的是，当子节点都未被分配时，父节点需要被更新为子节点之和。

#### 扩展练习Challenge：任意大小的内存单元slub分配算法（需要编程）

slub算法，实现两层架构的高效内存单元分配，第一层是基于页大小的内存分配，第二层是在第一层基础上实现基于任意大小的内存分配。可简化实现，能够体现其主体思想即可。

 - 参考[linux的slub分配算法/](http://www.ibm.com/developerworks/cn/linux/l-cn-slub/)，在ucore中实现slub分配算法。要求有比较充分的测试用例说明实现的正确性，需要有设计文档。

#### 扩展练习Challenge：硬件的可用物理内存范围的获取方法（思考题）
  - 如果 OS 无法提前知道当前硬件的可用物理内存范围，请问你有何办法让 OS 获取可用物理内存范围？

#### 在RISC-V架构中OS获取可用物理内存范围的方法：
系统启动时，OpenSBI将设备树（DTB）加载到物理内存中，并通过a1寄存器将其物理地址传递给内核。
```assembly
#entry.S
    la t0, boot_dtb
    sd a1, 0(t0)
```
内核入口首先保存DTB地址到全局变量boot_dtb，随后建立虚拟内存映射并跳转到内核初始化。在物理内存初始化阶段，内核调用get_memory_base()和get_memory_size()函数，这些函数通过DTB解析模块从保存的DTB地址中提取内存信息。
```c
//dtb.c
if (in_memory_node && strcmp(prop_name, "reg") == 0 && prop_len >= 16) {
    *mem_base = fdt64_to_cpu(reg_data[0]);
    *mem_size = fdt64_to_cpu(reg_data[1]);
}
```
遍历设备树结构，定位"memory"节点，读取其中的"reg"属性值，该属性明确记录了物理内存的起始地址和大小。
```c
//pmm.c
uint64_t mem_begin = get_memory_base();
uint64_t mem_size  = get_memory_size();
uint64_t mem_end   = mem_begin + mem_size;
```
完成对可用物理内存范围的识别与管理。

#### 本实验中重要的知识点及与OS原理的对应关系
1.	实验知识点：First-Fit、Best-Fit算法的具体实现
OS知识点：动态分区分配策略和分区分配算法
实验中的first fit和Best-Fit是动态分区分配策略的具体实现；知识点主要关注算法思想，实验需要处理链表操作、边界合并等工程细节
2.	free_list双向链表管理，page结构体的property字段
OS知识点：使用链表的存储管理
实验中的free_list对应原理的空闲分区表概念


#### OS原理中重要但实验未对应的知识点
1.	请求分页、页面置换算法
2.	缺页中断处理
3.	页表机制、TLB管理
4.	多级页表结构
