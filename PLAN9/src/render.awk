# render.awk - one term, rendered by codex index, in Wirth syntax notation.
#
#	awk -v name=N -v fig=F -v z=Z -v n=X -v trans=T -v codices=C -v codex=D -f render.awk
#
# fig: CODEX<tab>reference<tab>literal per witness that holds the term; Z.k and
# X.k: XOR and XNOR cells against zeroth k, one line per witness; trans:
# braille<tab>Aegean per witness. C gets the term over _CODICES[0..3], each held
# literal with its XOR and XNOR against each zeroth; D gets the same indices in
# braille and Aegean. A codex that lacks the term is optional whitespace,
# OPTWHITESPACE = { " " } . as in BNF written in WSN: the empty base, in its index's place.
function id(s) { gsub(/[^A-Za-z]/, "", s); return toupper(s) }
function prod(file, p, v) { gsub(/"/, "\"\"", v); print p " = \"" v "\" ." > file }
BEGIN {
	split("ALEPPO ROSETTA BEHISTUN LENINGRAD", CODEX, " ")
	split("YHWH|YAHWEH|GOD IS WORD|GOD OF WORD", ZERO, "|")
	while ((getline line < fig) > 0) { k++; split(line, f, "\t"); held[f[1]] = k; ref[k] = f[2]; lit[k] = f[3] }
	for (j = 1; j <= 4; j++) {
		i = 0; while ((getline line < (z "." j)) > 0) x[++i, j] = line
		i = 0; while ((getline line < (n "." j)) > 0) xn[++i, j] = line
	}
	i = 0; while ((getline line < trans) > 0) { split(line, f, "\t"); br[++i] = f[1]; ae[i] = f[2] }
	top = id(name) " ="
	for (j = 1; j <= 4; j++) { top = top ((CODEX[j] in held) ? " " CODEX[j] : " OPTWHITESPACE"); if (!(CODEX[j] in held)) ws = 1 }
	print top " ." > codices
	print top " ." > codex
	if (ws) { print "OPTWHITESPACE = { \" \" } ." > codices; print "OPTWHITESPACE = { \" \" } ." > codex }
	for (j = 1; j <= 4; j++) {
		c = CODEX[j]
		if (!(c in held)) continue
		i = held[c]
		seq = c " = " c "LITERAL " c "AT"
		for (k = 1; k <= 4; k++) seq = seq " " c "XOR" id(ZERO[k]) " " c "XNOR" id(ZERO[k])
		print seq " ." > codices
		prod(codices, c "LITERAL", lit[i])
		prod(codices, c "AT", ref[i])
		for (k = 1; k <= 4; k++) {
			prod(codices, c "XOR" id(ZERO[k]), x[i, k])
			prod(codices, c "XNOR" id(ZERO[k]), xn[i, k])
		}
		print c " = " c "BRAILLE " c "AEGEAN ." > codex
		prod(codex, c "BRAILLE", br[i])
		prod(codex, c "AEGEAN", ae[i])
	}
	close(codices); close(codex)
}
