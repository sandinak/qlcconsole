import { test } from 'node:test'
import assert from 'node:assert/strict'
import { QlcClient, Arm, MASTER_SLOTS, HIT_SLOTS } from '../src/client.js'
import { actionDefinitions } from '../src/actions.js'
import { feedbackDefinitions, levelBar } from '../src/feedbacks.js'
import { variableDefinitions, variableValues } from '../src/variables.js'
import { buildPresets } from '../src/presets.js'
import { SHOW } from './helpers.js'

/** A client that records what it would send, with the show state in it. */
function fake(state = SHOW) {
	const c = new QlcClient({ host: 'x', coalesceMs: 5 })
	c.sent = []
	c.send = (...w) => (c.sent.push(w.join('|')), true)
	c.state = structuredClone(state)
	c.connected = true
	return c
}
const ctxOf = (client, arm = new Arm()) => ({ client, arm })
const press = (defs, id, options = {}) => defs[id].callback({ options })

test('GO, Back, Stop, Sub GO, Hold are the Run words', async () => {
	const c = fake()
	const a = actionDefinitions(ctxOf(c))
	for (const id of ['go', 'back', 'stop', 'subgo', 'subback', 'hold', 'resume']) await press(a, id)
	assert.deepEqual(c.sent, ['go', 'back', 'stop', 'subgo', 'subback', 'pause', 'resume'])
})

test('GO a cue on a list', async () => {
	const c = fake()
	const a = actionDefinitions(ctxOf(c))
	await press(a, 'go_cue', { cue: ' 12 ', list: '' })
	await press(a, 'go_cue', { cue: '3', list: '2' })
	await press(a, 'go_cue', { cue: '', list: '2' })
	assert.deepEqual(c.sent, ['go|12', 'go|3|2'])
})

test('hits: the armed one, or the Nth by its id', async () => {
	const c = fake()
	const a = actionDefinitions(ctxOf(c))
	await press(a, 'hit', { which: 0 })
	await press(a, 'hit', { which: 2 })
	await press(a, 'hit', { which: 9 }) // no such hit
	assert.deepEqual(c.sent, ['hit', 'hit|22'])
})

test('blackout, Grand Master, group masters by position', async () => {
	const c = fake()
	const a = actionDefinitions(ctxOf(c))
	await press(a, 'blackout', { what: 'on' })
	await press(a, 'grand_set', { level: 30 })
	await press(a, 'master_set', { n: 2, level: 60 })
	await press(a, 'master_set', { n: 5, level: 60 }) // none there
	assert.deepEqual(c.sent, ['blackout|on', 'grand|30', 'master|9|60'])
})

test('press toggles a master between full and 0', async () => {
	const c = fake()
	const a = actionDefinitions(ctxOf(c))
	await press(a, 'master_toggle', { n: 1 }) // 50 -> full
	await press(a, 'master_toggle', { n: 2 }) // 100 -> 0
	await press(a, 'grand_toggle')
	assert.deepEqual(c.sent, ['master|7|100', 'master|9|0', 'grand|0'])
})

test('dial turns nudge, clamp and coalesce', async () => {
	const c = fake()
	const sent = []
	c.send = (...w) => (sent.push(w.join('|')), true)
	const a = actionDefinitions(ctxOf(c))
	for (let i = 0; i < 40; i++) await press(a, 'master_nudge', { n: 1, delta: 5 })
	assert.equal(c.masterLevel(1), 100) // shown clamped
	await new Promise((r) => setTimeout(r, 30))
	assert.deepEqual(sent, ['nudge|master|7|100']) // sum held to 100, one message
	await press(a, 'grand_nudge', { delta: -5 })
	await new Promise((r) => setTimeout(r, 30))
	assert.equal(sent.at(-1), 'nudge|grand|-5')
})

