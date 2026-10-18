#!/usr/bin/env python3
"""Build docs/book/YLang-Book.pdf from maintained YLang documentation."""
from pathlib import Path
import re
import sys
from xml.sax.saxutils import escape
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, PageBreak, Preformatted, Table, TableStyle, ListFlowable, ListItem, HRFlowable
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = Path(__file__).resolve().parents[2]
PDF = ROOT / "docs/book/YLang-Book.pdf"
CHAPTERS = [
    ("Part I — Start Here", "docs/book/source/part-1-start-here.md"),
    ("Part II — Language Foundations", "docs/book/source/part-2-language-basics.md"),
    ("Part III — Functions and Standard Library", "docs/book/source/part-3-functions-library.md"),
    ("Part IV — Errors and Extensions", "docs/book/source/part-4-errors-extensions.md"),
    ("Part V — Extensions and Reference", "docs/book/source/part-5-ecosystem-reference.md"),
]
NAVY, TEAL, INK = colors.HexColor("#17324D"), colors.HexColor("#167D8D"), colors.HexColor("#202B36")
MUTED, PALE, RULE = colors.HexColor("#607080"), colors.HexColor("#F3F6F8"), colors.HexColor("#D5DEE6")


def inline(text):
    pat = re.compile(r"(\x60[^\x60]+\x60|\*\*.+?\*\*|\*[^*]+\*|\[[^\]]+\]\([^)]+\))")
    out, pos = [], 0
    for m in pat.finditer(text):
        out.append(escape(text[pos:m.start()])); raw = m.group(0)
        if raw.startswith(chr(96)):
            out.append(f'<font name="Courier" color="{TEAL.hexval()}">{escape(raw[1:-1])}</font>')
        elif raw.startswith("**"):
            out.append(f"<b>{escape(raw[2:-2])}</b>")
        elif raw.startswith("*"):
            out.append(f"<i>{escape(raw[1:-1])}</i>")
        else:
            mlink = re.match(r"\[([^\]]+)\]\(([^)]+)\)", raw)
            label, target = mlink.groups()
            out.append(f'<link href="{escape(target)}" color="{TEAL.hexval()}">{escape(label)}</link>' if target.startswith(("https://", "http://")) else f"{escape(label)} ({escape(target)})")
        pos = m.end()
    out.append(escape(text[pos:])); return "".join(out)


