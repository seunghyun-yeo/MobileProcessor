#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <arpa/inet.h>



typedef struct
{
	bool invalid, taken, btbin,dpin;
	unsigned int pc,nextpc,btbindex,dpindex,inst,xorval;
}ifid;

typedef struct
{
	bool memtoreg,regwrite,invalid;
        int aluresult,wbdest,readdata,inst;
}memwb;

typedef struct
{
	bool memtoreg,regwrite,memwrite,memread,invalid;
        int aluresult, memwritedata,wbdest,inst;
}exmem;

typedef struct
{
	bool memtoreg, regwrite,memwrite,memread,branch,regdst,alusrc,invalid,taken;
        int rsread,rtread,signextimm,wbdest,BTBindex;
        unsigned int opcode,shamt,function,nextpc,inst,rs,rt,dpindex;
}idex;

//function
void fetch();
void decode();
void execute();
void memacc();
void writeback();
void setting();
void loadprogram();
void memupdate();
int distance(int);
//value
unsigned int pc=0,writebackdata,numofbranch=0,numofmisbranch=0;
unsigned int memory[0x100000];
int R[32];
ifid ifidin,ifidout;
idex idexin,idexout;
exmem exmemin,exmemout;
memwb memwbin,memwbout;
bool killprogram;
int btb[0xffffff][2];
int dp[0xffffff][2];
int gbh=0;
bool print;



void main()
{

	int cycle=0;
	setting();
	loadprogram();
	pc=0;
	while(!(ifidin.invalid&&idexin.invalid&&exmemin.invalid&&memwbin.invalid&&killprogram))
	{
		++cycle;
		if(print)printf("\n================%d=====",cycle);
	        if(print)printf("%x===============\n",pc);
		fetch();
		writeback();
		decode();
		execute();
		memacc();
		memupdate();
		if(print)printf("\nnextpc : %x\n",pc);
	}
	printf("All done\nThis program return : %d\n",R[2]);
	if(!print)printf("Cycle %d\n",cycle);
	float accuracy=numofbranch-numofmisbranch;
	if(numofbranch!=0)accuracy=accuracy/numofbranch*100;
	printf("Num of branch %d num of miss %d\n",numofbranch,numofmisbranch);
	printf("Accuracy : %f%\n",accuracy);
}

void setting()
{
        memset(R,0,sizeof(R));
	memset(btb,-1,sizeof(btb));
	memset(dp,-1,sizeof(dp));
        memset(memory,0,sizeof(memory));
        memset(&ifidin,0,sizeof(ifid));
        memset(&ifidout,0,sizeof(ifid));
        memset(&idexin,0,sizeof(idex));
        memset(&idexout,0,sizeof(idex));
        memset(&exmemin,0,sizeof(exmem));
        memset(&exmemout,0,sizeof(exmem));
        memset(&memwbin,0,sizeof(memwb));
        memset(&memwbout,0,sizeof(memwb));
        R[31]=-1;
        R[29]=0x100000;
        killprogram=0;
}

void loadprogram()
{
        FILE *stream;
        unsigned int line;
        size_t read =1;
	int tprint;
	char* path;
	path=(char*)malloc(8*sizeof(char));
	printf("Put the sample file : ");
	char* path1=".bin";
	path = "fib.bin";	
//	scanf("%s",path);
//	strcat(path,path1);
//path="fib.bin";
 	stream = fopen(path,"rb");
//	printf("Select print mode\n1 for all,0 for only result and cycle :");
//	scanf("%d",&tprint);
//	print=(bool)tprint;
	print=0;
        if (stream == NULL)
                exit(EXIT_FAILURE);
        while(read)
        {
                read=fread(&line,4,1,stream);
                line=(unsigned int)ntohl((uint32_t)line);
                memory[pc]=line;
                if(read==0)break;
                pc++;
        }
	fclose(stream);
        if(!(strcmp(path,"fib.bin")))
	{
		memory[4]=0x2402001e;
	}
}

void memupdate()
{
	memcpy(&ifidin,&ifidout,sizeof(ifid));
	memcpy(&idexin,&idexout,sizeof(idex));
	memcpy(&exmemin,&exmemout,sizeof(exmem));
	memcpy(&memwbin,&memwbout,sizeof(exmem));
}

