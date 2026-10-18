//Name functions from a file of lines "0x<entry> <name>" ('#' lines are skipped).
//Only functions still named FUN_... are renamed, so names given by hand stay.
//usage: -postScript RenameFuncs.java build/names.txt
//@category ReNFS

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.Files;
import java.nio.file.Paths;

public class RenameFuncs extends GhidraScript {
    @Override
    public void run() throws Exception {
        int n = 0, kept = 0;
        for (String line : Files.readAllLines(Paths.get(getScriptArgs()[0]))) {
            line = line.trim();
            if (line.isEmpty() || line.startsWith("#")) continue;
            String[] p = line.split("\\s+");
            Function f = getFunctionAt(toAddr(Long.decode(p[0])));
            if (f == null) { println("no function at " + p[0]); continue; }
            if (!f.getName().startsWith("FUN_")) { kept++; continue; }
            f.setName(p[1], SourceType.USER_DEFINED);
            n++;
        }
        println("RENAMED: " + n + " (" + kept + " already named)");
    }
}