test('the walk dial: turn puts a cue on standby, press GOes only while armed', async () => {
	const c = fake()
	let t = 0
	const arm = new Arm(5000, () => t)
	const a = actionDefinitions(ctxOf(c, arm))
	await press(a, 'go_armed') // knocked: nothing
	assert.deepEqual(c.sent, [])
	await press(a, 'standby', { dir: 'next', seconds: 5 })
	assert.deepEqual(c.sent, ['standby|next'])
	assert.equal(arm.armed, true)
	await press(a, 'go_armed')
	assert.deepEqual(c.sent, ['standby|next', 'go'])
	await press(a, 'go_armed') // used up
	await press(a, 'standby', { dir: 'prev', seconds: 2 })
	t += 3000 // too late
	await press(a, 'go_armed')
	assert.deepEqual(c.sent, ['standby|next', 'go', 'standby|prev'])
})

test('feedbacks follow the state', () => {
	const c = fake()
	const arm = new Arm()
	const f = feedbackDefinitions(ctxOf(c, arm))
	const ask = (id, options = {}) => f[id].callback({ options })
	assert.equal(ask('connected'), true)
	assert.equal(ask('remote_allowed'), true)
	assert.equal(ask('live'), true)
	assert.equal(ask('running'), true)
	assert.equal(ask('hold'), false)
	assert.equal(ask('blackout'), false)
	assert.equal(ask('next_ready'), true)
	assert.equal(ask('hit_on', { n: 1 }), false)
	assert.equal(ask('hit_on', { n: 2 }), true)
	assert.equal(ask('hit_armed', { n: 1 }), true)
	assert.equal(ask('hit_armed', { n: 2 }), false)
	assert.equal(ask('hit_on', { n: 30 }), false)
	assert.equal(ask('walk_armed'), false)
	arm.arm()
	assert.equal(ask('walk_armed'), true)
	c.state = { ...c.state, remote: false, mode: 'DESIGN', blackout: true }
	assert.equal(ask('remote_allowed'), false)
	assert.equal(ask('live'), false)
	assert.equal(ask('blackout'), true)
})

test('the level bar draws the master level, on the strip size it is asked for', () => {
	const c = fake()
	const f = feedbackDefinitions(ctxOf(c))
	const r = f.level_bar.callback({ options: { kind: 'master', n: 1 }, image: { width: 200, height: 100 } })
	assert.equal(r.imageBufferEncoding.pixelFormat, 'RGBA')
	assert.deepEqual(r.imageBufferPosition, { x: 0, y: 0, width: 200, height: 100 })
	const px = Buffer.from(r.imageBuffer, 'base64')
	assert.equal(px.length, 200 * 100 * 4)
	const row = 100 - 18 - 6 + 2 // inside the bar
	const at = (x) => px.subarray((row * 200 + x) * 4, (row * 200 + x) * 4 + 4)
	assert.equal(at(50)[1], 200) // 50% of 200 = 100: filled before ...
	assert.equal(at(150)[1], 30) // ... empty after
	// no such master: nothing drawn
	assert.deepEqual(f.level_bar.callback({ options: { kind: 'master', n: 6 } }), {})
	assert.equal(f.level_bar.callback({ options: { kind: 'grand', n: 1 }, image: { width: 10, height: 10 } }).imageBuffer.length > 0, true)
	assert.equal(Buffer.from(levelBar(10, 10, 150), 'base64').length, 400) // out of range is held
})