void fetch()
{
	if(pc==-1)
	{
		ifidout.invalid=1;
		if(print)printf("\nfetch\tinvalid");
		return;	
	}
	else ifidout.invalid=0;
	int x=0;
	int y=0;
	bool taken;
	bool btbin=1;
	bool dpin=1;
	int xorval=pc^(gbh&0x1fffff);
	if(print)printf("\nfetch\t");
	ifidout.inst=memory[pc/4];
	for(x=0;btb[x][0]!=pc&&x<100;x++)
	{
		if(btb[x][0]==-1)
		{
			btbin=0;
			break;
		}
	}
	for(y=0;dp[y][0]!=xorval&&y<0xffffff;y++)
	{
		if(dp[y][0]==-1)
		{
			dpin=0;
			break;
		}
	}
	ifidout.nextpc=pc+4;
	ifidout.pc=pc;
	pc=pc+4;
	if(btbin&dpin)
	{
		if(dp[y][1]==2||dp[y][1]==3) taken=1;
		else taken=0;
		if(taken) pc=btb[x][1];
	}
	else taken=0;
	ifidout.btbindex=x;
	ifidout.btbin=btbin;
	ifidout.dpindex=y;
	ifidout.dpin=dpin;
	ifidout.xorval=xorval;
	ifidout.taken=taken;
	if(print)printf("\t%x\t",ifidout.pc);
	if(print)printf("inst:%x",ifidout.inst);
}

void decode()
{
	if(ifidin.invalid)
	{
		if(print)printf("\ndecode\tinvalid");
		idexout.invalid=ifidin.invalid;
		return;
	}
	else idexout.invalid=ifidin.invalid;
	if(print)printf("\ndecode\tinst:%x\t",ifidin.inst);
	unsigned int opcode,rs,rt,rd,shamt,function,address,immediate;
	bool jump,regdst;
	unsigned int isa=ifidin.inst;
	opcode=(0xfc000000&isa)>>26;
        if(opcode==0)
        {
                if(print)printf("R type instruction : ");
                rs=(isa&0x03e00000)>>21;
                rt=(isa&0x001f0000)>>16;
                rd=(isa&0x0000f800)>>11;
                shamt=(isa&0x000007c0)>>6;
                function=(isa&0x0000003f);
		idexout.rsread=R[rs];
		idexout.rtread=R[rt];
                if(print)printf("%x %d %d %d %d %x",opcode,rs,rt,rd,shamt,function);
        }
        else if(opcode==0x2||opcode==0x3)
        {
                if(print)printf("J type instruction  :");
                address=(isa&0x03ffffff);
		if(print)printf("%x %x",opcode,address);
		rs=-1;
		rt=-1;
        }
        else
        {
                if(print)printf("I type instruction : ");
                rs=(isa&0x03e00000)>>21;
		idexout.rsread=R[rs];
                rt=(isa&0x001f0000)>>16;
		idexout.rtread=R[rt];
                immediate=(isa&0x0000ffff);
		if(immediate>=0x00008000) idexout.signextimm=immediate|0xffff0000;
		else idexout.signextimm=immediate;
                if(print)printf("%x %d %d %x",opcode,rs,rt,immediate);
	}

	if((opcode==0)&&(function!=0x9))regdst=1;
        else regdst=0;

        if((opcode!=0)&&(opcode!=0x4)&&(opcode!=0x5))idexout.alusrc=1;
        else idexout.alusrc=0;

        if(opcode==0x23) idexout.memtoreg=1;
        else idexout.memtoreg=0;

        if((opcode!=0x2b)&&(opcode!=0x4)&&(opcode!=0x5)&&(opcode!=0x2)&&(!((opcode==0)&&(function==0x8))))idexout.regwrite=1;
        else idexout.regwrite=0;

        if(opcode==0x23) idexout.memread=1;
        else idexout.memread=0;

        if(opcode==0x2b) idexout.memwrite=1;
        else idexout.memwrite=0;

        if((opcode==0x2)||(opcode==0x3)||((opcode==0)&&(function==0x9)))jump=1;
        else jump=0;

        if((opcode==0x4)||(opcode==0x5)) idexout.branch=1;
        else idexout.branch=0;


	if(jump) 
	{
	//	numofbranch++;
		pc=((address<<2)|(ifidin.nextpc&0xf0000000));
	}
	if(idexout.branch)
	{
		numofbranch++;
		if(!ifidin.btbin)
		{
			btb[ifidin.btbindex][0]=ifidin.pc;
			btb[ifidin.btbindex][1]=ifidin.nextpc+(idexout.signextimm<<2);
		}
		if(!ifidin.dpin)
		{
			dp[ifidin.dpindex][0]=ifidin.xorval;
			dp[ifidin.dpindex][1]=2;
		}
	}
	if(regdst) idexout.wbdest=rd;
	else if(opcode==0x3)idexout.wbdest=31;
	else idexout.wbdest=rt;
	idexout.opcode=opcode;
	idexout.shamt=shamt;
	idexout.function=function;
	idexout.nextpc=ifidin.nextpc;
	idexout.inst=ifidin.inst;
	idexout.rs=rs;
	idexout.rt=rt;
//	idexout.dpin=ifidin.dpin;
	idexout.taken=ifidin.taken;
	idexout.dpindex=ifidin.dpindex;
	if(opcode==0x02)memset(&idexout,0,sizeof(idex));
}

