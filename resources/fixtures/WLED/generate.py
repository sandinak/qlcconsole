#!/usr/bin/env python3
"""Generate the QLC+ WLED fixture definition (ArtNet/E1.31 "Effect" DMX mode).

Channel map: https://kno.wled.ge/interfaces/e1.31-dmx/#effect (15 channels).
The Effect/Palette DMX value IS the WLED index, so lists are firmware-specific.
The embedded lists below were pulled from a WLED 16.0.1 device (220 effects,
72 palettes). To rebuild for a different firmware:

    ./generate.py --device 192.168.20.185     # pulls /json/eff and /json/pal
"""
import sys, os, json
from urllib.request import urlopen
from xml.sax.saxutils import escape

EFFECTS = [
 'Solid', 'Blink', 'Breathe', 'Wipe', 'Wipe Random', 'Random Colors',
 'Sweep', 'Dynamic', 'Colorloop', 'Rainbow', 'Scan', 'Scan Dual',
 'Fade', 'Theater', 'Theater Rainbow', 'Running', 'Saw', 'Twinkle',
 'Dissolve', 'Dissolve Rnd', 'Sparkle', 'Sparkle Dark', 'Sparkle+', 'Strobe',
 'Strobe Rainbow', 'Strobe Mega', 'Blink Rainbow', 'Android', 'Chase', 'Chase Random',
 'Chase Rainbow', 'Chase Flash', 'Chase Flash Rnd', 'Rainbow Runner', 'Colorful', 'Traffic Light',
 'Sweep Random', 'Chase 2', 'Aurora', 'Stream', 'Scanner', 'Lighthouse',
 'Fireworks', 'Rain', 'Tetrix', 'Fire Flicker', 'Gradient', 'Loading',
 'Rolling Balls', 'Fairy', 'Two Dots', 'Fairytwinkle', 'Running Dual', 'Image',
 'Chase 3', 'Tri Wipe', 'Tri Fade', 'Lightning', 'ICU', 'Multi Comet',
 'Scanner Dual', 'Stream 2', 'Oscillate', 'Pride 2015', 'Juggle', 'Palette',
 'Fire 2012', 'Colorwaves', 'Bpm', 'Fill Noise', 'Noise 1', 'Noise 2',
 'Noise 3', 'Noise 4', 'Colortwinkles', 'Lake', 'Meteor', 'Copy Segment',
 'Railway', 'Ripple', 'Twinklefox', 'Twinklecat', 'Halloween Eyes', 'Solid Pattern',
 'Solid Pattern Tri', 'Spots', 'Spots Fade', 'Glitter', 'Candle', 'Fireworks Starburst',
 'Fireworks 1D', 'Bouncing Balls', 'Sinelon', 'Sinelon Dual', 'Sinelon Rainbow', 'Popcorn',
 'Drip', 'Plasma', 'Percent', 'Ripple Rainbow', 'Heartbeat', 'Pacifica',
 'Candle Multi', 'Solid Glitter', 'Sunrise', 'Phased', 'Twinkleup', 'Noise Pal',
 'Sine', 'Phased Noise', 'Flow', 'Chunchun', 'Dancing Shadows', 'Washing Machine',
 'Rotozoomer', 'Blends', 'TV Simulator', 'Dynamic Smooth', 'Spaceships', 'Crazy Bees',
 'Ghost Rider', 'Blobs', 'Scrolling Text', 'Drift Rose', 'Distortion Waves', 'Soap',
 'Octopus', 'Waving Cell', 'Pixels', 'Pixelwave', 'Juggles', 'Matripix',
 'Gravimeter', 'Plasmoid', 'Puddles', 'Midnoise', 'Noisemeter', 'Freqwave',
 'Freqmatrix', 'GEQ', 'Waterfall', 'Freqpixels', 'RSVD', 'Noisefire',
 'Puddlepeak', 'Noisemove', 'Noise2D', 'Perlin Move', 'Ripple Peak', 'Firenoise',
 'Squared Swirl', 'PacMan', 'DNA', 'Matrix', 'Metaballs', 'Freqmap',
 'Gravcenter', 'Gravcentric', 'Gravfreq', 'DJ Light', 'Funky Plank', 'Shimmer',
 'Pulser', 'Blurz', 'Drift', 'Waverly', 'Sun Radiation', 'Colored Bursts',
 'Julia', 'RSVD', 'RSVD', 'RSVD', 'Game Of Life', 'Tartan',
 'Polar Lights', 'Swirl', 'Lissajous', 'Frizzles', 'Plasma Ball', 'Flow Stripe',
 'Hiphotic', 'Sindots', 'DNA Spiral', 'Black Hole', 'Wavesins', 'Rocktaves',
 'Akemi', 'PS Volcano', 'PS Fire', 'PS Fireworks', 'PS Vortex', 'PS Fuzzy Noise',
 'PS Ballpit', 'PS Box', 'PS Attractor', 'PS Impact', 'PS Waterfall', 'PS Spray',
 'PS GEQ 2D', 'PS GEQ Nova', 'PS Ghost Rider', 'PS Blobs', 'PS DripDrop', 'PS Pinball',
 'PS Dancing Shadows', 'PS Fireworks 1D', 'PS Sparkler', 'PS Hourglass', 'PS Spray 1D', 'PS 1D Balance',
 'PS Chase', 'PS Starburst', 'PS GEQ 1D', 'PS Fire 1D', 'PS Sonic Stream', 'PS Sonic Boom',
 'PS Springy', 'PS Galaxy', 'Color Clouds', 'Slow Transition',
]

