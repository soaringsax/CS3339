#include <stdlib.h>
#include <stdio.h>

#define MEMSIZE 1048576
#define CONSTPC -2
// (treat $hi and $lo as a single register since they are always written together).
#define CONSTHILO -3

static int little_endian, icount, *instruction;
static int mem[MEMSIZE / 4];
// maintain counts

// expected values
//int cycles=728752,bubbles=253239,flushes=25995;
int cycles=5,bubbles=0,flushes=0;
int flush=0;//determining if flush is needed

//if any, is the destination of the instructions in the pipeline.
int destReg[6];

// The other array records, for each instruction in the pipeline, in which stage it generates its result
int whenAvail[6];


// move processes one step forward
void increment(){
    cycles++;
    int i;
    // move all things foward in arrays
    for(i=0;i<5; i++) {
        destReg[i+1]=destReg[i];
        whenAvail[i+1]=whenAvail[i];
    }
    
    // default new values to NOP
    destReg[0]=-1;
    whenAvail[0]=-1;
}

// check if bubble is needed, requres function input
void checkBubble(int registerInput){
    int stallsNeeded=0;
    // a bubble is needed when
    // register input requried for a function is not ready yet
    
    /*
     Most instructions need their inputs in the EX stage, except jr, beq, and bne, which need them already in the ID stage, and the second sw input, which is only needed in the MEM1 stage (the base register is still needed in EX)
     The trap 0x01 instruction reads register rs
     
     stalls needed is the differencebetween current location in pipeline and needed location in pipeline
     
     */
     int found = -1;
     int wanted=registerInput;
     int i;
        for(i=5;i>=0;i--){
            if(wanted==destReg[i])
            {
                found=i;
                if(i < whenAvail[i]){
                    stallsNeeded+=(whenAvail[i]-found);
                }
            }
        }

        // while bubble is needed
        while(stallsNeeded>0){
            // blow bubbles
            bubbles++;
            increment();
            stallsNeeded--;
        }
    
}// checkBubble(rs);


