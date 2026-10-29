#!/bin/sh
# gen.sh - on the host, with metaverse-build's own gate (invariants/authority/gate/AuthorityGate.native,
# built from AuthorityGate.hs over AuthorityProjection.hs): each prefix against each trace. A line of
# cases: name|prefix|trace|status|bytes out|its first line on standard error. The bytes out are kept
# in out/NAME.out. The prefix SPACE is one space; EMPTY is set and empty; UNSET is not set.
G=${GATE:-/home/main/devops/metaverse-build/invariants/authority/gate/AuthorityGate.native}
cd "$(dirname "$0")"
printf 'trace' > t.min
printf 'event_id e1 timestamp 1000\nevent_id e2 timestamp 1005\n' > t.lines
: > t.empty
rm -rf out cases; mkdir out
for p in EMPTY unknown cross valid user Unknown cross-domain SPACE UNSET; do
	for t in min lines empty; do
		n=$p.$t
		case $p in
		EMPTY) env ID_PREFIX= "$G" < t.$t > out/$n.out 2> out/$n.err ;;
		SPACE) env ID_PREFIX=' ' "$G" < t.$t > out/$n.out 2> out/$n.err ;;
		UNSET) env -u ID_PREFIX "$G" < t.$t > out/$n.out 2> out/$n.err ;;
		*) env ID_PREFIX="$p" "$G" < t.$t > out/$n.out 2> out/$n.err ;;
		esac
		echo "$n|$p|$t|$?|$(wc -c < out/$n.out | tr -d ' ')|$(head -1 out/$n.err)" >> cases
		rm out/$n.err
	done
done
