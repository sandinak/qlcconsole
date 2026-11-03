/*
  Q Light Controller Plus - qlcconsole
  webaccessrunpage.cpp

  The phone / tablet Run page: one self-contained document, served at /run.
  Everything the show says (names, numbers) goes in with textContent, never
  as HTML.

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

#include "webaccessrun.h"
#include "webaccessgamepad.h"

/* Split in three literals: a compiler limit on one string's length. */
static const char kHead[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#101214">
<title>qlcconsole Run</title>
<style>
:root{--bg:#101214;--card:#1b1f23;--fg:#eceff1;--dim:#8d99a3;--go:#2e9e4f;--go2:#38c161;--warn:#e0a21b;--stop:#c0392b;--line:#2b3238}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;height:100%;background:var(--bg);color:var(--fg);font:18px/1.3 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;touch-action:manipulation}
body{display:flex;flex-direction:column;min-height:100dvh;padding:env(safe-area-inset-top) 16px env(safe-area-inset-bottom)}
header{display:flex;align-items:center;gap:10px;padding:12px 0 6px}
#where{flex:1;min-width:0;font-size:16px;color:var(--dim);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.chip{font-size:13px;font-weight:700;letter-spacing:.06em;padding:3px 9px;border-radius:99px;background:var(--line);color:var(--dim)}
.chip.live{background:#c62828;color:#fff}
#dot{width:12px;height:12px;border-radius:50%;background:var(--stop)}
#dot.on{background:var(--go2)}
#section{font-size:15px;color:var(--warn);min-height:1.3em;padding:2px 0 8px;text-transform:uppercase;letter-spacing:.08em}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:14px 16px;margin-bottom:10px}
.card small{display:block;font-size:12px;letter-spacing:.12em;text-transform:uppercase;color:var(--dim);margin-bottom:2px}
.num{font-size:44px;font-weight:800;line-height:1.05;font-variant-numeric:tabular-nums}
.name{font-size:22px;overflow-wrap:anywhere}
#next .num{font-size:30px;color:var(--go2)}
#next .name{font-size:19px}
#sub{color:var(--warn);font-size:17px;min-height:1.3em;margin:-2px 0 6px}
main{flex:1;display:flex;flex-direction:column;justify-content:flex-start}
#pad{margin-top:auto;padding:8px 0 14px}
button{font:inherit;color:var(--fg);border:0;border-radius:18px;background:var(--card);border:1px solid var(--line);min-height:64px;width:100%;font-weight:700;letter-spacing:.04em}
button:active{filter:brightness(1.4)}
button:disabled{opacity:.35}
#go{min-height:30vh;min-height:30dvh;font-size:56px;background:var(--go);border-color:var(--go2);color:#fff;margin-bottom:10px}
#go:active{background:var(--go2)}
.row{display:flex;gap:10px;margin-bottom:10px}
#stop.armed{background:var(--stop);border-color:#e57368;color:#fff}
#subrow{display:none}
#masters{display:none}
.mrow{display:flex;align-items:center;gap:10px;margin:6px 0}
.mrow span{width:34%;font-size:16px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.mrow output{width:3.2em;text-align:right;font-variant-numeric:tabular-nums;color:var(--dim)}
.mrow input{flex:1;min-width:0;height:36px}
#hold.paused{background:var(--warn);border-color:var(--warn);color:#101214}
#msg{min-height:1.4em;text-align:center;font-size:15px;color:var(--warn);padding-bottom:6px}
#lock{position:fixed;inset:0;background:rgba(10,12,14,.96);display:none;flex-direction:column;justify-content:center;gap:14px;padding:24px}
#lock.show{display:flex}
#lock input{font:inherit;font-size:32px;text-align:center;letter-spacing:.3em;padding:14px;border-radius:14px;border:1px solid var(--line);background:var(--card);color:var(--fg);width:100%}
#lock p{margin:0;text-align:center;color:var(--dim)}
#lock .err{color:var(--warn);min-height:1.3em}
#status{display:none}
#status div{font-size:15px;padding:2px 0}
#status div.error{color:#ff8a80}
#status div.warning{color:var(--warn)}
#focuslink{display:none;color:var(--go2);font-weight:700;text-decoration:none}
#armed{display:none;text-align:center;font-size:14px;font-weight:700;letter-spacing:.1em;color:#101214;background:var(--warn);border-radius:8px;padding:3px;margin-bottom:8px}
#testbtn{width:auto;min-height:32px;font-size:14px;font-weight:400;padding:0 10px}
@media (min-width:700px){body{max-width:640px;margin:0 auto}}
@media (min-width:1000px) and (orientation:landscape){
 body{max-width:none;display:grid;grid-template-columns:1.2fr 1fr;grid-template-rows:auto auto 1fr;column-gap:20px}
 header,#section{grid-column:1 / span 2}
 main{grid-column:1;overflow:auto}
 #pad{grid-column:2;grid-row:3;margin-top:0;display:flex;flex-direction:column;justify-content:flex-end}
 #go{min-height:40dvh}
}
</style></head><body>
<header><span id="dot"></span><span id="where">Connecting&hellip;</span><a id="focuslink" href="focus">Focus &rsaquo;</a><span class="chip" id="mode">&nbsp;</span><button id="testbtn" type="button">Controller</button></header>
<div id="section"></div>
<main>
<div class="card" id="now"><small>On now</small><div class="num" id="nnum">&mdash;</div><div class="name" id="nname"></div></div>
<div id="sub"></div>
<div class="card" id="next"><small>Next</small><div class="num" id="xnum">&mdash;</div><div class="name" id="xname"></div></div>
<div class="card" id="status"><small>Show status</small><div id="slist"></div></div>
<div class="card" id="masters"><small>Masters</small><div id="mlist"></div></div>
</main>
<div id="pad">
<div id="msg"></div>
<div id="armed">LB HELD: GO ARMED</div>
<button id="go" disabled>GO</button>
<div class="row" id="subrow"><button id="subback" disabled>SUB BACK</button><button id="subgo" disabled>SUB GO</button></div>
<div class="row"><button id="back" disabled>BACK</button><button id="hold" disabled>HOLD</button><button id="stop" disabled>STOP</button></div>
</div>
<div id="lock"><p id="lockwhy">This show asks for its PIN before the page can send GO.</p>
<input id="pin" type="password" inputmode="numeric" pattern="[0-9]*" maxlength="8" autocomplete="off" placeholder="PIN">
<div class="err" id="pinerr"></div><button id="pinok">Unlock</button></div>
)HTML";

static const char kScript[] = R"HTML(<script>
(function(){
var $=function(i){return document.getElementById(i)};
var ws=null,st={},unlocked=false,retry=500,armedTimer=null,wake=null;
function store(k,v){try{if(v===null)sessionStorage.removeItem(k);else sessionStorage.setItem(k,v)}catch(e){}}
function load(k){try{return sessionStorage.getItem(k)}catch(e){return null}}
function msg(t){$('msg').textContent=t||''}
function cue(n,o,sub){
  var x=o||{},num=x.num||'—',name=x.name||'';
  if(sub&&sub.num){name=(name?name+' · ':'')+sub.num+(sub.name?' '+sub.name:'')}
  $(n+'num').textContent=num;$(n+'name').textContent=name}
function canSend(){return st.remote&&st.hasList&&(unlocked||!st.pin)}
function paint(){
  $('where').textContent=st.hasList?((st.production?st.production+' · ':'')+st.list):(st.remote===undefined?'Connecting…':'No main cue list');
  var m=$('mode');m.textContent=st.mode||'';m.className='chip'+(st.mode==='LIVE'?' live':'');
  $('section').textContent=st.section||'';
  cue('n',st.now,st.now&&st.now.sub);
  if(!st.running)$('nnum').textContent='Stopped';
  cue('x',st.next);
  if(st.hasList&&!st.next)$('xnum').textContent='End of the list';
  var s=st.subNext;$('sub').textContent=s?'Sub next: '+s.num+(s.name?' '+s.name:''):'';
  $('subrow').style.display=st.hasSub?'flex':'none';
  var ok=canSend();
  ['go','back','stop','subgo','subback'].forEach(function(i){$(i).disabled=!ok});
  $('hold').disabled=!ok||!st.running;
  $('hold').textContent=st.paused?'RESUME':'HOLD';
  $('hold').className=st.paused?'paused':'';
  masters(ok);
  var off='GO from the phone is off in Show Settings.';
  if(st.remote===false)msg(off);
  else if($('msg').textContent===off)msg('');
  if(st.remote&&st.pin&&!unlocked&&$('lock').className!=='show')askPin('');
  $('go').textContent=st.paused?'GO (paused)':'GO';
  statusList();padLabels()}
var mrows={},touching={};
function masters(ok){
  var list=st.masters||[],box=$('mlist'),seen={};
  $('masters').style.display=list.length?'block':'none';
  list.forEach(function(m){
    seen[m.id]=1;
    var r=mrows[m.id];
    if(!r){
      var d=document.createElement('div');d.className='mrow';
      var n=document.createElement('span');
      var i=document.createElement('input');i.type='range';i.min=0;i.max=100;
      var o=document.createElement('output');
      i.oninput=function(){o.textContent=i.value+'%';send('QLC+RUN|master|'+m.id+'|'+i.value)};
      i.onpointerdown=function(){touching[m.id]=1};
      i.onpointerup=i.onpointercancel=function(){delete touching[m.id]};
      d.appendChild(n);d.appendChild(i);d.appendChild(o);box.appendChild(d);
      r=mrows[m.id]={d:d,n:n,i:i,o:o}}
    r.n.textContent=m.name;r.i.disabled=!ok;
    if(!touching[m.id]){r.i.value=m.level;r.o.textContent=m.level+'%'}});
  Object.keys(mrows).forEach(function(k){if(!seen[k]){box.removeChild(mrows[k].d);delete mrows[k]}})}
function askPin(err){$('lock').className='show';$('pinerr').textContent=err||'';}
function send(t){if(ws&&ws.readyState===1)ws.send(t)}
function connect(){
  var u=(location.protocol==='https:'?'wss://':'ws://')+location.host+'/qlcplusWS';
  ws=new WebSocket(u);
  ws.onopen=function(){retry=500;$('dot').className='on';send('QLC+RUN|hello');
    var p=load('pin');if(p)send('QLC+RUN|pin|'+p)};
  ws.onclose=function(){$('dot').className='';ws=null;setTimeout(connect,retry);retry=Math.min(retry*2,8000)};
  ws.onerror=function(){try{ws.close()}catch(e){}};
  ws.onmessage=function(e){
    var p=String(e.data).split('|');
    if(p[0]!=='RUN')return;
    if(p[1]==='state'){try{st=JSON.parse(p.slice(2).join('|'))}catch(x){return}paint()}
    else if(p[1]==='pin'){
      if(p[2]==='ok'){unlocked=true;$('lock').className='';$('pin').value='';store('pin',pendingPin||load('pin'));paint()}
      else{unlocked=false;store('pin',null);askPin(p[2]==='locked'?'Too many tries. Wait a little.':'Wrong PIN.')}}
    else if(p[1]==='ack'){
      if(p[3]==='refused')msg('Refused: GO from the phone is off.');
      else if(p[3]==='needpin'){unlocked=false;askPin('')}
      else if(p[3]==='nothing')msg('Nothing to play.');
      else msg('')}}}
var pendingPin='';
$('pinok').onclick=function(){pendingPin=$('pin').value;send('QLC+RUN|pin|'+pendingPin)};
$('pin').onkeydown=function(e){if(e.key==='Enter')$('pinok').onclick()};
function cmd(c){send('QLC+RUN|'+c)}
$('go').onclick=function(){cmd('go');if(navigator.vibrate)navigator.vibrate(30)};
$('back').onclick=function(){cmd('back')};
$('subgo').onclick=function(){cmd('subgo')};
$('subback').onclick=function(){cmd('subback')};
$('hold').onclick=function(){cmd(st.paused?'resume':'pause')};
$('stop').onclick=function(){
  var b=$('stop');
  if(b.className==='armed'){b.className='';b.textContent='STOP';clearTimeout(armedTimer);cmd('stop');return}
  b.className='armed';b.textContent='TAP AGAIN TO STOP';
  armedTimer=setTimeout(function(){b.className='';b.textContent='STOP'},2000)};
function statusList(){
  var list=st.status||[],box=$('slist');while(box.firstChild)box.removeChild(box.firstChild);
  list.forEach(function(e){var d=document.createElement('div');d.className=e.level;d.textContent=e.text;box.appendChild(d)});
  $('status').style.display=list.length?'block':'none';
  $('focuslink').style.display=st.focus?'inline':'none'}
/* The controller: the pad's Run role (PadRun), from the state's map. */
function padAction(r){return (st.pad||{})[r]}
function padLabels(){var l={};for(var r in (st.pad||{}))l[r]=st.pad[r].label;QPad.labels(l)}
QPad.on({
  down:function(r){
    if(r==='start'){QPad.toggle(true);return}
    var a=padAction(r);if(!a)return;
    if(a.do==='arm'){$('armed').style.display='block';return}
    if(!QPad.down('lb')||!canSend())return;
    if(a.do==='hit')send('QLC+RUN|hit');else cmd(a.do);
    if(a.do==='go'&&navigator.vibrate)navigator.vibrate(30)},
  up:function(r){var a=padAction(r);if(a&&a.do==='arm')$('armed').style.display='none'}});
$('testbtn').onclick=function(){QPad.toggle(true)};
$('qpadclose').onclick=function(){QPad.toggle(false)};
if(navigator.wakeLock){var hold=function(){navigator.wakeLock.request('screen').then(function(l){wake=l}).catch(function(){})};
  hold();document.addEventListener('visibilitychange',function(){if(!document.hidden)hold()})}
connect();
})();
</script></body></html>
)HTML";

QString WebAccessRun::html()
{
    return QString::fromUtf8(kHead) + WebAccessGamepad::testPanelHtml()
         + QStringLiteral("<script>") + WebAccessGamepad::script() + QStringLiteral("</script>")
         + QString::fromUtf8(kScript);
}
