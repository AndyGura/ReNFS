//Name functions from a file of lines "0x<entry> <name>" ('#' lines are skipped).
//The file is the source of truth: a function with another name is renamed, and an address
//where Ghidra has no function (code reached through a table) gets one made there.
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
        int n = 0, kept = 0, made = 0;
        for (String line : Files.readAllLines(Paths.get(getScriptArgs()[0]))) {
            line = line.trim();
            if (line.isEmpty() || line.startsWith("#")) continue;
            String[] p = line.split("\\s+");
            ghidra.program.model.address.Address a = toAddr(Long.decode(p[0]));
            Function f = getFunctionAt(a);
            if (f == null) {
                disassemble(a);
                f = createFunction(a, p[1]);
                if (f == null) { println("no function at " + p[0] + " and none could be made"); continue; }
                made++;
                continue;
            }
            if (f.getName().equals(p[1])) { kept++; continue; }
            f.setName(p[1], SourceType.USER_DEFINED);
            n++;
        }
        println("RENAMED: " + n + ", MADE: " + made + " (" + kept + " already named)");
    }
}