PALETTES = [
 'Default', '* Random Cycle', '* Color 1', '* Colors 1&2', '* Color Gradient', '* Colors Only',
 'Party', 'Cloud', 'Lava', 'Ocean', 'Forest', 'Rainbow',
 'Rainbow Bands', 'Sunset', 'Rivendell', 'Breeze', 'Red & Blue', 'Yellowout',
 'Analogous', 'Splash', 'Pastel', 'Sunset 2', 'Beach', 'Vintage',
 'Departure', 'Landscape', 'Beech', 'Sherbet', 'Hult', 'Hult 64',
 'Drywet', 'Jul', 'Grintage', 'Rewhi', 'Tertiary', 'Fire',
 'Icefire', 'Cyane', 'Light Pink', 'Autumn', 'Magenta', 'Magred',
 'Yelmag', 'Yelblu', 'Orange & Teal', 'Tiamat', 'April Night', 'Orangery',
 'C9', 'Sakura', 'Aurora', 'Atlantica', 'C9 2', 'C9 New',
 'Temperature', 'Aurora 2', 'Retro Clown', 'Candy', 'Toxy Reaf', 'Fairy Reaf',
 'Semi Blue', 'Pink Candy', 'Red Reaf', 'Aqua Flash', 'Yelblu Hot', 'Lite Light',
 'Red Flash', 'Blink Red', 'Red Shift', 'Red Tide', 'Candy2', 'Traffic Light',
]

def fetch(ip):
    e = json.load(urlopen(f"http://{ip}/json/eff", timeout=6))
    p = json.load(urlopen(f"http://{ip}/json/pal", timeout=6))
    return e, p

def caps(names):
    out = [f'  <Capability Min="{i}" Max="{i}">{escape(n)}</Capability>' for i, n in enumerate(names)]
    last = len(names) - 1
    if last < 255:
        out.append(f'  <Capability Min="{last+1}" Max="255">Reserved (clamps to {escape(names[last])})</Capability>')
    return "\n".join(out)

