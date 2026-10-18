#!/bin/sh
# The Ghidra project and the decompiled functions, from NFSSE/NFS_DOS.EXE.
#   tools/rebuild.sh        (about an hour; logs in build/logs/)
# Needs tools/ghidra-ext/install.sh once per Ghidra install. Everything goes to build/.
set -e
cd "$(dirname "$0")/.."
G=/opt/ghidra/support/analyzeHeadless
P="build/ghidra renfs"
rm -rf build/ghidra build/decomp
mkdir -p build/ghidra build/logs build/decomp
python3 tools/unle.py NFSSE/NFS_DOS.EXE -o build/nfs_flat.bin --map build/nfs_layout.json --fixups
echo "import";   $G $P -import build/nfs_flat.bin -processor x86:LE:32:watcom -loader BinaryLoader \
    -loader-baseAddr 0x0 -noanalysis -scriptPath tools -postScript CreateEntry.java 0x94444 > build/logs/import.log 2>&1
echo "analyse";  $G $P -process nfs_flat.bin -analysisTimeoutPerFile 3600 > build/logs/analyze.log 2>&1
echo "index";    $G $P -process nfs_flat.bin -noanalysis -scriptPath tools \
    -postScript DecompRange.java build/decomp 0x10000 0xC0100 > build/logs/decomp1.log 2>&1
python3 tools/seeds.py build/nfs_layout.json build/nfs_flat.bin build/decomp/INDEX.txt > build/seeds.txt
echo "seed";     $G $P -process nfs_flat.bin -scriptPath tools -postScript SeedFuncs.java build/seeds.txt > build/logs/seed.log 2>&1
echo "analyse";  $G $P -process nfs_flat.bin -analysisTimeoutPerFile 3600 > build/logs/analyze2.log 2>&1
echo "rename";   $G $P -process nfs_flat.bin -noanalysis -scriptPath tools \
    -postScript RenameFuncs.java tools/names.txt > build/logs/rename.log 2>&1
rm -rf build/decomp && mkdir build/decomp
echo "decompile"; $G $P -process nfs_flat.bin -noanalysis -scriptPath tools \
    -postScript DecompRange.java build/decomp 0x10000 0xC0100 > build/logs/decomp.log 2>&1
echo "done: $(wc -l < build/decomp/INDEX.txt) functions"
