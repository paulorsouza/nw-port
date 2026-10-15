// Creates the listed functions (one per line, 0x...) and writes the decompiled C of each one.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;

public class RangeDecompile extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        List<String> lines = Files.readAllLines(Paths.get(a[0]));
        PrintWriter out = new PrintWriter(new BufferedWriter(new FileWriter(a[1])));
        DecompInterface d = new DecompInterface();
        d.toggleCCode(true);
        d.openProgram(currentProgram);
        int n = 0;
        for (String s : lines) {
            if (monitor.isCancelled()) break;
            s = s.trim();
            if (s.isEmpty()) continue;
            Address addr = toAddr(Long.decode(s));
            Function f = getFunctionAt(addr);
            if (f == null) {
                if (getInstructionAt(addr) == null) new DisassembleCommand(addr, null, true).applyTo(currentProgram, monitor);
                f = createFunction(addr, null);
            }
            out.println("//==== " + s);
            if (f == null) { out.println("// no function"); continue; }
            DecompileResults r = d.decompileFunction(f, 60, monitor);
            if (r != null && r.decompileCompleted()) out.println(r.getDecompiledFunction().getC());
            else out.println("// failed: " + (r == null ? "null" : r.getErrorMessage()));
            if (++n % 200 == 0) { out.flush(); println("progress " + n + "/" + lines.size()); }
        }
        out.close();
        d.dispose();
        println("done " + n);
    }
}
