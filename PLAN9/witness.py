#!/usr/bin/env python3
"""Each catalogued figure, as written in each held witness: its words, letters and
vowel marks kept, accents (U+0591-U+05AF), maqaf and verse marks removed.
Writes in/figures/<FIGURE>.txt: one line per witness, CODEX<TAB>reference<TAB>literal,
and <FIGURE>.path and <FIGURE>.name: its _CATALOG path and the catalog's own name for it
(the NAME production of its entry, _CATALOG/<path>/<last>.wsn).
Only reads the witnesses; Plan 9 does the rest."""
import json, os, re, sys, unicodedata

W = os.path.expanduser("~/private/codex-witness")
CATALOG = "/home/main/Programs/ZIGUAROTH/_CATALOG"
PATHS = {"YHWH": "YHWH", "ELOHIM": "ELOHIM", "YHWHELOHIM": "YHWH/ELOHIM"}
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "in", "figures")

# figure: its consonants, and where each witness holds it (Sefaria numbering)
FIGURES = {
    "YHWH": ("יהוה", [("ALEPPO", "aleppo-mam", "Deuteronomy", 29, 1), ("LENINGRAD", "leningrad", "Deuteronomy", 29, 1)]),
    "ELOHIM": ("אלהים", [("LENINGRAD", "leningrad", "Genesis", 1, 1)]),
    "YHWHELOHIM": ("יהוה אלהים", [("LENINGRAD", "leningrad", "Genesis", 2, 4)]),
}

def verse(dirname, book, ch, v):
    d = json.load(open(f"{W}/{dirname}/{book}.{ch}.json"))
    text = d["versions"][0]["text"][v - 1]
    text = re.sub(r"<[^>]+>|&[a-z]+;", "", text)
    text = re.sub(r"[֑-ֽ֯׀׃׆]", "", unicodedata.normalize("NFC", text))
    return text.replace("־", " ").split()

def consonants(w):
    return "".join(c for c in w if "א" <= c <= "ת")

for name, (cons, places) in FIGURES.items():
    n = len(cons.split())
    lines = []
    for codex, dirname, book, ch, v in places:
        words = verse(dirname, book, ch, v)
        hit = next(i for i in range(len(words) - n + 1)
                   if " ".join(consonants(w) for w in words[i:i + n]) == cons)
        lines.append(f"{codex}\t{book} {ch}:{v}\t{' '.join(words[hit:hit + n])}")
    open(f"{OUT}/{name}.txt", "w").write("\n".join(lines) + "\n")
    path = PATHS[name]
    entry = open(f"{CATALOG}/{path}/{path.split('/')[-1].lower()}.wsn", encoding="utf-8").read()
    open(f"{OUT}/{name}.path", "w").write(path + "\n")
    open(f"{OUT}/{name}.name", "w").write(re.search(r'^NAME = "((?:[^"]|"")*)" \.$', entry, re.M).group(1).replace('""', '"') + "\n")
    print(name, *lines, sep="\n  ")
