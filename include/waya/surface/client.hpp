#pragma once
/// \file client.hpp
/// The terminal. This is the ~6 KB browser client, isolated from the C++ runtime
/// so the transport and the app logic stay clean. It holds NO app state and NO
/// app logic: it opens a WebSocket, decodes packed binary frames, and paints
/// them with the browser's own engine — coalescing all ops of a frame into a
/// single requestAnimationFrame so the DOM is touched once per frame.
///
/// It is a VT100 for the web: exactly as dumb (just applies what the bytes say),
/// smarter only in that it paints with real DOM/CSS/fonts/GPU. The wire protocol
/// it speaks is documented in docs/internals/wire-protocol.md; the encoder side
/// is surface/binary.hpp. Everything the client does — morph-in-place (so focus/
/// caret survive), rAF coalescing, FLIP list animation, optimistic press, ripple,
/// event delegation, reconnect + session resume, dev hot-reload — is here and
/// only here.
///
/// `client(port)` returns the <script> tag the shell embeds. The runtime
/// (surface/live.hpp) composes it into the served document; nothing else needs
/// to know how the terminal works.

#include <string>

namespace waya::surface::detail {

/// The embedded browser client. `port` is baked into the WebSocket URL.
inline std::string client(int port) {
    return
    "<script>(function(){"
    "var R=document.getElementById('root'),S=document.getElementById('wsheet');"
    "var dec=new TextDecoder();"
    // — binary frame reader (LEB128 varints) → {css, ops:[[op,path,payload]]} —
    "function readFrame(buf){var b=new Uint8Array(buf),p=0;"
    "function vi(){var x=0,s=0,c;do{c=b[p++];x|=(c&0x7f)<<s;s+=7;}while(c&0x80);return x>>>0;}"
    "function str(){var n=vi(),o=p;p+=n;return dec.decode(b.subarray(o,o+n));}"
    "var css=str();var nop=vi();var ops=[];"
    "for(var i=0;i<nop;i++){var k=b[p++];var d=vi();var path='';"
    "for(var j=0;j<d;j++){path+=(j?'.':'')+vi();}"
    // payload shape by opcode:
    //   remove(5), move(7): no string.  move carries [from,to] varints.
    //   set_prop(4): two strings [prop,value].  insert_at(8): [to,html].
    //   everything else: a single html/text string.
    "var payload;"
    "if(k===5){payload='';}"
    "else if(k===7){payload=[vi(),vi()];}"
    "else if(k===4){var pr=str();payload=[pr,str()];}"
    "else if(k===8){var to=vi();payload=[to,str()];}"
    "else{payload=str();}"
    "ops.push([k,path,payload]);}"
    "return{css:css,ops:ops};}"
    "function frag(html){var d=document.createElement('div');d.innerHTML=html;return d.firstChild;}"
    "function at(p){var e=R.firstElementChild;if(p==='')return e;var q=p.split('.');"
    "for(var i=0;i<q.length;i++){e=e.childNodes[+q[i]];if(!e)return null;}return e;}"
    "function parent(p){var q=p.split('.');q.pop();return at(q.join('.'));}"
    // Copy attributes from `nw` onto the live element `e`, deleting stale ones.
    // Reuses the SAME DOM node, so focus, caret and selection survive an update.
    "function morphAttrs(e,nw){"
    "for(var i=e.attributes.length-1;i>=0;i--){var a=e.attributes[i].name;if(!nw.hasAttribute(a))e.removeAttribute(a);}"
    "for(var j=0;j<nw.attributes.length;j++){var b=nw.attributes[j];if(e.getAttribute(b.name)!==b.value)e.setAttribute(b.name,b.value);}}"
    // Is `e` a form control the user could be editing right now?
    "function editable(e){var t=e.tagName;return t==='INPUT'||t==='TEXTAREA'||t==='SELECT';}"
    // In-place update of a control: morph attrs, but DON'T fight the user for
    // the value of the field they're focused in (the DOM already holds it).
    "function morphControl(e,nw){var focused=(document.activeElement===e);morphAttrs(e,nw);"
    "if(!focused){if(nw.tagName==='SELECT'){e.value=nw.value;}"
    "else if('value'in nw&&e.value!==nw.value){e.value=nw.value;}"
    "if('checked'in nw&&e.checked!==nw.checked)e.checked=nw.checked;"
    "if('textContent'in nw&&nw.tagName==='TEXTAREA'&&e.value!==nw.value)e.value=nw.value;}}"
    // apply ONE op. Each opcode maps to exactly one DOM mutation on exactly one
    // channel (see docs/internals/wire-protocol-design.md). Opcodes:
    //   0 replace   swap the whole element for a fresh subtree
    //   1 set_shell morph attrs+class in place; body/children untouched
    //   2 set_text  set textContent (span label, button label)
    //   3 set_inner set innerHTML (markup/SVG body)
    //   4 set_prop  set ONE reflected property [prop,value] (value/checked/src)
    //   5 remove / 6 insert(append) / 7 move / 8 insert_at   structural
    //   9 paint     full-surface repaint
    "function setProp(e,pr,v){"
    // 'checked' is a boolean property; '' clears it. Don't fight a focused field.
    "if(pr==='checked'){e.checked=!!v;return;}"
    "if(pr==='value'){if(document.activeElement!==e)e.value=v;return;}"
    "if(pr==='src'){e.src=v;e.setAttribute('src',v);return;}"
    "e.setAttribute(pr,v);}"
    "function apply(op){var k=op[0],p=op[1],e=at(p);"
    "if(k===9){R.innerHTML=op[2];return;}"
    "if(!e&&k!==6&&k!==8)return;"
    // 1 set_shell: morph attributes in place so focus/caret/scroll survive; the
    // body (text/inner/children) is carried by its own ops, so we NEVER touch it
    // here. Fall back to a full replace only if the tag itself changed.
    "if(k===1){var nw=frag(op[2]);if(!nw){return;}if(nw.tagName!==e.tagName){e.replaceWith(nw);}else{morphAttrs(e,nw);}return;}"
    "if(k===2){e.textContent=op[2];return;}"
    "if(k===3){e.innerHTML=op[2];return;}"
    "if(k===4){setProp(e,op[2][0],op[2][1]);return;}"
    "if(k===0){e.replaceWith(frag(op[2]));return;}"
    "if(k===5){e.remove();return;}"
    "if(k===6){var pa=at(p);if(pa)pa.appendChild(frag(op[2]));return;}"
    // 8 insert_at: op[2]=[to,html]; insert BEFORE the current child at `to`.
    "if(k===8){var pa=at(p);if(pa){var nd=frag(op[2][1]);var ref=pa.childNodes[op[2][0]];pa.insertBefore(nd,ref||null);}return;}"
    // 7 move: op[2]=[from,to]; detach the child at `from`, reinsert before `to`.
    "if(k===7){var pa=at(p);if(pa){var from=op[2][0],to=op[2][1];var nd=pa.childNodes[from];if(nd){nd.remove();var ref=pa.childNodes[to];pa.insertBefore(nd,ref||null);}}return;}}"
    // — rAF-coalesced paint: queue frames, apply them all in one animation frame —
    // With FLIP: before applying, snapshot the positions of [data-wa-flip]
    // elements; after, animate each from its old box to its new one (a smooth
    // reorder). Freshly-inserted [data-wa-flip] nodes get an entrance instead.
    //
    // PERF: the snapshot forces a synchronous layout (getBoundingClientRect on
    // every flip node), so a live/animated app that repaints 30-60x/s would
    // thrash layout every frame even when nothing reordered. We therefore scan
    // the queued frames FIRST and only take the snapshot when a frame actually
    // carries a structural op (insert/insert_at/remove/move) — a pure attribute/
    // text/path tick (the common animation case) skips FLIP entirely and never
    // reads geometry. This is the single biggest responsiveness win for
    // real-time surfaces.
    "var q=[],raf=0;"
    "function flipSnapshot(){var m={};document.querySelectorAll('[data-wa-flip]').forEach(function(el){var k=el.getAttribute('data-wa-flip');if(k)m[k]=el.getBoundingClientRect();});return m;}"
    "function flipPlay(prev){document.querySelectorAll('[data-wa-flip]').forEach(function(el){var k=el.getAttribute('data-wa-flip');var o=prev[k];var n=el.getBoundingClientRect();"
    "if(!o){el.animate([{opacity:0,transform:'translateY(8px) scale(.98)'},{opacity:1,transform:'none'}],{duration:220,easing:'cubic-bezier(.2,.7,.2,1)'});return;}"
    "var dx=o.left-n.left,dy=o.top-n.top;if(dx||dy){el.animate([{transform:'translate('+dx+'px,'+dy+'px)'},{transform:'none'}],{duration:260,easing:'cubic-bezier(.2,.7,.2,1)'});}});}"
    // does any queued frame carry a structural (reordering) op? only then is a
    // FLIP snapshot worth its forced reflow.
    "function structural(frames){for(var fi=0;fi<frames.length;fi++){var os=frames[fi].ops;for(var i=0;i<os.length;i++){var k=os[i][0];if(k===7||k===8||k===6||k===5||k===0)return true;}}return false;}"
    "function flush(){raf=0;var frames=q;q=[];var willMove=structural(frames);var prev=willMove?flipSnapshot():null;"
    "for(var fi=0;fi<frames.length;fi++){var m=frames[fi];if(m.css)S.textContent+=m.css;"
    "for(var i=0;i<m.ops.length;i++){apply(m.ops[i]);}}"
    "if(prev&&Object.keys(prev).length)flipPlay(prev);}"
    "function paint(m){q.push(m);if(!raf)raf=requestAnimationFrame(flush);}"
    "var ws,started=false;"
    // A stable per-tab session id, kept in sessionStorage so it SURVIVES a
    // reconnect (wifi blip, laptop sleep) but not a fresh tab. The server uses
    // it to rebind to the retained Model instead of resetting to init() — so a
    // dropped connection doesn't wipe a half-filled form or a wizard step.
    "var _sid=sessionStorage.getItem('waya-sid');"
    "if(!_sid){_sid=(Date.now().toString(36)+Math.random().toString(36).slice(2,10));sessionStorage.setItem('waya-sid',_sid);}"
    "function route(){if(ws&&ws.readyState===1)ws.send('@route|'+location.pathname+location.search);}"
    "function connect(){var _wsproto=(location.protocol==='https:'?'wss://':'ws://');"
    "var _wshost=location.host||(location.hostname+':"+std::to_string(port)+"');"
    "ws=new WebSocket(_wsproto+_wshost+'/?r='+encodeURIComponent(location.pathname+location.search)+'&s='+_sid);"
    "ws.binaryType='arraybuffer';"
    "ws.onopen=function(){if(started){S.textContent='';R.innerHTML='';}started=true;route();};"
    // Text frames are runtime control messages (navigation, dev hot-reload);
    // binary frames are paints. This keeps one socket doing input, output, effects.
    "ws.onmessage=function(ev){if(typeof ev.data==='string'){ctl(ev.data);return;}paint(readFrame(ev.data));};"
    "ws.onclose=function(){setTimeout(connect,300);};ws.onerror=function(){try{ws.close()}catch(_){}}}"
    // control: "@nav|<url>" pushes history + re-routes; "@url|<url>" only syncs
    // the address bar (deep-link) without a route; "@build|<id>" is the dev
    // hot-reload signal — if the server's build id changed since we first
    // connected, a rebuild happened, so hard-reload to pick up new shell/JS/CSS.
    "var _build=null;"
    "function ctl(s){var b=s.indexOf('|'),k=s.slice(0,b),v=s.slice(b+1);"
    "if(k==='@build'){if(_build===null)_build=v;else if(_build!==v)location.reload();}"
    "else if(k==='@nav'){history.pushState({},'',v);route();}"
    "else if(k==='@rep'){history.replaceState({},'',v);route();}"
    "else if(k==='@url'){history.pushState({},'',v);}}"
    "window.addEventListener('popstate',route);"
    "connect();"
    "document.addEventListener('click',function(ev){var t=ev.target.closest('[data-tap]');"
    // If a [data-stop] element sits between the click and the tap target, the
    // click was 'inside' (e.g. modal content) — don't fire the outer tap
    // (e.g. a backdrop close). This is on_backdrop / stop() done right.
    "if(t&&ws&&ws.readyState===1){var st=ev.target.closest('[data-stop]');"
    "if(st&&t.contains(st)&&st!==t)return;ev.preventDefault();"
    // Optimistic: an element opted into instant-busy gets [data-busy] the moment
    // it's clicked (disabled + dimmed), cleared automatically when the next
    // paint replaces/updates it. Makes a server round-trip feel instant.
    "if(t.hasAttribute('data-opt'))t.setAttribute('data-busy','1');"
    "ws.send(t.dataset.tap);}});"
    // Ripple: on pointerdown of a [data-wa-ripple] element, spawn an ink circle
    // at the pointer that scales+fades via the wa-ripple keyframe, then removes
    // itself. No Model state — pure client polish.
    "document.addEventListener('pointerdown',function(ev){var t=ev.target.closest&&ev.target.closest('[data-wa-ripple]');if(!t)return;"
    "var r=t.getBoundingClientRect();var d=Math.max(r.width,r.height);var ink=document.createElement('span');ink.className='wa-ripple-ink';"
    "ink.style.width=ink.style.height=d+'px';ink.style.left=(ev.clientX-r.left-d/2)+'px';ink.style.top=(ev.clientY-r.top-d/2)+'px';"
    "ink.style.background=t.getAttribute('data-wa-ripple-color')||'#fff';ink.style.opacity='.35';"
    "t.appendChild(ink);setTimeout(function(){ink.remove();},600);});"
    // tap_pop: on pointerdown of a [data-wa-pop] element, retrigger the wa-pop
    // scale animation immediately — instant tactile feedback with no round-trip.
    // Removing+reflowing+re-adding the class restarts the animation on rapid
    // repeated taps. Pure client polish; no Model state.
    "document.addEventListener('pointerdown',function(ev){var t=ev.target.closest&&ev.target.closest('[data-wa-pop]');if(!t)return;"
    "t.classList.remove('wa-tap-pop-go');void t.offsetWidth;t.classList.add('wa-tap-pop-go');},true);"
    // input/change carry a payload. Checkboxes & radios send their checked
    // state ("true"/"false"); every other control sends its value. So one path
    // serves text, textarea, select, checkbox and radio uniformly.
    "function payload(t){return (t.type==='checkbox'||t.type==='radio')?String(t.checked):t.value;}"
    "document.addEventListener('input',function(ev){var t=ev.target;"
    "if(t.dataset&&t.dataset.input!=null&&ws&&ws.readyState===1){ws.send('i'+t.dataset.input+'|'+payload(t));}});"
    "document.addEventListener('change',function(ev){var t=ev.target;"
    "if(t.dataset&&t.dataset.change!=null&&ws&&ws.readyState===1){ws.send('c'+t.dataset.change+'|'+payload(t));}});"
    // Generic events wired via data-ev-<type>="<msg>[|<arg>]". One delegated
    // listener per type; `e<msg>|<payload>` goes up. Keyboard events carry the
    // key as payload and honor an arg filter (on_key("Enter",..)); form submit
    // serialises the form's named fields; drop carries the dragged payload.
    "function evattr(el,type){return el&&el.dataset?el.dataset['ev'+type[0].toUpperCase()+type.slice(1)]:null;}"
    "function sendev(spec,pl){if(!ws||ws.readyState!==1)return;var bar=spec.indexOf('|');var msg=bar<0?spec:spec.slice(0,bar);ws.send('e'+msg+'|'+pl);}"
    // Key filter: "data-ev-keydown=<token>|<spec>". <spec> is a key name
    // ("Enter") OR a combo like "mod+k", "ctrl+shift+p", "alt+ArrowDown". `mod`
    // means Cmd on macOS / Ctrl elsewhere. Matching is case-insensitive on the
    // final key so "mod+k" fires for K too.
    "function evMatch(spec,ev){var bar=spec.indexOf('|');if(bar<0)return true;var s=spec.slice(bar+1);"
    "if(s.indexOf('+')<0)return s===ev.key;"
    "var parts=s.toLowerCase().split('+');var want=parts.pop();"
    "var needMod=parts.indexOf('mod')>=0,needCtrl=parts.indexOf('ctrl')>=0,needShift=parts.indexOf('shift')>=0,needAlt=parts.indexOf('alt')>=0;"
    "var isMac=/Mac|iPhone|iPad/.test(navigator.platform||navigator.userAgent);"
    "var modOk=needMod?(isMac?ev.metaKey:ev.ctrlKey):true;"
    "if(needMod&&!modOk)return false;if(needCtrl&&!ev.ctrlKey)return false;"
    "if(needShift&&!ev.shiftKey)return false;if(needAlt&&!ev.altKey)return false;"
    "return (ev.key||'').toLowerCase()===want;}"
    // keydown: walk up to the nearest node wiring keydown, honor the key filter.
    "document.addEventListener('keydown',function(ev){var t=ev.target;while(t&&t!==document){var s=evattr(t,'keydown');"
    "if(s!=null&&evMatch(s,ev)){ev.preventDefault();sendev(s,ev.key);return;}t=t.parentElement;}});"
    // Global shortcuts: an element tagged [data-ev-shortcut] fires from ANYWHERE
    // (no focus needed) — for Cmd+K palettes etc. Registered at document level.
    "document.addEventListener('keydown',function(ev){var els=document.querySelectorAll('[data-ev-shortcut]');"
    "for(var i=0;i<els.length;i++){var s=els[i].getAttribute('data-ev-shortcut');if(s&&evMatch('|'+s.split('|')[1],ev)){ev.preventDefault();sendev(s.split('|')[0],ev.key);return;}}},true);"
    // focus/blur are non-bubbling → capture phase.
    "document.addEventListener('focus',function(ev){var s=evattr(ev.target,'focus');if(s!=null)sendev(s,payload(ev.target)||'');},true);"
    "document.addEventListener('blur',function(ev){var s=evattr(ev.target,'blur');if(s!=null)sendev(s,payload(ev.target)||'');},true);"
    // pointer enter/leave (capture; non-bubbling).
    "document.addEventListener('pointerenter',function(ev){var s=evattr(ev.target,'pointerenter');if(s!=null)sendev(s,'');},true);"
    "document.addEventListener('pointerleave',function(ev){var s=evattr(ev.target,'pointerleave');if(s!=null)sendev(s,'');},true);"
    // form submit: gather named fields into a=1&b=2 (URL-encoded).
    "document.addEventListener('submit',function(ev){var f=ev.target.closest('[data-ev-submit]');if(!f)return;ev.preventDefault();"
    "var d=new FormData(f),ps=[];d.forEach(function(v,k){ps.push(encodeURIComponent(k)+'='+encodeURIComponent(v));});"
    "sendev(f.dataset.evSubmit,ps.join('&'));});"
    // drag & drop: dragstart stashes the source's payload (its name attr);
    // dragover allows the drop; drop delivers "<dragged>:<target-arg>" so the app
    // learns WHAT was dropped and WHERE (the target's data-drop-arg, e.g. a column).
    "document.addEventListener('dragstart',function(ev){var t=ev.target.closest('[draggable=true]');if(t){ev.dataTransfer.setData('text/plain',t.getAttribute('name')||'');ev.dataTransfer.effectAllowed='move';}});"
    "document.addEventListener('dragover',function(ev){if(ev.target.closest&&ev.target.closest('[data-ev-drop]'))ev.preventDefault();});"
    "document.addEventListener('drop',function(ev){var t=ev.target.closest('[data-ev-drop]');if(t){ev.preventDefault();var src=ev.dataTransfer.getData('text/plain');var arg=t.getAttribute('data-drop-arg');sendev(t.dataset.evDrop,arg!=null?src+':'+arg:src);}});"
    "})();</script>";
}

} // namespace waya::surface::detail
