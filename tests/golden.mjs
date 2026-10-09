// The golden corpus through the JavaScript wrapper and the WebAssembly module (CAS 011): every line's status and
// text, byte for byte, as tests/golden_check.c checks them natively. node tests/golden.mjs <build dir> <corpus>
import { readFileSync } from "node:fs";
import { pathToFileURL } from "node:url";
import { resolve } from "node:path";

const [dir, corpus] = process.argv.slice(2);
const { loadCasimir, OPS } = await import(pathToFileURL(resolve(dir, "casim.mjs")).href);
const cas = await loadCasimir();
const MODEL = { 1: "STU", 33: "33", 35: "35" };
const ANGLE = ["DEG", "RAD", "GRAD"];
const un = (s) => (s === "\\N" ? null : s.replace(/\\(.)/g, (_, c) => (c === "t" ? "\t" : c === "n" ? "\n" : c)));
let n = 0, failed = 0;
for (const line of readFileSync(corpus, "utf8").split("\n")) {
    if (!line || line.startsWith("#")) continue;
    const [op, model, v, angle, rm, arena, cap, eqn, repl, repl2, status, out] = line.split("\t");
    if (!OPS.includes(op)) throw new Error(`malformed line: ${line}`);
    const got = cas.run(op, un(eqn), { model: MODEL[model], var: String.fromCharCode(65 + Number(v)),
        angle: ANGLE[Number(angle)], replModel: MODEL[rm], arena: Number(arena), cap: Number(cap) }, un(repl), un(repl2));
    n++;
    if (got.status !== status || got.out !== un(out)) {
        if (++failed <= 10) console.error(`FAIL ${op} ${eqn}\n  want ${status} ${out}\n  got  ${got.status} ${got.out}`);
    }
}
console.log(`golden (wasm): ${corpus}, Casimir ${cas.version}\nchecks: ${n} failed: ${failed}`);
process.exit(failed || !n ? 1 : 0);
