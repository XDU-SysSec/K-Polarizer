#ifndef EGALITO_PASS_DUMPLINK_H
#define EGALITO_PASS_DUMPLINK_H

#include "chunkpass.h"

class DumpLinkPass : public ChunkPass {
private:
    Module *module;
    address_t mapbase;
public:
    virtual void visit(Module *module);
private:
    virtual void visit(Instruction *instruction);
    virtual void visit(DataSection *section);
    void dump(Reloc *reloc, Module *module);
    void output(address_t source, Link *link);
    void outputPair(address_t addr1, address_t addr2);

    // author added
    void output_ptr(address_t source, address_t base, size_t disp_size, address_t disp_value, Link *link, bool relative, std::string sec);
};

#endif
