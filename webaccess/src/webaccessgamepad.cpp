/*
  Q Light Controller Plus - qlcconsole
  webaccessgamepad.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "webaccessgamepad.h"

using namespace GamepadRole;

QList<Role> WebAccessGamepad::standardButtons()
{
    // https://w3c.github.io/gamepad/#remapping, in index order
    return { A, B, X, Y, LB, RB, LeftTrigger, RightTrigger, Back, Start, LS, RS,
             DpadUp, DpadDown, DpadLeft, DpadRight, Guide };
}

QList<Role> WebAccessGamepad::standardAxes()
{
    return { LeftX, LeftY, RightX, RightY };
}

QList<WebAccessGamepad::Key> WebAccessGamepad::keys()
{
    /* The console's Focus mode keys (FocusModePanel::handleKey()), as the
       roles that do the same there: what a Steam Input layout binds the
       Deck's back grips to, and what a keyboard on a laptop browser does. */
    return {
        { "PageUp", LeftTrigger },   { "PageDown", RightTrigger },
        { "[", LB },                 { "]", RB },
        { "Enter", A },              { "Backspace", B },
        { "0", X },                  { "g", Y },
        { "t", LS },                 { "w", RS },
        { ",", DpadDown },           { ".", DpadUp },
        { ";", DpadLeft },           { "'", DpadRight },
    };
}

static QByteArray jsonText(const QJsonValue &v)
{
    if (v.isArray())
        return QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact);
    return QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact);
}

/* Everything the pages show goes in with textContent, never as HTML. */
static const char kScript[] = R"JS(
var QPad=(function(){
var BTN=@BTN@,AX=@AX@,KEYS=@KEYS@,PRESS=@PRESS@,RELEASE=@RELEASE@,DZ=@DZ@,SEND=@SEND@;
var h={},padDown={},keyDown={},down={},axes=[0,0,0,0],vals={},pad=null,lastKey='',open=false,last=0,labels={},rtt=null,frames=0;
var rest=[0,0,0,0],learn=null;
function corr(v,r){var o=v>=r?(v-r)/(1-r):(v-r)/(1+r);return Math.max(-1,Math.min(1,o))}
function reset(t){rest=[0,0,0,0];learn={until:t+2000,lo:[],hi:[],at:[],done:[]}}
function track(t,raw){var out=[0,0,0,0];
  for(var a=0;a<4;a++){var v=raw[a]||0;
    if(learn){if(learn.lo[a]===undefined||Math.max(learn.hi[a],v)-Math.min(learn.lo[a],v)>0.03){learn.lo[a]=learn.hi[a]=v;learn.at[a]=t}
      else{learn.lo[a]=Math.min(learn.lo[a],v);learn.hi[a]=Math.max(learn.hi[a],v)}
      var m=(learn.lo[a]+learn.hi[a])/2;if(!learn.done[a]&&t-learn.at[a]>=300&&Math.abs(m)<=0.3){rest[a]=m;learn.done[a]=true}}
    out[a]=corr(v,rest[a])}
  if(learn&&t>=learn.until)learn=null;return out}
function $q(i){return document.getElementById(i)}
function set(r){var d=!!(padDown[r]||keyDown[r]);if(!!down[r]===d)return;down[r]=d;
  if(open&&d&&r==='start'){toggle(false);return}
  if(!open){if(d&&h.down)h.down(r);if(!d&&h.up)h.up(r)}}
function vel(a){var d=a/2;if(Math.abs(d)<DZ)return 0;var s=d>0?1:-1;return s*Math.min(1,(Math.abs(d)-DZ)/(0.5-DZ))}
function poll(t){
  var list=navigator.getGamepads?navigator.getGamepads():[],p=null;
  for(var i=0;i<list.length;i++){if(list[i]&&list[i].connected){p=list[i];break}}
  if((p&&p.id)!==(pad&&pad.id)){reset(t);if(h.pad)h.pad(p)}
  pad=p;
  if(p){
    for(var b=0;b<p.buttons.length&&b<BTN.length;b++){
      var r=BTN[b];if(!r)continue;
      var v=p.buttons[b].value,pr=p.buttons[b].pressed;
      var val=(v>0&&v<1)?v:(pr?1:0);vals[r]=val;
      padDown[r]=padDown[r]?val>RELEASE:val>=PRESS;set(r)}
    axes=track(t,[p.axes[0],p.axes[1],p.axes[2],p.axes[3]])}
  else{for(var k in padDown){padDown[k]=false;set(k)}axes=[0,0,0,0]}
  var dt=last?Math.min(0.1,(t-last)/1000):0;last=t;
  if(!open&&h.frame)h.frame(dt,axes);
  if(open&&(++frames%3)===0)paint();
  requestAnimationFrame(poll)}
function typing(e){var n=e.target&&e.target.tagName;return n==='INPUT'||n==='TEXTAREA'||n==='SELECT'}
document.addEventListener('keydown',function(e){
  if(typing(e))return;lastKey=e.key;var r=KEYS[e.key];
  if(r){e.preventDefault();if(!e.repeat){keyDown[r]=true;set(r)}}
  else if(h.key&&!open)h.key(e);
  if(open)paint()});
document.addEventListener('keyup',function(e){var r=KEYS[e.key];if(r){keyDown[r]=false;set(r)}});
window.addEventListener('blur',function(){for(var k in keyDown){keyDown[k]=false;set(k)}});
function el(tag,cls,text){var e=document.createElement(tag);if(cls)e.className=cls;if(text!==undefined)e.textContent=text;return e}
var cells={},bars=[],NAMES={back:'VIEW',start:'MENU',guide:'GUIDE / STEAM',dpad_up:'D-PAD UP',dpad_down:'D-PAD DOWN',dpad_left:'D-PAD LEFT',dpad_right:'D-PAD RIGHT'};
function lab(r){return labels[r]||(r==='start'?'Opens and closes this test':'Not used on this page')}
function build(){
  var g=$q('qpadbtns');if(!g||g.childNodes.length)return;
  BTN.forEach(function(r){var c=el('div','qpb');c.appendChild(el('b','',NAMES[r]||r.toUpperCase()));
    var l=el('span','',lab(r));c.appendChild(l);g.appendChild(c);cells[r]={c:c,l:l}});
  var ax=$q('qpadaxes');['Left X','Left Y','Right X','Right Y'].forEach(function(n,i){
    var row=el('div','qpa');row.appendChild(el('span','',n));var bar=el('i');var fill=el('u');bar.appendChild(fill);
    row.appendChild(bar);var o=el('output','','0.00');row.appendChild(o);ax.appendChild(row);bars[i]={f:fill,o:o}})}
function paint(){
  build();
  $q('qpadid').textContent=pad?(pad.id+' · mapping: '+(pad.mapping||'none')):'No controller seen yet: press any button on it.';
  $q('qpadwarn').textContent=pad&&pad.mapping!=='standard'?'This browser does not give this controller the standard layout: buttons may be in other places.':'';
  BTN.forEach(function(r){var c=cells[r];if(!c)return;c.c.className='qpb'+(down[r]?' on':'');
    var v=vals[r];c.l.textContent=lab(r)+(v>0&&v<1?' '+v.toFixed(2):'')});
  bars.forEach(function(b,i){var v=axes[i];b.f.style.left=(50+Math.min(0,v)*50)+'%';b.f.style.width=(Math.abs(v)*50)+'%';b.o.textContent=v.toFixed(2)});
  $q('qpadkey').textContent=lastKey?('Last key: '+lastKey+(KEYS[lastKey]?' = '+KEYS[lastKey].toUpperCase():' (not used)')):'Last key: none';
  $q('qpadrtt').textContent=rtt===null?'':'Round trip to the console: '+rtt+' ms'}
function toggle(on){open=on===undefined?!open:on;var t=$q('qpadtest');if(t)t.className=open?'show':'';
  if(open){for(var k in down){if(down[k]&&h.up)h.up(k)}paint()}}
requestAnimationFrame(poll);
window.addEventListener('gamepadconnected',function(){if(open)paint()});
return{on:on,vel:vel,send:SEND,deadzone:DZ,toggle:toggle,isOpen:function(){return open},
  track:track,reset:reset,rest:function(){return rest.slice()},
  labels:function(l){labels=l||{};for(var r in cells)cells[r].l.textContent=lab(r)},
  rtt:function(ms){rtt=ms;if(open)paint()},pad:function(){return pad},down:function(r){return !!down[r]}};
function on(x){h=x||{}}
})();
)JS";

