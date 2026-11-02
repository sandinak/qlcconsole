import { WebSocketServer } from 'ws'

/** A stand-in for qlcconsole's web access: records what the module sends. */
export async function mockServer() {
	const wss = new WebSocketServer({ port: 0, host: '127.0.0.1', path: '/qlcplusWS' })
	await new Promise((r) => wss.on('listening', r))
	const received = []
	let sock = null
	wss.on('connection', (s, req) => {
		sock = s
		s.origin = req.headers.origin
		s.on('message', (d) => received.push(d.toString()))
	})
	return {
		port: wss.address().port,
		received,
		state(obj) {
			sock.send('RUN|state|' + JSON.stringify(obj))
		},
		raw(text) {
			sock.send(text)
		},
		get origin() {
			return sock?.origin
		},
		close: () => new Promise((r) => { for (const c of wss.clients) c.terminate(); wss.close(r) }),
	}
}

export const until = async (fn, ms = 2000) => {
	const t = Date.now()
	while (!fn()) {
		if (Date.now() - t > ms) throw new Error('timed out waiting for ' + fn)
		await new Promise((r) => setTimeout(r, 5))
	}
}

export const SHOW = {
	api: 1,
	remote: true,
	pin: false,
	mode: 'LIVE',
	list: 'Act One',
	production: 'Tonight',
	running: true,
	paused: false,
	blackout: false,
	grand: 100,
	now: { num: '1', name: 'Open' },
	next: { num: '2', name: 'Verse' },
	masters: [{ id: 7, name: 'Backs', level: 50 }, { id: 9, name: 'Fronts', level: 100 }],
	hits: [{ id: 21, name: 'Breathe', on: false, armed: true }, { id: 22, name: 'Wide', on: true, armed: false }],
}
