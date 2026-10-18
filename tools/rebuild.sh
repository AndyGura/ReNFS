#!/bin/sh
# The Ghidra project and every function decompiled, from the game's DOS program.
#   tools/rebuild.sh              everything (about an hour; logs in build/logs/)
#   tools/rebuild.sh --decomp     only decompile again (after new names in tools/names.txt)
# Set EXE and NAME below once per project. Needs tools/ghidra-ext/install.sh once per
# Ghidra install. Everything it writes goes to build/ (git-ignored, never published).
set -e
cd "$(dirname "$0")/.."
EXE=${EXE:-NFSSE/NFS_DOS.EXE}      # the program, as the player's copy has it
NAME=${NAME:-renfs}             # the Ghidra project's name
FLAT=${FLAT:-nfs}              # build/<FLAT>_flat.bin, build/<FLAT>_layout.json
G=/opt/ghidra/support/analyzeHeadless
P="build/ghidra $NAME"
BIN=${FLAT}_flat.bin
L=build/${FLAT}_layout.json

# from the program's own layout: entry, the code's range, where the BSS begins
lay() { python3 -I -c "
import json; d = json.load(open('$L'))
code = [o for o in d['objects'] if o['flags'] & 4]
data = [o for o in d['objects'] if not o['flags'] & 4]
print({'entry': hex(d['entry_va']),
       'lo': hex(min(o['base'] for o in code)),
       'hi': hex(max(o['base'] + o['vsize'] for o in code)),
       'bss': hex(max(o['base'] + o['npages'] * 0x1000 for o in data))}['$1'])"; }

decompile() {
    rm -rf build/decomp && mkdir -p build/decomp
    DECOMP_BSS=$(lay bss) $G $P -process $BIN -noanalysis -scriptPath tools \
        -postScript DecompRange.java build/decomp $(lay lo) $(lay hi) > build/logs/decomp.log 2>&1
}

if [ "$1" = "--decomp" ]; then
    echo "rename";    $G $P -process $BIN -noanalysis -scriptPath tools \
        -postScript RenameFuncs.java tools/names.txt > build/logs/rename.log 2>&1
    echo "decompile"; decompile
    echo "done: $(wc -l < build/decomp/INDEX.txt) functions"; exit 0
fi

rm -rf build/ghidra build/decomp
mkdir -p build/ghidra build/logs build/decomp
python3 tools/unle.py "$EXE" -o build/$BIN --map $L --fixups
echo "import";    $G $P -import build/$BIN -processor x86:LE:32:watcom -loader BinaryLoader \
    -loader-baseAddr 0x0 -noanalysis -scriptPath tools -postScript CreateEntry.java $(lay entry) \
    > build/logs/import.log 2>&1
echo "analyse";   $G $P -process $BIN -analysisTimeoutPerFile 3600 > build/logs/analyze.log 2>&1
echo "index";     DECOMP_BSS=$(lay bss) $G $P -process $BIN -noanalysis -scriptPath tools \
    -postScript DecompRange.java build/decomp $(lay lo) $(lay hi) > build/logs/decomp1.log 2>&1
python3 tools/seeds.py $L build/$BIN build/decomp/INDEX.txt > build/seeds.txt
echo "seed";      $G $P -process $BIN -scriptPath tools -postScript SeedFuncs.java build/seeds.txt \
    > build/logs/seed.log 2>&1
echo "analyse";   $G $P -process $BIN -analysisTimeoutPerFile 3600 > build/logs/analyze2.log 2>&1
[ -f tools/names.txt ] || touch tools/names.txt
echo "rename";    $G $P -process $BIN -noanalysis -scriptPath tools \
    -postScript RenameFuncs.java tools/names.txt > build/logs/rename.log 2>&1
echo "decompile"; decompile
echo "done: $(wc -l < build/decomp/INDEX.txt) functions"
