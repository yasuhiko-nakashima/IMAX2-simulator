
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/armfile64.c,v 1.6 2022/03/29 00:06:56 nakashim Exp nakashim $";

/* ARM Instruction Simulator           */
/*        Copyright (C) 2007 by NAIST. */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* armfile.c 2005/3/22 */

#ifndef _SYS_ELF_H
#define _SYS_ELF_H

#ifndef _SYS_ELFTYPES_H
#define _SYS_ELFTYPES_H

typedef unsigned long long  Elf64_Addr;
typedef unsigned short      Elf32_Half;
typedef unsigned long long  Elf64_Off;
typedef int                 Elf32_Sword;
typedef unsigned int        Elf32_Word;
typedef unsigned long long  Elf64_Word;

#endif

#define ELF32_FSZ_ADDR	8
#define ELF32_FSZ_HALF	2
#define ELF32_FSZ_OFF	8
#define ELF32_FSZ_SWORD	4
#define ELF32_FSZ_WORD	4

/* ELF header */

#define EI_NIDENT	16

typedef struct {
	unsigned char	e_ident[EI_NIDENT];	/* ident bytes */
	Elf32_Half	e_type;			/* file type */
	Elf32_Half	e_machine;		/* target machine */
	Elf32_Word	e_version;		/* file version */
	Elf64_Addr	e_entry;		/* start address */
	Elf64_Off	e_phoff;		/* phdr file offset */
	Elf64_Off	e_shoff;		/* shdr file offset */
	Elf32_Word	e_flags;		/* file flags */
	Elf32_Half	e_ehsize;		/* sizeof ehdr */
	Elf32_Half	e_phentsize;		/* sizeof phdr */
	Elf32_Half	e_phnum;		/* number phdrs */
	Elf32_Half	e_shentsize;		/* sizeof shdr */
	Elf32_Half	e_shnum;		/* number shdrs */
	Elf32_Half	e_shstrndx;		/* shdr string index */
} Elf32_Ehdr;

#define EI_MAG0		0		/* e_ident[] indexes */
#define EI_MAG1		1
#define EI_MAG2		2
#define EI_MAG3		3
#define EI_CLASS	4
#define EI_DATA		5
#define EI_VERSION	6
#define EI_PAD		7

#define ELFMAG0		0x7f		/* EI_MAG */
#define ELFMAG1		'E'
#define ELFMAG2		'L'
#define ELFMAG3		'F'
#define ELFMAG		"\177ELF"
#define SELFMAG		4

#define ELFCLASSNONE	0		/* EI_CLASS */
#define ELFCLASS32	1
#define ELFCLASS64	2
#define ELFCLASSNUM	3

#define ELFDATANONE	0		/* EI_DATA */
#define ELFDATA2LSB	1
#define ELFDATA2MSB	2
#define ELFDATANUM	3

#define ET_NONE		0		/* e_type */
#define ET_REL		1
#define ET_EXEC		2
#define ET_DYN		3
#define ET_CORE		4
#define ET_NUM		5

#define	ET_LOPROC	0xff00		/* processor specific range */
#define	ET_HIPROC	0xffff

#define EM_NONE		0		/* e_machine */
#define EM_M32		1		/* AT&T WE 32100 */
#define EM_SPARC	2		/* Sun SPARC */
#define EM_386		3		/* Intel 80386 */
#define EM_68K		4		/* Motorola 68000 */
#define EM_88K		5		/* Motorola 88000 */
#define EM_486		6		/* Intel 80486 */
#define EM_860		7		/* Intel i860 */
#define	EM_MIPS		8
#define	EM_UXPM		9
#define	EM_UXPX		10
#define EM_NUM		11


#define EV_NONE		0		/* e_version, EI_VERSION */
#define EV_CURRENT	1
#define EV_NUM		2

/* Program header */

