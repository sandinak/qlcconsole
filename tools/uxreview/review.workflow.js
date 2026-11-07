export const meta = {
  name: 'ux-review',
  description: 'Review UI screenshots against docs/dev/ux_rules.json: one ux-reviewer per packet, in parallel, then merge into one report',
  whenToUse: 'After tools/uxreview/uxreview.py plan has written RUN/packets.json; pass that file\'s JSON as args',
  phases: [
    { title: 'Review', detail: 'one ux-reviewer subagent per packet (6-10 screenshots each)' },
    { title: 'Merge', detail: 'uxreview.py merge: dedupe, findings.json + report.md' },
  ],
}

// args: the JSON of RUN/packets.json, as an object (not a string):
//   { run, agent, merge, packets: [{ packet, path, set, screens, images }] }
// Each reviewer writes RUN/pNN/findings.json + findings.md itself; the
// merge reads those files, so a reviewer that dies shows up in the report
// as "not reviewed" instead of vanishing.
const RESULT = {
  type: 'object',
  properties: {
    packet: { type: 'string' },
    findings: { type: 'integer', description: 'number of findings written (0 for none)' },
  },
  required: ['packet', 'findings'],
}

const input = typeof args === 'string' ? JSON.parse(args) : (args || {})
const packets = input.packets || []
if (!packets.length) {
  log('No packets in args: run tools/uxreview/uxreview.py plan first and pass RUN/packets.json.')
  return { error: 'no packets' }
}
log(`${packets.length} packets, ${packets.reduce((n, p) => n + p.images, 0)} screenshots`)

phase('Review')
const done = await parallel(packets.map(p => () =>
  agent(
    `Review the packet ${p.path} (${p.set}: ${p.screens.join(', ')}). ` +
    `Follow your instructions (${input.agent}): read every image, check the packet's rules, ` +
    `write findings.json and findings.md next to packet.json. ` +
    `Return the packet id and how many findings you wrote.`,
    { label: `${p.packet} ${p.set}`, phase: 'Review', agentType: 'ux-reviewer', schema: RESULT })))

const ok = done.filter(Boolean)
const failed = packets.filter((p, i) => !done[i]).map(p => p.packet)
if (failed.length) log(`No result from: ${failed.join(', ')} (the merge lists them as not reviewed)`)

phase('Merge')
const merged = await agent(
  `Run this command with Bash and reply with its output only, unchanged: ${input.merge}`,
  { label: 'merge', phase: 'Merge', effort: 'low' })

return {
  run: input.run,
  report: `${input.run}/report.md`,
  findings: `${input.run}/findings.json`,
  reviewed: ok.length,
  failed,
  merge: merged,
}
