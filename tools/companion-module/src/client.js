// The link to qlcconsole: the web access websocket and its Run protocol
// (webaccess/src/webaccessrun.*, docs/features/show-control.md "Stream Deck
// (Companion)"). No Companion code in here, so the tests run it alone.
import { EventEmitter } from 'node:events'
import WebSocket from 'ws'

export const DEFAULT_PORT = 9999
export const PATH = '/qlcplusWS'

/** Slots the module defines variables, feedbacks and presets for. */
export const MASTER_SLOTS = 8
export const HIT_SLOTS = 32

export const clamp = (n, lo, hi) => Math.min(hi, Math.max(lo, n))

/**
 * One connection to qlcconsole. Emits:
 *   'status' (state: 'connecting'|'ok'|'closed'|'badpin', message)
 *   'state'  (state object)  -- the show's state, as the Run page gets it
 *   'ack'    (what, answer)  -- "ok", "nothing", "refused", "needpin", "design"
 */
export class QlcClient extends EventEmitter {
	/**
	 * @param {object} o
	 * @param {string} o.host
	 * @param {number} [o.port]
	 * @param {string} [o.pin]          the show's PIN, if it has one
	 * @param {number} [o.coalesceMs]   dial turns inside this window go out as one
	 * @param {number} [o.reconnectMs]
	 */
	constructor(o) {
		super()
		this.host = o.host
		this.port = o.port || DEFAULT_PORT
		this.pin = o.pin || ''
		this.coalesceMs = o.coalesceMs ?? 40
		this.reconnectMs = o.reconnectMs ?? 3000
		this.ws = null
		this.state = {}
		this.connected = false
		this.destroyed = false
		this.pending = new Map() // nudge key -> { words, delta }
		this.flushTimer = null
		this.reconnectTimer = null
		/** Levels a dial has moved but the show hasn't reported yet (key -> percent). */
		this.optimistic = new Map()
	}

	get url() {
		const host = this.host.includes(':') && !this.host.startsWith('[') ? `[${this.host}]` : this.host
		return `ws://${host}:${this.port}${PATH}`
	}

	connect() {
		if (this.destroyed) return
		this.emit('status', 'connecting', `Connecting to ${this.url}`)
		const ws = new WebSocket(this.url)
		this.ws = ws
		ws.on('open', () => {
			this.connected = true
			this.send('hello')
			if (this.pin) this.send('pin', this.pin)
			this.emit('status', 'ok')
		})
		ws.on('message', (data) => this.onMessage(data.toString()))
		ws.on('error', (err) => this.emit('status', 'closed', err.message))
		ws.on('close', () => {
			if (this.ws !== ws) return
			this.connected = false
			this.emit('status', 'closed', 'Disconnected')
			if (!this.destroyed) {
				this.reconnectTimer = setTimeout(() => this.connect(), this.reconnectMs)
			}
		})
	}

	destroy() {
		this.destroyed = true
		clearTimeout(this.reconnectTimer)
		clearTimeout(this.flushTimer)
		this.pending.clear()
		const ws = this.ws
		this.ws = null
		if (ws) ws.close()
	}

	onMessage(text) {
		if (text.startsWith('RUN|state|')) {
			try {
				this.state = JSON.parse(text.slice('RUN|state|'.length))
			} catch {
				return
			}
			this.optimistic.clear()
			this.emit('state', this.state)
		} else if (text.startsWith('RUN|ack|')) {
			const [, , what, answer] = text.split('|')
			this.emit('ack', what, answer)
		} else if (text.startsWith('RUN|pin|')) {
			const answer = text.slice('RUN|pin|'.length)
			if (answer !== 'ok') this.emit('status', 'badpin', `PIN ${answer}`)
		}
	}

	/** Send one Run word: send('go', '2', '1') is "QLC+RUN|go|2|1". */
	send(...words) {
		if (!this.connected || !this.ws || this.ws.readyState !== WebSocket.OPEN) return false
		const clean = words.map((w) => String(w).replaceAll('|', ' '))
		this.ws.send(['QLC+RUN', ...clean].join('|'))
		return true
	}

	/**
	 * A dial turn: @a delta percent on master/grand. Turns of one dial inside
	 * the coalescing window are added and go out as ONE message, so a fast
	 * spin sends a few messages, not hundreds. The app clamps at 0..100; the
	 * sum here is held to +-100 so a long spin can't overflow anything.
	 * Returns the level the dial now shows, when it is known.
	 */
	nudge(kind, id, delta) {
		const key = kind === 'grand' ? 'grand' : `master:${id}`
		const words = kind === 'grand' ? ['nudge', 'grand'] : ['nudge', 'master', id]
		const entry = this.pending.get(key) || { words, delta: 0 }
		entry.delta = clamp(entry.delta + delta, -100, 100)
		this.pending.set(key, entry)
		if (!this.flushTimer) this.flushTimer = setTimeout(() => this.flush(), this.coalesceMs)
		const now = this.levelOf(key)
		if (now === undefined) return undefined
		const shown = clamp(now + delta, 0, 100)
		this.optimistic.set(key, shown)
		return shown
	}

	flush() {
		this.flushTimer = null
		for (const { words, delta } of this.pending.values()) {
			if (delta !== 0) this.send(...words, delta)
		}
		this.pending.clear()
	}

	levelOf(key) {
		if (this.optimistic.has(key)) return this.optimistic.get(key)
		if (key === 'grand') return this.state.grand
		const id = Number(key.slice('master:'.length))
		return (this.state.masters || []).find((m) => m.id === id)?.level
	}

	/** Group master @a n (1-based, in the order the show lists them). */
	master(n) {
		return (this.state.masters || [])[n - 1]
	}

	/** Hit @a n (1-based, in tree order). */
	hit(n) {
		return (this.state.hits || [])[n - 1]
	}

	/** The level a master shows now (a dial's own turns included), or undefined. */
	masterLevel(n) {
		const m = this.master(n)
		return m ? this.levelOf(`master:${m.id}`) : undefined
	}

	grandLevel() {
		return this.levelOf('grand')
	}
}

/**
 * The "armed" rule for a dial that GOes on a press: turning it arms it for
 * @a windowMs; a press GOes only while armed, then disarms. So a knocked dial
 * can't GO the show.
 */
export class Arm {
	constructor(windowMs = 5000, now = () => Date.now()) {
		this.windowMs = windowMs
		this.now = now
		this.until = 0
	}
	arm() {
		this.until = this.now() + this.windowMs
	}
	get armed() {
		return this.now() < this.until
	}
	/** True once if armed (and disarms). */
	take() {
		const was = this.armed
		this.until = 0
		return was
	}
}
