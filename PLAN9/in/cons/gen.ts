// gen.ts - on the host, with h2gnn's own engine (~/devops/h2gnn-enhanced src/core/geometric-subsidiarity.ts):
// for every group of n from 1 to 24 and every count of yes votes, the solid its mapToGeometric(n) picks
// and whether its hasConsensus(votes, solid) holds. One line each: n yes solid consensus.
//	node --experimental-strip-types gen.ts > decisions
import { GeometricSubsidiarityEngine } from "/home/main/devops/h2gnn-enhanced/src/core/geometric-subsidiarity.ts";

const g = new GeometricSubsidiarityEngine();
const out: string[] = [];
for (let n = 1; n <= 24; n++) {
  const solid = g.mapToGeometric(n).solid;
  for (let yes = 0; yes <= n; yes++) {
    const votes = Array.from({ length: n }, (_, i) => i < yes);
    out.push(`${n} ${yes} ${solid} ${g.hasConsensus(votes, solid) ? "yes" : "no"}`);
  }
}
console.log(out.join("\n"));