test('variables: current and next cue, masters, hits', () => {
	const c = fake()
	const v = variableValues(c, true)
	assert.equal(v.current_num, '1')
	assert.equal(v.current_name, 'Open')
	assert.equal(v.next_num, '2')
	assert.equal(v.next_name, 'Verse')
	assert.equal(v.mode, 'LIVE')
	assert.equal(v.remote, 'yes')
	assert.equal(v.master_1_name, 'Backs')
	assert.equal(v.master_1_level, 50)
	assert.equal(v.master_3_name, '')
	assert.equal(v.hit_1_name, 'Breathe')
	assert.equal(v.hit_2_on, 'yes')
	assert.equal(v.hit_2_armed, 'no')
	assert.equal(v.walk_armed, 'yes')
	// a sub-cue on now is the cue
	c.state.now = { num: '1', name: 'Open', sub: { num: '1.1', name: 'Pop' } }
	assert.equal(variableValues(c).current_num, '1.1')
	// every value has a definition
	const defs = variableDefinitions()
	for (const k of Object.keys(variableValues(c))) assert.ok(defs[k], k)
	for (const k of Object.keys(defs)) assert.ok(k in variableValues(c), k)
	assert.equal(Object.keys(defs).filter((k) => /^master_\d+_name$/.test(k)).length, MASTER_SLOTS)
	assert.equal(Object.keys(defs).filter((k) => /^hit_\d+_name$/.test(k)).length, HIT_SLOTS)
})

test('presets only name actions, feedbacks and variables that exist', () => {
	const c = fake()
	const ctx = ctxOf(c)
	const actions = actionDefinitions(ctx)
	const feedbacks = feedbackDefinitions(ctx)
	const vars = variableDefinitions()
	const { structure, presets } = buildPresets(5)
	const listed = structure.flatMap((s) => s.definitions.flatMap((g) => g.presets))
	assert.deepEqual([...listed].sort(), Object.keys(presets).sort())
	assert.ok(presets.go.steps[0].down.some((x) => x.actionId === 'go'))
	assert.ok(presets.go.style.text.includes('$(qlcconsole:next_num)'))
	for (const [id, p] of Object.entries(presets)) {
		for (const step of p.steps) {
			for (const key of ['down', 'up', 'rotate_left', 'rotate_right']) {
				for (const a of step[key] ?? []) assert.ok(actions[a.actionId], `${id}: action ${a.actionId}`)
			}
		}
		for (const f of p.feedbacks) assert.ok(feedbacks[f.feedbackId], `${id}: feedback ${f.feedbackId}`)
		for (const m of p.style.text.matchAll(/\$\(qlcconsole:(\w+)\)/g)) assert.ok(vars[m[1]], `${id}: variable ${m[1]}`)
	}
	// boolean feedbacks carry a style, advanced ones none
	for (const p of Object.values(presets))
		for (const f of p.feedbacks) assert.equal(feedbacks[f.feedbackId].type === 'boolean', !!f.style)
})

test('the dials: rotate is a nudge of the set step, press the toggle', () => {
	const { presets } = buildPresets(10)
	const d = presets.dial_master_2.steps[0]
	assert.deepEqual(d.rotate_left, [{ actionId: 'master_nudge', options: { n: 2, delta: -10 } }])
	assert.deepEqual(d.rotate_right, [{ actionId: 'master_nudge', options: { n: 2, delta: 10 } }])
	assert.deepEqual(d.down, [{ actionId: 'master_toggle', options: { n: 2 } }])
	const w = presets.dial_walk.steps[0]
	assert.equal(w.rotate_right[0].actionId, 'standby')
	assert.equal(w.down[0].actionId, 'go_armed')
	assert.ok(presets.dial_grand)
	assert.equal(Object.keys(presets).filter((k) => k.startsWith('hit_') && k !== 'hit_armed').length, HIT_SLOTS)
})

test('the manifest is complete', async () => {
	const { readFileSync } = await import('node:fs')
	const m = JSON.parse(readFileSync(new URL('../companion/manifest.json', import.meta.url)))
	const schema = JSON.parse(readFileSync(new URL('../node_modules/@companion-module/base/assets/manifest.schema.json', import.meta.url)))
	for (const k of schema.$defs.ModuleManifest.required) assert.ok(k in m, k)
	assert.ok(schema.$defs.ModuleManifestRuntime.properties.type.enum.includes(m.runtime.type))
	assert.equal(m.id, 'qlcconsole')
	const pkg = JSON.parse(readFileSync(new URL('../package.json', import.meta.url)))
	assert.equal(pkg.version, m.version)
})
