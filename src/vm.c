#include<karat/vm/vm.h>
#include<karat/log.h>
#include<karat/alloc.h>
#include<karat/vm/opcode.h>
#include<karat/vm/cpu.h>
#include<karat/mod.h>

#include<unistd.h>

#define rmode_bin(cpu, opcode, op) \
    (cpu)->regs[opcode.r.A] = (cpu)->regs[opcode.r.B] op (cpu)->regs[opcode.r.C]

#define imode_bin(cpu, opcode, op) \
    (cpu)->regs[opcode.i.A] = (cpu)->regs[opcode.i.B] op opcode.i.Cx

#define bmode_cmp(cpu, opcode, cmp) \
        (cpu)->pc = (cpu)->regs[opcode.i.A] cmp (cpu)->regs[opcode.i.B] \
        ? opcode.i.Cx : (cpu)->pc

#define push(vm, value) \
    ({  \
        *(typeof(value) *)((vm)->memory + (vm)->cpu.sp) = (value); \
        (vm)->cpu.sp += sizeof(value); \
     })

#define pop(vm, type) \
    ({ \
        (vm)->cpu.sp -= sizeof(type); \
        *(type *)((vm)->memory + (vm)->cpu.sp); \
     })

#define match_as(x, y) \
    typeof(y) x = y; \
    switch(x)

#define handle(code, body) \
    case code ## _CODE: { body; } break

#define default_handler(body) \
    default: { body; } break

static const char *default_modules[] = {
    "mod/con",
    "mod/vga",
    NULL
};

static inline void vm_step(struct vm *vm, const struct prog_t *prog)
{
    struct cpu *cpu = &vm->cpu;
    union opcode op = *(union opcode *)prog_loc(prog, cpu->pc);
    cpu->pc += sizeof(union opcode);

    match_as(code, (uint8_t)op.I) {
    /* No-mode instructions */
    handle(HALT,    cpu->pc = prog_size(prog));
    handle(NOP, {});
    handle(RET,     cpu->pc = pop(vm, reg_t));
    handle(PUSHA,
        for(register size_t i = 0; i < GENERAL_REGS; ++i){
            push(vm, cpu->regs[i]);
        }
    );
    handle(POPA,
        for(register size_t i = GENERAL_REGS; i; --i){
            cpu->regs[i - 1] = pop(vm, reg_t);
        }
    );
    /* Register-mode instructions */
    handle(READ,    cpu->regs[op.r.A] = *(reg_t *)&vm->memory[cpu->regs[op.r.B]]);
    handle(STOR,    *(reg_t *)&vm->memory[cpu->regs[op.r.B]] = cpu->regs[op.r.A]);
    handle(INC,     ++cpu->regs[op.r.A]);
    handle(DEC,     --cpu->regs[op.r.A]);
    handle(ADDS,    rmode_bin(cpu, op, +));
    handle(MODR,    rmode_bin(cpu, op, %));
    handle(XORR,    rmode_bin(cpu, op, ^));
    handle(SUBS,    rmode_bin(cpu, op, -));
    handle(MULS,    rmode_bin(cpu, op, *));
    handle(JMPR,    cpu->pc = cpu->regs[op.i.A]);
    handle(PUSH,    push(vm, (reg_t)cpu->regs[op.r.A]));
    handle(PUSHB,   push(vm, (uint8_t)cpu->regs[op.r.A]));
    handle(POP,     cpu->regs[op.r.A] = pop(vm, reg_t));
    handle(PUT, ({
        reg_t which_mod = cpu->regs[op.r.A];
        struct kmod *mod = vm->mods[which_mod];
        mod->on_port_write(mod->mod_data, cpu->regs[op.r.B], cpu->regs[op.r.C]);
    }));
    /* Intermediate-mode instructions */
    handle(ADDIU,   imode_bin(cpu, op, +));
    handle(SUBIS,   imode_bin(cpu, op, -));
    handle(LOADK,   cpu->regs[op.i.A] = op.i.Cx);
    handle(COPY,    cpu->regs[op.r.A] = cpu->regs[op.r.B]);
    handle(PUSHK,   push(vm, (reg_t)op.i.Cx));
    /* Branch-mode instructions */
    handle(BEQ,     bmode_cmp(cpu, op, ==));
    handle(BNE,     bmode_cmp(cpu, op, !=));
    handle(BGT,     bmode_cmp(cpu, op, >));
    handle(BLT,     bmode_cmp(cpu, op, <));
    handle(JMP,     cpu->pc = op.b.Ax);
    handle(CALL, {
        push(vm, cpu->pc);
        cpu->pc = op.b.Ax;
    });
    default_handler(
        if(op.I >= MAX_OPCODES){
            err("mangled opcode out of range: 0x%02X", op.I);
        }else{
            err("unimplemented opcode 0x%02X (%ls)", op.I, op_defs[op.I].mnemonic);
        }
    );
    };
}

void vm_run(struct vm *vm, const struct vm_options opts, const struct prog_t *prog)
{
    memset(vm, 0, sizeof(*vm));
    vm->memory = s_malloc(opts.memory_size); 
    memset(vm->memory, 0, opts.memory_size);
    memset(vm->mods, 0, MAX_MODS * sizeof(struct kmod *));

    for(size_t i = 0; default_modules[i]; ++i){
        vm->mods[i] = s_alloc(struct kmod);
        module_load(vm->mods[i], default_modules[i]);
    }

    vm->cpu.pc = prog_entry(prog);
    while(vm->cpu.pc < prog_size(prog)){
        vm_step(vm, prog);
    }

    for(size_t i = 0; default_modules[i]; ++i){
        module_unload(vm->mods[i]);
        s_free(vm->mods[i]);
    }

    s_free(vm->memory);
}
