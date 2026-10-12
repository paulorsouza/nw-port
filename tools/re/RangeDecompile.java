// Cria as funções listadas (uma por linha, 0x...) e grava o C descompilado de cada uma.
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
            if (f == null) { out.println("// sem funcao"); continue; }
            DecompileResults r = d.decompileFunction(f, 60, monitor);
            if (r != null && r.decompileCompleted()) out.println(r.getDecompiledFunction().getC());
            else out.println("// falhou: " + (r == null ? "null" : r.getErrorMessage()));
            if (++n % 200 == 0) { out.flush(); println("progresso " + n + "/" + lines.size()); }
        }
        out.close();
        d.dispose();
        println("fim " + n);
    }
}
