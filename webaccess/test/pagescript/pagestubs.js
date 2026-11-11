/*
  Q Light Controller Plus - qlcconsole
  pagestubs.js

  Just enough of a browser for the web pages' scripts (/run, /focus and
  the gamepad reader they share) to run in a QJSEngine: a DOM built from
  the page's own markup, a fake WebSocket that records what the page
  sends, fake timers and animation frames the test steps by hand, and
  navigator.getGamepads() returning the pad the test describes. Nothing
  here is the page's logic; the test drives the page through these.

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

var window = this;

/* -------- The DOM: elements, from the page's markup -------- */

function El(tag) {
    this.tagName = String(tag).toUpperCase();
    this.id = '';
    this.textContent = '';
    this.className = '';
    this.style = {};
    this.disabled = false;
    this.value = '';
    this.type = '';
    this.title = '';
    this.childNodes = [];
    this.attrs = {};
    this.parentNode = null;
}
El.prototype.appendChild = function (c) {
    if (c.parentNode) c.parentNode.removeChild(c);
    c.parentNode = this;
    this.childNodes.push(c);
    return c;
};
El.prototype.removeChild = function (c) {
    var i = this.childNodes.indexOf(c);
    if (i < 0) throw new Error('removeChild: not a child');
    this.childNodes.splice(i, 1);
    c.parentNode = null;
    return c;
};
Object.defineProperty(El.prototype, 'firstChild', {
    get: function () { return this.childNodes.length ? this.childNodes[0] : null; }
});
El.prototype.getAttribute = function (n) {
    return Object.prototype.hasOwnProperty.call(this.attrs, n) ? this.attrs[n] : null;
};
El.prototype.setAttribute = function (n, v) { this.attrs[n] = String(v); if (n === 'id') this.id = String(v); };
El.prototype.addEventListener = function (type, fn) { (this['__on' + type] = this['__on' + type] || []).push(fn); };
El.prototype.walk = function (fn) {
    for (var i = 0; i < this.childNodes.length; i++) {
        fn(this.childNodes[i]);
        this.childNodes[i].walk(fn);
    }
};
/* Selectors the pages use: "tag", "tag[attr]" */
function matches(el, sel) {
    var m = /^([a-z0-9]*)(?:\[([a-z-]+)\])?$/i.exec(sel);
    if (!m) throw new Error('selector not stubbed: ' + sel);
    if (m[1] && el.tagName !== m[1].toUpperCase()) return false;
    if (m[2] && el.getAttribute(m[2]) === null) return false;
    return true;
}
El.prototype.querySelectorAll = function (sel) {
    var out = [];
    this.walk(function (e) { if (matches(e, sel)) out.push(e); });
    return out;
};
El.prototype.querySelector = function (sel) {
    var all = this.querySelectorAll(sel);
    return all.length ? all[0] : null;
};
/** Everything the person reads in it: its own text and its children's. */
El.prototype.text = function () {
    var t = this.textContent;
    this.walk(function (e) { t += e.textContent; });
    return t;
};

var VOID = { meta: 1, input: 1, br: 1, img: 1, link: 1, hr: 1 };
function entities(t) {
    return t.replace(/&nbsp;/g, ' ').replace(/&hellip;/g, '…').replace(/&mdash;/g, '—')
            .replace(/&rsaquo;/g, '›').replace(/&lsaquo;/g, '‹').replace(/&deg;/g, '°')
            .replace(/&uarr;/g, '↑').replace(/&darr;/g, '↓').replace(/&larr;/g, '←')
            .replace(/&rarr;/g, '→').replace(/&#(\d+);/g, function (m, n) { return String.fromCharCode(+n); })
            .replace(/&amp;/g, '&');
}

var document = {
    body: new El('body'),
    hidden: false,
    __byId: {},
    __listeners: {},
    getElementById: function (id) { return this.__byId[id] || null; },
    createElement: function (tag) { return new El(tag); },
    createTextNode: function (t) { var e = new El('#text'); e.textContent = String(t); return e; },
    addEventListener: function (type, fn) { (this.__listeners[type] = this.__listeners[type] || []).push(fn); },
    querySelectorAll: function (sel) { return this.body.querySelectorAll(sel); },
    querySelector: function (sel) { return this.body.querySelector(sel); }
};

/** Build the body from the page's markup (scripts and styles left out). */
function __loadMarkup(html) {
    html = html.replace(/<script[\s\S]*?<\/script>/g, '').replace(/<style[\s\S]*?<\/style>/g, '')
               .replace(/<!--[\s\S]*?-->/g, '');
    var b = html.indexOf('<body>');
    if (b >= 0) html = html.substring(b + 6);
    var stack = [document.body];
    var re = /<(\/?)([a-zA-Z0-9]+)([^>]*)>|([^<]+)/g, m;
    while ((m = re.exec(html)) !== null) {
        var top = stack[stack.length - 1];
        if (m[4] !== undefined) {
            top.textContent += entities(m[4]);
            continue;
        }
        var tag = m[2].toLowerCase();
        if (m[1]) {
            // a close: back to the element it closes
            for (var i = stack.length - 1; i > 0; i--)
                if (stack[i].tagName === tag.toUpperCase()) { stack.length = i; break; }
            continue;
        }
        if (tag === 'html' || tag === 'head' || tag === 'body' || tag === 'title') continue;
        var el = new El(tag);
        var ar = /([a-zA-Z-]+)(?:="([^"]*)")?/g, a;
        while ((a = ar.exec(m[3])) !== null) {
            var v = a[2] === undefined ? '' : entities(a[2]);
            el.attrs[a[1]] = v;
            if (a[1] === 'id') { el.id = v; document.__byId[v] = el; }
            else if (a[1] === 'class') el.className = v;
            else if (a[1] === 'disabled') el.disabled = true;
            else if (a[1] === 'type') el.type = v;
            else if (a[1] === 'title') el.title = v;
        }
        top.appendChild(el);
        if (!VOID[tag] && !/\/$/.test(m[3])) stack.push(el);
    }
}

