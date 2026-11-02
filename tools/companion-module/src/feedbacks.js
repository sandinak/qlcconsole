import { MASTER_SLOTS, HIT_SLOTS, clamp } from './client.js'

const GREEN = 0x00aa44
const RED = 0xcc0000
const AMBER = 0xcc8800
const WHITE = 0xffffff
const BLACK = 0x000000

const which = (max, label = 'Number') => ({ id: 'n', type: 'number', label, default: 1, min: 1, max, asInteger: true, clampValues: true })

/** RGBA pixels (as base64) of a level bar filling @a percent of the width. */
export function levelBar(width, height, percent, fg = [0, 200, 90], bg = [30, 30, 30]) {
	const buf = Buffer.alloc(width * height * 4)
	const barH = Math.max(2, Math.round(height * 0.18))
	const top = height - barH - Math.round(height * 0.06)
	const filled = Math.round((width * clamp(percent, 0, 100)) / 100)
	for (let y = top; y < top + barH; y++) {
		for (let x = 0; x < width; x++) {
			const c = x < filled ? fg : bg
			const o = (y * width + x) * 4
			buf[o] = c[0]
			buf[o + 1] = c[1]
			buf[o + 2] = c[2]
			buf[o + 3] = 255
		}
	}
	return buf.toString('base64')
}

export function feedbackDefinitions(ctx) {
	const { client, arm } = ctx
	const s = () => client.state || {}
	return {
		connected: {
			name: 'Connected to qlcconsole',
			type: 'boolean',
			defaultStyle: { bgcolor: GREEN, color: WHITE },
			options: [],
			callback: () => client.connected,
		},
		remote_allowed: {
			name: 'Remote control is allowed (Show Settings)',
			description: 'Use Invert to light a key when remote control is OFF',
			type: 'boolean',
			defaultStyle: { bgcolor: GREEN, color: WHITE },
			options: [],
			showInvert: true,
			callback: () => !!s().remote,
		},
		live: {
			name: 'Operate (LIVE) mode',
			description: 'On in Operate; invert for Design',
			type: 'boolean',
			defaultStyle: { bgcolor: RED, color: WHITE },
			options: [],
			showInvert: true,
			callback: () => s().mode === 'LIVE',
		},
		running: {
			name: 'The list is running',
			type: 'boolean',
			defaultStyle: { bgcolor: GREEN, color: WHITE },
			options: [],
			callback: () => !!s().running,
		},
		hold: {
			name: 'The list is on Hold',
			type: 'boolean',
			defaultStyle: { bgcolor: AMBER, color: BLACK },
			options: [],
			callback: () => !!s().paused,
		},
		blackout: {
			name: 'Blackout is on',
			type: 'boolean',
			defaultStyle: { bgcolor: RED, color: WHITE },
			options: [],
			callback: () => !!s().blackout,
		},
		next_ready: {
			name: 'A cue is on standby',
			type: 'boolean',
			defaultStyle: { bgcolor: 0x004488, color: WHITE },
			options: [],
			callback: () => !!s().next,
		},
		hit_on: {
			name: 'Hit is on',
			type: 'boolean',
			defaultStyle: { bgcolor: RED, color: WHITE },
			options: [which(HIT_SLOTS, 'Hit number')],
			callback: (f) => !!client.hit(Number(f.options.n))?.on,
		},
		hit_armed: {
			name: 'Hit is armed (may fire now)',
			type: 'boolean',
			defaultStyle: { bgcolor: 0x004488, color: WHITE },
			options: [which(HIT_SLOTS, 'Hit number')],
			callback: (f) => !!client.hit(Number(f.options.n))?.armed,
		},
		walk_armed: {
			name: 'Walk dial is armed for GO',
			type: 'boolean',
			defaultStyle: { bgcolor: AMBER, color: BLACK },
			options: [],
			callback: () => arm.armed,
		},
		level_bar: {
			name: 'Level bar (master or Grand Master)',
			description: 'Draws a bar along the bottom of the key or the touch strip',
			type: 'advanced',
			affectedProperties: ['imageBuffer'],
			options: [
				{ id: 'kind', type: 'dropdown', label: 'Of', default: 'master', choices: [{ id: 'master', label: 'Group master' }, { id: 'grand', label: 'Grand Master' }] },
				which(MASTER_SLOTS, 'Group master number'),
			],
			callback: (f) => {
				const level = f.options.kind === 'grand' ? client.grandLevel() : client.masterLevel(Number(f.options.n))
				const w = f.image?.width ?? 72
				const h = f.image?.height ?? 72
				if (level === undefined) return {}
				return {
					imageBuffer: levelBar(w, h, level),
					imageBufferEncoding: { pixelFormat: 'RGBA' },
					imageBufferPosition: { x: 0, y: 0, width: w, height: h },
				}
			},
		},
	}
}
