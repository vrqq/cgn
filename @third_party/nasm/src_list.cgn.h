#pragma once
#include <string>
#include <initializer_list>

std::initializer_list<std::string> ndisasm_sources = { "disasm/ndisasm.c" };

std::initializer_list<std::string> nasmlib_sources = {
  "asm/assemble.c",
  "asm/directbl.c",
  "asm/directiv.c",
  "asm/error.c",
  "asm/eval.c",
  "asm/exprdump.c",
  "asm/exprlib.c",
  "asm/floats.c",
  "asm/labels.c",
  "asm/listing.c",
  "asm/parser.c",
  "asm/pptok.c",
  "asm/pragma.c",
  "asm/preproc-nop.c",
  "asm/preproc.c",
  "asm/quote.c",
  "asm/rdstrnum.c",
  "asm/segalloc.c",
  "asm/srcfile.c",
  "asm/stdscan.c",
  "asm/strfunc.c",
  "asm/tokhash.c",
  "asm/warnings.c",
  "common/common.c",
  "disasm/disasm.c",
  "disasm/sync.c",
  "macros/macros.c",
  "nasmlib/alloc.c",
  "nasmlib/asprintf.c",
  "nasmlib/badenum.c",
  "nasmlib/bsi.c",
  "nasmlib/crc64.c",
  "nasmlib/file.c",
  "nasmlib/filename.c",
  "nasmlib/hashtbl.c",
  "nasmlib/ilog2.c",
  "nasmlib/md5c.c",
  "nasmlib/mmap.c",
  "nasmlib/nctype.c",
  "nasmlib/path.c",
  "nasmlib/perfhash.c",
  "nasmlib/raa.c",
  "nasmlib/rbtree.c",
  "nasmlib/readnum.c",
  "nasmlib/realpath.c",
  "nasmlib/rlimit.c",
  "nasmlib/saa.c",
  "nasmlib/string.c",
  "nasmlib/strlist.c",
  "nasmlib/ver.c",
  "nasmlib/zerobuf.c",
  "output/codeview.c",
  "output/legacy.c",
  "output/nulldbg.c",
  "output/nullout.c",
  "output/outaout.c",
  "output/outas86.c",
  "output/outbin.c",
  "output/outcoff.c",
  "output/outdbg.c",
  "output/outelf.c",
  "output/outform.c",
  "output/outieee.c",
  "output/outlib.c",
  "output/outmacho.c",
  "output/outobj.c",
  "output/outrdf2.c",
  "stdlib/snprintf.c",
  "stdlib/strlcpy.c",
  "stdlib/strnlen.c",
  "stdlib/strrchrnul.c",
  "stdlib/vsnprintf.c",
  "x86/disp8.c",
  "x86/iflag.c",
  "x86/insnsa.c",
  "x86/insnsb.c",
  "x86/insnsd.c",
  "x86/insnsn.c",
  "x86/regdis.c",
  "x86/regflags.c",
  "x86/regs.c",
  "x86/regvals.c",
};

std::initializer_list<std::string> nasm_sources = { "asm/nasm.c" };
