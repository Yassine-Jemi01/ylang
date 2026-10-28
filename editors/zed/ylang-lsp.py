#!/usr/bin/env python3
"""YLang Language Server Protocol server over stdio."""
import json, os, re, shutil, subprocess, sys, tempfile
from pathlib import Path
from urllib.parse import urlparse, unquote

DOCS = {}
KEYWORDS = "function return let const mut if else loop for in while break continue struct enum import as true false and or not null print".split()
TYPES = "int float double bool char string void".split()

def send(obj):
    data = json.dumps(obj, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    sys.stdout.buffer.write(("Content-Length: %d\r\n\r\n" % len(data)).encode() + data)
    sys.stdout.buffer.flush()

def respond(i, result):
    send({"jsonrpc":"2.0","id":i,"result":result})

def notify(method, params):
    send({"jsonrpc":"2.0","method":method,"params":params})

def read_message():
    headers = {}
    while True:
        line = sys.stdin.buffer.readline()
        if not line: return None
        if line in (b"\r\n", b"\n"): break
        k, sep, v = line.decode("ascii", "replace").partition(":")
        if sep: headers[k.lower()] = v.strip()
    n = int(headers.get("content-length", "0"))
    if not n: return None
    try: return json.loads(sys.stdin.buffer.read(n).decode("utf-8"))
    except (ValueError, UnicodeDecodeError): return None

def file_path(uri):
    p = urlparse(uri)
    return Path(unquote(p.path)) if p.scheme == "file" else None

def symbols(source):
    out = []
    patterns = [
        (r"\bfunction\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*(?:->\s*([A-Za-z_]\w*(?:\[\])?))?", "function"),
        (r"\bstruct\s+([A-Za-z_]\w*)", "struct"),
        (r"\benum\s+([A-Za-z_]\w*)", "enum"),
        (r"\blet\s+(?:(?:const|mut)\s+)?([A-Za-z_]\w*)\s+([A-Za-z_]\w*)", "typed variable"),
        (r"\blet\s+([A-Za-z_]\w*)\s*=", "variable"),
    ]
    for pattern, kind in patterns:
        for m in re.finditer(pattern, source):
            if kind == "function":
                name, detail = m.group(1), "function(" + (m.group(2) or "").strip() + ") -> " + (m.group(3) or "unknown")
            elif kind == "typed variable":
                name, detail = m.group(2), "variable: " + m.group(1)
            else:
                name, detail = m.group(1), kind
            offset = source.find(name, m.start(), m.end())
            prefix = source[:offset]
            out.append({"name":name, "detail":detail, "kind":kind,
                        "line":prefix.count("\n"), "character":len(prefix.rsplit("\n",1)[-1])})
    return sorted(out, key=lambda x:(x["line"],x["character"]))

def word_at(source, line, col):
    lines = source.splitlines()
    if line >= len(lines): return ""
    row = lines[line]; col = min(col, len(row)); a = b = col
    while a and (row[a-1].isalnum() or row[a-1] == "_"): a -= 1
    while b < len(row) and (row[b].isalnum() or row[b] == "_"): b += 1
    return row[a:b]

def compiler():
    configured = os.environ.get("YLANG_COMPILER")
    return configured or shutil.which("ylang") or str(Path(__file__).resolve().parents[2] / "build" / "ylang")

def publish_diagnostics(uri, source):
    exe = compiler(); path = file_path(uri); temp = None
    result = []
    try:
        if not Path(exe).exists() and not shutil.which(exe):
            notify("textDocument/publishDiagnostics", {"uri":uri,"diagnostics":[]}); return
        if path and path.exists():
            check_path = path
        else:
            parent = path.parent if path else Path.cwd()
            try: fd, temp = tempfile.mkstemp(prefix=".ylang-lsp-", suffix=".yl", dir=str(parent))
            except OSError: fd, temp = tempfile.mkstemp(prefix="ylang-lsp-", suffix=".yl")
            with os.fdopen(fd, "w", encoding="utf-8") as f: f.write(source)
            check_path = Path(temp)
        p = subprocess.run([exe,"check",str(check_path)], capture_output=True, text=True, timeout=8)
        output = p.stderr + "\n" + p.stdout
        blocks = re.split(r"(?m)(?=^(?:error|warning)\[)", output)
        for block in blocks:
            h = re.search(r"(?m)^(error|warning)\[([^\]]+)\]:\s*(.+)$", block)
            loc = re.search(r"(?m)^\s*-->\s+(.+?):(\d+):(\d+)\s*$", block)
            if not h or not loc: continue
            if Path(loc.group(1)).name != check_path.name and str(check_path) not in loc.group(1): continue
            line, col = max(0,int(loc.group(2))-1), max(0,int(loc.group(3))-1)
            result.append({"range":{"start":{"line":line,"character":col},"end":{"line":line,"character":col+1}},
                           "severity":1 if h.group(1)=="error" else 2,"code":h.group(2),
                           "source":"ylang","message":h.group(3).strip()})
    except (OSError, subprocess.SubprocessError):
        pass
    finally:
        if temp:
            try: os.unlink(temp)
            except OSError: pass
    notify("textDocument/publishDiagnostics", {"uri":uri,"diagnostics":result})

def handle(m):
    method, p, i = m.get("method"), m.get("params") or {}, m.get("id")
    if method == "initialize":
        respond(i, {"capabilities":{"textDocumentSync":{"openClose":True,"change":1,"save":{}},
                 "completionProvider":{"triggerCharacters":[".",":"]},"hoverProvider":True,
                 "definitionProvider":True,"documentSymbolProvider":True},
                 "serverInfo":{"name":"ylang-lsp","version":"0.1.0"}})
    elif method == "shutdown": respond(i, None)
    elif method == "exit": raise SystemExit
    elif method == "textDocument/didOpen":
        d=p.get("textDocument",{}); uri=d.get("uri",""); DOCS[uri]=d.get("text",""); publish_diagnostics(uri,DOCS[uri])
    elif method == "textDocument/didChange":
        uri=p.get("textDocument",{}).get("uri",""); changes=p.get("contentChanges",[])
        if changes and uri in DOCS: DOCS[uri]=changes[-1].get("text",DOCS[uri]); publish_diagnostics(uri,DOCS[uri])
    elif method == "textDocument/didSave":
        uri=p.get("textDocument",{}).get("uri","")
        if uri in DOCS: publish_diagnostics(uri,DOCS[uri])
    elif method == "textDocument/didClose":
        uri=p.get("textDocument",{}).get("uri",""); DOCS.pop(uri,None)
        notify("textDocument/publishDiagnostics",{"uri":uri,"diagnostics":[]})
    elif method == "textDocument/completion":
        uri=p.get("textDocument",{}).get("uri",""); source=DOCS.get(uri,"")
        items=[{"label":x,"kind":14,"detail":"YLang keyword"} for x in KEYWORDS]
        items += [{"label":x,"kind":25,"detail":"Built-in type"} for x in TYPES]
        items += [{"label":x["name"],"kind":3,"detail":x["detail"]} for x in symbols(source)]
        respond(i,{"isIncomplete":False,"items":items})
    elif method in ("textDocument/hover","textDocument/definition"):
        uri=p.get("textDocument",{}).get("uri",""); source=DOCS.get(uri,"")
        pos=p.get("position",{}); word=word_at(source,pos.get("line",0),pos.get("character",0))
        found=next((x for x in symbols(source) if x["name"]==word),None)
        if method.endswith("hover"):
            value={"contents":{"kind":"markdown","value":found["detail"]+" "+found["name"]}} if found else ({"contents":{"kind":"markdown","value":"Built-in YLang symbol: "+word}} if word in KEYWORDS+TYPES else None)
            respond(i,value)
        elif found:
            l,c=found["line"],found["character"]
            respond(i,{"uri":uri,"range":{"start":{"line":l,"character":c},"end":{"line":l,"character":c+len(word)}}})
        else: respond(i,None)
    elif method == "textDocument/documentSymbol":
        uri=p.get("textDocument",{}).get("uri",""); result=[]
        for x in symbols(DOCS.get(uri,"")):
            l,c,n=x["line"],x["character"],x["name"]; k=12 if x["kind"]=="function" else 23 if x["kind"]=="struct" else 10 if x["kind"]=="enum" else 13
            r={"start":{"line":l,"character":c},"end":{"line":l,"character":c+len(n)}}
            result.append({"name":n,"detail":x["detail"],"kind":k,"range":r,"selectionRange":r})
        respond(i,result)
    elif i is not None: respond(i,None)

def main():
    while True:
        m=read_message()
        if m is None: break
        try: handle(m)
        except SystemExit: break
        except Exception as e:
            if "id" in m: send({"jsonrpc":"2.0","id":m["id"],"error":{"code":-32603,"message":str(e)}})
if __name__ == "__main__": main()