def main():
    eff, pal = EFFECTS, PALETTES
    if len(sys.argv) == 3 and sys.argv[1] == "--device":
        eff, pal = fetch(sys.argv[2])
        print(f"fetched {len(eff)} effects, {len(pal)} palettes from {sys.argv[2]}")
    phys = ("  <Physical>\n"
            '   <Bulb Type="LED" Lumens="0" ColourTemperature="0"/>\n'
            '   <Dimensions Weight="0" Width="100" Height="100" Depth="30"/>\n'
            '   <Lens Name="Other" DegreesMin="0" DegreesMax="0"/>\n'
            '   <Focus Type="Fixed" PanMax="0" TiltMax="0"/>\n'
            '   <Technical PowerConsumption="15" DmxConnector="Other"/>\n'
            "  </Physical>")
    xml = f'''<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE FixtureDefinition>
<!-- WLED "Effect" DMX mode. Effect/palette lists ({len(eff)} effects,
     {len(pal)} palettes); DMX value on the Effect/Palette channel = WLED index. -->
<FixtureDefinition xmlns="http://www.qlcplus.org/FixtureDefinition">
 <Creator>
  <Name>Q Light Controller Plus</Name>
  <Version>4.14.4 GIT</Version>
  <Author>Branson</Author>
 </Creator>
 <Manufacturer>WLED</Manufacturer>
 <Model>Effect Mode</Model>
 <Type>Color Changer</Type>
 <Channel Name="Master Dimmer" Preset="IntensityMasterDimmer"/>
 <Channel Name="Effect">
  <Group Byte="0">Effect</Group>
{caps(eff)}
 </Channel>
 <Channel Name="Effect Speed">
  <Group Byte="0">Speed</Group>
  <Capability Min="0" Max="255">Effect speed (slow to fast)</Capability>
 </Channel>
 <Channel Name="Effect Intensity">
  <Group Byte="0">Effect</Group>
  <Capability Min="0" Max="255">Effect intensity (low to high)</Capability>
 </Channel>
 <Channel Name="Palette">
  <Group Byte="0">Colour</Group>
{caps(pal)}
 </Channel>
 <Channel Name="Effect Option">
  <Group Byte="0">Effect</Group>
  <Capability Min="0" Max="255">Effect option / segment macro (see WLED docs)</Capability>
 </Channel>
 <Channel Name="Red" Preset="IntensityRed"/>
 <Channel Name="Green" Preset="IntensityGreen"/>
 <Channel Name="Blue" Preset="IntensityBlue"/>
 <Channel Name="Secondary Red">
  <Group Byte="0">Intensity</Group>
  <Colour>Red</Colour>
  <Capability Min="0" Max="255">Secondary Red</Capability>
 </Channel>
 <Channel Name="Secondary Green">
  <Group Byte="0">Intensity</Group>
  <Colour>Green</Colour>
  <Capability Min="0" Max="255">Secondary Green</Capability>
 </Channel>
 <Channel Name="Secondary Blue">
  <Group Byte="0">Intensity</Group>
  <Colour>Blue</Colour>
  <Capability Min="0" Max="255">Secondary Blue</Capability>
 </Channel>
 <Channel Name="Tertiary Red">
  <Group Byte="0">Intensity</Group>
  <Colour>Red</Colour>
  <Capability Min="0" Max="255">Tertiary Red</Capability>
 </Channel>
 <Channel Name="Tertiary Green">
  <Group Byte="0">Intensity</Group>
  <Colour>Green</Colour>
  <Capability Min="0" Max="255">Tertiary Green</Capability>
 </Channel>
 <Channel Name="Tertiary Blue">
  <Group Byte="0">Intensity</Group>
  <Colour>Blue</Colour>
  <Capability Min="0" Max="255">Tertiary Blue</Capability>
 </Channel>
 <Mode Name="Effect (15ch)">
{phys}
  <Channel Number="0">Master Dimmer</Channel>
  <Channel Number="1">Effect</Channel>
  <Channel Number="2">Effect Speed</Channel>
  <Channel Number="3">Effect Intensity</Channel>
  <Channel Number="4">Palette</Channel>
  <Channel Number="5">Effect Option</Channel>
  <Channel Number="6">Red</Channel>
  <Channel Number="7">Green</Channel>
  <Channel Number="8">Blue</Channel>
  <Channel Number="9">Secondary Red</Channel>
  <Channel Number="10">Secondary Green</Channel>
  <Channel Number="11">Secondary Blue</Channel>
  <Channel Number="12">Tertiary Red</Channel>
  <Channel Number="13">Tertiary Green</Channel>
  <Channel Number="14">Tertiary Blue</Channel>
 </Mode>
 <Mode Name="Single DRGB (4ch)">
{phys}
  <Channel Number="0">Master Dimmer</Channel>
  <Channel Number="1">Red</Channel>
  <Channel Number="2">Green</Channel>
  <Channel Number="3">Blue</Channel>
 </Mode>
 <Mode Name="Single RGB (3ch)">
{phys}
  <Channel Number="0">Red</Channel>
  <Channel Number="1">Green</Channel>
  <Channel Number="2">Blue</Channel>
 </Mode>
</FixtureDefinition>
'''
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "WLED-Effect-Mode.qxf")
    open(out, "w").write(xml)
    print("wrote", out, "with", len(eff), "effects,", len(pal), "palettes")

if __name__ == "__main__":
    main()