typedef struct {
	Elf64_Word	p_type;		/* entry type */
	Elf64_Off	p_offset;	/* file offset */
	Elf64_Addr	p_vaddr;	/* virtual address */
	Elf64_Addr	p_paddr;	/* physical address */
	Elf64_Word	p_filesz;	/* file size */
	Elf64_Word	p_memsz;	/* memory size */
	Elf32_Word	p_flags;	/* entry flags */
	Elf32_Word	p_align;	/* memory/file alignment */
} Elf32_Phdr;

#define PT_NULL		0		/* p_type */
#define PT_LOAD		1
#define PT_DYNAMIC	2
#define PT_INTERP	3
#define PT_NOTE		4
#define PT_SHLIB	5
#define PT_PHDR		6
#define PT_NUM		7

#define PT_LOPROC	0x70000000	/* processor specific range */
#define PT_HIPROC	0x7fffffff

#define PF_R		0x4		/* p_flags */
#define PF_W		0x2
#define PF_X		0x1

#define PF_MASKPROC	0xf0000000	/* processor specific values */

/* Section header */

typedef struct {
	Elf32_Word	sh_name;	/* section name */
	Elf32_Word	sh_type;	/* SHT_... */
	Elf64_Word	sh_flags;	/* SHF_... */
	Elf64_Addr	sh_addr;	/* virtual address */
	Elf64_Off	sh_offset;	/* file offset */
	Elf64_Word	sh_size;	/* section size */
	Elf32_Word	sh_link;	/* misc info */
	Elf32_Word	sh_info;	/* misc info */
	Elf64_Word	sh_addralign;	/* memory alignment */
	Elf64_Word	sh_entsize;	/* entry size if table */
} Elf32_Shdr;

#define SHT_NULL	0		/* sh_type */
#define SHT_PROGBITS	1
#define SHT_SYMTAB	2
#define SHT_STRTAB	3
#define SHT_RELA	4
#define SHT_HASH	5
#define SHT_DYNAMIC	6
#define SHT_NOTE	7
#define SHT_NOBITS	8
#define SHT_REL		9
#define SHT_SHLIB	10
#define SHT_DYNSYM	11
#define SHT_NUM		12
#define SHT_LOUSER	0x80000000
#define SHT_HIUSER	0xffffffff

#define	SHT_LOPROC	0x70000000	/* processor specific range */
#define	SHT_HIPROC	0x7fffffff

#define SHF_WRITE	0x1		/* sh_flags */
#define SHF_ALLOC	0x2
#define SHF_EXECINSTR	0x4

#define SHF_MASKPROC	0xf0000000	/* processor specific values */

#define SHN_UNDEF	0		/* special section numbers */
#define SHN_LORESERVE	0xff00
#define SHN_ABS		0xfff1
#define SHN_COMMON	0xfff2
#define SHN_HIRESERVE	0xffff

#define SHN_LOPROC	0xff00		/* processor specific range */
#define SHN_HIPROC	0xff1f

/* Symbol table */

typedef struct {
	Elf32_Word	st_name;
	Elf64_Addr	st_value;
	Elf32_Word	st_size;
	unsigned char	st_info;	/* bind, type: ELF_32_ST_... */
	unsigned char	st_other;
	Elf32_Half	st_shndx;	/* SHN_... */
} Elf32_Sym;

#define STN_UNDEF	0

#define ELF32_ST_BIND(info)		((info) >> 4)
#define ELF32_ST_TYPE(info)		((info) & 0xf)
#define ELF32_ST_INFO(bind,type)	(((bind)<<4)+((type)&0xf))

#define STB_LOCAL	0		/* BIND */
#define STB_GLOBAL	1
#define STB_WEAK	2
#define STB_NUM		3

#define STB_LOPROC	13		/* processor specific range */
#define STB_HIPROC	15

#define STT_NOTYPE	0		/* TYPE */
#define STT_OBJECT	1
#define STT_FUNC	2
#define STT_SECTION	3
#define STT_FILE	4
#define STT_NUM		5

#define STT_LOPROC	13		/* processor specific range */
#define STT_HIPROC	15

/* Relocation */

typedef struct {
	Elf64_Addr	r_offset;
	Elf32_Word	r_info;		/* sym, type: ELF32_R_... */
} Elf32_Rel;