void execute()
{
	if(idexin.invalid)
	{
		if(print)printf("\nexecute\tinvalid");
		exmemout.invalid=idexin.invalid;
		return;
	}
	else exmemout.invalid=idexin.invalid;
	int input1,input2,trsread,trtread;
	bool bcond;
	int forwarda,forwardb;
	forwarda=distance(idexin.rs);
	forwardb=distance(idexin.rt);
	trsread=idexin.rsread;
	trtread=idexin.rtread;
	if(forwarda==0);
	else if(forwarda==1)trsread=exmemin.aluresult;
	else if(forwarda==2)trsread=writebackdata;
	else;
	if(forwardb==0);
	else if(forwardb==1)trtread=exmemin.aluresult;
	else if(forwardb==2)trtread=writebackdata;
	else;
	if(idexin.alusrc) input2=idexin.signextimm;
	else input2=trtread;
	input1=trsread;
	if(print)printf("\nexecute\tinst:%x\t",idexin.inst);
        if(idexin.opcode==0)
        {
                switch (idexin.function)
                {
                        case 0x20 ://add
                                exmemout.aluresult=input1+input2;
                                if(print)printf("add");
                        break;
                        case 0x21 ://addu
                                exmemout.aluresult=input1+input2;
                                if(print)printf("addu");
                        break;
                        case 0x24 ://and
                                exmemout.aluresult=input1&input2;
                                if(print)printf("and");
                        break;
                        case 0x27 ://nor
                                exmemout.aluresult=~(input1|input2);
                                if(print)printf("nor");
                        break;
                        case 0x25 ://or
                                exmemout.aluresult=input1|input2;
                                if(print)printf("or");
                        break;
                        case 0x2a ://slt
                                if(input1<input2)exmemout.aluresult=1;
				else exmemout.aluresult=0;
                                if(print)printf("slt");
                        break;
                        case 0x2b ://sltu
                                exmemout.aluresult=(input1<input2)? 1:0;
                                if(print)printf("sltu");
                        break;
                        case 0 ://sll
                                exmemout.aluresult=input2<<idexin.shamt;
                                if(print)printf("sll");
                        break;
                        case 0x02 ://srl
                                exmemout.aluresult=input2>>idexin.shamt;
                                if(print)printf("srl");
                        break;
                        case 0x22 ://sub
                                exmemout.aluresult=input1-input2;
                                if(print)printf("sub");
                        break;
                        case 0x23 ://subu
                                exmemout.aluresult=input1-input2;
                                if(print)printf("subu");
                        break;
			case 0x8 ://jr
				pc=input1;
				if(print)printf("jr");
			break;
/*                        case 0x9 ://jalr
                                exmemout.aluresult=;
                                if(print)printf("\t\t\t\t\tpc : %x\n",pc);
                                ra=pc + 0x4;
                                if(print)printf("\t\t\t\t\tra : %x\n",ra);
                                if(print)printf("Jalr");
                        break;*/
                        default :
                                if(print)printf("Not defined instruction \n");
                        break;
                }

        }
        else//i type
        {
                switch (idexin.opcode)
                {
                        case 0x8 : //addi
                                exmemout.aluresult=input1+input2;
                                if(print)printf("addi");
                        break;
                        case 0x9 : //addiu
                                exmemout.aluresult=input1+input2;
                                if(print)printf("addiu");
                        break;
                        case 0xc ://andi
                                exmemout.aluresult=input1&input2;
                                if(print)printf("andi");
                        break;
                        case 0x4 ://beq
                                if(input1==input2) bcond=1;
                                else bcond=0;
                                if(print)printf("beq");
                        break;
                        case 0x05 ://bne
                                if(input1!=input2) bcond=1;
                                else bcond=0;
                                if(print)printf("bne");
                        break;
                        case 0x24 ://lbu
                                exmemout.aluresult=input1+input2;
                                if(print)printf("lbu");
                        break;
                        case 0x25 ://lhu
                                exmemout.aluresult=input1+input2;
                                if(print)printf("lhu");
                        break;
                        case 0x30 ://ll
                                exmemout.aluresult=input1+input2;
                                if(print)printf("ll");
                        break;
                        case 0xf ://lui
                                exmemout.aluresult=((input2<<16)&0xffff0000);
                                if(print)printf("lui");
                        break;
                        case 0x23 ://lw
                                exmemout.aluresult=input1+input2;
                                if(print)printf("lw");
                        break;
                        case 0xd ://ori
                                exmemout.aluresult=input1|(input2&0x0000ffff);
                                if(print)printf("ori");
                        break;
                        case 0xa ://slti
                                if(input1<input2) exmemout.aluresult=1;
                                else exmemout.aluresult=0;
                                if(print)printf("slti");
                        break;
                        case 0xb ://sltiu
                                exmemout.aluresult=(input1<input2)? 1:0;
                                if(print)printf("sltiu");
                        break;
                        case 0x28 ://sb
                                exmemout.aluresult=input1+input2;
                                if(print)printf("sb");
                        break;
                        case 0x38 ://sc
                                exmemout.aluresult=input1+input2;
                                if(input2==1) exmemout.aluresult=1;
                                else exmemout.aluresult=0;
                                if(print)printf("sc");
                        break;
                        case 0x29 ://sh
                                exmemout.aluresult=(0x0000ffff&input2);
                                if(print)printf("sh");
                        break;
                        case 0x2b ://sw
                                exmemout.aluresult=input1+input2;
                                if(print)printf("sw");
                        break;
			case 0x03 ://jal
				exmemout.aluresult=idexin.nextpc+4;
			break;
                        default :
                                if(print)printf("Not defined instruction\n");
                        break;
                }
        }
	if(idexin.branch)
	{
		gbh=gbh*2+bcond;
		if(bcond!=idexin.taken)
		{
			numofmisbranch++;
			idexout.invalid=1;
			ifidout.invalid=1;
			if(idexin.taken)pc=idexin.nextpc;
			if(!idexin.taken)pc=idexin.nextpc+(idexin.signextimm<<2);
		}
		if(bcond)
		{
			if(dp[idexin.dpindex][1]==0)dp[idexin.dpindex][1]=1;
			else if(dp[idexin.dpindex][1]==1)dp[idexin.dpindex][1]=3;
			else if(dp[idexin.dpindex][1]==2)dp[idexin.dpindex][1]=3;
			else if(dp[idexin.dpindex][1]==3)dp[idexin.dpindex][1]=3;
		}
		else
		{

			if(dp[idexin.dpindex][1]==0)dp[idexin.dpindex][1]=0;
			else if(dp[idexin.dpindex][1]==1)dp[idexin.dpindex][1]=0;
			else if(dp[idexin.dpindex][1]==2)dp[idexin.dpindex][1]=0;
			else if(dp[idexin.dpindex][1]==3)dp[idexin.dpindex][1]=2;
		}
	}
	exmemout.memtoreg=idexin.memtoreg;
        exmemout.memwrite=idexin.memwrite;
        exmemout.memread=idexin.memread;
	exmemout.wbdest=idexin.wbdest;
	exmemout.memwritedata=trtread;
	exmemout.inst=idexin.inst;
	exmemout.regwrite=idexin.regwrite;
	if(bcond&idexin.branch)memset(&exmemout,0,sizeof(exmem));
	if(print)printf(" %d %d\tresult:%d",input1,input2,exmemout.aluresult);
	if(pc==-1)
	{
		ifidout.invalid=1;
		idexout.invalid=1;
	}
}

