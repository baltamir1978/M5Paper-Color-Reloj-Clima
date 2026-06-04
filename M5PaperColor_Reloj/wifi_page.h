// Pagina web del MODO WIFI (gestor de la microSD). En un .h aparte porque el
// preprocesador de .ino de Arduino rompe los raw strings multilinea grandes.
#pragma once

static const char WIFI_HTML[] PROGMEM = R"HTMLPAGE(
<!doctype html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>PaperColor SD</title><style>
:root{--bg:#0e1116;--card:#171b22;--card2:#1e242e;--line:#2a313c;--txt:#e7ebf0;--mut:#94a3b8;--acc:#3b82f6;--acch:#2f6fe0;--red:#ef4444;--grn:#22c55e}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--txt);font:15px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
header{position:sticky;top:0;z-index:5;background:var(--card);padding:14px 16px;box-shadow:0 2px 10px rgba(0,0,0,.45)}
header h1{margin:0;font-size:17px;font-weight:600}
.bc{margin-top:6px;font-size:13px;color:var(--mut);word-break:break-all}
.bc a{color:var(--acc);text-decoration:none}
.tools{display:flex;flex-wrap:wrap;gap:8px;align-items:center;padding:12px 14px;border-bottom:1px solid var(--line)}
.btn{display:inline-flex;align-items:center;gap:6px;background:var(--card2);color:var(--txt);border:1px solid var(--line);border-radius:10px;padding:9px 13px;font-size:14px;cursor:pointer;text-decoration:none;white-space:nowrap}
.btn:hover{background:#2a3240}
.btn.p{background:var(--acc);border-color:var(--acc);color:#fff}
.btn.p:hover{background:var(--acch)}
.btn.d:hover{background:#3a2226;color:var(--red)}
.txt{background:var(--card2);border:1px solid var(--line);color:var(--txt);border-radius:10px;padding:9px 11px;font-size:14px;width:130px}
.list{padding:6px 8px}
.row{display:flex;align-items:center;gap:11px;padding:10px 8px;border-radius:12px}
.row:hover{background:var(--card)}
.row .ic{font-size:22px;width:26px;text-align:center;flex-shrink:0}
.row .nm{flex:1;min-width:0}
.row .nm b{font-weight:500;display:block;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.row .nm a{color:var(--txt);text-decoration:none}
.row .sz{color:var(--mut);font-size:12px;margin-top:2px}
.row .act{display:flex;gap:6px;flex-shrink:0}
.row .act .btn{padding:7px 10px;font-size:13px}
.muted{color:var(--mut);text-align:center;padding:36px 16px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(108px,1fr));gap:8px;padding:12px}
.grid img{width:100%;height:108px;object-fit:cover;border-radius:12px;cursor:pointer;background:var(--card)}
.modal{display:none;position:fixed;inset:0;z-index:20;background:rgba(8,10,14,.97);flex-direction:column}
.modal .bar{display:flex;align-items:center;gap:10px;padding:10px 12px;background:var(--card);border-bottom:1px solid var(--line)}
.modal .bar .t{font-weight:600;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.modal .bd{flex:1;display:flex;justify-content:center;align-items:center;overflow:auto;padding:10px}
.modal .bd img,.modal .bd video{max-width:100%;max-height:100%;border-radius:8px}
#ta{flex:1;width:100%;background:#0a0d12;color:#cdd9f0;border:0;padding:14px;font:13px/1.55 ui-monospace,SFMono-Regular,Consolas,monospace;resize:none;outline:none}
.st{font-size:13px;margin-right:4px}
</style></head><body>

<header><h1>📂 PaperColor &middot; microSD</h1><div class="bc" id="bc">/</div></header>

<div class="tools">
 <label class="btn">⬆ Subir<input type="file" id="f" hidden onchange="up()"></label>
 <input class="txt" id="nd" placeholder="nueva carpeta">
 <button class="btn" onclick="mk()">📁 Crear</button>
 <button class="btn" onclick="gal()">🖼️ Galería</button>
 <button class="btn p" onclick="cf()">⚙️ Config</button>
</div>

<div class="list" id="list"></div>

<div class="modal" id="vwr">
 <div class="bar"><span class="t">Ver</span><button class="btn" onclick="cv()">✕ Cerrar</button></div>
 <div class="bd" id="vc"></div>
</div>

<div class="modal" id="ed">
 <div class="bar"><span class="t" id="ettl">Editar</span><span class="st" id="est"></span>
  <button class="btn p" onclick="sv()">💾 Guardar</button><button class="btn" onclick="ce()">✕ Cerrar</button></div>
 <textarea id="ta" spellcheck="false"></textarea>
</div>

<script>
var cwd="/";
var IMG=/\.(jpg|jpeg|png|gif|bmp|webp)$/i, VID=/\.(mp4|mov|m4v|webm|mkv)$/i,
    AUD=/\.(mp3|m4a|flac|wav|aac|ogg)$/i, TXT=/\.(txt|json|csv|cfg|md|ino|h|c|log|xml|html|js)$/i;
function ec(s){return s.replace(/'/g,"\\'")}
function u(p){return encodeURIComponent(p)}
function icon(n){if(IMG.test(n))return"🖼️";if(VID.test(n))return"🎬";if(AUD.test(n))return"🎵";if(TXT.test(n))return"📄";return"📦"}
function fmt(b){if(b<1024)return b+" B";if(b<1048576)return (b/1024).toFixed(0)+" KB";return (b/1048576).toFixed(1)+" MB"}
function setbc(){var ps=cwd.split("/").filter(Boolean),h="<a onclick=\"ld('/')\">inicio</a>",a="";
 ps.forEach(function(p){a+="/"+p;h+=" / <a onclick=\"ld('"+ec(a)+"')\">"+p+"</a>"});document.getElementById("bc").innerHTML=h}
function ld(d){cwd=d;setbc();
 fetch("/list?dir="+u(d)).then(function(r){return r.json()}).then(function(es){
  es.sort(function(a,b){return (b.dir-a.dir)||a.name.localeCompare(b.name)});var h="";
  es.forEach(function(e){var path=(d==="/"?"":d)+"/"+e.name;
   if(e.dir){
    h+="<div class=row><div class=ic>📁</div><div class=nm><b><a onclick=\"ld('"+ec(path)+"')\">"+e.name+"</a></b></div>"+
       "<div class=act><button class='btn d' onclick=\"rm('"+ec(path)+"')\">🗑️</button></div></div>";
   }else{var act="";
    if(IMG.test(e.name))act+="<button class=btn onclick=\"vw('"+ec(path)+"',0)\">👁 Ver</button>";
    else if(VID.test(e.name))act+="<button class=btn onclick=\"vw('"+ec(path)+"',1)\">▶ Ver</button>";
    else if(TXT.test(e.name))act+="<button class=btn onclick=\"ed('"+ec(path)+"')\">✏️</button>";
    act+="<a class=btn href=\"/dl?path="+u(path)+"&dl=1\">⬇</a>";
    act+="<button class='btn d' onclick=\"rm('"+ec(path)+"')\">🗑️</button>";
    h+="<div class=row><div class=ic>"+icon(e.name)+"</div><div class=nm><b>"+e.name+"</b><div class=sz>"+fmt(e.size)+"</div></div><div class=act>"+act+"</div></div>";
   }});
  document.getElementById("list").innerHTML=h||"<div class=muted>Carpeta vacía</div>";});}
function rm(p){if(confirm("¿Borrar "+p+" ?"))fetch("/rm?path="+u(p)).then(function(){ld(cwd)})}
function mk(){var n=document.getElementById("nd").value.trim();if(!n)return;
 fetch("/mkdir?path="+u((cwd==="/"?"":cwd)+"/"+n)).then(function(){document.getElementById("nd").value="";ld(cwd)})}
function up(){var fi=document.getElementById("f"),f=fi.files[0];if(!f)return;var fd=new FormData();fd.append("f",f);
 fetch("/up?dir="+u(cwd),{method:"POST",body:fd}).then(function(){fi.value="";ld(cwd)})}
function vw(p,v){document.getElementById("vc").innerHTML=v?
  "<video src='/dl?path="+u(p)+"' controls autoplay playsinline></video>":"<img src='/dl?path="+u(p)+"'>";
 document.getElementById("vwr").style.display="flex"}
function gal(){fetch("/list?dir="+u(cwd)).then(function(r){return r.json()}).then(function(es){var g="";
 es.forEach(function(e){if(!e.dir&&IMG.test(e.name)){var p=(cwd==="/"?"":cwd)+"/"+e.name;
  g+="<img loading=lazy src='/dl?path="+u(p)+"' onclick=\"vw('"+ec(p)+"',0)\">"}});
 document.getElementById("vc").innerHTML=g?"<div class=grid>"+g+"</div>":"<div class=muted>No hay imágenes en esta carpeta</div>";
 document.getElementById("vwr").style.display="flex"})}
function cv(){document.getElementById("vc").innerHTML="";document.getElementById("vwr").style.display="none"}
var edPath="";
function setst(m,ok){var e=document.getElementById("est");e.textContent=m||"";e.style.color=ok?"#22c55e":"#ef4444"}
function ed(p){edPath=p;document.getElementById("ettl").textContent=p;setst("");
 fetch("/dl?path="+u(p)).then(function(r){return r.text()}).then(function(t){
  if(p==="/config.json"){try{t=JSON.stringify(JSON.parse(t),null,2)}catch(e){}}
  document.getElementById("ta").value=t;document.getElementById("ed").style.display="flex"})}
function cf(){ed("/config.json")}
function sv(){var t=document.getElementById("ta").value;
 if(edPath==="/config.json"){try{JSON.parse(t)}catch(e){setst("JSON invalido: "+e.message,false);return}}
 var fd=new FormData();fd.append("path",edPath);fd.append("data",t);
 fetch("/save",{method:"POST",body:fd}).then(function(){
  setst(edPath==="/config.json"?"Guardado — reinicia para aplicar":"Guardado ✓",true);ld(cwd)})}
function ce(){document.getElementById("ed").style.display="none"}
ld("/");
</script></body></html>
)HTMLPAGE";