typedef struct {
	Elf64_Addr	r_offset;
	Elf32_Word	r_info;		/* sym, type: ELF32_R_... */
	Elf32_Sword	r_addend;
} Elf32_Rela;

#define ELF32_R_SYM(info)	((info)>>8)
#define ELF32_R_TYPE(info)	((unsigned char)(info))
#define ELF32_R_INFO(sym,type)	(((sym)<<8)+(unsigned char)(type))

#endif

#include "csim.h"

/*  A.OUT STRUCT */

/* READ_ELF() */
read_armelf(file)
     char *file;
{
  struct aout_head {
    unsigned short  a_info;
    unsigned short  a_magic;
    int             a_text;
    int             a_data;
    int             a_bss;
    int             a_syms;
    int             a_entry;
    int             a_trsize;
    int             a_drsize;
  };
  struct aout_head Ahead;

  struct rel {
    int             r_off;
    unsigned int    r_sym: 24,
                    r_extern: 1,
                            : 2,
                    r_type: 5;
    int             r_addend;
  };

  struct sym {
    int             s_name;
    unsigned int    s_type: 8,
                          : 24;
    int             s_value;
  };

  struct rel *Drel;
  struct rel *Trel;
  struct sym *Sym;

  /*  ELF  STRUCT  */

  Elf32_Ehdr      Ehead;

  Elf32_Rel       *E_textrel;
  Elf32_Rel       *E_datarel;
  Elf32_Sym       *E_sym;

  int Phead_index0;
  int Phead_index1;

  Elf32_Phdr      Phead[3];
  Elf32_Shdr      Shead[32];

  /*  BUFFER  */

  char *Text_p, *Data_p, *Textrel_p;
  char *Datarel_p, *Sym_p, *Str_p;
  char *Shstr_p;
  int  len, count;
  int  trsz_num, drsz_num;
  int  textndx   = 0;
  int  rodtndx   = 0;
  int  datandx   = 0;
  int  bssndx    = 0;
  int  symtabndx = 0;
  int  strtabndx = 0;

  int  baseaddr = 0;

  /* FILEOPEN */
  FILE *fp;
  int i, j;
  unsigned char *memp;

  if (!file) {
    printf("Target program not specified\n");
    exit(1);
  }

  fp = fopen(file, "r");
  if (fp == NULL){
    printf("Can't open elf_file %s\n", file);
    exit(1);
  }

  /* READ ELF_HEADER */
  if (fread(&Ehead, 1, sizeof Ehead, fp) !=  (sizeof Ehead)){
    printf("Can't read elf_header\n");
    exit(1);
  }
#if 0
  printf("eh00:type=%08.8x machine=%08.8x version=%08.8x entry=%08.8x phoff=%08.8x shoff=%08.8x flags=%08.8x ehsize=%08.8x phentsize=%08.8x phnum=%08.8x shenteize=%08.8x shnum=%08.8x shstrndx=%08.8x\n",
  (int)Ehead.e_type,
  (int)Ehead.e_machine,
  (int)Ehead.e_version,
  (int)Ehead.e_entry,
  (int)Ehead.e_phoff,
  (int)Ehead.e_shoff,
  (int)Ehead.e_flags,
  (int)Ehead.e_ehsize,
  (int)Ehead.e_phentsize,
  (int)Ehead.e_phnum,
  (int)Ehead.e_shentsize,
  (int)Ehead.e_shnum,
  (int)Ehead.e_shstrndx);
#endif
  fseek(fp, Ehead.e_shoff, 0);
  for (i=0; i<32 && i<Ehead.e_shnum; i++) {
    fread(&Shead[i], 1, Ehead.e_shentsize, fp);
#if 0
    printf("sh%02.2d:name=%08.8x type=%08.8x flags=%08.8x addr=%08.8x offset=%08.8x size=%08.8x link=%08.8x info=%08.8x addralign=%08.8x entsize=%08.8x\n",
           i,
           (int)Shead[i].sh_name,
           (int)Shead[i].sh_type,
           (int)Shead[i].sh_flags,
           (int)Shead[i].sh_addr,
           (int)Shead[i].sh_offset,
           (int)Shead[i].sh_size,
           (int)Shead[i].sh_link,
           (int)Shead[i].sh_info,
           (int)Shead[i].sh_addralign,
           (int)Shead[i].sh_entsize);
#endif
  }

  Shstr_p = (char *)malloc(Shead[Ehead.e_shstrndx].sh_size);
  fseek(fp, Shead[Ehead.e_shstrndx].sh_offset, 0);
  if (fread(Shstr_p, 1, Shead[Ehead.e_shstrndx].sh_size, fp) != Shead[Ehead.e_shstrndx].sh_size) {
    printf("Can't read shstr\n");
    exit(1);
  }

  for (i=0; i<32 && i<Ehead.e_shnum; i++) {
    if      (!strcmp(Shstr_p+Shead[i].sh_name, ".text"))
      textndx = i;
    else if (!strcmp(Shstr_p+Shead[i].sh_name, ".rodata"))
      rodtndx = i;
    else if (!strcmp(Shstr_p+Shead[i].sh_name, ".data"))
      datandx = i;
    else if (!strcmp(Shstr_p+Shead[i].sh_name, ".bss"))
      bssndx = i;
    else if (!strcmp(Shstr_p+Shead[i].sh_name, ".symtab"))
      symtabndx = i;
    else if (!strcmp(Shstr_p+Shead[i].sh_name, ".strtab"))
      strtabndx = i;
  }

  printf(" ELF:text=%08.8x ", (int)Shead[textndx].sh_size);
  printf("rodt=%08.8x ", (int)Shead[rodtndx].sh_size);
  printf("data=%08.8x ", (int)Shead[datandx].sh_size);
  printf("bss=%08.8x ", (int)Shead[bssndx].sh_size);
  printf("symt=%08.8x ", (int)Shead[symtabndx].sh_size);
  printf("strt=%08.8x\n", (int)Shead[strtabndx].sh_size-1);

  /* READ PROGRAM_HEADER */
  fseek(fp, Ehead.e_phoff, 0);
  for (i=0; i<Ehead.e_phnum; i++) {
    if (fread(&Phead[i], 1, sizeof *Phead, fp) !=  (sizeof *Phead)){
      printf("Can't read program_header\n");
      exit(1);
    }
#if 0
    printf("ph%d:type=%08.8x offset=%08.8x vaddr=%08.8x paddr=%08.8x filesz=%08.8x memsz=%08.8x flags=%08.8x align=%08.8x\n",
	   i,
	   (int)Phead[i].p_type,
	   (int)Phead[i].p_offset,
	   (int)Phead[i].p_vaddr,
	   (int)Phead[i].p_paddr,
	   (int)Phead[i].p_filesz,
	   (int)Phead[i].p_memsz,
	   (int)Phead[i].p_flags,
	   (int)Phead[i].p_align);
#endif
  }
  if ((int)Phead[0].p_type == 1) {
    Phead_index0 = 0;
    Phead_index1 = 1;
  }
  else { /* maybe 0 is EXIDX */
    Phead_index0 = 1;
    Phead_index1 = 2;
  }
  if ((int)Shead[datandx].sh_size == 0) {
    /* MALLOC */
    if ((Text_p = (char *)malloc((int)Phead[Phead_index0].p_memsz)) == NULL){
      printf("Can't malloc elf_text=%d\n", (int)Phead[Phead_index0].p_memsz);
      exit(1);
    }
    memset(Text_p, 0, (int)Phead[Phead_index0].p_memsz-1);
  }
  else {
    /* MALLOC */
    if ((Text_p = (char *)malloc((int)Phead[Phead_index1].p_vaddr-(int)Phead[Phead_index0].p_vaddr)) == NULL){
      printf("Can't malloc elf_text=%d\n", (int)Phead[Phead_index1].p_vaddr-(int)Phead[Phead_index0].p_vaddr);
      exit(1);
    }
    memset(Text_p, 0, (int)Phead[Phead_index1].p_vaddr-(int)Phead[Phead_index0].p_vaddr);
    if ((Data_p = (char *)malloc(((int)Phead[Phead_index1].p_memsz-1|7)+1)) == NULL){
      printf("Can't malloc elf_data\n");
      exit(1);
    }
    memset(Data_p, 0, ((int)Phead[Phead_index1].p_memsz-1|7)+1);
  }

  /* READ (text, data) */
  if ((int)Shead[textndx].sh_size) {
    fseek(fp, (int)Phead[Phead_index0].p_offset, 0);
    if (fread(Text_p, 1, (int)Phead[Phead_index0].p_filesz, fp) != (int)Phead[Phead_index0].p_filesz){
      printf("Can't read elf_text\n");
      exit(1);
    }
  }
  if ((int)Shead[datandx].sh_size) {
    fseek(fp, (int)Phead[Phead_index1].p_offset, 0);
    if (fread(Data_p, 1, (int)Phead[Phead_index1].p_filesz, fp) != (int)Phead[Phead_index1].p_filesz){
      printf("Can't read elf_data\n");
      exit(1);
    }
  }

  fclose(fp);

/* CONV_DATA() */

  /* ELF_HEADER --> A.OUT_HEADER */
  if (Shead[datandx].sh_size == 0) {
    Ahead.a_info   = 0x00c8;
    Ahead.a_magic  = 0x0107;
    Ahead.a_text   = (int)Shead[textndx].sh_size ? (int)Phead[Phead_index0].p_memsz:0;
    Ahead.a_data   = 0;
    Ahead.a_bss    = 0;
    Ahead.a_syms   = 0;
    Ahead.a_entry  = (int)Shead[textndx].sh_addr; /*Phead[Phead_index0].p_vaddr;*/
    Ahead.a_trsize = 0;
    Ahead.a_drsize = 0;
  }
  else {
    Ahead.a_info   = 0x00c8;
    Ahead.a_magic  = 0x0107;
    Ahead.a_text   = (int)Phead[Phead_index1].p_vaddr-(int)Phead[Phead_index0].p_vaddr;
    Ahead.a_data   = ((int)Phead[Phead_index1].p_memsz-1|7)+1;
    Ahead.a_bss    = 0;
    Ahead.a_syms   = 0;
    Ahead.a_entry  = (int)Shead[textndx].sh_addr; /*Phead[Phead_index0].p_vaddr;*/
    Ahead.a_trsize = 0;
    Ahead.a_drsize = 0;
  }

  printf(" BSD:text=%08.8x ", Ahead.a_text);
  printf("data=%08.8x ", Ahead.a_data);
  printf("bss=-------- ");
  printf("symt=-------- ");
  printf("strt=%08.8x\n", (int)Shead[strtabndx].sh_size-1);

/* WRITE_AOUT() */

  if (Ahead.a_entry + Ahead.a_text + Ahead.a_data > ALOCLIMIT) {
    printf("Program too large\n");
    exit(1);
  }
  memp = &mem[0][HDRADDR];                 /* initial malloc pointer */
  *(Ull*)memp = ((Ahead.a_entry+Ahead.a_text+Ahead.a_data)+(LINESIZE-1))&~(LINESIZE-1);
  memp = &mem[0][HDRADDR+8];               /* latest malloc pointer */
  *(Ull*)memp = ((Ahead.a_entry+Ahead.a_text+Ahead.a_data)+(LINESIZE-1))&~(LINESIZE-1);
  memp = &mem[0][HDRADDR+16];              /* initial stack pointer */
  *(Ull*)memp = STACKINIT;

  memcpy(&mem[0][Ahead.a_entry], Text_p, Ahead.a_text);
  memcpy(&mem[0][Ahead.a_entry+Ahead.a_text], Data_p, Ahead.a_data);

  free(Text_p);
  free(Data_p);
  free(Shstr_p);

  Ahead.a_entry =  Ehead.e_entry;
  return (Ahead.a_entry);
}
