import { InstanceBase, InstanceStatus } from '@companion-module/base'
import { QlcClient, Arm, DEFAULT_PORT } from './client.js'
import { actionDefinitions } from './actions.js'
import { feedbackDefinitions } from './feedbacks.js'
import { variableDefinitions, variableValues } from './variables.js'
import { buildPresets } from './presets.js'

export const UpgradeScripts = []

export default class QlcConsoleInstance extends InstanceBase {
	async init(config, _isFirstInit, secrets) {
		this.arm = new Arm()
		this.armTimer = null
		this.setVariableDefinitions(variableDefinitions())
		this.connect(config, secrets)
	}

	connect(config, secrets) {
		this.client?.destroy()
		if (!config?.host) {
			this.updateStatus(InstanceStatus.BadConfig, 'Set the host of the computer running qlcconsole')
			return
		}
		this.client = new QlcClient({ host: config.host, port: config.port || DEFAULT_PORT, pin: secrets?.pin })
		const ctx = {
			client: this.client,
			arm: this.arm,
			armChanged: () => this.armChanged(),
			levelChanged: () => this.refresh(),
		}
		this.setActionDefinitions(actionDefinitions(ctx))
		this.setFeedbackDefinitions(feedbackDefinitions(ctx))
		const { structure, presets } = buildPresets(Number(config.step) || 5)
		this.setPresetDefinitions(structure, presets)

		this.client.on('status', (state, message) => {
			if (state === 'ok') this.updateStatus(InstanceStatus.Ok)
			else if (state === 'connecting') this.updateStatus(InstanceStatus.Connecting, message)
			else if (state === 'badpin') this.updateStatus(InstanceStatus.AuthenticationFailure, message)
			else this.updateStatus(InstanceStatus.ConnectionFailure, message)
			this.refresh()
		})
		this.client.on('state', () => this.refresh())
		this.client.on('ack', (what, answer) => {
			if (answer === 'refused') this.log('warn', `${what} refused: turn on "Allow GO from the phone Run page" in qlcconsole's Show Settings`)
			else if (answer === 'needpin') this.log('warn', `${what}: the show has a PIN; set it in this connection's settings`)
			else if (answer === 'design') this.log('info', `${what}: hits fire in Operate only`)
		})
		this.client.connect()
	}

	refresh() {
		this.setVariableValues(variableValues(this.client, this.arm.armed))
		this.checkAllFeedbacks()
	}

	armChanged() {
		this.refresh()
		clearTimeout(this.armTimer)
		this.armTimer = setTimeout(() => this.refresh(), this.arm.windowMs + 50)
	}

	async destroy() {
		clearTimeout(this.armTimer)
		this.client?.destroy()
	}

	async configUpdated(config, secrets) {
		this.connect(config, secrets)
	}

	getConfigFields() {
		return [
			{ type: 'static-text', id: 'info', width: 12, label: 'qlcconsole', value: 'On the computer running qlcconsole: Connections > Devices > Show control & sync > Web access, tick "Start web access with this show" and "Reachable from other devices on the network". In File > Show Settings tick "Allow GO from the phone Run page". Only on a trusted show network.' },
			{ type: 'textinput', id: 'host', width: 8, label: 'Host (IP or name)', default: '127.0.0.1', minLength: 1 },
			{ type: 'number', id: 'port', width: 4, label: 'Port', default: DEFAULT_PORT, min: 1, max: 65535, asInteger: true },
			{ type: 'number', id: 'step', width: 4, label: 'Percent per dial click', default: 5, min: 1, max: 25, asInteger: true },
			{ type: 'secret-text', id: 'pin', width: 8, label: "The show's PIN (if it has one)" },
		]
	}
}
