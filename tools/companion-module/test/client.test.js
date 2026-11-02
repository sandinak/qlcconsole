import { test } from 'node:test'
import assert from 'node:assert/strict'
import { QlcClient, Arm } from '../src/client.js'
import { mockServer, until, SHOW } from './helpers.js'

async function connected(opts = {}) {
	const server = await mockServer()
	const client = new QlcClient({ host: '127.0.0.1', port: server.port, coalesceMs: 20, reconnectMs: 50, ...opts })
	client.connect()
	await until(() => client.connected)
	await until(() => server.received.length > 0)
	return { server, client, done: () => (client.destroy(), server.close()) }
}

test('it says hello, with the PIN when there is one, and sends no Origin', async () => {
	const { server, client, done } = await connected({ pin: '4321' })
	await until(() => server.received.length >= 2)
	assert.deepEqual(server.received.slice(0, 2), ['QLC+RUN|hello', 'QLC+RUN|pin|4321'])
	assert.equal(server.origin, undefined)
	await done()
	assert.ok(client.destroyed)
})

test('the show state arrives as the Run page gets it', async () => {
	const { server, client, done } = await connected()
	const seen = []
	client.on('state', (s) => seen.push(s))
	server.state(SHOW)
	await until(() => seen.length === 1)
	assert.equal(client.state.next.name, 'Verse')
	assert.equal(client.master(2).name, 'Fronts')
	assert.equal(client.hit(1).id, 21)
	assert.equal(client.grandLevel(), 100)
	await done()
})

test('acks and a bad PIN are reported', async () => {
	const { server, client, done } = await connected()
	const acks = []
	const status = []
	client.on('ack', (w, a) => acks.push([w, a]))
	client.on('status', (s) => status.push(s))
	server.raw('RUN|ack|go|refused')
	server.raw('RUN|pin|bad')
	await until(() => acks.length === 1 && status.includes('badpin'))
	assert.deepEqual(acks[0], ['go', 'refused'])
	await done()
})

test('a fast spin goes out as one message, held to +-100', async () => {
	const { server, client, done } = await connected()
	server.state(SHOW)
	await until(() => client.state.masters)
	server.received.length = 0
	for (let i = 0; i < 300; i++) client.nudge('master', 7, 1)
	await until(() => server.received.length >= 1)
	await new Promise((r) => setTimeout(r, 80))
	assert.deepEqual(server.received, ['QLC+RUN|nudge|master|7|100'])
	// opposite turns inside a window cancel out and send nothing
	server.received.length = 0
	client.nudge('master', 7, 5)
	client.nudge('master', 7, -5)
	await new Promise((r) => setTimeout(r, 80))
	assert.deepEqual(server.received, [])
	await done()
})

test('two dials keep their own sums; the Grand Master is its own key', async () => {
	const { server, client, done } = await connected()
	server.state(SHOW)
	await until(() => client.state.masters)
	server.received.length = 0
	client.nudge('master', 7, -5)
	client.nudge('master', 9, 5)
	client.nudge('master', 9, 5)
	client.nudge('grand', 0, -10)
	await until(() => server.received.length === 3)
	assert.deepEqual([...server.received].sort(), ['QLC+RUN|nudge|grand|-10', 'QLC+RUN|nudge|master|7|-5', 'QLC+RUN|nudge|master|9|10'])
	await done()
})

test('a dial shows its own turns at once, clamped at 0 and 100, until the show reports', async () => {
	const { server, client, done } = await connected()
	server.state(SHOW)
	await until(() => client.state.masters)
	assert.equal(client.nudge('master', 7, 20), 70)
	assert.equal(client.masterLevel(1), 70)
	assert.equal(client.nudge('master', 7, 100), 100)
	assert.equal(client.nudge('master', 7, -500), 0)
	assert.equal(client.nudge('grand', 0, 10), 100)
	server.state({ ...SHOW, masters: [{ id: 7, name: 'Backs', level: 40 }, SHOW.masters[1]] })
	await until(() => client.masterLevel(1) === 40)
	await done()
})

test('nothing is sent while disconnected, and it reconnects', async () => {
	const { server, client, done } = await connected()
	for (const c of []) c
	client.ws.terminate()
	await until(() => !client.connected)
	assert.equal(client.send('go'), false)
	await until(() => client.connected, 3000)
	assert.equal(client.send('go'), true)
	await done()
	void server
})

test('words cannot carry the separator', async () => {
	const { server, client, done } = await connected()
	server.received.length = 0
	client.send('go', 'a|b')
	await until(() => server.received.length === 1)
	assert.equal(server.received[0], 'QLC+RUN|go|a b')
	await done()
})

test('Arm: a turn arms for a window, a press takes it once', () => {
	let t = 1000
	const arm = new Arm(5000, () => t)
	assert.equal(arm.armed, false)
	assert.equal(arm.take(), false)
	arm.arm()
	assert.equal(arm.armed, true)
	t += 4999
	assert.equal(arm.armed, true)
	assert.equal(arm.take(), true)
	assert.equal(arm.take(), false)
	arm.arm()
	t += 5001
	assert.equal(arm.armed, false)
	assert.equal(arm.take(), false)
})
