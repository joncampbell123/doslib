
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

using namespace std;

#include <string>
#include <vector>

typedef size_t ModuleRef;
static const size_t ModuleRefUndef = ~((size_t)0ul);

typedef size_t NameRef;
static const size_t NameRefUndef = ~((size_t)0ul);

typedef size_t SegmentRef;
static const size_t SegmentRefUndef = ~((size_t)0ul);

typedef size_t GroupRef;
static const size_t GroupRefUndef = ~((size_t)0ul);

typedef size_t ExternRef;
static const size_t ExternRefUndef = ~((size_t)0ul);

static const uint32_t LineNumberUndef = ~((uint32_t)0ul);
static const uint32_t OffsetUndef = ~((uint32_t)0ul);

struct Module {
    string              file;                   /* name of file, <stdin>, or <internal ref....> */
};

struct Name {
    string              name;                   /* the LNAME */
    ModuleRef           module;                 /* module it came from */
    uint8_t             scope;                  /* scope (global, module local) */

    static const uint8_t undefScope = 0;
    static const uint8_t globalScope = 1;
    static const uint8_t moduleScope = 2;

    Name() : module(ModuleRefUndef), scope(undefScope) { }
};

struct Extern {
    string              name;
    ModuleRef           module;
    uint8_t             scope;

    static const uint8_t undefScope = 0;
    static const uint8_t globalScope = 1;
    static const uint8_t moduleScope = 2;

    Extern() : module(ModuleRefUndef), scope(undefScope) { }
};

struct LineNumber {
    uint32_t            line;
    uint32_t            offset;

    LineNumber() : line(LineNumberUndef), offset(OffsetUndef) { }
};

struct FixupRef {
    uint8_t             type;
    size_t              index;

    static const uint8_t    undefType = 0;
    static const uint8_t    segdefType = 1;
    static const uint8_t    grpdefType = 2;
    static const uint8_t    extdefType = 3;

    FixupRef() : type(undefType), index(~((size_t)0)) { }
};

struct Fixup {
    FixupRef            target;
    FixupRef            frame;
    uint32_t            target_displacement;
    uint32_t            location;               /* where in the segment to apply the fixup */
    uint8_t             relative;
    uint8_t             locationtype;

    static const uint8_t    undefRelative = 0;
    static const uint8_t    segmentRelative = 1;
    static const uint8_t    selfRelative = 2;

    static const uint8_t    undefLocationType = 0;
    static const uint8_t    lowbyteLocationType = 1;        // low byte (8-bit displacement or low byte of 16-bit)
    static const uint8_t    offset16LocationType = 2;       // 16-bit offset
    static const uint8_t    base16LocationType = 3;         // 16-bit base, logical segment base
    static const uint8_t    far1616LocationType = 4;        // 16:16 (base:offset) FAR pointer
    static const uint8_t    highbyteLocationType = 5;       // high byte
    static const uint8_t    offset32LocationType = 6;       // 32-bit offset
    static const uint8_t    far1632LocationType = 7;        // 16:32 (base:offset) FAR pointer

    Fixup() : target_displacement(0), location(~((uint32_t)0)), relative(undefRelative), locationtype(undefLocationType) { }
};

struct Segment {
    NameRef             segname;
    NameRef             classname;
    NameRef             overlayname;
    uint32_t            alignment;              /* must be a power of 2. Not going to support >= 4GB alignment, that's absurd. */
    uint32_t            segment_length;         /* length of the segment */
    uint32_t            segment_data_length;    /* length of the segment with data already in it (i.e. can be 0 for stack or bss while segment_length != 0) */
    uint8_t             use;                    /* == 16 (use16) == 32 (use32) == 64 (use64) == 255 (mixed) */

    static const uint8_t use16 = 16;
    static const uint8_t use32 = 32;
    static const uint8_t use64 = 64;
    static const uint8_t useMixed = 255;
    static const uint8_t useUndef = 0;

    vector<Extern>      externdef;
    vector<LineNumber>  linenumbers;
    vector<Fixup>       fixups;

    Segment() : segname(NameRefUndef), classname(NameRefUndef), overlayname(NameRefUndef), alignment(0), segment_length(0), segment_data_length(0), use(useUndef) { }
};

struct GroupDefRec {
    uint8_t                 type;
    size_t                  ref;

    static const uint8_t TypeUndef = 0;
    static const uint8_t TypeSegDef = 0xFFul;

    GroupDefRec() : type(TypeUndef), ref(NameRefUndef) { }
};

struct Group {
    NameRef                 groupname;
    vector<GroupDefRec>     grouprec;

    Group() : groupname(NameRefUndef) { }
};

int main(int argc,char **argv) {
    return 0;
}

