import { MASTER_SLOTS, HIT_SLOTS } from './client.js'

const WHITE = 0xffffff
const BLACK = 0x000000
const DARK = 0x202020
const act = (actionId, options = {}) => ({ actionId, options })
const fb = (feedbackId, options, style, isInverted) => ({ feedbackId, options, style, ...(isInverted ? { isInverted } : {}) })

const button = (name, text, down, feedbacks = [], bgcolor = DARK, size = 'auto') => ({
	type: 'simple',
	name,
	style: { text, size, color: WHITE, bgcolor },
	steps: [{ down, up: [] }],
	feedbacks,
})

const dial = (name, text, { left, right, press }, feedbacks = []) => ({
	type: 'simple',
	name,
	style: { text, size: '14', color: WHITE, bgcolor: DARK },
	steps: [{ down: press ? [press] : [], up: [], rotate_left: [left], rotate_right: [right] }],
	feedbacks,
})

/**
 * The presets: { structure, presets } for setPresetDefinitions().
 * @param {number} step  percent a dial click moves a master
 */
export function buildPresets(step = 5) {
	const presets = {}
	const run = []
	const add = (id, def, list) => {
		presets[id] = def
		list.push(id)
	}

	add('go', button('GO (shows the next cue)', 'GO\n$(qlcconsole:next_num)\n$(qlcconsole:next_name)', [act('go')],
		[fb('next_ready', {}, { bgcolor: 0x006633 }), fb('remote_allowed', {}, { bgcolor: 0x550000 }, true)], DARK, '14'), run)
	add('back', button('Back', 'BACK', [act('back')]), run)
	add('stop', button('Stop', 'STOP', [act('stop')], [], 0x550000), run)
	add('subgo', button('Sub GO', 'SUB\nGO', [act('subgo')]), run)
	add('hold', button('Hold', 'HOLD', [act('hold')], [fb('hold', {}, { bgcolor: 0xcc8800, color: BLACK })]), run)
	add('resume', button('Resume', 'RESUME', [act('resume')]), run)
	add('now', button('Cue on now (display)', 'NOW\n$(qlcconsole:current_num)\n$(qlcconsole:current_name)', [], [fb('running', {}, { bgcolor: 0x006633 })], DARK, '14'), run)
	add('blackout', button('Blackout', 'BLACK\nOUT', [act('blackout', { what: 'toggle' })], [fb('blackout', {}, { bgcolor: 0xcc0000 })]), run)
	add('mode', button('Design / Operate (display)', '$(qlcconsole:mode)', [], [fb('live', {}, { bgcolor: 0xcc0000 })]), run)

	const hits = []
	for (let n = 1; n <= HIT_SLOTS; n++) {
		add(`hit_${n}`, button(`Hit ${n}`, `$(qlcconsole:hit_${n}_name)`, [act('hit', { which: n })],
			[fb('hit_on', { n }, { bgcolor: 0xcc0000 }), fb('hit_armed', { n }, { bgcolor: 0x004488 })], DARK, '14'), hits)
	}
	add('hit_armed', button('Fire the armed hit', 'HIT', [act('hit', { which: 0 })]), hits)

	const dials = []
	for (let n = 1; n <= MASTER_SLOTS; n++) {
		add(`dial_master_${n}`, dial(`Dial: group master ${n}`, `$(qlcconsole:master_${n}_name)\n$(qlcconsole:master_${n}_level)%`, {
			left: act('master_nudge', { n, delta: -step }),
			right: act('master_nudge', { n, delta: step }),
			press: act('master_toggle', { n }),
		}, [{ feedbackId: 'level_bar', options: { kind: 'master', n } }]), dials)
	}
	add('dial_grand', dial('Dial: Grand Master', 'GRAND\n$(qlcconsole:grand)%', {
		left: act('grand_nudge', { delta: -step }),
		right: act('grand_nudge', { delta: step }),
		press: act('grand_toggle'),
	}, [{ feedbackId: 'level_bar', options: { kind: 'grand', n: 1 } }]), dials)
	add('dial_walk', dial('Dial: walk the cues (turn: standby, press: GO if armed)', 'NEXT\n$(qlcconsole:next_num)\n$(qlcconsole:next_name)', {
		left: act('standby', { dir: 'prev', seconds: 5 }),
		right: act('standby', { dir: 'next', seconds: 5 }),
		press: act('go_armed'),
	}, [fb('walk_armed', {}, { bgcolor: 0xcc8800, color: BLACK })]), dials)

	const structure = [
		{ id: 'run', name: 'Run the show', definitions: [{ id: 'run', type: 'simple', name: 'Run', presets: run }] },
		{ id: 'hits', name: 'Hits', definitions: [{ id: 'hits', type: 'simple', name: 'Hits (32, then the armed one)', presets: hits }] },
		{ id: 'dials', name: 'Dials (Stream Deck +)', definitions: [{ id: 'dials', type: 'simple', name: 'Masters, Grand Master, walk the cues', presets: dials }] },
	]
	return { structure, presets }
}
