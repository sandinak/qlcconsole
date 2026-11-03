/*
  Q Light Controller Plus - qlcconsole
  webaccessfocuspage.cpp

  The focus remote's page: one self-contained document, served at /focus.
  Phone first; at 1280x800 (a Steam Deck) and wider, three columns. Every
  name from the show goes in with textContent, never as HTML.

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

#include "webaccessfocus.h"
#include "webaccessgamepad.h"

static const char kHead[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#101214">
<title>qlcconsole Focus</title>
<style>
:root{--bg:#101214;--card:#1b1f23;--fg:#eceff1;--dim:#8d99a3;--go:#2e9e4f;--go2:#38c161;--warn:#e0a21b;--stop:#c0392b;--line:#2b3238}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;background:var(--bg);color:var(--fg);font:17px/1.3 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;touch-action:manipulation}
body{padding:env(safe-area-inset-top) 16px env(safe-area-inset-bottom);min-height:100dvh;display:flex;flex-direction:column}
header{display:flex;align-items:center;gap:10px;padding:10px 0 6px}
header a{color:var(--dim);font-size:15px}
#where{flex:1;min-width:0;font-size:15px;color:var(--dim);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.chip{font-size:13px;font-weight:700;letter-spacing:.06em;padding:3px 9px;border-radius:99px;background:var(--line);color:var(--dim)}
.chip.live{background:#c62828;color:#fff}
#dot{width:12px;height:12px;border-radius:50%;background:var(--stop)}
#dot.on{background:var(--go2)}
main{flex:1;display:grid;grid-template-columns:1fr;gap:10px;align-content:start}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:12px 14px}
.card small{display:block;font-size:12px;letter-spacing:.12em;text-transform:uppercase;color:var(--dim);margin-bottom:2px}
#fname{font-size:34px;font-weight:800;line-height:1.1;overflow-wrap:anywhere}
#ftarget{font-size:19px;color:var(--go2);margin-top:2px}
#fnote{color:var(--warn);font-size:15px;min-height:1.2em}
.vals{display:flex;flex-wrap:wrap;gap:6px 18px;margin-top:8px;font-variant-numeric:tabular-nums}
.vals span b{font-size:22px}
#fstate{font-weight:700}
#fstate.unsaved{color:var(--warn)}
#fstate.saved{color:var(--go2)}
#progress{font-size:15px;color:var(--dim);margin-top:6px}
ul{list-style:none;margin:0;padding:0}
li{padding:8px 10px;border-radius:10px;border:1px solid transparent;cursor:pointer;display:flex;gap:8px;align-items:center}
li.cur{border-color:var(--go2);background:#14261a}
li em{margin-left:auto;font-style:normal;color:var(--dim);font-size:14px}
li.unsaved em{color:var(--warn)}
#calls,#fixtures{max-height:30vh;overflow:auto}
button{font:inherit;color:var(--fg);border-radius:12px;background:var(--card);border:1px solid var(--line);min-height:52px;width:100%;font-weight:700;padding:4px 6px}
button:active{filter:brightness(1.4)}
button:disabled{opacity:.35}
button small{display:block;font-size:11px;font-weight:400;color:var(--dim)}
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:8px}
.grid3{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
#keep{background:var(--go);border-color:var(--go2);color:#fff}
#keep small{color:#e6f5ea}
#off{display:none;padding:24px 0;text-align:center;color:var(--dim)}
#msg{min-height:1.4em;text-align:center;font-size:15px;color:var(--warn);padding:4px 0}
#lock{position:fixed;inset:0;background:rgba(10,12,14,.96);display:none;flex-direction:column;justify-content:center;gap:14px;padding:24px;z-index:10}
#lock.show{display:flex}
#lock input{font:inherit;font-size:32px;text-align:center;letter-spacing:.3em;padding:14px;border-radius:14px;border:1px solid var(--line);background:var(--card);color:var(--fg);width:100%}
#lock p{margin:0;text-align:center;color:var(--dim)}
#lock .err{color:var(--warn);min-height:1.3em}
#padhint{font-size:14px;color:var(--dim)}
#testbtn{width:auto;min-height:32px;font-size:14px;font-weight:400;margin:10px auto;display:block;background:none;border:0;color:var(--dim);text-decoration:underline}
main>section:nth-child(2){order:-1}
@media (min-width:1000px) and (orientation:landscape){
 html,body{height:100%;overflow:hidden}
 body{max-width:1600px;margin:0 auto}
 #testbtn{margin:6px 0 6px auto}
 main{grid-template-columns:1fr 1.3fr 1fr;grid-template-rows:1fr;min-height:0}
 main>section{display:flex;flex-direction:column;gap:10px;min-height:0}
 main>section:nth-child(2){order:0}
 #calls,#fixtures{max-height:none;flex:1;overflow:auto}
 .listcard{flex:1;display:flex;flex-direction:column;min-height:0}
}
</style></head><body>
<header><span id="dot"></span><span id="where">Connecting&hellip;</span><span class="chip" id="mode">&nbsp;</span>
<a href="run">Run</a></header>
<div id="off" class="card">No focus is running on the console. Start <b>Focus mode</b> or <b>Venue focus</b> there (Lighting Studio &#9656; Focus); this page follows it.</div>
<main id="main" style="display:none">
<section>
<div class="card listcard"><small>Focus calls</small><ul id="calls"></ul></div>
<div class="card listcard"><small id="fixtureshead">Fixtures</small><ul id="fixtures"></ul></div>
</section>
<section>
<div class="card"><small id="fpos">Fixture</small><div id="fname">&mdash;</div><div id="ftarget"></div><div id="fnote"></div>
<div class="vals"><span>Pan <b id="vpan">0.00</b>&deg;</span><span>Tilt <b id="vtilt">0.00</b>&deg;</span>
<span id="vbeamw">Beam <b id="vbeam"></b>&deg;</span><span id="vfocw">Focus <b id="vfoc"></b>%</span></div>
<div class="vals"><span id="fstate"></span><span id="fdelta"></span></div>
<div id="progress"></div></div>
<div id="msg"></div>
<div class="grid3">
<span></span><button data-nudge="0,1">&uarr;</button><span></span>
<button data-nudge="-1,0">&larr;</button><button id="stepbtn">0.1&deg;</button><button data-nudge="1,0">&rarr;</button>
<span></span><button data-nudge="0,-1">&darr;</button><span></span></div>
<p id="padhint">Left stick moves fast, right stick fine. Arrow keys nudge (Shift: 1&deg;).</p>
</section>
<section>
<div class="grid">
<button data-role="lt">LT<small></small></button><button data-role="rt">RT<small></small></button>
<button data-role="lb">LB<small></small></button><button data-role="rb">RB<small></small></button>
<button id="keep" data-role="a">A<small></small></button><button data-role="b">B<small></small></button>
<button data-role="x">X<small></small></button><button data-role="y">Y<small></small></button>
<button data-role="dpad_up">&#9650;<small></small></button><button data-role="dpad_down">&#9660;<small></small></button>
<button data-role="dpad_left">&#9664;<small></small></button><button data-role="dpad_right">&#9654;<small></small></button>
<button data-role="ls">LS<small></small></button><button data-role="rs">RS<small></small></button>
<button data-role="back">View<small></small></button>
</div>
</section>
</main>
<button id="testbtn" type="button">Controller test</button>
<div id="lock"><p>This show asks for its PIN before the page can work the lights.</p>
<input id="pin" type="password" inputmode="numeric" pattern="[0-9]*" maxlength="8" autocomplete="off" placeholder="PIN">
<div class="err" id="pinerr"></div><button id="pinok">Unlock</button></div>
)HTML";

static const char kScript[] = R"HTML(<script>
(function(){
var $=function(i){return document.getElementById(i)};
var ws=null,st={},unlocked=false,retry=500,pendingPin='',step=0.1,pan=0,tilt=0,lastSend=0,reps={},pings={};
function store(k,v){try{if(v===null)sessionStorage.removeItem(k);else sessionStorage.setItem(k,v)}catch(e){}}
function load(k){try{return sessionStorage.getItem(k)}catch(e){return null}}
function msg(t){$('msg').textContent=t||''}
function send(t){if(ws&&ws.readyState===1)ws.send(t)}
function canSend(){return st.remote&&st.active&&(unlocked||!st.pin)}
function item(text,right,cls,onclick){var li=document.createElement('li');var s=document.createElement('span');s.textContent=text;li.appendChild(s);
  var e=document.createElement('em');e.textContent=right||'';li.appendChild(e);li.className=cls||'';li.onclick=onclick;return li}
function fill(id,items){var u=$(id);while(u.firstChild)u.removeChild(u.firstChild);items.forEach(function(i){u.appendChild(i)})}
function paint(){
  var m=$('mode');m.textContent=st.mode||'';m.className='chip'+(st.mode==='LIVE'?' live':'');
  $('off').style.display=st.active?'none':'block';
  $('main').style.display=st.active?'':'none';
  $('where').textContent=st.active?((st.callName?st.callName:'Focus mode')+(st.venue?' · '+st.venue:'')):'Focus remote';
  QPad.labels(st.labels||{});
  document.querySelectorAll('button[data-role]').forEach(function(b){
    var l=(st.labels||{})[b.getAttribute('data-role')];b.querySelector('small').textContent=l||'';b.disabled=!canSend()||!l});
  document.querySelectorAll('button[data-nudge]').forEach(function(b){b.disabled=!canSend()});
  if(!st.active)return;
  var calls=st.calls||[];
  fill('calls',calls.map(function(c,i){return item(c.name,c.done+'/'+c.count,i===st.call?'cur':'',function(){send('QLC+FOCUS|call|'+i)})}));
  $('calls').parentNode.style.display=calls.length?'':'none';
  var f=st.fixture||{};
  fill('fixtures',(st.fixtures||[]).map(function(x){return item(x.name,x.unsaved?'not saved':(x.checked?'saved':''),
    (x.id===f.id?'cur ':'')+(x.unsaved?'unsaved':''),function(){send('QLC+FOCUS|fixture|'+x.id)})}));
  $('fixtureshead').textContent=st.scope==='group'?'Fixtures (the whole call)':'Fixtures';
  $('fpos').textContent=(st.scope==='group'?'Whole call · ':'')+(f.count?'Fixture '+f.index+' of '+f.count:'Fixture');
  $('fname').textContent=f.name||'—';
  $('ftarget').textContent=(st.targetPage?'Moving the target: ':'→ ')+(st.target||'');
  $('fnote').textContent=st.holding?'Holding: nothing moves until it is let go on the console.':(st.note||'');
  $('vpan').textContent=(f.pan||0).toFixed(2);$('vtilt').textContent=(f.tilt||0).toFixed(2);
  $('vbeamw').style.display=f.beam===undefined?'none':'';$('vbeam').textContent=f.beam===undefined?'':f.beam;
  $('vfocw').style.display=f.focus===undefined?'none':'';$('vfoc').textContent=f.focus===undefined?'':f.focus;
  var s=$('fstate');s.textContent=f.unsaved?'Not saved':(f.checked?'Saved':'');s.className=f.unsaved?'unsaved':(f.checked?'saved':'');
  $('fdelta').textContent=f.delta||'';
  var left=Math.max(0,(st.total||0)-(st.checked||0));
  $('progress').textContent=(st.checked||0)+' saved, '+left+' to go'+(st.unsaved?' · '+st.unsaved+' not saved':'');
  if(st.remote===false)msg('Remote control is off in Show Settings (Allow remote GO).');
  if(st.remote&&st.pin&&!unlocked&&$('lock').className!=='show')askPin('')}
function askPin(err){$('lock').className='show';$('pinerr').textContent=err||''}
function press(r){if(canSend())send('QLC+FOCUS|press|'+r)}
function nudge(p,t){if(canSend()&&(p||t))send('QLC+FOCUS|nudge|'+p.toFixed(3)+'|'+t.toFixed(3))}
function isDpad(r){return r.indexOf('dpad_')===0}
QPad.on({
  down:function(r){if(r==='start'){QPad.toggle(true);return}
    press(r);if(isDpad(r)){var d=st.repeatDelay||400,iv=st.repeatInterval||80;
      reps[r]=setTimeout(function(){reps[r]=setInterval(function(){press(r)},iv)},d)}},
  up:function(r){if(reps[r]){clearTimeout(reps[r]);clearInterval(reps[r]);delete reps[r]}},
  frame:function(dt,a){
    var c=st.coarse||8,f=st.fine||1,v=QPad.vel;
    var dp=(v(a[0])*c+v(a[2])*f)*dt,dtl=-(v(a[1])*c+v(a[3])*f)*dt;
    pan+=dp;tilt+=dtl;
    var now=performance.now();
    if(now-lastSend>=QPad.send&&(Math.abs(pan)>=0.005||Math.abs(tilt)>=0.005)){nudge(pan,tilt);pan=0;tilt=0;lastSend=now}
    if(dp===0&&dtl===0&&Math.abs(pan)<0.005&&Math.abs(tilt)<0.005){pan=0;tilt=0}},
  key:function(e){var s=e.shiftKey?1:step,m={ArrowLeft:[-s,0],ArrowRight:[s,0],ArrowUp:[0,s],ArrowDown:[0,-s]}[e.key];
    if(m){e.preventDefault();nudge(m[0],m[1])}}});
document.querySelectorAll('button[data-role]').forEach(function(b){b.onclick=function(){press(b.getAttribute('data-role'))}});
document.querySelectorAll('button[data-nudge]').forEach(function(b){b.onclick=function(){var d=b.getAttribute('data-nudge').split(',');nudge(d[0]*step,d[1]*step)}});
$('stepbtn').onclick=function(){step=step>=1?0.01:step*10;step=Math.round(step*100)/100;$('stepbtn').textContent=step+'°'};
$('testbtn').onclick=function(){QPad.toggle(true)};
$('qpadclose').onclick=function(){QPad.toggle(false)};
function ping(){if(ws&&ws.readyState===1){var t=Math.round(performance.now()*1000)/1000;pings[t]=1;send('QLC+FOCUS|ping|'+t)}}
setInterval(ping,2000);
function connect(){
  ws=new WebSocket((location.protocol==='https:'?'wss://':'ws://')+location.host+'/qlcplusWS');
  ws.onopen=function(){retry=500;$('dot').className='on';send('QLC+FOCUS|hello');var p=load('pin');if(p)send('QLC+RUN|pin|'+p);ping()};
  ws.onclose=function(){$('dot').className='';ws=null;setTimeout(connect,retry);retry=Math.min(retry*2,8000)};
  ws.onerror=function(){try{ws.close()}catch(e){}};
  ws.onmessage=function(e){
    var p=String(e.data).split('|');
    if(p[0]==='FOCUS'&&p[1]==='state'){try{st=JSON.parse(p.slice(2).join('|'))}catch(x){return}paint()}
    else if(p[0]==='FOCUS'&&p[1]==='pong'){var t=parseFloat(p[2]);if(pings[p[2]]){delete pings[p[2]];QPad.rtt(Math.round(performance.now()-t))}}
    else if(p[0]==='FOCUS'&&p[1]==='ack'){
      if(p[3]==='refused')msg('Refused: remote control is off in Show Settings.');
      else if(p[3]==='needpin'){unlocked=false;askPin('')}
      else if(p[3]==='nothing')msg('Nothing to do: is focus running on the console?');
      else msg('')}
    else if(p[0]==='RUN'&&p[1]==='pin'){
      if(p[2]==='ok'){unlocked=true;$('lock').className='';$('pin').value='';store('pin',pendingPin||load('pin'));paint()}
      else{unlocked=false;store('pin',null);askPin(p[2]==='locked'?'Too many tries. Wait a little.':'Wrong PIN.')}}}}
$('pinok').onclick=function(){pendingPin=$('pin').value;send('QLC+RUN|pin|'+pendingPin)};
$('pin').onkeydown=function(e){if(e.key==='Enter')$('pinok').onclick()};
if(navigator.wakeLock){var hold=function(){navigator.wakeLock.request('screen').catch(function(){})};
  hold();document.addEventListener('visibilitychange',function(){if(!document.hidden)hold()})}
connect();
})();
</script></body></html>
)HTML";

QString WebAccessFocus::html()
{
    return QString::fromUtf8(kHead) + WebAccessGamepad::testPanelHtml()
         + QStringLiteral("<script>") + WebAccessGamepad::script() + QStringLiteral("</script>")
         + QString::fromUtf8(kScript);
}