void memacc()
{
	if(exmemin.invalid)
	{
		if(print)printf("\nmemacc\tinvalid");
		memwbout.invalid=exmemin.invalid;
		return;
	}
	else memwbout.invalid=exmemin.invalid;
	if(print)printf("\nmemacc\tinst:%x\t",exmemin.inst);
	if(exmemin.memwrite)
	{
		memory[exmemin.aluresult]=exmemin.memwritedata;
		if(print)printf("memory[%x]=%d(writevalue)",exmemin.aluresult,exmemin.memwritedata);
	}
	else if(exmemin.memread)
	{
		memwbout.readdata=memory[exmemin.aluresult];
		if(print)printf("%d(readvalue) from memory[%x]",memwbout.readdata,exmemin.aluresult);
	}
	else;
	memwbout.wbdest=exmemin.wbdest;
	memwbout.memtoreg=exmemin.memtoreg;
	memwbout.aluresult=exmemin.aluresult;
	memwbout.memtoreg=exmemin.memtoreg;
	memwbout.regwrite=exmemin.regwrite;
	memwbout.inst=exmemin.inst;
}

void writeback()
{
	if(memwbin.invalid)
	{
		if(print)printf("\nwrbck\tinvalid");
		killprogram=1;
		return;
	}
	else killprogram=0;
	if(print)printf("\nwrbck\tinst:%x\t",memwbin.inst);
	if(memwbin.regwrite)
	{
		if(memwbin.memtoreg)writebackdata=memwbin.readdata;
		else writebackdata=memwbin.aluresult;
		R[memwbin.wbdest]=writebackdata;
		if(print)printf("R[%d]=%x",memwbin.wbdest,writebackdata);
	}
}

int distance(int a)
{
	if(exmemin.wbdest!=0&&a==exmemin.wbdest&&exmemin.regwrite==1) return 1;
	if(memwbin.wbdest!=0&&a==memwbin.wbdest&&memwbin.regwrite==1) return 2;
	else return 0;
}
