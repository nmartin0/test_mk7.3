# Public sources

Every public document this project has read for its design, with
where it came from and whether a copy is carried here. Documents are
read for what they describe; code is never taken from them
(`docs/provenance/precedent.md`).

A document is **carried** in `docs/reference/` only when it grants
copying: OSF's and The Open Group's "Permission to reproduce this
document without fee is hereby granted, provided that the copyright
notice and this permission notice appear in all copies", or a
software licence that covers it. Every other document is **cited**:
its address, its size and the SHA-256 of the copy fetched on 23
October 2026 are recorded below, so that anyone can fetch the same
bytes and check them. Copyright subsists without a notice, so a
document published for anonymous FTP but carrying no grant is cited,
not carried -- the rule `README.md` already applies.

Every public document met in this work is kept, by the maintainer's
rule of 23 October 2026: carried here if it grants copying, and
otherwise listed below and kept outside git, in the maintainer's
archive `mk73-uncarried-docs` (its `SHA256SUMS` matches the hashes
here).

## Carried here

| file | document | grant |
|---|---|---|
| `osf-ri/os/osfmk/mk7.3/RelNotes7.Book.ps` | *MK7.3 Release Notes* (The Open Group RI, 1997) | OSF and The Open Group, front matter |
| `osf-ri/os/osfmk/mk6.1/relnotes.ps` | *OSF-RI MK6.1 Release Notes* (Marc J. Teller, 1994) | OSF, front matter |
| `osf-ri/os/spec/spec93/kernel_interface.ps.Z` | *OSF Mach Kernel Interfaces*, Final Draft (Keith Loepere, ed., OSF and CMU, 3 May 1993) | OSF, front matter |
| `osf-ri/os/spec/spec93/kernel_principles.ps.Z` | *OSF Mach Kernel Principles*, Final Draft (Keith Loepere, 3 May 1993) | OSF, front matter |
| `osf-ri/os/spec/spec93/server_library.ps.Z` | *OSF Mach Server Library Interfaces*, Final Draft (Keith Loepere, ed., 3 May 1993) | OSF, front matter |
| `osf-ri/os/spec/spec93/server_writer.ps.Z` | *OSF Mach Server Writer's Guide*, Final Draft (3 May 1993) | OSF, front matter |
| `osf-ri/os/spec/spec94/SPEC94_DRAFT_1.ps` | *OSF Mach* Spec '94, Draft 1 (OSF RI, 1994) | OSF, front matter |
| `xnu-792/osfmk/man/` (263 files) | OSF's Mach kernel manual pages, as HTML, in Apple's XNU at tag `xnu-792` (`github.com/apple-oss-distributions/xnu`, commit `14e3d835ecf9736db9bf15b7bc859981275e1fc0`) | `xnu-792/APPLE_LICENSE`, the Apple Public Source License as that tag carries it; the pages are the *Kernel Interfaces* above, whose own grant OSF gave |

The first two were carried before; `README.md` has their SHA-1s, and
now the rest's.

## Cited, not carried

The design book:

| document | bytes | SHA-256 |
|---|---|---|
| *Design of the OSF/1 Operating System, Release 1.2* (Open Software Foundation; P T R Prentice Hall, 1993, ISBN 0-13-202813-6), the maintainer's PDF | 25,637,674 | `d6609f27517dc1b3ce90943308971752b51ecfbe88ad2ee627de3d18cb5531e5` |

"Copyright ©1993 Open Software Foundation, Inc. ... All rights
reserved." It is a published book, cited by section and quoted only
briefly; it is not one of OSF's freely reproducible documents.

From OSFMK 6.1's own documentation (`github.com/traviolia/osfmk6.1`,
commit `15d1a5531c807bbc1765bbc3e07f7cce80290553`, `doc/`), which
this project already uses as its second precedent:

| document | bytes | SHA-256 |
|---|---|---|
| Michael Condict, Don Bolinger, Dave Mitchell, Eamonn McManus, et al., *Microkernel Modularity with Integrated Kernel Performance* (OSF RI, 24 June 1994), `doc/mk_papers/collocation_paper.ps` | 120,744 | `8be3c8a78be7aacc2e614dbcca406963ead77de75bc75bea6337404c9ffc6b5b` |

### OSF Research Institute papers, without a permission notice

From the Open Group Research Institute's FTP area as mirrored at
`ftp.gwdg.de/pub/misc/opengroup/ri/`. Published by OSF for anonymous
FTP, but no copy carries a grant to reproduce it, so they are cited, not
carried.

