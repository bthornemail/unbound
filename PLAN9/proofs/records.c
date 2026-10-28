/* the records, in the order of proofs/js-docs/JS-PROOF-INDEX.md */
#include "proof.h"

void i108(void), i109(void), i110(void), i111(void), i112(void);
void i113(void), i114(void), i115(void), i116(void), i117(void), i118(void), i119(void), i120(void);
void i121(void), i123(void), i124(void), i125(void), i126(void), i127(void), i127b(void);

Record records[] = {
	{ "I-108", "head = modulus, tail = prime", i108 },
	{ "I-109", "two chains, shared zero (derived)", i109 },
	{ "I-110", "cover = uncovering; NaN/RangeError/TypeError split", i110 },
	{ "I-111", "faces are the reverse view; no seam", i111 },
	{ "I-112", "stellated tetrahedron; swap = presentation", i112 },
	{ "I-113", "V/E/F partition; stellation at 0x3", i113 },
	{ "I-114", "prefix = index length, not value", i114 },
	{ "I-115", "0n vertex, 0u reference; swap permutes order", i115 },
	{ "I-116", "index 60 pinned; 0,2,1 interchange", i116 },
	{ "I-117", "PINE box", i117 },
	{ "I-118", "decimal = derivative, scientific = exception", i118 },
	{ "I-119", "globalThis node; base-36 overlay", i119 },
	{ "I-120", "3! overlay (streams, GED)", i120 },
	{ "I-121", "nibble-pair root; (index, value)", i121 },
	{ "I-123", "S3 at four scales; XOR/Fano/byte", i123 },
	{ "I-124", "4 circuits / 7 positions / 12th meta", i124 },
	{ "I-125", "compare-exchange chain + 72/60/48 descent", i125 },
	{ "I-126", "Node Buffer: byte indexing, endianness, native swaps", i126 },
	{ "I-127", "error taxonomy (space/range/frame) + DOM", i127 },
	{ "I-127b", "octet of Atomics spaces: 4 widths x 2 signedness = 8", i127b },
	{ nil },
};
