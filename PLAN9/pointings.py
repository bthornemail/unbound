#!/usr/bin/env python3
"""The Name as pointed at each verse the Leningrad Codex is cited for, read with
witness.py's rules: NFC, accents (U+0591-U+05AF), maqaf and verse marks removed,
letters and vowel marks kept. Every occurrence in the verse, in order, since a verse
can hold the Name more than once (Judges 16:28). Writes in/pointings/YHWH.txt:
CODEX<TAB>reference<TAB>occurrence<TAB>literal. Kept apart from in/figures, whose
zrender holds one witness per codex. Only reads the witnesses; the verse text is
not written, only the Name."""
import json, os, re, unicodedata

W = os.path.expanduser("~/private/codex-witness")
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "in", "pointings")
NAME = "יהוה"

# the six presentations of Wikipedia's Tetragrammaton article (Leningrad Codex), Sefaria numbering
PLACES = [("Genesis", 2, 4), ("Genesis", 3, 14), ("Judges", 16, 28),
          ("Genesis", 15, 2), ("I_Kings", 2, 26), ("Ezekiel", 24, 24)]

def verse(book, ch, v):
    d = json.load(open(f"{W}/leningrad/{book}.{ch}.json"))
    text = d["versions"][0]["text"][v - 1]
    text = re.sub(r"<[^>]+>|&[a-z]+;", "", text)
    text = re.sub("[\u0591-\u05AF\u05BD\u05C0\u05C3\u05C6]", "", unicodedata.normalize("NFC", text))
    return text.replace("־", " ").split()

def consonants(w):
    return "".join(c for c in w if "א" <= c <= "ת")

os.makedirs(OUT, exist_ok=True)
lines = []
for book, ch, v in PLACES:
    hits = [w for w in verse(book, ch, v) if consonants(w) == NAME]
    for k, w in enumerate(hits, 1):
        lines.append(f"LENINGRAD\t{book.replace('_', ' ')} {ch}:{v}\t{k}\t{w}")
open(f"{OUT}/YHWH.txt", "w").write("\n".join(lines) + "\n")
print(*lines, sep="\n")