class BookDoc(BaseDocTemplate):
    def __init__(self, path, **kw):
        super().__init__(path, **kw); self._chapter_seq = 0
        frame = Frame(self.leftMargin, self.bottomMargin, self.width, self.height, id="main", leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        self.addPageTemplates([PageTemplate(id="book", frames=frame, onPage=self.decorate)])

    def beforeDocument(self):
        self._chapter_seq = 0

    def decorate(self, canvas, doc):
        canvas.saveState()
        if doc.page > 1:
            w, h = A4; canvas.setStrokeColor(RULE); canvas.setLineWidth(.5)
            canvas.line(20*mm, h-15*mm, w-20*mm, h-15*mm); canvas.line(20*mm, 15*mm, w-20*mm, 15*mm)
            canvas.setFont("Helvetica", 8); canvas.setFillColor(MUTED)
            canvas.drawString(20*mm, h-11.5*mm, "YLang 2.0.0 - Standalone Book")
            canvas.drawRightString(w-20*mm, 10*mm, str(doc.page))
        canvas.restoreState()

    def afterFlowable(self, item):
        if isinstance(item, Paragraph) and item.style.name == "Chapter":
            title = item.getPlainText(); key = f"chapter-{self._chapter_seq}"; self._chapter_seq += 1
            self.canv.bookmarkPage(key); self.canv.addOutlineEntry(title, key, 0, False)
            self.notify("TOCEntry", (0, title, self.page, key))


def styles():
    s = getSampleStyleSheet()
    s.add(ParagraphStyle(name="CoverTitle", fontName="Helvetica-Bold", fontSize=30, leading=36, alignment=TA_CENTER, textColor=NAVY, spaceAfter=8*mm))
    s.add(ParagraphStyle(name="CoverSub", fontName="Helvetica", fontSize=12, leading=18, alignment=TA_CENTER, textColor=MUTED, spaceAfter=6*mm))
    s.add(ParagraphStyle(name="CoverVersion", fontName="Helvetica-Bold", fontSize=10, leading=14, alignment=TA_CENTER, textColor=TEAL, spaceBefore=8*mm))
    s.add(ParagraphStyle(name="Chapter", fontName="Helvetica-Bold", fontSize=18, leading=23, textColor=NAVY, spaceAfter=6*mm, keepWithNext=True))
    s.add(ParagraphStyle(name="Section", fontName="Helvetica-Bold", fontSize=12, leading=16, textColor=TEAL, spaceBefore=4*mm, spaceAfter=2*mm, keepWithNext=True))
    s.add(ParagraphStyle(name="SubSection", fontName="Helvetica-Bold", fontSize=10, leading=13, textColor=NAVY, spaceBefore=3*mm, spaceAfter=2*mm, keepWithNext=True))
    s.add(ParagraphStyle(name="Body", fontName="Helvetica", fontSize=8.8, leading=12.6, textColor=INK, spaceAfter=2.4*mm, allowWidows=0, allowOrphans=0))
    s.add(ParagraphStyle(name="BulletText", parent=s["Body"], leftIndent=3*mm, spaceAfter=1*mm))
    s.add(ParagraphStyle(name="Source", fontName="Helvetica-Oblique", fontSize=7.5, leading=10, textColor=MUTED, spaceAfter=4*mm))
    s.add(ParagraphStyle(name="BookCodeStyle", fontName="Courier", fontSize=7.2, leading=9.4, textColor=INK))
    s.add(ParagraphStyle(name="ContentsTitle", fontName="Helvetica-Bold", fontSize=22, leading=27, textColor=NAVY, spaceAfter=7*mm))
    s.add(ParagraphStyle(name="TOC0", fontName="Helvetica-Bold", fontSize=8, leading=10, leftIndent=0, textColor=NAVY, spaceBefore=0.4*mm))
    s.add(ParagraphStyle(name="Cell", fontName="Helvetica", fontSize=7.2, leading=9, textColor=INK))
    s.add(ParagraphStyle(name="CellHead", fontName="Helvetica-Bold", fontSize=7.2, leading=9, textColor=colors.white))
    return s


def codebox(text, s):
    t = Table([[Preformatted(text.rstrip(), s["BookCodeStyle"], maxLineLength=106, splitChars=" \t")]], colWidths=[A4[0]-40*mm])
    t.setStyle(TableStyle([("BACKGROUND",(0,0),(-1,-1),PALE),("BOX",(0,0),(-1,-1),.5,RULE),("LEFTPADDING",(0,0),(-1,-1),3*mm),("RIGHTPADDING",(0,0),(-1,-1),3*mm),("TOPPADDING",(0,0),(-1,-1),2*mm),("BOTTOMPADDING",(0,0),(-1,-1),2*mm),("VALIGN",(0,0),(-1,-1),"TOP")]))
    return t


def parse_md(text, s):
    lines=text.splitlines(); out=[]; code=[]; in_code=False; bullets=[]; i=0
    def flush():
        nonlocal bullets
        if bullets:
            elems=[ListItem(Paragraph(inline(txt),s["BulletText"]),leftIndent=3*mm) for txt,_ in bullets]
            num=bullets[0][1]
            out.append(ListFlowable(elems,bulletType="1" if num else "bullet",leftIndent=5*mm,bulletFontName="Helvetica",bulletFontSize=8,bulletColor=TEAL,spaceAfter=2*mm)); bullets=[]
    while i<len(lines):
        line=lines[i]; z=line.strip()
        if z.startswith(chr(96) * 3):
            flush()
            if not in_code: code=[]; in_code=True
            else: out.extend([Spacer(1,1.5*mm),codebox("\n".join(code),s),Spacer(1,2.5*mm)]); code=[]; in_code=False
            i+=1; continue
        if in_code: code.append(line); i+=1; continue
        if not z: flush(); i+=1; continue
        if z.startswith("|") and i+1<len(lines) and re.match(r"\s*\|?\s*:?-{2,}",lines[i+1]):
            flush(); rows=[]
            while i<len(lines) and lines[i].strip().startswith("|"):
                row=[c.strip() for c in lines[i].strip().strip("|").split("|")]
                if not all(re.fullmatch(r":?-{2,}:?",c.replace(" ","")) for c in row): rows.append(row)
                i+=1
            if rows:
                n=max(map(len,rows)); data=[]
                for ri,row in enumerate(rows):
                    row += [""]*(n-len(row)); st=s["CellHead"] if ri==0 else s["Cell"]
                    data.append([Paragraph(inline(c),st) for c in row])
                t=Table(data,colWidths=[(A4[0]-40*mm)/n]*n,repeatRows=1)
                t.setStyle(TableStyle([("BACKGROUND",(0,0),(-1,0),NAVY),("GRID",(0,0),(-1,-1),.4,RULE),("VALIGN",(0,0),(-1,-1),"TOP"),("LEFTPADDING",(0,0),(-1,-1),1.5*mm),("RIGHTPADDING",(0,0),(-1,-1),1.5*mm),("TOPPADDING",(0,0),(-1,-1),1.2*mm),("BOTTOMPADDING",(0,0),(-1,-1),1.2*mm)]))
                out.extend([t,Spacer(1,3*mm)])
            continue
        if z.startswith("# Chapter "):
            flush()
            if out: out.append(PageBreak())
            out.append(Paragraph(inline(z[2:]),s["Chapter"]))
        elif z.startswith("## Chapter "):
            flush()
            if out: out.append(PageBreak())
            out.append(Paragraph(inline(z[3:]),s["Chapter"]))
        elif z.startswith("# "): flush(); out.append(Paragraph(inline(z[2:]),s["Section"]))
        elif z.startswith("## "): flush(); out.append(Paragraph(inline(z[3:]),s["Section"]))
        elif z.startswith("### "): flush(); out.append(Paragraph(inline(z[4:]),s["SubSection"]))
        elif z.startswith("#### "): flush(); out.append(Paragraph(inline(z[5:]),s["SubSection"]))
        elif z.startswith("---"): flush(); out.append(HRFlowable(width="100%",thickness=.5,color=RULE,spaceBefore=2*mm,spaceAfter=3*mm))
        elif z.startswith("- ") or re.match(r"^\d+\.\s",z):
            num=bool(re.match(r"^\d+\.\s",z)); bullets.append((re.sub(r"^\d+\.\s+","",z) if num else z[2:],num))
        elif z.startswith("!"): flush(); out.append(Paragraph("[Illustration omitted from the standalone PDF edition.]",s["Source"]))
        else: flush(); out.append(Paragraph(inline(z),s["Body"]))
        i+=1
    flush()
    if in_code and code: out.append(codebox("\n".join(code),s))
    return out


def main():
    missing=[p for _,p in CHAPTERS if not (ROOT/p).is_file()]
    if missing: print("Missing documentation files:\n"+"\n".join(missing),file=sys.stderr); return 2
    s=styles(); doc=BookDoc(str(PDF),pagesize=A4,leftMargin=20*mm,rightMargin=20*mm,topMargin=22*mm,bottomMargin=22*mm,title="YLang 2.0.0 - Standalone Book",author="YLang Project",subject="Stable core and experimental extensions")
    story=[Spacer(1,30*mm),Paragraph("THE YLANG BOOK",s["CoverTitle"]),Paragraph("A detailed beginner-friendly guide to YLang 2.0.0",s["CoverSub"]),Spacer(1,8*mm),HRFlowable(width="34%",hAlign="CENTER",thickness=2,color=TEAL,spaceAfter=8*mm),Paragraph("Stable core language, step-by-step examples, standard library, troubleshooting, and experimental extensions",s["CoverSub"]),Spacer(1,32*mm),Paragraph("VERSION 2.0.0 STABLE",s["CoverVersion"]),Paragraph("Standalone book edition - detailed chapters maintained with the source repository",s["CoverSub"]),Spacer(1,15*mm),Paragraph("The default compiler generates C and uses GCC or Clang. Experimental extensions are labelled separately and do not share the stable-core compatibility guarantee.",s["Body"]),PageBreak(),Paragraph("Contents",s["ContentsTitle"])]
    toc=TableOfContents(); toc.levelStyles=[s["TOC0"]]; toc.dotsMinLevel=0; story.extend([toc,PageBreak()])
    for title,rel in CHAPTERS:
        body=(ROOT/rel).read_text(encoding="utf-8")
        body=re.sub(r"^\s*#\s+[^\n]*\n+","",body,count=1)
        body=re.sub(r"^---\s*\n.*?\n---\s*\n","",body,count=1,flags=re.S)
        story.append(Paragraph(escape(title),s["Section"]))
        story.extend(parse_md(body,s)); story.append(PageBreak())
    if isinstance(story[-1],PageBreak): story.pop()
    doc.multiBuild(story)
    print(f"Wrote {PDF} ({PDF.stat().st_size} bytes)")
    return 0

if __name__ == "__main__": raise SystemExit(main())
