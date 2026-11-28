// midi-probe.swift: what a MIDI board really sends, read straight from CoreMIDI.
//
//   swift tools/midi-probe.swift                      list every source and destination
//   swift tools/midi-probe.swift "LPD8 mk2" [secs]    print what that source sends (default 60 s)
//   swift tools/midi-probe.swift "LPD8 mk2" 60 --lpd8-paint
//       the same, after sending the LPD8 mk2's one pad-colour message once:
//       pads 1-8 red, amber, yellow, green, cyan, blue, magenta, white
//       (F0 47 7F 4C 06 00 30, 8 x R G B as two 7-bit bytes, F7). Its PROGRAM
//       or NOTE button puts the board's own colours back.
//
// Read-only otherwise: nothing is sent to any port unless --lpd8-paint is
// given, and then only that message. Run it with qlcconsole closed or the
// board not patched there (both may read a source at once; this is only so
// the app's own pad colours don't repaint over the test colours).
// Used to check a board's real messages against its profile
// (docs/features/control-surfaces.md, "The Akai LPD8 and LPD8 mk2").

import CoreMIDI
import Foundation

func name(_ e: MIDIEndpointRef) -> String {
    var s: Unmanaged<CFString>?
    if MIDIObjectGetStringProperty(e, kMIDIPropertyDisplayName, &s) == noErr, let v = s?.takeRetainedValue() {
        return v as String
    }
    return "?"
}

func describe(_ b: [UInt8]) -> String {
    let hex = b.map { String(format: "%02X", $0) }.joined(separator: " ")
    guard let st = b.first else { return hex }
    let ch = Int(st & 0x0F) + 1
    switch st & 0xF0 {
    case 0x90 where b.count > 2: return "\(hex)   note \(b[1]) on, ch \(ch), velocity \(b[2])"
    case 0x80 where b.count > 2: return "\(hex)   note \(b[1]) off, ch \(ch)"
    case 0xB0 where b.count > 2: return "\(hex)   CC \(b[1]) = \(b[2]), ch \(ch)"
    case 0xC0 where b.count > 1: return "\(hex)   program change \(b[1]), ch \(ch)"
    case 0xA0 where b.count > 2: return "\(hex)   poly aftertouch \(b[1]) = \(b[2]), ch \(ch)"
    case 0xD0 where b.count > 1: return "\(hex)   channel aftertouch \(b[1]), ch \(ch)"
    case 0xF0: return "\(hex)   SysEx (\(b.count) bytes)"
    default: return hex
    }
}

let args = CommandLine.arguments
let sources = (0..<MIDIGetNumberOfSources()).map { MIDIGetSource($0) }
let dests = (0..<MIDIGetNumberOfDestinations()).map { MIDIGetDestination($0) }

if args.count < 2 {
    print("Sources:")
    sources.forEach { print("  \"\(name($0))\"") }
    print("Destinations:")
    dests.forEach { print("  \"\(name($0))\"") }
    exit(0)
}

let want = args[1]
let secs = args.count > 2 ? (Double(args[2]) ?? 60) : 60
guard let src = sources.first(where: { name($0) == want }) else {
    print("No source named \"\(want)\". Sources: " + sources.map { "\"\(name($0))\"" }.joined(separator: ", "))
    exit(1)
}

var client = MIDIClientRef()
guard MIDIClientCreateWithBlock("midi-probe" as CFString, &client, nil) == noErr else {
    print("CoreMIDI refused a client")
    exit(1)
}
var inPort = MIDIPortRef()
let start = Date()
MIDIInputPortCreateWithProtocol(client, "in" as CFString, ._1_0, &inPort) { list, _ in
    for packet in list.unsafeSequence() {
        // MIDI 1.0 in UMP: type 2 = channel voice (one word), type 3 = SysEx 7-bit.
        let words = Array(packet.words())
        guard let w = words.first else { continue }
        let t = String(format: "%7.3f", Date().timeIntervalSince(start))
        switch w >> 28 {
        case 0x2:
            let b = [UInt8((w >> 16) & 0xFF), UInt8((w >> 8) & 0xFF), UInt8(w & 0xFF)]
            let len = (b[0] & 0xF0) == 0xC0 || (b[0] & 0xF0) == 0xD0 ? 2 : 3
            print("\(t)  IN  " + describe(Array(b.prefix(len))))
        case 0x3:
            print("\(t)  IN  SysEx part (\((w >> 16) & 0x0F) bytes)")
        default:
            break
        }
    }
}
MIDIPortConnectSource(inPort, src, nil)
print("Listening to \"\(want)\" for \(Int(secs)) s: press each pad, turn each knob.")

if args.contains("--lpd8-paint") {
    guard let dst = dests.first(where: { name($0) == want }) else {
        print("No destination named \"\(want)\": nothing sent.")
        exit(1)
    }
    let colours: [(Int, Int, Int)] = [(255, 0, 0), (255, 120, 0), (255, 255, 0), (0, 255, 0),
                                      (0, 255, 255), (0, 0, 255), (255, 0, 255), (255, 255, 255)]
    var msg: [UInt8] = [0xF0, 0x47, 0x7F, 0x4C, 0x06, 0x00, 0x30]
    for (r, g, b) in colours {
        for v in [r, g, b] { msg += [UInt8((v >> 7) & 0x7F), UInt8(v & 0x7F)] }
    }
    msg.append(0xF7)
    var outPort = MIDIPortRef()
    MIDIOutputPortCreate(client, "out" as CFString, &outPort)
    var buf = [UInt8](repeating: 0, count: 1024)
    buf.withUnsafeMutableBytes { raw in
        let list = raw.baseAddress!.assumingMemoryBound(to: MIDIPacketList.self)
        var p = MIDIPacketListInit(list)
        p = MIDIPacketListAdd(list, 1024, p, 0, msg.count, msg)
        MIDISend(outPort, dst, list)
    }
    print("Sent the pad-colour message (\(msg.count) bytes): "
          + msg.map { String(format: "%02X", $0) }.joined(separator: " "))
}

RunLoop.main.run(until: Date().addingTimeInterval(secs))
print("Done.")
