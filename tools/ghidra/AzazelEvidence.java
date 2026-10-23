// Bounded advisory evidence. Run only through tools/ghidra_probe.py.
// @category Azazel6
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import com.google.gson.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class AzazelEvidence extends GhidraScript {
    private String hex(Address a) { return String.format("0x%08X", a.getOffset()); }
    private JsonArray refs(Address address, boolean incoming) {
        JsonArray rows = new JsonArray();
        Iterable<Reference> references = incoming
            ? currentProgram.getReferenceManager().getReferencesTo(address)
            : java.util.Arrays.asList(currentProgram.getReferenceManager().getReferencesFrom(address));
        for (Reference ref : references) {
            if (rows.size() == 256) break;
            JsonObject row = new JsonObject();
            row.addProperty("from", ref.getFromAddress().toString());
            row.addProperty("to", ref.getToAddress().toString());
            row.addProperty("type", ref.getReferenceType().toString());
            rows.add(row);
        }
        return rows;
    }
    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 3 || args.length > 18) throw new IllegalArgumentException("Expected output, hash, 1..16 roots");
        String hash = currentProgram.getExecutableSHA256();
        String language = currentProgram.getLanguageID().toString();
        if (!args[1].equalsIgnoreCase(hash) || !language.equals("Allegrex:LE:32:default"))
            throw new IllegalStateException("Wrong executable or processor");
        JsonObject report = new JsonObject();
        report.addProperty("input_sha256", hash.toLowerCase());
        report.addProperty("language", language);
        report.addProperty("ghidra_version", ghidra.framework.Application.getApplicationVersion());
        report.addProperty("compiler", currentProgram.getCompilerSpec().getCompilerSpecID().toString());
        report.addProperty("limitations", "Advisory static analysis; no global auto-analysis. References capped at 256 per direction/site. VFPU prefixes are not fully modeled.");
        JsonArray functions = new JsonArray();
        DecompInterface decompiler = new DecompInterface();
        try {
            if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Decompiler unavailable");
            for (int index = 2; index < args.length; index++) {
                monitor.checkCancelled();
                Address address = toAddr(Long.parseLong(args[index].substring(2), 16));
                if (getInstructionAt(address) == null) disassemble(address);
                Function function = getFunctionAt(address);
                if (function == null && getFunctionContaining(address) == null) function = createFunction(address, null);
                if (function == null) function = getFunctionContaining(address);
                JsonObject row = new JsonObject();
                row.addProperty("requested", hex(address));
                row.add("incoming", refs(address, true));
                JsonArray instructions = new JsonArray();
                if (function != null) {
                    row.addProperty("entry", hex(function.getEntryPoint()));
                    row.addProperty("name", function.getName());
                    row.addProperty("body_min", hex(function.getBody().getMinAddress()));
                    row.addProperty("body_max", hex(function.getBody().getMaxAddress()));
                    var iterator = currentProgram.getListing().getInstructions(function.getBody(), true);
                    while (iterator.hasNext() && instructions.size() < 4096) {
                        Instruction instruction = iterator.next();
                        JsonObject item = new JsonObject();
                        item.addProperty("address", hex(instruction.getAddress()));
                        item.addProperty("bytes", java.util.HexFormat.of().formatHex(instruction.getBytes()));
                        item.addProperty("text", instruction.toString());
                        item.addProperty("flow", instruction.getFlowType().toString());
                        item.addProperty("delay_slots", instruction.getDelaySlotDepth());
                        item.add("references", refs(instruction.getAddress(), false));
                        instructions.add(item);
                    }
                    row.addProperty("instructions_truncated", iterator.hasNext());
                    var result = decompiler.decompileFunction(function, 15, monitor);
                    if (result.decompileCompleted()) {
                        String code = result.getDecompiledFunction().getC();
                        row.addProperty("pseudocode", code.substring(0, Math.min(code.length(), 65536)));
                        row.addProperty("pseudocode_truncated", code.length() > 65536);
                    } else row.addProperty("decompiler_error", result.getErrorMessage());
                } else row.addProperty("error", "No function recovered; inspect verified machine instructions");
                row.add("instructions", instructions);
                functions.add(row);
            }
        } finally { decompiler.dispose(); }
        report.add("functions", functions);
        Files.writeString(Path.of(args[0]), new GsonBuilder().setPrettyPrinting().create().toJson(report), StandardCharsets.UTF_8);
    }
}