| document | bytes | SHA-256 |
|---|---|---|
| [Youcef Laribi, *Performance analysis of Amadeus on top of OSF/1 MK* (18 January 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/Comandos_Perf.ps) | 250,689 | `cacd14b8efadd087ed7381a2a02e9dd4e28e90c0939dec1a81a04f487f3f71fe` |
| [Paul J. Roy, *Unix File Access and Caching in a Multicomputer Environment* (OSF RI; USENIX Mach III, 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/FileAccess.machnix.ps) | 120,075 | `f27e878ff6b22d4d9030865cc8c2066a439ae1e9764727757f24206a24a97d17` |
| [OSF RI Advanced Development, *MIG guide*](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/MIG_GUIDE.ps) | 86,278 | `727c09d962585a47a64abf5f7588a14dba6e4feb432cad11225ceb7f3e1642f1` |
| [OSF RI Advanced Development, *MIG protocol*](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/MIG_PROTOCOL.ps) | 65,795 | `a34bd623ed346c66a2a77f0e03130304bb64ce7fea54e88c337432c8b888e978` |
| [Eric Paire, *Extending Mach NORMA IPC and XMM to SMP Nodes* (OSF RI)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/MP_NORMA.ps) | 132,200 | `8bbfe97bc479975e06460d6bcbe72c7c53f976b6e399ace841b62c5303ad9c49` |
| [Alan Langerman (ed.), *NORMA IPC Version Two: Requirements* (OSF RI)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/NORMA_IPC_req.ps) | 366,020 | `b76897d651d7a5bae71ee7e42d6c7453fc0195294b4e69c7c0816f128423ce2d` |
| [José Rogado, *A Prototype File System for the Cluster OS* (OSF RI)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/Pfs.ps) | 100,508 | `b5f729ac14cd98604b90b5476289ffd86224ef9bc14ec11ad23083786078d062` |
| [*Alpha/Mach Integration Study*, a proposal to Rome Laboratory (OSF RI, 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/RIMemo_alpha.pdf) | 83,454 | `e8fbd3869f47b9b97241fff8ef9dca2ea34519e2fcaa6723d79c620dbdf08145` |
| [Philippe Bernadat, David L. Black, *Real Memory Mach* (OSF RI)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/RealMemMach.ps) | 207,246 | `2f244b70c5a5e97b4656086175c3e81280efb80c821024a7051a8888d41ba2d0` |
| [Ira Goldstein, *Summary of OSF RI System Technology Accomplishments for 1992 and Directions for 1993* (December 1992)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/acc.gold.ps) | 44,063 | `6b3f761f8687f875dcc0f594e873e54fcfa82199ef425708a73c88f0a5e18d85` |
| [Paul Roy, David Noveck, Durriya Netterwala, *The File System Architecture of OSF/1 AD Version 2* (OSF RI)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/fsys_arch.ps) | 85,288 | `b23798baa995622ed3fb27f3d1e8dd1e4fd66f16be1de92d725ab842c16415de` |
| [Youcef Laribi, Xavier Rousset de Pina, *Debugging an object-oriented system using the Mach interface* (2 December 1992)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/gdb_mach3.ps) | 100,095 | `ea5a5f7a99862c5ba28caf1e92cf5ae7b26f8dba8e89d687e6e5e08dc474aa5e` |
| [Franco Travostino, *Mach3 Locking Protocol* (OSF RI, October 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/locking.ps) | 169,418 | `6cbd4b5854f2433bb533a03f2183541ab01a4b6d4008969b008b2af00de014a5` |
| [Simon Patience, *Redirecting System Calls in Mach 3.0: An alternative to the emulator* (OSF RI; USENIX Mach III, 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/noemu.ps) | 149,880 | `e7633001f15e2edf1e086489dc6aa75cee3cc4119e636174c69f1e7dbade3d11` |
| [*NORMA IPC: A Task-to-Task Communication System for Multicomputer Systems* (OSF RI, October 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/norma.ps) | 175,502 | `583dd3ec754957d1b3a10ed68b5e48ebde6f46d6589da26519189e16774c8f93` |
| [Bryant, Sears, Black, *An Introduction to Mach 3.0's XMM Subsystem* (OSF RI)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/nxmm.ps) | 98,937 | `6d007723de3ba032ea5fd4ce3bdae55786b52368025e54041bb03c4461418cbd` |
| [*NORMA IPC Version Two: Architecture and Design* (OSF RI, February 1994)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad2_papers/kernel/dipc.94-02-03.ps) | 346,876 | `2d235d20703747a0a5301cdece5a6d41aa48c7835176050b891b6f8716ff3a5e` |
| [Dean, Dominijanni, *An RPC-Based External Memory Management Interface: Architecture* (April 1994)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad2_papers/kernel/emmi.94-04-13.ps) | 111,515 | `6c312bda05f2265a25619946d09cedb9e5b018ea72ac5ad0e32247abec9ed000` |
| [*Kernel to Kernel Transport Interface for the Mach Kernel* (February 1994; its RPC chapter "authored and copyrighted by Intel Corporation")](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad2_papers/kernel/kkt.94-02-03.ps) | 110,664 | `3cce06f6b7c034d513c522b02803d44f4b2d6298ea57c7946291ee34c0e698f9` |
| [*MK6* performance project paper (April 1994)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad2_papers/kernel/mk6.94-04-13.ps) | 153,693 | `327449b60116c21dc803c678c8b3a50d65c9e064f7532ee518963d67e80b3397` |
| [*Kernel to Kernel Transport Interface for the Mach Kernel*, earlier draft (January 1994; Intel-copyrighted chapter)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad2_papers/kernel/transport.94-01-18.ps) | 121,980 | `1a5d3a15f071e3895ca2a1a1873abd17c7da559127ce547a00e6458b360fc9bb` |
| [Michelle Dominijanni, *Multiple Coherency Models in XMM: Architecture and Design* (February 1994)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad2_papers/kernel/xmm_coherency.94-02.ps) | 70,648 | `e1371d935e89f111ea0292fa20e0d5664198c7cfff3ea0396d410cfcb6769e24` |
| [*File system architecture of OSF/1 AD Version 2* (August 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad2_papers/osf1/fsys_arch.93.08.ps) | 70,953 | `27d812fc49a1a0a0da4f511942a03a1c5ffa563eddb3e7d3256555f21d567734` |
| [*File system requirements of OSF/1 AD Version 2* (July 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad2_papers/osf1/fsys_req.93.07.ps) | 32,174 | `9105dbb0cfce441eacea7cfd0099935344ae79b560ab799217698f621bc2882c` |
| [David Noveck, *Interposition Mechanism for the OSF/1 AD Version 2 File System* (March 1994)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad2_papers/osf1/intp_design.94.03.ps) | 89,693 | `0f8012ccde0f85f5a750b63da9af161ccde86652fdc9b43ea2b9193d900ff7a5` |
| [John LoVerso, Paul Roy, *The Network Architecture of OSF/1 AD Version 2* (February 1994)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad2_papers/osf1/net_arch.94.02.ps) | 111,673 | `5e82c675284bf799ce0fdd07ae48ac005ebe23e9e26a1799c6d9dd6ac8ddae07` |
| [Patience, Rabii, *The Design of the Process Management Component of OSF/1 AD Version 2* (November 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad2_papers/osf1/pm_design.93.11.ps) | 90,576 | `346e5e682ce586003a2acfd3c065e73d7538d9645522702b4fa5662902731d1b` |
| [Rabii, Noveck, Gilbane, *Distributing Process Management in OSF/1 AD Version 3* (March 1996)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad2_papers/osf1/pm_design.96.03.ps) | 178,131 | `210a2d6b4ab897364795c133b105064da0dfbd704970d821cc1716077165bed7` |
| [David Noveck, *Token Module for OSF/1 AD Version 2* (July 1994)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad2_papers/osf1/token_design.94.07.ps) | 185,194 | `c58c8a051914b2f0baa2c8f7865e9785f097a9f0bef3166dd69b00af2ce60d81` |
| [Zajcew, Roy, Black, Peak, Guedes, Kemp, et al., *An OSF/1 Unix for Massively Parallel Multicomputers* (USENIX Winter 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/osf1ad_usenix.ps) | 354,110 | `ab3e240a643042930ff46f68a10082ddc99a23f0b39967d95394ffd742d16c39` |
| [Faramarz Rabii, Simon Patience, *The Process Management Architecture of OSF/1 AD Version 2* (OSF RI, 20 September 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/pm_arch.ps) | 107,782 | `e3c40c7833ad6d74013337af2cd0fc4128733b8b4a04f3e612ac116a6aef6952` |
| [Randall W. Dean, *Porting OSF/1 AD* (OSF RI, 27 September 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/porting.ps) | 23,958 | `de6b9e767ba425c834b6ad46f4758ba214a8944dd67abef2031a9c35a30d8082` |
| [Dan Swartzendruber, *OSF Real-Time Mach: Making the Mach Kernel Preemptible* (8 October 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/preemption.ps) | 41,667 | `cbab5c8eb8e130e5234fa4e59d49c99e76b6cecf92e7a6c53d520c01a59f33b6` |
| [Ira Goldstein, *The OSF Research Institute Mission Statement*](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/ri.miss.ps) | 32,506 | `d84cd7bcdf371c699f5792e5c25e1949c317d98082b420aa24194d126920bca6` |
| [Michael Condict, *Optimizing Performance of Mach-based Systems By Server Co-Location: A Detailed Design* (OSF RI, 24 August 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/server_colocation_design.ps) | 328,852 | `a412532425ccfc82d0f8d10af6e6680680c630e039c035fddbabf30d721ee9cf` |
| [José Rogado, *A Strawman Proposal for the Cluster Project* (OSF RI)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/straw.jose.ps) | 203,065 | `3d4e1bc70933980cc3366f1af241c4e7bbfdf01f3e0abecbd2693fcdf4f2f13e` |
| [Reynolds, Travostino, MacDonald, Ellistion, Loepere, *Design for a New Untyped IPC for Mach* (OSF RI)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/untyped_IPC.ps) | 146,811 | `f17a4894e7a52669816b5d846a1f729dbfd526d0309c5c87e58fdc939d192673` |
| [Travostino, Reynolds, *x-kernel Evaluation at the OSF RI*](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/papers/xkernel.ps) | 98,483 | `3d90f3cb82a5013f15d363e0766bddb1c0891b62ab7b348a7a1fe8a44924b724` |
| [MK6.1 directory README (10/14/1994)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/osfmk/mk6.1/README) | 494 | `648bd030f0d8fe22e6e54585d57df01f223ea9d150134560d484b7b7fe95bee2` |
| [*OSF-RI MK6.1 Kernel API Changes*](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/osfmk/mk6.1/api-changes.ps) | 167,516 | `c37382ee90691391097bbf91acabc7b42bda51a2de1a46d41856607e686ae0c9` |
| [Douglas M. Wells, *A Trusted, Scalable, Real-Time Operating System Environment* (IEEE Dual-Use Technologies and Applications Conference, 1994)](https://ftp.gwdg.de/pub/misc/opengroup/ri/real-time/papers/dual-use.paper.pdf) | 47,429 | `3d12fbc96634f8ca454fd24b774199d418422d7784b23e3cf2a627c4cbfee71c` |
| [Spec '93 directory INDEX](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/spec/spec93/INDEX) | 467 | `07a276548fccd03e1da5915dc1800f955df93e54aebc8b6942d871af87c2ca81` |
| [Keith Loepere, *OSF Mach Kernel Interface Changes* (14 May 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/spec/spec93/interface-changes.ps.Z) | 86,147 | `d4c1224bca991938546d3cdc2c0c3716e41ee769fb406b5e0fb40e1ef33223f6` |

### Copies of what is carried here, or of its parts

The mirror's chapter files of the MK7.3 Release Notes and the three
parts of Spec '94 Draft 1 duplicate the whole books carried under
`osf-ri/`; recorded so the mirror's copies can be checked against them.

| document | bytes | SHA-256 |
|---|---|---|
| [relnotes.ps](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/osfmk/mk6.1/relnotes.ps) | 252,776 | `5d73589d5d2ce1bbcc0d706bc2b920dacd7be44d3310c4367d4731943d25f64d` |
| [API.ps](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/osfmk/mk7.3/API.ps) | 547,863 | `c058d7424b29058e720e35cbcd74795df99fa6ecfa2d074dcc77a06e6e861a1d` |
| [Changes.ps](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/osfmk/mk7.3/Changes.ps) | 118,608 | `80105e1f6cffed5d72d90f1959aa3a2d5e2632bd4c24331e4d08e9df1ea6a26f` |
| [Copyright.ps](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/osfmk/mk7.3/Copyright.ps) | 82,546 | `a6adf2a13da362d87bf6dc9e4eb14023473564530cc5a3474bd178e2edf52c50` |
| [Features.ps](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/osfmk/mk7.3/Features.ps) | 206,323 | `81afa4d9001492fe27d841ae9405f376bdbf8096bdcae131879144086e0b595c` |
| [Install-MK7.ps](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/osfmk/mk7.3/Install-MK7.ps) | 195,208 | `063d78e626c30f0e93ed6f642e245c8fadbf2f156acd2043e5e442acb1791a61` |
| [RelNotes7.Book.ps](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/osfmk/mk7.3/RelNotes7.Book.ps) | 900,694 | `62b0b74d442951d1f353a7abd5c3f2ac77cc10dbec8f6c76eb62bd26c2c0150c` |
| [RelNotes7TOC.ps](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/osfmk/mk7.3/RelNotes7TOC.ps) | 98,664 | `6095896e8df3a9a83f2f9705a5dabc0688e4e3398bb12f141abf2cf1fb6ed8cf` |
| [Title.ps](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/osfmk/mk7.3/Title.ps) | 86,233 | `c1fb851c363d91665a25679c7927139b64f994f073c89154d556627e738836ab` |
| [kernel_interface.ps.Z](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/spec/spec93/kernel_interface.ps.Z) | 699,113 | `b0dea31dfe4f985221cc4f9a7aa8d857dbf54d42f463360e2e23b9a3207e6205` |
| [kernel_principles.ps.Z](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/spec/spec93/kernel_principles.ps.Z) | 274,441 | `e4903d45b8145d636e7996a633117815431125717b59c5e937bf438ec2f36ad5` |
| [server_library.ps.Z](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/spec/spec93/server_library.ps.Z) | 127,973 | `eb355bc251d02fa3e32d5b92ff148a9a2c8fd96d69f7aa2c895871551577f0ac` |
| [server_writer.ps.Z](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/spec/spec93/server_writer.ps.Z) | 454,000 | `7f6dd21f8911165b568e617e0b863e81f0f87b402b6b83b79f78c2d5f316a36f` |
| [SPEC94_DRAFT_1.ps](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/spec/spec94/SPEC94_DRAFT_1.ps) | 2,430,981 | `790e0ebdf0871a0e9b52c284e56abacef1a26cc5a3b0c081c97603813e9d0781` |
| [SPEC94_DRAFT_1_PART_A.ps](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/spec/spec94/SPEC94_DRAFT_1_PART_A.ps) | 1,156,661 | `e97535707281d5c48b8b07654d6790e49bb6c94361f9cd88e76137fc2509a3b3` |
| [SPEC94_DRAFT_1_PART_B.ps](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/spec/spec94/SPEC94_DRAFT_1_PART_B.ps) | 1,562,048 | `8d5072301be6213387fdcc9fcf8b3f82c9c3cb0f199757e320ee7b088ec2018b` |
| [SPEC94_DRAFT_1_PART_C.ps](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/spec/spec94/SPEC94_DRAFT_1_PART_C.ps) | 852,567 | `c95ec52c919aa7185a6c5de8c2c73bd5d2865057c77b583973b30b203ea90589` |

### Digital Equipment Corporation manuals

From bitsavers.org. *OSF/1 Internals* says "No part of this publication
may be reproduced" without IAPS's permission; the DEC manuals carry
DEC's copyright. Cited by page; never quoted at length.

| document | bytes | SHA-256 |
|---|---|---|
| [*OSF/1 Internals*, Volume I (Institute for Advanced Professional Studies, for Digital Equipment Corporation, 1992)](https://bitsavers.org/pdf/dec/osf/OSF1_Internals_Vol1_1992.pdf) | 8,397,013 | `4af5c203a93860bdc752a2ae6eaca201d8a009d634a84595b451a94c52b69439` |
| [*OSF/1 Internals*, Volume II (IAPS for DEC, 1992)](https://bitsavers.org/pdf/dec/osf/OSF1_Internals_Vol2_1992.pdf) | 12,113,619 | `44f35b73a6b204af2d506f55bbc143cfcc5734573162ce63c27f6d5cec524931` |
| [*OSF/1 Internals* Student Workbook (Hewlett-Packard H2596-90001, May 1991; slides © OSF 1990-91)](https://bitsavers.org/pdf/dec/osf/OSF1_Internals_Student_Workbook_may91.pdf) | 17,729,905 | `c84c2fe5a8518bf27dd9d1227f3491baa09de6fde8de29a450cb13a860b30261` |
| [DEC OSF/1 *Kernel Debugging*, AA-PS2TB-TE (February 1994)](https://bitsavers.org/pdf/dec/osf/2.0/AA-PS2TB-TE_Kernel_Debugging_Feb94.pdf) | 4,733,904 | `21eb321ff0849b2bac98254c6e8fa2e938c079ae2dc9fdd2eb354dafd2a5ca44` |
| [DEC OSF/1 *Writing Device Drivers*, Volume 1: Tutorial, AA-PUBVB-TE (February 1994)](https://bitsavers.org/pdf/dec/osf/2.0/AA-PUBVB-TE_Writing_Device_Drivers_Volume_1_Tutorial_Feb94.pdf) | 25,279,200 | `4495fe080d217a4e027beb1ea1e821d61c5fa761ac85417a2d848baf13a1b750` |
| [DEC OSF/1 *Guide to Realtime Programming*, AA-PS33B-TE (February 1994)](https://bitsavers.org/pdf/dec/osf/2.0/AA-PS33B-TE_Guide_to_Realtime_Programming_Feb94.pdf) | 7,712,650 | `052af36bc2237b12b79c2ebcbe60233c13688498a4849e89e571edb12237aaa0` |
| [DEC OSF/1 *System Tuning and Performance Management*, AA-Q0R3A-TE (February 1994)](https://bitsavers.org/pdf/dec/osf/2.0/AA-Q0R3A-TE_System_Tuning_and_Performance_Mangement_Feb94.pdf) | 4,000,396 | `c2dc658e8f3679cf8e6076d1b1f3d9945ac597cf2d290a995301e2d3157543ab` |
| [*DEC OSF/1 3.0 Technical Overview*, AA-Q0R1B-TE (August 1994)](https://bitsavers.org/pdf/dec/osf/3.x/AA-Q0R1B-TE_DEC_OSF1_3.0_Technical_Overview_199408.pdf) | 8,933,523 | `6d1253add82a69b348be2c27d95d3fb9d4a2f3e4edbb4d5bd88c618ea42b2761` |
| [*DEC OSF/1 3.0 Release Notes*, AA-PS2BD-TE (August 1994)](https://bitsavers.org/pdf/dec/osf/3.x/AA-PS2BD-TE_DEC_OSF1_3.0_Release_Notes_199408.pdf) | 6,236,488 | `02e7009b7807bcc7547b6921923ee3b12f811d5c2422646c27afed59df3c6ff2` |

### Intel Paragon manuals

From bitsavers.org, `pdf/intel/supercomputer/Paragon/`; Intel's
copyright. Cited, not carried.

| document | bytes | SHA-256 |
|---|---|---|
| [312486-001 Paragon OSF1 Commands Reference Manual Apr1993](https://bitsavers.org/pdf/intel/supercomputer/Paragon/312486-001_Paragon_OSF1_Commands_Reference_Manual_Apr1993.pdf) | 5,162,598 | `09266da3de2d1be7af142d2ee578755a63b7ad915cd41d3462ba7d89ab472eb8` |
| [312487-001 Paragon OSF1 C System Calls Reference Manual Apr1993](https://bitsavers.org/pdf/intel/supercomputer/Paragon/312487-001_Paragon_OSF1_C_System_Calls_Reference_Manual_Apr1993.pdf) | 10,271,677 | `249b008bdebb4342756ef9b486f6091223744081ad668e7450f9b34f9318fac6` |
| [312489-001 Paragon OSF1 Users Guide Apr1993](https://bitsavers.org/pdf/intel/supercomputer/Paragon/312489-001_Paragon_OSF1_Users_Guide_Apr1993.pdf) | 20,543,554 | `93c0a2e14279788b384c7ac683fc2c05f9b18e0336a2d606b456e878338f8f46` |
| [312544-004 Paragon System Administrators Guide May1995](https://bitsavers.org/pdf/intel/supercomputer/Paragon/312544-004_Paragon_System_Administrators_Guide_May1995.pdf) | 26,119,327 | `7a8abf3c8207a4a6f9da22564a9ebdafa5f835c834b5c42f62853b1756eaea74` |
| [312853-005 Paragon System Source Code Product Release 1.3 Users Guide Jul1995](https://bitsavers.org/pdf/intel/supercomputer/Paragon/312853-005_Paragon_System_Source_Code_Product_Release_1.3_Users_Guide_Jul1995.pdf) | 1,893,498 | `80783f84b803239f0db3b0e6f1143bfd4c0e51d4db973d800c922709d55f951f` |
| [312853-006 Paragon System Source Code Product Release 1.4 Users Guide May1996](https://bitsavers.org/pdf/intel/supercomputer/Paragon/312853-006_Paragon_System_Source_Code_Product_Release_1.4_Users_Guide_May1996.pdf) | 1,817,958 | `75e6d871d650c31c5a5e27ee692f06fdcff933e1a981c7ea2dc55b4579d46e9c` |
| [312857-003 OSF1 Documentation Errata May1995](https://bitsavers.org/pdf/intel/supercomputer/Paragon/312857-003_OSF1_Documentation_Errata_May1995.pdf) | 1,634,800 | `f7d3506062d6b680d42a5f16b387dbbbca917da9581d77e8a4b8d4b0310e7457` |
| [313057-001 Paragon System Software Release 1.2 Release Notes Jun1994](https://bitsavers.org/pdf/intel/supercomputer/Paragon/313057-001_Paragon_System_Software_Release_1.2_Release_Notes_Jun1994.pdf) | 8,513,887 | `9166c4ab660ecd696f244aee0bc14bb13cd2ae75070f6b8c5730cd6411d28a8d` |
| [313167-001 Paragon System Software Release 1.3 Release Notes May1995](https://bitsavers.org/pdf/intel/supercomputer/Paragon/313167-001_Paragon_System_Software_Release_1.3_Release_Notes_May1995.pdf) | 4,406,290 | `fe4987ff430f1a0c1b39d9fda84c9ba01f88d66e1a4b016b7f54f5eb807cdbd3` |
| [313168-001 Paragon System Software Release 1.3 Installation Guide May1995](https://bitsavers.org/pdf/intel/supercomputer/Paragon/313168-001_Paragon_System_Software_Release_1.3_Installation_Guide_May1995.pdf) | 3,179,554 | `fd44a2e0d16a5ba39ed0bcf5c843c68681c3d16e8118e6205102d0e98f93682f` |
| [641067-001 Paragon System Source Code Product Release 1.3 Release Notes Jul1995](https://bitsavers.org/pdf/intel/supercomputer/Paragon/641067-001_Paragon_System_Source_Code_Product_Release_1.3_Release_Notes_Jul1995.pdf) | 608,508 | `5f0c4b79bd36ff43f5953730fb9012879c8ac6716665b44bf62259f7598a2a92` |
| [654099-001 Paragon System Software Release 1.4 Installation Guide Apr1996](https://bitsavers.org/pdf/intel/supercomputer/Paragon/654099-001_Paragon_System_Software_Release_1.4_Installation_Guide_Apr1996.pdf) | 3,641,804 | `387ec6e25b2956e39b1320e1fe5ba95f987959c1c2877d2e39782512f80ec327` |
| [654100-001 Paragon System Software Release 1.4 Release Notes Apr1996](https://bitsavers.org/pdf/intel/supercomputer/Paragon/654100-001_Paragon_System_Software_Release_1.4_Release_Notes_Apr1996.pdf) | 5,491,791 | `d195bddf2bf660d7f33d9a72c7c05bc3ff847a754fe5c7447c4acd82e0e99301` |
| [658861-001 Paragon System Source Code Product Release 1.4 Release Notes May1996](https://bitsavers.org/pdf/intel/supercomputer/Paragon/658861-001_Paragon_System_Source_Code_Product_Release_1.4_Release_Notes_May1996.pdf) | 608,938 | `329c0338d61a46b1f93cd782b452ca101433bc8b8831cf398b5f6c086ba4d1ce` |

### USENIX papers

The authors' copyright. The two 1991 papers are scans without a text
layer, read here by OCR.

| document | bytes | SHA-256 |
|---|---|---|
| [Don Bolinger, Shashi Mangalat, *Parallelizing Signal Handling and Process Management in OSF/1* (USENIX Mach Symposium, 1991), pp. 105-122](https://www.usenix.org/legacy/publications/library/proceedings/mach91/bolinger.pdf) | 1,092,258 | `8998bc7934d4c920b8060749d6a727389a3ba19bf263f1d26a98aae44f311bf4` |
| [Rand A. Hoven, *Mach Interfaces to Support Guest OS Debugging* (USENIX Mach Symposium, 1991), pp. 131-147](https://www.usenix.org/legacy/publications/library/proceedings/mach91/hoven.pdf) | 992,378 | `9fb8e6466d56cd094039ffa6551e12e5b46f41cf6409b4e991a27d49a2fc97da` |
| [Paul J. Roy, *Unix File Access and Caching in a Multicomputer Environment* (USENIX Mach III, 1993)](https://www.usenix.org/legacy/publications/library/proceedings/mach3/full_papers/roy.ps) | 218,828 | `c9156291de02abe9198008bc588d221f2aa2e7da04758aa657a6ea4c0c283d81` |

### CMU Mach documents

From CMU's Mach document archive as mirrored at
`ftp.lip6.fr/pub/mach/cmu/doc/`. No permission notice; cited, not
carried.

| document | bytes | SHA-256 |
|---|---|---|
| [Golub, Dean, Forin, Rashid, *Unix as an Application Program* (USENIX Summer 1990)](https://ftp.lip6.fr/pub/mach/cmu/doc/published/mach3_intro.ps.gz) | 22,437 | `81c121827d514fa27939586ac96a3d07e5a70357eb86ac75039a91bc188676c4` |
| [CMU, *Mach exception handling*](https://ftp.lip6.fr/pub/mach/cmu/doc/unpublished/exception.ps.gz) | 21,603 | `4fd5b75a180225b879716e4834a5fedf029cb3bf5ae7bd3528fd80507efd526d` |
| [CMU, *Mach system calls*](https://ftp.lip6.fr/pub/mach/cmu/doc/unpublished/machsys.ps.gz) | 34,639 | `fb17e45a0b7dc36a422a975e056aa493c6b2d36d30d5728a3fbbdcfbc3ea5e07` |
| [CMU, *Setting up Mach 3.0*](https://ftp.lip6.fr/pub/mach/cmu/doc/unpublished/mach3_setup.ps.gz) | 10,560 | `62b2d4905d834cb7bfe7f96a01226c5e1d77227b96bc3b80485f80cc3f9c37fa` |
| [CMU, *Mach 3.0 at CMU*](https://ftp.lip6.fr/pub/mach/cmu/doc/unpublished/mach3_cmu.ps.gz) | 18,543 | `b9c3ae84d390c219ca18a0b6068de22345272737a87fa9c727778ee79a9d693e` |
| [CMU, *Mach bibliography*](https://ftp.lip6.fr/pub/mach/cmu/doc/mach_bib.ps.gz) | 41,438 | `37ebadab5bf68d965198f45a4cef1411d68ddf096c458da3b5e47a9cdae2a29d` |
| [CMU document archive README](https://ftp.lip6.fr/pub/mach/cmu/doc/README) | 812 | `d515875a75b6263852345c3ad5f56e490fcdc3b328341840d453db5a1914ddf9` |

### Manual pages, from typewritten.org

Fetched on 23 October 2026, with the site allowed for this work. The
pages are DEC's and OSF's copyright; cited, not carried. Each page's
SHA-256 is listed in the file named.

| manual | pages | hashes |
|---|---|---|
| OSF/1 1.0 (TIN) MIPS, `typewritten.org/Manual/DEC/OSF:1/1.0/mips/`: all of section 2, and the pages of sections 3, 4, 5, 7 and 8 that `docs/osf1-calls.md` uses | 332 | `osf1-1.0-man.sha256` |
| DEC OSF/1 3.0 for Alpha AXP, `typewritten.org/Manual/DEC/OSF:1/3.0/`, section 2, for `docs/osf1-calls.md` appendix A | 205 | `osf1-3.0-man2.sha256` |

The site also holds DEC OSF/1 X2.0-8 (MIPS), DEC OSF/1 "SILVER"
Baselevel 4, HP OSF/1 A.01.00 (sections 1, 3 and 5 only), Digital UNIX
and Tru64; not used yet.

### Pages read on the web, not fetched as files

- OSF's Mach manual pages as served from the XNU repository
  (carried above, `xnu-792/`).
- *MK7.3a Real-Time Microkernel for the OSF/1 OS, Release Notes*
  (The Open Group RI, 28 October 1998), the parts shown at
  `academia.edu/41024405`.
- Digital UNIX V4.0 *System Administration*, "Configuring the
  Kernel", and *Technical Overview*, "Symmetrical Multiprocessing",
  as mirrored at `cs.auckland.ac.nz/references/unix/digital/`.
- DEC OSF/1 1.0 (MIPS) and 3.0, and Tru64 UNIX 5.0a and 5.1b, manual
  pages as kept at `typewritten.org/Manual/DEC/`: `mkfifo(3)`,
  `mknod(2)`, `fattach(3)`, `ffm(4)`, `kopt(8)`, `config(8)`,
  `sys_attrs_vfs(5)`. Read with the web tool; the site does not answer
  the workspace's own fetches.
- Tru64 UNIX manual pages, section 9r (`unix_master`, `simple_lock`,
  `lock_init`, `lock_write`, `mpsleep`), at `unix.com/man-page/osf1/`.
- Lepreau, Hibler, Ford, Law, *In-Kernel Servers on Mach 3.0:
  Implementation and Performance* (USENIX Mach III, 1993),
  `bford.info/pub/os/inks.pdf`; the maintainer's copy has SHA-256
  `1a640a736232e523a53a868446a632c04dd6d3ebbefc1b3c39d8056c9824b8d9`,
  405,201 bytes.
- Bryan Ford, Jay Lepreau, *Evolving Mach 3.0 to a Migrating Thread
  Model* (USENIX Winter 1994), `bford.info/pub/os/thread-migrate.pdf`.
- Johannes Helander, *Unix under Mach: The Lites Server* (M.Sc.
  thesis, Helsinki University of Technology, 1994).
- For the law: 17 U.S.C. §§ 102 and 107; 18 U.S.C. § 1839; 37 C.F.R.
  § 202.1; the Copyright Office's Circulars 33 and 61;
  *Computer Associates v. Altai*, 982 F.2d 693 (2d Cir. 1992); *Feist
  v. Rural*, 499 U.S. 340 (1991); *Sega v. Accolade*, 977 F.2d 1510
  (9th Cir. 1992); *Google v. Oracle*, 593 U.S. 1 (2021); *Harper &
  Row v. Nation*, 471 U.S. 539 (1985); *Religious Technology Center
  v. Lerma*, 908 F. Supp. 1362 (E.D. Va. 1995); *Unix System
  Laboratories v. Berkeley Software Design* (D.N.J., 3 March 1993).
  Statutes, regulations, court opinions and the Copyright Office's
  circulars are not under copyright and may be carried here once
  their official sources (`govinfo.gov`, `copyright.gov`,
  `ecfr.gov`, `courtlistener.com`) are reachable from the session
  that fetches them.
- Commentary, cited only: Elizabeth Rowe, "Saving Trade Secret
  Disclosures on the Internet" (*Wake Forest Law Review* 42, 2007);
  the Digital Media Law Project's "Publishing Trade Secrets"; the
  Wine Developer FAQ; Wikipedia's articles "Clean-room design",
  "ReactOS" and "OSF/1".

### Found on 23 October 2026, second search

OSF/1 MK release notes posted by OSF to the `osf1-mk` mailing list, and
CMU's notes on OSF's releases, as CMU's Mach FAQ keeps them
(`ftp.lip6.fr/pub/mach/cmu/FAQ/`). No permission notice; cited, not
carried.

| document | bytes | SHA-256 |
|---|---|---|
| [James Loveluck, Patrick Roudaud, *OSF/1 MK5.0.1 Release Notes*, version 1.1 (OSF RI Grenoble, 19 May 1993)](https://ftp.lip6.fr/pub/mach/cmu/FAQ/OSF1-mk.rel5.0.1) | 26,710 | `69b25691a95b16340c17185374a36b16a86538056fd44dc0fea53aa4a088951b` |
| [James Loveluck, *Announcing Documentation for Microkernel in OSF/1 MK 5.0* (23 April 1993)](https://ftp.lip6.fr/pub/mach/cmu/FAQ/OSF1-mk.rel5.0.doc) | 5,061 | `c9475cdb2c172a47ce54a6ea396a9590ca832f7f5b8c65504b9c6bd918a78c6a` |
| [CMU Mach FAQ, *OSF releases of Mach* (12 November 1992)](https://ftp.lip6.fr/pub/mach/cmu/FAQ/OSF.releases) | 2,070 | `f615c214ef43fcdc098da154186664d5e752a02c625ebc28adb3281fbc2445d4` |
| [CMU Mach FAQ, *Hard copies of OSF/RI documents* (19 March 1992)](https://ftp.lip6.fr/pub/mach/cmu/FAQ/OSF.docs) | 5,068 | `764bb7ca135931df54eb4b765c865e1b07971527aeac4aac4f1cceb8f4b204a4` |

Dejan Milojičić's task-migration papers, from OSF RI's
`os/task_migration/` on the same mirror as above. No permission notice;
cited, not carried.

| document | bytes | SHA-256 |
|---|---|---|
| [Milojičić et al., *Task Migration on the top of the Mach Microkernel* (USENIX Mach III, 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/task_migration/machIII.ps.Z) | 121,815 | `060c646b5689008acbd462f140cea3e24f941bd11fbd9654d7ce4be386f07a38` |
| [Milojičić et al., *Process vs. Task Migration* (HICSS-29, 1996)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/task_migration/pmvtm.ps) | 184,778 | `967e046b9e8c7f80274721be25b3e90d9b45c1361da802fccfb3c5128faccec1` |
| [Milojičić et al., *Concurrency, a Case Study in Remote Tasking and Distributed IPC* (HICSS-29, 1996)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/task_migration/concurrency.ps) | 231,265 | `f98830ad4eb2c4796278f2ad8646f153c59adb963af366762dfc3e14e777446e` |
| [Milojičić, Black, Sears, *Operating System Support for Concurrent Remote Task Creation* (IPPS, 1995)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/task_migration/ipps6.ps) | 102,242 | `36e41858cf8865800d0bbb5f98b6c3de21607acd414130cf25c85c71a935f142` |
| [Milojičić et al., *Experiences with Load Distribution on top of the Mach Microkernel* (SEDMS IV, 1993)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/task_migration/sedmsIV.ps.Z) | 109,025 | `41da5e7e1036f3c02dd09123667d043fc49fdaf63e6e732270a939b43eb7cc06` |
| [Milojičić et al., *Load Distribution on Microkernels* (FTDCS IV)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/task_migration/ftdcsIV.ps.Z) | 104,861 | `b48ed2336f80e731d8c51727c6737b836463e6c4ecabc036062013fe547d524e` |
| [Condict, Milojičić, Reynolds, Bolinger, *Towards a World-Wide Civilization of Objects* (1996)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/task_migration/fdr/SigOps96a.frame.ps) | 151,423 | `bdcea1fddb3fe529c5379e4d16043a09f6e86d88f730d894f282a1586ee3a7e3` |

CMU's Mach documents as mirrored at Lysator
(`hina.lysator.liu.se/pub/doc/OS/mach/`). No permission notice; cited,
not carried.

| document | bytes | SHA-256 |
|---|---|---|
| [Baron, Black, Bolosky, Chew, Draves et al., *Mach Kernel Interface Manual* (CMU)](https://hina.lysator.liu.se/pub/doc/OS/mach/manual.doc) | 174,504 | `296aa362b62168b6d91221094b2a123f3695caab4af95e062bc6b6fb479bd27b` |
| [CMU, *The Mach 3.0 micro-kernel: source directories*](https://hina.lysator.liu.se/pub/doc/OS/mach/kern-dir.doc) | 4,734 | `551be0fb67d18299933c3f3ccd7951c0f8cf933159f2e2b77f3fbe660b518a60` |
| [Rich Draves, *Unix-server configurations* (CMU, 9 March 1992)](https://hina.lysator.liu.se/pub/doc/OS/mach/build.ux) | 1,365 | `e709877dddaecba8af6204f68dded57abae8e26b63a5180502146fabec5e01c3` |
| [Daniel Julin, *Multi-Server availability* (mail, CMU, 11 September 1992)](https://hina.lysator.liu.se/pub/doc/OS/mach/multi-server.info) | 10,964 | `42bbdb6ff106d38b84a71a2dd7635bbffd3c746f0da180b3ee7f992f9f551a3e` |

### Recorded on 24 October 2026, from earlier downloads

Fetched during the searches of 22 and 23 October but not listed until
now. None carries a permission notice; cited, not carried.

More of CMU's Mach FAQ (`ftp.lip6.fr/pub/mach/cmu/FAQ/`):

| document | bytes | SHA-256 |
|---|---|---|
| [James Loveluck, *Announcing OSF/1 MK5.0* (mail to `osf1-mk`, 23 April 1993)](https://ftp.lip6.fr/pub/mach/cmu/FAQ/obsolete/osf1-mk.rel5.0) | 23,733 | `d714e4ce574b2b9af046d8039f2a2a31ec1f2ce9b7d72606b4d775b4f3a42cec` |
| [CMU Mach FAQ, *INDEX*](https://ftp.lip6.fr/pub/mach/cmu/FAQ/INDEX) | 3,122 | `0fcba2283a651becd6281ae4927db4fbb26046650225333f1eed7ff0d8d9bd13` |
| [CMU Mach FAQ, *Mach 2.5/3.0 for the Intel 80386/80486*](https://ftp.lip6.fr/pub/mach/cmu/FAQ/i386_announce) | 5,549 | `4456aa3e31cefb780186404570443179dfc8cff104554e8da5abdb460dcb057c` |
| [Mike Accetta, *RS6K announcement* (CMU, 3 January 1994)](https://ftp.lip6.fr/pub/mach/cmu/FAQ/rs6k_announce) | 1,943 | `7224362b29253bcbee45efabea7f2fcbcf6b54ce2cc590faadc55bcaeb534618` |
| [Mary Thompson, *What machines does Mach run on* (CMU, January 1994)](https://ftp.lip6.fr/pub/mach/cmu/FAQ/platforms) | 4,281 | `29b9688c5f2b289fb0582f3d9fef9c3df591280f0299dbd430a097588ca4a5f1` |
| [CMU Mach FAQ, *Mach status* (October 1994)](https://ftp.lip6.fr/pub/mach/cmu/FAQ/status.Oct94) | 711 | `d47065262d24a2d4179caf7a1de72e112d57211c003959591a578ec5a1d950af` |
| [Mary R. Thompson, *How to get a Mach license* (CMU, October 1994)](https://ftp.lip6.fr/pub/mach/cmu/FAQ/license.info) | 7,459 | `f0409306d4c1e8f66a3aa12c97a42318b420de7bba414f6c33c9383dbd4cfb58` |

CMU's Mach project web pages (`www.cs.cmu.edu/afs/cs/project/mach/public/www/doc/`):

| document | bytes | SHA-256 |
|---|---|---|
| [CMU Mach project, *OSF Kernel and Server programming manuals*](https://www.cs.cmu.edu/afs/cs/project/mach/public/www/doc/osf.html) | 2,873 | `17e49074c795917ba00aa87f1792fa8a9757be8d58676475732c8df82d3bd1ce` |
| [CMU Mach project, *Mach Installation Guides and How-To Documents*](https://www.cs.cmu.edu/afs/cs/project/mach/public/www/doc/install.html) | 5,672 | `05cbc651a565315f3765ac1795c102dcf9c20dd8483a9379feb099bacfc3f3b9` |
| [CMU Mach project, *Mach publications*](https://www.cs.cmu.edu/afs/cs/project/mach/public/www/doc/publications.html) | 23,387 | `e24d0e40a0dc1db8dd7e2d98dbf81eaa9e1a4095bc2caa2d763d31ae7f65bd6e` |

More of OSF RI's `os/task_migration/`:

| document | bytes | SHA-256 |
|---|---|---|
| [OSF RI, task-migration presentation (`advisory.pres.ps`)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/task_migration/advisory.pres.ps) | 390,889 | `6e61d48901056ba43304df9761e21502643925efe0880e747690835c2c683af5` |
| [OSF RI, task-migration presentation (`osdi.pres.ps`)](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/task_migration/osdi.pres.ps) | 447,934 | `4e2e3d6a9e32fbcf305cc07c0530a11f1e1f77b828828996f7bbe95b13381459` |
| [Dejan Milojičić, *Load Distribution, Implementation for the Mach Microkernel* (thesis, Kaiserslautern, 1993): announcement](https://ftp.gwdg.de/pub/misc/opengroup/ri/os/task_migration/thesis.info) | 19,027 | `5a2e87e934ef41639af8005b9b47afd8feab4a5305dfbe106738a8377397efb3` |

The Archons Project's technical reports on the Alpha real-time
operating system (CMU; not DEC's Alpha), from OSF RI's
`real-time/alpha/techreps/`. The `OLD.` versions there were not fetched.

| document | bytes | SHA-256 |
|---|---|---|
| [Archons *Alpha* reports: the index](https://ftp.gwdg.de/pub/misc/opengroup/ri/real-time/alpha/techreps/README) | 985 | `79ece3bc11342743db033be5eb3c9e62e1915d394edff653440cd9febb650823` |
| [Archons report 88011, *Requirements and Rationale*](https://ftp.gwdg.de/pub/misc/opengroup/ri/real-time/alpha/techreps/reqrat.ps) | 410,990 | `d1f7af6ea1c3cfc2843b7d9e49123ef0816708fec1d48aa6728f466c4339ed52` |
| [Archons report 88021, *Programming Model*](https://ftp.gwdg.de/pub/misc/opengroup/ri/real-time/alpha/techreps/progmod.ps) | 590,953 | `1ee83f0347f05d61a78dc4cdbfaf0c27d6029f587984980fb6c4e25d97d4a8f3` |
| [Archons report 88033, *The Alpha Distributed Computer System Testbed*](https://ftp.gwdg.de/pub/misc/opengroup/ri/real-time/alpha/techreps/testbed.ps) | 492,283 | `526197fd117d943b8f78f95cef6c91ecbea13ba5aa0bcc6e36e3b2a3de9526fe` |
| [Archons report 88041, *Programming Utilities*](https://ftp.gwdg.de/pub/misc/opengroup/ri/real-time/alpha/techreps/progutl.ps) | 226,244 | `aa570d03f7d2f494447bdd2d96dd902bc8cd5d81c234cd5a1b0e68fd59a39a7d` |
| [Archons report 88042, *Programming Language Support*](https://ftp.gwdg.de/pub/misc/opengroup/ri/real-time/alpha/techreps/proglang.ps) | 304,190 | `278cf6d797c1e9ba9abc3906d189b0c40f3f217f24dd32c05516dff93bcf33a3` |
| [Archons report 88051, *Kernel Internals*](https://ftp.gwdg.de/pub/misc/opengroup/ri/real-time/alpha/techreps/kintrnl.ps) | 666,446 | `b5c38a4e90c5d7af77d51b17e37e0cccd2cdb20051388aafa68913f135d304b3` |
| [Archons report 88061, *Kernel Interface*](https://ftp.gwdg.de/pub/misc/opengroup/ri/real-time/alpha/techreps/kintrfc.ps) | 215,389 | `8c0b7ae7c5cf29fa82559bf9e16c13ded11a816a1bd1befe3242b825ccac16b6` |
| [Archons report 88071, *Program Maintenance Manual*](https://ftp.gwdg.de/pub/misc/opengroup/ri/real-time/alpha/techreps/pmntman.ps) | 695,903 | `13271e0ce851a01d5e0519eb4c890599fc1b0e519e57233789cf26fcf656032b` |
| [Archons report 88121, *An Example Real-Time Command, Control and Battle Management Application for Alpha*](https://ftp.gwdg.de/pub/misc/opengroup/ri/real-time/alpha/techreps/c2appl.ps) | 519,819 | `490a01a2144e533b087365c1b576783ee6703e9b732e019247679a8d43f6c5bf` |
| [Archons report 90021, *Scheduler Evaluation Experiments*](https://ftp.gwdg.de/pub/misc/opengroup/ri/real-time/alpha/techreps/sched.ps) | 301,219 | `bb3579623aab8f10f80f78bad43ba622c5df7ea6c79646cafb9f9281cd3894c1` |
