// Casimir in JavaScript: the WebAssembly module (casim.js and casim.wasm, built by scripts/wasm.sh) behind one
// function per operation. Each returns {status, out}: status is a name ("OK", "SYNTAX", "IMPROPER" ...; docs/language.md
// says what each means for each operation) and out is the answer's text, or what the status reports.
//
//   import { loadCasimir } from "./casim.mjs";
//   const cas = await loadCasimir();
//   cas.derive("X^2×SIN(X)");                 // {status: "OK", out: "2×X×SIN(X)+X^2×COS(X)"}
//   cas.defint("SIN(X)", "0", "π");           // {status: "OK", out: "2"}
//
// opts, every one optional: var ("X"; a letter A-Z), angle ("RAD", "DEG" or "GRAD"), model ("35", "33" or "STU",
// how powers chain), replModel (subst's replacement, pdiv's and pgcd's second polynomial; default model), arena (bytes
// of working memory, default 65536; the calculator's is 8800), cap (bytes of answer with its NUL, default 65536; the
// calculator's equation slot is 192).
import createCasimirModule from "./casim.js";

export const STATUSES = ["OK", "SYNTAX", "NOT_DIFFERENTIABLE", "TOO_LONG", "BAD_ARG", "NO_ROOM", "NOT_POLYNOMIAL",
    "NOT_NUMERIC", "NOT_INTEGRABLE", "PARTIAL", "UNDEFINED", "IMPROPER", "UNDECIDED", "NO_CLOSED_FORM"];
export const OPS = ["derive", "simplify", "expand", "collect", "subst", "solve", "integrate", "factor", "defint",
    "pdiv", "pgcd", "cancel", "partfrac", "taylor", "limit", "sum"];
const ANGLES = { DEG: 0, RAD: 1, GRAD: 2 };
const MODELS = { STU: 1, 33: 33, 35: 35 };

function letter(v) {
    if (typeof v !== "string" || !/^[A-Z]$/.test(v)) throw new TypeError(`casimir: var must be a letter A-Z, not ${v}`);
    return v.charCodeAt(0) - 65;
}

function pick(table, key, what) {
    const k = String(key);
    if (!(k in table)) throw new TypeError(`casimir: ${what} must be one of ${Object.keys(table).join(", ")}, not ${key}`);
    return table[k];
}

export async function loadCasimir(moduleOptions = {}) {
    const m = await createCasimirModule(moduleOptions);

    // The one call every operation makes: casim_run with the request's fields, in casim.h's order.
    function call(op, text, opts = {}, repl = null, repl2 = null) {
        if (typeof text !== "string") throw new TypeError("casimir: the text must be a string");
        const model = pick(MODELS, opts.model ?? "35", "model");
        const st = m.ccall("casimjs_run", "number",
            ["number", "string", "number", "number", "string", "number", "number", "string", "number", "number"],
            [OPS.indexOf(op), text, model, letter(opts.var ?? "X"), repl,
             opts.replModel === undefined ? model : pick(MODELS, opts.replModel, "replModel"),
             pick(ANGLES, opts.angle ?? "RAD", "angle"), repl2, opts.arena ?? 65536, opts.cap ?? 65536]);
        return { status: STATUSES[st] ?? String(st), out: m.UTF8ToString(m.ccall("casimjs_out", "number", [], [])) };
    }

    return {
        version: m.UTF8ToString(m.ccall("casimjs_version", "number", [], [])),
        run: (op, text, opts, repl, repl2) => call(op, text, opts, repl, repl2),
        derive: (text, opts) => call("derive", text, opts),
        simplify: (text, opts) => call("simplify", text, opts),
        expand: (text, opts) => call("expand", text, opts),
        collect: (text, opts) => call("collect", text, opts),
        subst: (text, repl, opts) => call("subst", text, opts, repl),
        solve: (text, opts) => call("solve", text, opts),
        integrate: (text, opts) => call("integrate", text, opts),
        factor: (text, opts) => call("factor", text, opts),
        defint: (text, lo, hi, opts) => call("defint", text, opts, lo, hi),
        pdiv: (f, g, opts) => call("pdiv", f, opts, g),
        pgcd: (f, g, opts) => call("pgcd", f, opts, g),
        cancel: (text, opts) => call("cancel", text, opts),
        partfrac: (text, opts) => call("partfrac", text, opts),
        taylor: (text, point, n, opts) => {     // CAS 014: about point (a text, as defint's bounds), to order n (0-12)
            if (!Number.isInteger(n)) throw new TypeError(`casimir: the order must be a whole number, not ${n}`);
            return call("taylor", text, opts, String(point), String(n));
        },
        // CAS 015: as var goes to point (a text as defint's bounds, or "∞", "-∞"), from side "+", "-" or "" (both)
        limit: (text, point, side, opts) => call("limit", text, opts, String(point), side ?? ""),
        // CAS 018: for var (default "X"; a sum's is often "K") from lo to hi (texts as defint's bounds; hi may be "∞")
        sum: (text, lo, hi, opts) => call("sum", text, opts, String(lo), String(hi)),
    };
}
