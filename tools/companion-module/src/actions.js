import { MASTER_SLOTS, HIT_SLOTS } from './client.js'

const text = (id, label, tooltip) => ({ id, type: 'textinput', label, default: '', tooltip })
const num = (id, label, def, min, max) => ({ id, type: 'number', label, default: def, min, max, asInteger: true, clampValues: true })

/**
 * The actions. @a ctx: { client, arm } (see main.js); every callback only
 * sends a Run word, so the app's own rules (remote GO, PIN, Operate for hits)
 * decide, and the answer comes back as an 'ack' the module logs.
 */
export function actionDefinitions(ctx) {
	const { client, arm } = ctx
	const word = (...w) => async () => void client.send(...w)
	const masterId = (n) => client.master(n)?.id

	return {
		go: { name: 'GO', description: 'GO the main cue list (the next cue on standby)', options: [], callback: word('go') },
		back: { name: 'Back', options: [], callback: word('back') },
		stop: { name: 'Stop', options: [], callback: word('stop') },
		subgo: { name: 'Sub GO', options: [], callback: word('subgo') },
		subback: { name: 'Sub Back', options: [], callback: word('subback') },
		hold: { name: 'Hold (pause the running list)', options: [], callback: word('pause') },
		resume: { name: 'Resume (after Hold)', options: [], callback: word('resume') },
		go_cue: {
			name: 'GO a cue (and list)',
			description: 'GO a cue by its number, on the main list or on the list with the given number',
			options: [
				{ ...text('cue', 'Cue number', 'As in the cue list, for example 12 or 2.1.3'), minLength: 1 },
				text('list', 'List number (empty: the main list)'),
			],
			callback: async (e) => {
				const cue = String(e.options.cue ?? '').trim()
				if (!cue) return
				const list = String(e.options.list ?? '').trim()
				client.send('go', cue, ...(list ? [list] : []))
			},
		},
		standby: {
			name: 'Move standby (no GO)',
			description: 'Put the next or previous cue on standby without playing it',
			options: [
				{ id: 'dir', type: 'dropdown', label: 'Direction', default: 'next', choices: [{ id: 'next', label: 'Next' }, { id: 'prev', label: 'Previous' }] },
				num('seconds', 'Arm GO if armed for (seconds)', 5, 1, 60),
			],
			callback: async (e) => {
				client.send('standby', e.options.dir === 'prev' ? 'prev' : 'next')
				arm.windowMs = Number(e.options.seconds ?? 5) * 1000
				arm.arm()
				ctx.armChanged?.()
			},
		},
		go_armed: {
			name: 'GO if armed (walk dial press)',
			description: 'GO only if a Move standby happened a few seconds ago, so a knock on the dial does not GO the show',
			options: [],
			callback: async () => {
				if (arm.take()) client.send('go')
				ctx.armChanged?.()
			},
		},
		hit: {
			name: 'Fire / release a hit',
			description: 'As a Virtual Console button: the armed hit at the narrowest place; press again to let it go. Hits fire in Operate only.',
			options: [num('which', 'Hit number (0: the armed hit)', 0, 0, HIT_SLOTS)],
			callback: async (e) => {
				const n = Number(e.options.which ?? 0)
				if (n === 0) return void client.send('hit')
				const h = client.hit(n)
				if (h) client.send('hit', h.id)
			},
		},
		blackout: {
			name: 'Blackout',
			options: [
				{ id: 'what', type: 'dropdown', label: 'Blackout', default: 'toggle', choices: [{ id: 'toggle', label: 'Toggle' }, { id: 'on', label: 'On' }, { id: 'off', label: 'Off' }] },
			],
			callback: async (e) => void client.send('blackout', e.options.what),
		},
		master_set: {
			name: 'Group master: set level',
			options: [num('n', 'Group master number', 1, 1, MASTER_SLOTS), num('level', 'Level (percent)', 100, 0, 100)],
			callback: async (e) => {
				const id = masterId(Number(e.options.n))
				if (id !== undefined) client.send('master', id, e.options.level)
			},
		},
		master_nudge: {
			name: 'Group master: nudge (dial turn)',
			description: 'Add or take away percent; turns in a burst go out as one',
			options: [num('n', 'Group master number', 1, 1, MASTER_SLOTS), num('delta', 'Percent (negative: down)', 5, -100, 100)],
			callback: async (e) => {
				const id = masterId(Number(e.options.n))
				if (id === undefined) return
				client.nudge('master', id, Number(e.options.delta))
				ctx.levelChanged?.()
			},
		},
		master_toggle: {
			name: 'Group master: to full / to 0 (dial press)',
			description: 'Below 100: to full. At 100: to 0.',
			options: [num('n', 'Group master number', 1, 1, MASTER_SLOTS)],
			callback: async (e) => {
				const n = Number(e.options.n)
				const id = masterId(n)
				if (id === undefined) return
				client.send('master', id, (client.masterLevel(n) ?? 0) >= 100 ? 0 : 100)
			},
		},
		grand_set: {
			name: 'Grand Master: set level',
			options: [num('level', 'Level (percent)', 100, 0, 100)],
			callback: async (e) => void client.send('grand', e.options.level),
		},
		grand_nudge: {
			name: 'Grand Master: nudge (dial turn)',
			options: [num('delta', 'Percent (negative: down)', 5, -100, 100)],
			callback: async (e) => {
				client.nudge('grand', 0, Number(e.options.delta))
				ctx.levelChanged?.()
			},
		},
		grand_toggle: {
			name: 'Grand Master: to full / to 0 (dial press)',
			options: [],
			callback: async () => void client.send('grand', (client.grandLevel() ?? 0) >= 100 ? 0 : 100),
		},
	}
}