/* -------- Time: performance.now, timers and frames, stepped by the test -------- */

var __now = 1000;
var __timers = [];
var __timerId = 0;
var __frames = [];
var performance = { now: function () { return __now; } };
function setTimeout(fn, ms) { __timers.push({ id: ++__timerId, at: __now + (ms || 0), fn: fn, every: 0 }); return __timerId; }
function setInterval(fn, ms) { __timers.push({ id: ++__timerId, at: __now + ms, fn: fn, every: ms }); return __timerId; }
function clearTimeout(id) { __timers = __timers.filter(function (t) { return t.id !== id; }); }
var clearInterval = clearTimeout;
function requestAnimationFrame(fn) { __frames.push(fn); return __frames.length; }

/** Move the clock on by @a ms in steps of @a frameMs, running the timers
 *  that fall due and one animation frame per step (as a browser would). */
function __advance(ms, frameMs) {
    frameMs = frameMs || 16;
    var end = __now + ms;
    while (__now < end) {
        __now = Math.min(end, __now + frameMs);
        for (;;) {
            var due = null;
            __timers.forEach(function (t) { if (t.at <= __now && (due === null || t.at < due.at)) due = t; });
            if (due === null) break;
            if (due.every) due.at += due.every; else clearTimeout(due.id);
            due.fn();
        }
        var f = __frames;
        __frames = [];
        f.forEach(function (fn) { fn(__now); });
    }
}

/* -------- The browser around the page -------- */

var location = { protocol: 'http:', host: 'console.local:9999' };
var __store = {};
var sessionStorage = {
    getItem: function (k) { return Object.prototype.hasOwnProperty.call(__store, k) ? __store[k] : null; },
    setItem: function (k, v) { __store[k] = String(v); },
    removeItem: function (k) { delete __store[k]; }
};
var __vibrations = [];
var __pads = [];
var navigator = {
    getGamepads: function () { return __pads; },
    vibrate: function (ms) { __vibrations.push(ms); return true; }
};
var __windowListeners = {};
window.addEventListener = function (type, fn) { (__windowListeners[type] = __windowListeners[type] || []).push(fn); };

/* -------- The socket: what the page sends, and the console's answers -------- */

var __sockets = [];
function WebSocket(url) {
    this.url = url;
    this.readyState = 0;
    this.sent = [];
    __sockets.push(this);
}
WebSocket.prototype.send = function (t) {
    if (this.readyState !== 1) throw new Error('send on a socket that is not open');
    this.sent.push(String(t));
};
WebSocket.prototype.close = function () {
    if (this.readyState === 3) return;
    this.readyState = 3;
    if (this.onclose) this.onclose();
};
function __ws() { return __sockets[__sockets.length - 1]; }
function __open() { var s = __ws(); s.readyState = 1; if (s.onopen) s.onopen(); }
function __recv(text) { var s = __ws(); if (s.onmessage) s.onmessage({ data: text }); }
/** What the page sent since the last call, and forget it. */
function __take() { var s = __ws(); var out = s.sent; s.sent = []; return out; }

/* -------- The controller -------- */

/** One standard-layout pad: @a pressed {role: value 0..1}, @a axes [lx, ly, rx, ry]. */
function __setPad(pressed, axes, id) {
    var buttons = [];
    for (var i = 0; i < __BTN.length; i++) {
        var v = pressed && pressed[__BTN[i]] !== undefined ? pressed[__BTN[i]] : 0;
        buttons.push({ value: v, pressed: v >= 0.5 });
    }
    __pads = [{ id: id || 'Steam Deck Controller (STANDARD GAMEPAD Vendor: 28de Product: 1205)',
                connected: true, mapping: 'standard', buttons: buttons, axes: axes || [0, 0, 0, 0] }];
}
function __noPad() { __pads = []; }

/** A key going down or up on the page. */
function __key(type, key, shift, target) {
    var e = { type: type, key: key, shiftKey: !!shift, repeat: false, target: target || document.body,
              defaultPrevented: false, preventDefault: function () { this.defaultPrevented = true; } };
    (document.__listeners[type] || []).forEach(function (fn) { fn(e); });
    return e;
}
function __click(id) {
    var e = document.getElementById(id);
    if (e === null) throw new Error('no element #' + id);
    if (e.disabled) return false;
    e.onclick();
    return true;
}
