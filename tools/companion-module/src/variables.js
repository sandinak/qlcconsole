import { MASTER_SLOTS, HIT_SLOTS } from './client.js'

export function variableDefinitions() {
	const v = {
		connected: { name: 'Connected to qlcconsole (yes/no)' },
		mode: { name: 'Design or Operate (LIVE / DESIGN)' },
		remote: { name: 'Remote control allowed in Show Settings (yes/no)' },
		pin_required: { name: 'The show has a PIN (yes/no)' },
		production: { name: 'Production' },
		list: { name: 'Cue list' },
		section: { name: 'Song section' },
		running: { name: 'List running (yes/no)' },
		hold: { name: 'List paused (yes/no)' },
		current_num: { name: 'Cue on now: number' },
		current_name: { name: 'Cue on now: name' },
		next_num: { name: 'Next cue (on standby): number' },
		next_name: { name: 'Next cue (on standby): name' },
		blackout: { name: 'Blackout on (yes/no)' },
		grand: { name: 'Grand Master (percent)' },
		walk_armed: { name: 'Walk dial armed for GO (yes/no)' },
	}
	for (let i = 1; i <= MASTER_SLOTS; i++) {
		v[`master_${i}_name`] = { name: `Group master ${i}: name` }
		v[`master_${i}_level`] = { name: `Group master ${i}: level (percent)` }
	}
	for (let i = 1; i <= HIT_SLOTS; i++) {
		v[`hit_${i}_name`] = { name: `Hit ${i}: name` }
		v[`hit_${i}_on`] = { name: `Hit ${i}: on (yes/no)` }
		v[`hit_${i}_armed`] = { name: `Hit ${i}: armed (yes/no)` }
	}
	return v
}

const yn = (b) => (b ? 'yes' : 'no')

/** Every variable's value, from the show's state (see WebAccessRun::stateJson). */
export function variableValues(client, walkArmed = false) {
	const s = client.state || {}
	const cue = (c, f) => (c && c[f] !== undefined ? String(c[f]) : '')
	const now = s.now?.sub ?? s.now
	const out = {
		connected: yn(client.connected),
		mode: s.mode ?? '',
		remote: yn(s.remote),
		pin_required: yn(s.pin),
		production: s.production ?? '',
		list: s.list ?? '',
		section: s.section ?? '',
		running: yn(s.running),
		hold: yn(s.paused),
		current_num: cue(now, 'num'),
		current_name: cue(now, 'name'),
		next_num: cue(s.next, 'num'),
		next_name: cue(s.next, 'name'),
		blackout: yn(s.blackout),
		grand: client.grandLevel() ?? '',
		walk_armed: yn(walkArmed),
	}
	for (let i = 1; i <= MASTER_SLOTS; i++) {
		const m = client.master(i)
		out[`master_${i}_name`] = m ? m.name : ''
		out[`master_${i}_level`] = m ? (client.masterLevel(i) ?? '') : ''
	}
	for (let i = 1; i <= HIT_SLOTS; i++) {
		const h = client.hit(i)
		out[`hit_${i}_name`] = h ? h.name : ''
		out[`hit_${i}_on`] = yn(h?.on)
		out[`hit_${i}_armed`] = yn(h?.armed)
	}
	return out
}
