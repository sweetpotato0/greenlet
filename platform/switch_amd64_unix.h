/*
 * this is the internal transfer function.
 *
 * HISTORY
 * 3-May-13   Ralf Schmitt  <ralf@systemexit.de>
 *     Add support for strange GCC caller-save decisions
 *     (ported from switch_aarch64_gcc.h)
 * 18-Aug-11  Alexey Borzenkov  <snaury@gmail.com>
 *      Correctly save rbp, csr and cw
 * 01-Apr-04  Hye-Shik Chang    <perky@FreeBSD.org>
 *      Ported from i386 to amd64.
 * 24-Nov-02  Christian Tismer  <tismer@tismer.com>
 *      needed to add another magic constant to insure
 *      that f in slp_eval_frame(PyFrameObject *f)
 *      STACK_REFPLUS will probably be 1 in most cases.
 *      gets included into the saved stack area.
 * 17-Sep-02  Christian Tismer  <tismer@tismer.com>
 *      after virtualizing stack save/restore, the
 *      stack size shrunk a bit. Needed to introduce
 *      an adjustment STACK_MAGIC per platform.
 * 15-Sep-02  Gerd Woetzel       <gerd.woetzel@GMD.DE>
 *      slightly changed framework for spark
 * 31-Avr-02  Armin Rigo         <arigo@ulb.ac.be>
 *      Added ebx, esi and edi register-saves.
 * 01-Mar-02  Samual M. Rushing  <rushing@ironport.com>
 *      Ported from i386.
 */

#define STACK_REFPLUS 1

#ifdef SLP_EVAL

/* #define STACK_MAGIC 3 */
/* the above works fine with gcc 2.96, but 2.95.3 wants this */
#define STACK_MAGIC 0

#define REGS_TO_SAVE "r12", "r13", "r14", "r15"

static int
slp_switch(void)
{
    int err;
    void* rbp;
    void* rbx;
    unsigned int csr;
    unsigned short cw;
    register long *stackref, stsizediff;
    __asm__ volatile ("" : : : REGS_TO_SAVE);
    __asm__ volatile ("fstcw %0" : "=m" (cw));  // 浮点检查保存控制器
    __asm__ volatile ("stmxcsr %0" : "=m" (csr));
    //这里先获取以及保存rbp,rbx,rsp三个寄存器的数据
    __asm__ volatile ("movq %%rbp, %0" : "=m" (rbp));
    __asm__ volatile ("movq %%rbx, %0" : "=m" (rbx));
    __asm__ ("movq %%rsp, %0" : "=g" (stackref));  // //将栈顶的地址（rsp）保存到 stackref
    {
        // 这里会将栈之间的偏移保存到 stsizediff 变量
        // 这里是宏定义，如果要切换到的 greenlet 并没有运行，那么这里的宏定义里面就会提前返回1，那么 g_initialstub 里面就可以开始 run 的执行了
        // 这里可以看到其实栈的保存是最终地址保存到 slp_switch 这一层的
        SLP_SAVE_STATE(stackref, stsizediff);  // 把栈的信息保存到堆空间里面去

        // 修改 rbp 以及 rsp 寄存器的值，用于将当前栈切换到要执行的 greenlet 的执行栈
        // 将ebp和esp指向目标栈，从这里开始从目标栈开始运行
        // 将ebp和esp都设置为stsizediff
        __asm__ volatile (
            "addq %0, %%rsp\n"
            "addq %0, %%rbp\n"
            :
            : "r" (stsizediff)
            );

        // 恢复栈（target）中数据
        // 切换到新的greenlet之后，需要恢复之前保存的栈上的数据
        SLP_RESTORE_STATE();
        __asm__ volatile ("xorq %%rax, %%rax" : "=a" (err));
    }

    // 恢复的是之前保存在target栈中的变量
    // 前后 rbp、rbx 的值已经改变了，因为栈的数据已经变化了
    // 因为这里已经恢复了切换到的 greenlet 的栈信息，所以这里恢复的其实是切换到的 greenlet 的栈寄存器的地址
    __asm__ volatile ("movq %0, %%rbx" : : "m" (rbx));
    __asm__ volatile ("movq %0, %%rbp" : : "m" (rbp));
    __asm__ volatile ("ldmxcsr %0" : : "m" (csr));
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("" : : : REGS_TO_SAVE);
    return err;
}

#endif

/*
 * further self-processing support
 */

/*
 * if you want to add self-inspection tools, place them
 * here. See the x86_msvc for the necessary defines.
 * These features are highly experimental und not
 * essential yet.
 */