QString WebAccessGamepad::script()
{
    QJsonArray buttons;
    for (Role r : standardButtons())
        buttons.append(key(r));
    QJsonArray axes;
    for (Role r : standardAxes())
        axes.append(key(r));
    QJsonObject keyMap;
    for (const Key &k : keys())
        keyMap[QLatin1String(k.key)] = key(k.role);

    QString s = QString::fromUtf8(kScript);
    s.replace(QLatin1String("@BTN@"), QString::fromUtf8(jsonText(buttons)));
    s.replace(QLatin1String("@AX@"), QString::fromUtf8(jsonText(axes)));
    s.replace(QLatin1String("@KEYS@"), QString::fromUtf8(jsonText(keyMap)));
    s.replace(QLatin1String("@PRESS@"), QString::number(kPressAt));
    s.replace(QLatin1String("@RELEASE@"), QString::number(kReleaseAt));
    s.replace(QLatin1String("@DZ@"), QString::number(kDeadzone));
    s.replace(QLatin1String("@SEND@"), QString::number(kSendMs));
    return s;
}

QString WebAccessGamepad::testPanelHtml()
{
    return QStringLiteral(R"HTML(<style>
#qpadtest{position:fixed;inset:0;z-index:20;display:none;overflow:auto;background:#101214;color:#eceff1;padding:16px;font:15px/1.3 system-ui,sans-serif}
#qpadtest.show{display:block}
#qpadtest h2{font-size:18px;margin:0 0 6px}
#qpadtest p{margin:4px 0;color:#8d99a3}
#qpadwarn{color:#e0a21b}
#qpadbtns{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:6px;margin:10px 0}
.qpb{border:1px solid #2b3238;border-radius:10px;padding:6px 8px;background:#1b1f23;min-height:52px}
.qpb b{display:block;font-size:15px}
.qpb span{font-size:12px;color:#8d99a3}
.qpb.on{background:#2e9e4f;border-color:#38c161}
.qpb.on span{color:#fff}
.qpa{display:flex;align-items:center;gap:8px;margin:6px 0}
.qpa span{width:5em}
.qpa i{flex:1;height:14px;background:#1b1f23;border:1px solid #2b3238;border-radius:7px;position:relative;overflow:hidden}
.qpa u{position:absolute;top:0;bottom:0;background:#38c161}
.qpa output{width:3.5em;text-align:right;font-variant-numeric:tabular-nums}
#qpadclose{margin-top:10px}
</style>
<div id="qpadtest" role="dialog" aria-label="Controller test"><h2>Controller test</h2>
<p>Nothing is sent to the console while this is open. Press each control: its box lights up.</p>
<p id="qpadid"></p><p id="qpadwarn"></p><div id="qpadbtns"></div><div id="qpadaxes"></div>
<p id="qpadkey"></p><p id="qpadrtt"></p>
<button id="qpadclose" type="button">Close the test</button></div>
)HTML");
}
