# fill.awk - one figure's publication page and catalog fields, from the template.
#
#	awk -v name=N -v path=P -v fig=F -v z=Z -v trans=T -v out=O -f fill.awk term.html
#
# fig: CODEX<tab>reference<tab>literal per witness; Z.1 .. Z.4: zdiff's cells
# against each zeroth, one line per witness; trans: braille<tab>Aegean per
# witness. Writes the page to stdout and the catalog fields to O.
function put(s, slot, v,	i) {
	while ((i = index(s, slot)) > 0) s = substr(s, 1, i - 1) v substr(s, i + length(slot))
	return s
}
function esc(s) { s = put(s, "&", "&amp;"); s = put(s, "<", "&lt;"); return s }
BEGIN {
	FS = "\t"
	split("ALEPPO ROSETTA BEHISTUN LENINGRAD", CODEX, " ")
	split("YHWH|YAHWEH|GOD IS WORD|GOD OF WORD", ZERO, "|")
	n = 0
	while ((getline line < fig) > 0) { n++; split(line, f, "\t"); cx[n] = f[1]; ref[n] = f[2]; lit[n] = f[3]; held[f[1]] = n }
	for (k = 1; k <= 4; k++) { i = 0; while ((getline line < (z "." k)) > 0) cell[++i, k] = line }
	i = 0; while ((getline line < trans) > 0) { split(line, f, "\t"); br[++i] = f[1]; ae[i] = f[2] }
	wit = ""; flips = ""; src = ""
	for (j = 1; j <= 4; j++) {
		c = CODEX[j]
		if (c in held) {
			i = held[c]
			wit = wit "  { idx: " j - 1 ", name: \"" c "\", src: \"" ref[i] "\", text: \"" lit[i] "\" },\n"
			row = "        <tr><td>[" j - 1 "] " c "</td><td class=\"heb\">" lit[i] "</td>"
			for (k = 1; k <= 4; k++) row = row "<td>" cell[i, k] "</td>"
			flips = flips row "<td>" br[i] "</td><td>" ae[i] "</td></tr>\n"
		} else
			wit = wit "  { idx: " j - 1 ", name: \"" c "\", src: \"absent: the empty base\", text: \"\" },\n"
	}
	for (i = 1; i <= n; i++) {
		if (cx[i] == "LENINGRAD") src = src "    <p>Leningrad: Westminster Leningrad Codex, " ref[i] " (Sefaria's Hebrew numbering), public domain, via Sefaria.</p>\n"
		if (cx[i] == "ALEPPO") src = src "    <p>Aleppo: Miqra according to the Masorah (MAM), based on the Aleppo Codex, he.wikisource via Sefaria, CC BY-SA; " lit[i] " is shared under that licence.</p>\n"
	}
	absent = ""
	for (j = 1; j <= 4; j++) if (!(CODEX[j] in held)) absent = absent (absent == "" ? "" : ", ") CODEX[j]
	src = src "    <p>Accents removed. Not in " absent ": the empty base.</p>"
	for (i = 1; i <= n; i++) {
		print "  \"" cx[i] " at\": \"" ref[i] "\"" > out
		print "  \"" cx[i] " literal\": \"" lit[i] "\"" > out
		for (k = 1; k <= 4; k++) print "  \"" cx[i] " from " ZERO[k] "\": \"" cell[i, k] "\"" > out
		print "  \"" cx[i] " braille\": \"" br[i] "\"" > out
		print "  \"" cx[i] " aegean\": \"" ae[i] "\"" > out
	}
	close(out)
}
{
	if ($0 == "${WITNESSES}") { printf "%s", wit; next }
	if ($0 == "${FLIPS}") { printf "%s", flips; next }
	if ($0 == "${SOURCES}") { print src; next }
	print put(put($0, "${NAME}", esc(name)), "${PATH}", path)
}