// adds when registers will have valid results to the pipeline
// also does stalls when needed
void addToPipeline(int readyAt,int outputReg){
    //  IF ID EX MEM1 MEM2 WB
    /*if flushing, then flush
     
     The jr, j, and jal instructions are always followed by one flush cycle (stall). The beq and bne instructions are followed by one flush cycle only if they are taken.
     */
    int i;
    if (flush){
        flush=0;
        flushes++;
        for (i=0;i<6;i++)
            increment();
    }

    // else add to array
    else{
        /*
         IF ID EX MEM1 MEM2 WB
         */
        
        /*
         Instruction results become available at the end of the respective stage. Most results become available in the EX stage, except mult, which becomes available in MEM1, div, which become available in WB (i.e., it cannot be forwarded), lw, whose result becomes available in MEM2, and jal, whose result becomes available in ID.
         
         For simplicity, let’s assume that trap instructions follow the same timing as add instructions. The trap 0x01 instruction reads register rs and the trap 0x05 instruction writes register rt.
         */
        /* switch is redundant due to when function is called*/
        
        
        destReg[0]=outputReg;
        whenAvail[0]=readyAt;
    }
    if(
    /* add to pipeline
     Instruction results become available at the end of the respective stage. Most results become available in the EX stage, except mult, which becomes available in MEM1, div, which become available in WB (i.e., it cannot be forwarded), lw, whose result becomes available in MEM2, and jal, whose result becomes available in ID.
     
     For simplicity, let’s assume that trap instructions follow the same timing as add instructions. The trap 0x01 instruction reads register rs and the trap 0x05 instruction writes register rt.
     */
    destReg[0]=outputReg;//replaces destreg[0] but doesn't move what was there down to destreg[1]
    whenAvail[0]=readyAt;
    printf("register: %s available at %d",outputReg,readyAt);
    
}// addToPipeline(2,rd);


static int Convert(unsigned int x)
{
    return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}

static int Fetch(int pc)
{
    pc = (pc - 0x00400000) >> 2;
    if ((unsigned)pc >= icount) {
        fprintf(stderr, "instruction fetch out of range\n");
        exit(-1);
    }
    return instruction[pc];
}

static int LoadWord(int addr)
{
    if ((addr & 3) != 0) {
        fprintf(stderr, "unaligned data access\n");
        exit(-1);
    }
    addr -= 0x10000000;
    if ((unsigned)addr >= MEMSIZE) {
        fprintf(stderr, "data access out of rangeLW\n");
        exit(-1);
    }
    return mem[addr / 4];
}

static void StoreWord(int data, int addr)
{
    if ((addr & 3) != 0) {
        fprintf(stderr, "unaligned data access\n");
        exit(-1);
    }
    addr -= 0x10000000;
    if ((unsigned)addr >= MEMSIZE) {
        fprintf(stderr, "data access out of rangeSW\n");
        exit(-1);
    }
    mem[addr / 4] = data;
}

static void Interpret(int start)
{
    int instr, opcode, rs, rt, rd, shamt, funct, uimm, simm, addr;
    int pc, hi, lo;
    int reg[32];
    int cont = 1, count = 0, i;
    long long wide;
    
    lo = hi = 0;
    pc = start;
    for (i = 1; i < 32; i++) reg[i] = 0;
    reg[28] = 0x10008000;  // gp
    reg[29] = 0x10000000 + MEMSIZE;  // sp
    
    
    while (cont) {
        increment();// increment counter
        count++;
        instr = Fetch(pc);
        pc += 4;
        reg[0] = 0;  // $zero
        
        opcode = (unsigned)instr >> 26;
        rs = (instr >> 21) & 0x1f;
        rt = (instr >> 16) & 0x1f;
        rd = (instr >> 11) & 0x1f;
        shamt = (instr >> 6) & 0x1f;
        funct = instr & 0x3f;
        uimm = instr & 0xffff;
        simm = ((signed)uimm << 16) >> 16;
        addr = instr & 0x3ffffff;
        
        
        switch (opcode) {
            case 0x00:
                switch (funct) {
                    case 0x00: /* sll */ reg[rd] = reg[rs] << shamt;
                        checkBubble(rs); //addToPipeline(2,rd);
                        break;//R[rd]=R[rs]≪shamt
                    case 0x03: /* sra */ reg[rd] = reg[rs] >> shamt;
                        checkBubble(rs); //addToPipeline(2,rd);
                        break;// R[rd]=R[rs]≫>shamt
                    case 0x08: /* jr */ pc = reg[rs]; flush=1;
                        checkBubble(rs); //addToPipeline(2,CONSTPC);
                        break;// PC=R[rs]
                    case 0x10: /* mfhi */ reg[rd] = hi;
                        checkBubble(CONSTHILO); //addToPipeline(2,rd);
                        break;// R[rd]=Hi
                    case 0x12: /* mflo */ reg[rd] = lo;
                        checkBubble(CONSTHILO); //addToPipeline(2,rd);
                        break;// R[rd]=Lo
                    case 0x18: /* mult */ wide = reg[rs]; wide *= reg[rt]; lo = wide & 0xffffffff; hi = wide >> 32;
                        checkBubble(rs); checkBubble(rt); //addToPipeline(3,CONSTHILO); 
                        break;
                    case 0x1a: /* div */ if (reg[rt] == 0) {fprintf(stderr, "division by zero: pc = 0x%x\n", pc - 4); cont = 0;} else {lo = reg[rs] / reg[rt]; hi = reg[rs] % reg[rt];}
                        checkBubble(rs); checkBubble(rt); //addToPipeline(5,CONSTHILO); 
                        break;
                    case 0x21: /* addu */ reg[rd] = reg[rs] + reg[rt];
                        checkBubble(rs); checkBubble(rt); //addToPipeline(2,rd); 
                        break;// R[rd]=R[rs]+R[rt]
                    case 0x23: /* subu */ reg[rd] = reg[rs] - reg[rt];
                        checkBubble(rs); checkBubble(rt); //addToPipeline(2,rd); 
                        break;// R[rd]=R[rs]-R[rt]
                    case 0x2a: /* slt */
                        if(reg[rs]<reg[rt])
                            reg[rd]=1;
                        else reg[rd]=0;
                        checkBubble(rs); checkBubble(rt); //addToPipeline(2,rd); 
                        break;// R[rd]=(R[rs]<R[rt])?1:0
                    default: fprintf(stderr, "unimplemented instruction: pc = 0x%x\n", pc - 4); cont = 0;
                }
                break;
            case 0x02: /* j */ pc = (pc & 0xf0000000) + addr*4;
                flush=1; //addToPipeline(2,CONSTPC) ;
                break;// PC=JumpAddr
            case 0x03: /* jal */ reg[31] = pc; pc = (pc & 0xf0000000) + addr * 4;
                flush=1; //addToPipeline(1,31); 
                break;// R[31]=PC+4; PC=JumpAddr
            case 0x04: /* beq */
                if(reg[rs]==reg[rt]){
                    pc=pc+(simm<<2);
                    flush=1;
                }
                //addToPipeline(2,CONSTPC);
                break;// if(R[rs]==R[rt]) PC=PC+4+BranchAddr
            case 0x05: /* bne */
                if(reg[rs]!=reg[rt]){
                    pc=pc+(simm<<2);
                    flush=1;
                }
                checkBubble(rs); checkBubble(rt); //addToPipeline(2,CONSTPC); 
                break;// TODO if(R[rs]!=R[rt]) PC=PC+4+BranchAddr
            case 0x09: /* addiu */ reg[rt] = reg[rs] + simm;
               checkBubble(rs);  //addToPipeline(2,rt); 
               break;// R[rt]=R[rs]+UnsignExtImm
            case 0x0c: /* andi */ reg[rt] = reg[rs] & uimm;
                checkBubble(rs); //addToPipeline(2,rt);
                break;// R[rt]=R[rs]&ZeroExtImm
            case 0x0f: /* lui */ reg[rt]= simm <<16;
                //addToPipeline(2,rt);
                break; // R[rt]={imm,16’b0}
            case 0x1a: /* trap */
                switch (addr & 0xf) {
                    case 0x00: printf("\n"); break;
                        //The trap 0x01 instruction reads register rs
                    case 0x01: printf(" %d ", reg[rs]);
                        checkBubble(rs); break;
                    case 0x05: printf("\n? "); fflush(stdout); scanf("%d", &reg[rt]); break;
                    case 0x0a: cont = 0; break;
                    default: fprintf(stderr, "unimplemented trap: pc = 0x%x\n", pc - 4); cont = 0;
                }
                break;
            case 0x23: /* lw */ reg[rt] = LoadWord(reg[rs]+simm);
                checkBubble(reg[rs]+simm); //addToPipeline(4,rt); 
                break;  // call LoadWord function R[rt]=M[R[rs]+SignExtImm]
            case 0x2b: /* sw */ StoreWord(reg[rt], reg[rs]+simm);
                checkBubble(rt); //addToPipeline(2,reg[rs]+simm); 
                break;  // call StoreWord function M[R[rs]+SignExtImm]=R[rt]
            default: fprintf(stderr, "unimplemented instruction: pc = 0x%x\n", pc - 4); cont = 0;
        }
    }
    
    printf("\nprogram finished at pc = 0x%x (%d instructions executed)\n", pc, count);
    printf("cycles = %d\n",cycles);
    printf("bubbles = %d\n",bubbles);
    printf("flushes = %d\n",flushes);
    
    if(count == (cycles - 5 - bubbles - flushes)){
        printf("Check passed\n");
    }
    else printf("Check failed, %d ≠ %d - 5 - %d - %d\n",count, cycles, bubbles, flushes);
}

int main(int argc, char *argv[])
{
    int c, start;
    FILE *f;
    
    printf("CS 3339 - MIPS Interpreter\n");
    if (argc != 2) {fprintf(stderr, "usage: %s executable\n", argv[0]); exit(-1);}
    if (sizeof(int) != 4) {fprintf(stderr, "error: need 4-byte integers\n"); exit(-1);}
    if (sizeof(long long) != 8) {fprintf(stderr, "error: need 8-byte long longs\n"); exit(-1);}
    
    c = 1;
    little_endian = *((char *)&c);
    f = fopen(argv[1], "rb");
    if (f == NULL) {fprintf(stderr, "error: could not open file %s\n", argv[1]); exit(-1);}
    c = fread(&icount, 4, 1, f);
    if (c != 1) {fprintf(stderr, "error: could not read count from file %s\n", argv[1]); exit(-1);}
    if (little_endian) {
        icount = Convert(icount);
    }
    c = fread(&start, 4, 1, f);
    if (c != 1) {fprintf(stderr, "error: could not read start from file %s\n", argv[1]); exit(-1);}
    if (little_endian) {
        start = Convert(start);
    }
    
    instruction = (int *)(malloc(icount * 4));
    if (instruction == NULL) {fprintf(stderr, "error: out of memory\n"); exit(-1);}
    c = fread(instruction, 4, icount, f);
    if (c != icount) {fprintf(stderr, "error: could not read (all) instructions from file %s\n", argv[1]); exit(-1);}
    fclose(f);
    if (little_endian) {
        for (c = 0; c < icount; c++) {
            instruction[c] = Convert(instruction[c]);
        }
    }
    
    printf("running %s\n\n", argv[1]);
    Interpret(start);
}
