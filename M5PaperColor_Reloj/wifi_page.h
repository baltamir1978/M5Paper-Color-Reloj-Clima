// Pagina web del MODO WIFI (gestor de la microSD + config grafica).
// En un .h aparte porque el preprocesador de .ino de Arduino rompe los raw strings grandes.
#pragma once

static const char WIFI_HTML[] PROGMEM = R"HTMLPAGE(
<!doctype html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>PaperColor SD</title><style>
:root{--bg:#0e1116;--card:#171b22;--card2:#1e242e;--line:#2a313c;--txt:#e7ebf0;--mut:#94a3b8;--acc:#3b82f6;--acch:#2f6fe0;--red:#ef4444;--grn:#22c55e}
body.light{--bg:#f4f6fa;--card:#ffffff;--card2:#eef1f6;--line:#d7dce4;--txt:#1a2230;--mut:#5a6677;--acc:#2563eb;--acch:#1d4ed8;--red:#dc2626;--grn:#16a34a}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--txt);font:15px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
header{position:sticky;top:0;z-index:5;background:var(--card);padding:14px 16px;box-shadow:0 2px 10px rgba(0,0,0,.45)}
header h1{margin:0;font-size:17px;font-weight:600}
.bc{margin-top:6px;font-size:13px;color:var(--mut);word-break:break-all}
.bc a{color:var(--acc);text-decoration:none}
.tools{display:flex;flex-wrap:wrap;gap:8px;align-items:center;padding:12px 14px;border-bottom:1px solid var(--line)}
.btn{display:inline-flex;align-items:center;gap:6px;background:var(--card2);color:var(--txt);border:1px solid var(--line);border-radius:10px;padding:9px 13px;font-size:14px;cursor:pointer;text-decoration:none;white-space:nowrap}
.btn:hover{background:#2a3240}
.btn.p{background:var(--acc);border-color:var(--acc);color:#fff}.btn.p:hover{background:var(--acch)}
.btn.d{color:var(--red)}
.txt{background:var(--card2);border:1px solid var(--line);color:var(--txt);border-radius:10px;padding:9px 11px;font-size:14px;width:130px}
.list{padding:6px 8px}
.row{display:flex;align-items:center;gap:11px;padding:11px 8px;border-radius:12px}
.row:hover{background:var(--card)}
.row .ic{font-size:22px;width:26px;text-align:center;flex-shrink:0}
.row .nm{flex:1;min-width:0;cursor:pointer}
.row .nm b{font-weight:500;display:block;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.row .nm .sz{color:var(--mut);font-size:12px;margin-top:2px}
.row .mb{flex-shrink:0;background:none;border:0;color:var(--mut);font-size:22px;line-height:1;padding:6px 10px;cursor:pointer;border-radius:8px}
.row .mb:hover{background:var(--card2);color:var(--txt)}
.up{color:var(--acc);font-weight:500}
.muted{color:var(--mut);text-align:center;padding:36px 16px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(108px,1fr));gap:8px;padding:12px}
.grid img{width:100%;height:108px;object-fit:cover;border-radius:12px;cursor:pointer;background:var(--card)}
.modal{display:none;position:fixed;inset:0;z-index:20;background:rgba(8,10,14,.97);flex-direction:column}
.modal .bar{display:flex;align-items:center;gap:8px;padding:10px 12px;background:var(--card);border-bottom:1px solid var(--line)}
.modal .bar .t{font-weight:600;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.modal .bd{flex:1;overflow:auto}
.modal .bdc{flex:1;display:flex;justify-content:center;align-items:center;overflow:auto;padding:10px}
.modal .bdc img,.modal .bdc video{max-width:100%;max-height:100%;border-radius:8px}
.nav{position:absolute;top:50%;transform:translateY(-50%);display:none;background:rgba(0,0,0,.45);color:#fff;border:0;font-size:34px;line-height:1;width:46px;height:66px;border-radius:10px;cursor:pointer}
.nav:active{background:rgba(0,0,0,.7)}.np{left:8px}.nn{right:8px}
#ta{width:100%;height:100%;background:#0a0d12;color:#cdd9f0;border:0;padding:14px;font:13px/1.55 ui-monospace,Consolas,monospace;resize:none;outline:none}
.st{font-size:12px;margin-right:2px;max-width:40vw;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.sec{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px 12px 14px;margin:10px 12px}
.sec h3{margin:0 0 10px;font-size:14px;color:var(--mut);text-transform:uppercase;letter-spacing:.04em}
.fld{margin:9px 0}.fld label{display:block;font-size:12px;color:var(--mut);margin-bottom:4px}
.fld input{width:100%;background:var(--card2);border:1px solid var(--line);color:var(--txt);border-radius:9px;padding:10px;font-size:14px}
.chk{display:flex;align-items:center;gap:9px;margin:8px 0;font-size:14px}.chk input{width:18px;height:18px}
.subrow{display:flex;gap:6px;margin-bottom:7px}.subrow input{flex:1;min-width:0;background:var(--card2);border:1px solid var(--line);color:var(--txt);border-radius:9px;padding:9px;font-size:14px}
.sheet{display:none;position:fixed;inset:0;z-index:30;background:rgba(0,0,0,.55)}
.sheet .panel{position:absolute;left:0;right:0;bottom:0;background:var(--card);border-radius:16px 16px 0 0;padding:8px 8px 16px}
.sheet .panel .h{padding:10px 12px;color:var(--mut);font-size:13px;border-bottom:1px solid var(--line);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.sheet .it{display:block;width:100%;text-align:left;background:none;border:0;color:var(--txt);font-size:16px;padding:15px 14px;border-radius:10px;cursor:pointer}
.sheet .it:hover{background:var(--card2)}.sheet .it.d{color:var(--red)}
</style></head><body>

<header><h1>📂 PaperColor &middot; microSD</h1><div class="bc" id="bc">/</div></header>

<div class="tools">
 <label class="btn">⬆ Subir<input type="file" id="f" hidden onchange="up()"></label>
 <input class="txt" id="nd" placeholder="nueva carpeta">
 <button class="btn" onclick="mk()">📁 Crear</button>
 <button class="btn" onclick="gal()">🖼️ Galería</button>
 <button class="btn p" onclick="cfgOpen()">⚙️ Configuración</button>
 <button class="btn" id="thb" onclick="theme()" title="Tema claro/oscuro" style="margin-left:auto">🌙</button>
</div>

<div class="list" id="list"></div>

<div class="modal" id="vwr"><div class="bar"><span class="t" id="vtt">Ver</span><button class="btn" onclick="cv()">✕ Cerrar</button></div>
 <div class="bdc" id="vc"></div>
 <button class="nav np" id="vprev" onclick="navImg(-1)">‹</button><button class="nav nn" id="vnext" onclick="navImg(1)">›</button></div>

<div class="modal" id="ed"><div class="bar"><span class="t" id="ettl">Editar</span><span class="st" id="est"></span>
 <button class="btn p" onclick="sv()">💾 Guardar</button><button class="btn" onclick="ce()">✕</button></div><textarea id="ta" spellcheck="false"></textarea></div>

<div class="modal" id="cfg"><div class="bar"><span class="t">Configuración</span><span class="st" id="cfst"></span>
 <button class="btn" onclick="cfgRaw()">{ } JSON</button><button class="btn p" onclick="cfgSave()">💾 Guardar</button><button class="btn" onclick="cfgClose()">✕</button></div>
 <div class="bd" id="cfgbd"></div></div>

<div class="sheet" id="sheet" onclick="if(event.target==this)closeSheet()"><div class="panel" id="spanel"></div></div>

<script>
var cwd="/";
var g_imgs=[],g_ii=0,g_isImg=false;
var IMG=/\.(jpg|jpeg|png|gif|bmp|webp)$/i,VID=/\.(mp4|mov|m4v|webm|mkv)$/i,AUD=/\.(mp3|m4a|flac|wav|aac|ogg)$/i,TXT=/\.(txt|json|csv|cfg|md|ino|h|c|log|xml|html|js)$/i;
function ec(s){return s.replace(/'/g,"\\'")}
function at(s){return (s==null?"":(""+s)).replace(/&/g,"&amp;").replace(/"/g,"&quot;").replace(/</g,"&lt;")}
function u(p){return encodeURIComponent(p)}
function kind(n){if(IMG.test(n))return"i";if(VID.test(n))return"v";if(TXT.test(n))return"t";return"o"}
function icon(n){if(IMG.test(n))return"🖼️";if(VID.test(n))return"🎬";if(AUD.test(n))return"🎵";if(TXT.test(n))return"📄";return"📦"}
function fmt(b){if(b<1024)return b+" B";if(b<1048576)return (b/1024).toFixed(0)+" KB";return (b/1048576).toFixed(1)+" MB"}
function parent(d){return d.replace(/\/[^/]+\/?$/,"")||"/"}
function setbc(){var ps=cwd.split("/").filter(Boolean),h="<a onclick=\"ld('/')\">inicio</a>",a="";
 ps.forEach(function(p){a+="/"+p;h+=" / <a onclick=\"ld('"+ec(a)+"')\">"+p+"</a>"});document.getElementById("bc").innerHTML=h}
function ld(d){cwd=d;setbc();
 fetch("/list?dir="+u(d)).then(function(r){return r.json()}).then(function(es){
  es.sort(function(a,b){return (b.dir-a.dir)||a.name.localeCompare(b.name)});var h="";
  g_imgs=es.filter(function(e){return !e.dir&&IMG.test(e.name)}).map(function(e){return (d==="/"?"":d)+"/"+e.name});
  if(d!=="/")h+="<div class=row><div class=ic>⬅</div><div class=nm up onclick=\"ld('"+ec(parent(d))+"')\"><b>.. atrás</b></div></div>";
  es.forEach(function(e){var p=(d==="/"?"":d)+"/"+e.name;
   if(e.dir)h+="<div class=row><div class=ic>📁</div><div class=nm onclick=\"ld('"+ec(p)+"')\"><b>"+e.name+"</b></div><button class=mb onclick=\"menu('"+ec(p)+"','d','"+ec(e.name)+"')\">⋯</button></div>";
   else h+="<div class=row><div class=ic>"+icon(e.name)+"</div><div class=nm onclick=\"prim('"+ec(p)+"','"+kind(e.name)+"')\"><b>"+e.name+"</b><div class=sz>"+fmt(e.size)+"</div></div><button class=mb onclick=\"menu('"+ec(p)+"','"+kind(e.name)+"','"+ec(e.name)+"')\">⋯</button></div>";
  });
  document.getElementById("list").innerHTML=h||"<div class=muted>Carpeta vacía</div>";});}
function prim(p,k){if(k==="i")vw(p,0);else if(k==="v")vw(p,1);else if(k==="t")ed(p);else location.href="/dl?path="+u(p)+"&dl=1"}
// --- menu de acciones por archivo (el borrar deja de estar siempre a la vista) ---
function menu(p,k,name){var h="<div class=h>"+name+"</div>";
 if(k==="d")h+="<button class=it onclick=\"closeSheet();ld('"+ec(p)+"')\">📂 Abrir</button>";
 if(k==="i")h+="<button class=it onclick=\"closeSheet();vw('"+ec(p)+"',0)\">👁 Ver</button>";
 if(k==="v")h+="<button class=it onclick=\"closeSheet();vw('"+ec(p)+"',1)\">▶ Reproducir</button>";
 if(k==="t")h+="<button class=it onclick=\"closeSheet();ed('"+ec(p)+"')\">✏️ Editar</button>";
 if(k!=="d")h+="<a class=it href=\"/dl?path="+u(p)+"&dl=1\" onclick=\"closeSheet()\">⬇ Descargar</a>";
 h+="<button class='it d' onclick=\"closeSheet();rm('"+ec(p)+"')\">🗑️ Borrar</button>";
 document.getElementById("spanel").innerHTML=h;document.getElementById("sheet").style.display="block";}
function closeSheet(){document.getElementById("sheet").style.display="none"}
function rm(p){if(confirm("¿Borrar "+p+" ?"))fetch("/rm?path="+u(p)).then(function(){ld(cwd)})}
function mk(){var n=document.getElementById("nd").value.trim();if(!n)return;fetch("/mkdir?path="+u((cwd==="/"?"":cwd)+"/"+n)).then(function(){document.getElementById("nd").value="";ld(cwd)})}
function up(){var fi=document.getElementById("f"),f=fi.files[0];if(!f)return;var fd=new FormData();fd.append("f",f);fetch("/up?dir="+u(cwd),{method:"POST",body:fd}).then(function(){fi.value="";ld(cwd)})}
function arrows(){var s=(g_isImg&&g_imgs.length>1)?"block":"none";document.getElementById("vprev").style.display=s;document.getElementById("vnext").style.display=s}
function showImg(){var p=g_imgs[g_ii];document.getElementById("vc").innerHTML="<img src='/dl?path="+u(p)+"'>";
 document.getElementById("vtt").textContent=p.split("/").pop()+"  ("+(g_ii+1)+"/"+g_imgs.length+")";arrows()}
function navImg(d){if(!g_isImg||g_imgs.length<2)return;g_ii=(g_ii+d+g_imgs.length)%g_imgs.length;showImg()}
function vw(p,v){var vwr=document.getElementById("vwr");
 if(v){g_isImg=false;document.getElementById("vc").innerHTML="<video src='/dl?path="+u(p)+"' controls autoplay playsinline></video>";
  document.getElementById("vtt").textContent=p.split("/").pop();arrows();vwr.style.display="flex";return}
 g_isImg=true;g_ii=g_imgs.indexOf(p);if(g_ii<0){g_imgs=[p];g_ii=0}showImg();vwr.style.display="flex"}
function gal(){fetch("/list?dir="+u(cwd)).then(function(r){return r.json()}).then(function(es){var g="";
 g_imgs=es.filter(function(e){return !e.dir&&IMG.test(e.name)}).map(function(e){return (cwd==="/"?"":cwd)+"/"+e.name});
 g_imgs.forEach(function(p){g+="<img loading=lazy src='/dl?path="+u(p)+"' onclick=\"vw('"+ec(p)+"',0)\">"});
 document.getElementById("vc").innerHTML=g?"<div class=grid>"+g+"</div>":"<div class=muted>No hay imágenes en esta carpeta</div>";
 g_isImg=false;arrows();document.getElementById("vtt").textContent="Galería";document.getElementById("vwr").style.display="flex"})}
function cv(){g_isImg=false;arrows();document.getElementById("vc").innerHTML="";document.getElementById("vwr").style.display="none"}
function theme(){document.body.classList.toggle("light");var l=document.body.classList.contains("light");
 try{localStorage.setItem("pcTheme",l?"l":"d")}catch(e){}document.getElementById("thb").textContent=l?"☀️":"🌙"}
// --- editor de texto / JSON crudo ---
var edPath="";
function setst(id,m,ok){var e=document.getElementById(id);e.textContent=m||"";e.style.color=ok?"#22c55e":"#ef4444"}
function ed(p){edPath=p;document.getElementById("ettl").textContent=p;setst("est","");
 fetch("/dl?path="+u(p)).then(function(r){return r.text()}).then(function(t){
  if(p==="/config.json"){try{t=JSON.stringify(JSON.parse(t),null,2)}catch(e){}}
  document.getElementById("ta").value=t;document.getElementById("ed").style.display="flex"})}
function sv(){var t=document.getElementById("ta").value;
 if(edPath==="/config.json"){try{JSON.parse(t)}catch(e){setst("est","JSON invalido: "+e.message,false);return}}
 var fd=new FormData();fd.append("path",edPath);fd.append("data",t);
 fetch("/save",{method:"POST",body:fd}).then(function(){setst("est",edPath==="/config.json"?"Guardado, reinicia para aplicar":"Guardado",true);ld(cwd)})}
function ce(){document.getElementById("ed").style.display="none"}
// --- CONFIGURACION GRAFICA (lee/escribe config.json) ---
var g_cfg={};
function wrow(s,p){return "<div class=subrow><input class=wssid placeholder='SSID' value=\""+at(s)+"\"><input class=wpass placeholder='clave' value=\""+at(p)+"\"><button class='btn d' onclick=\"this.closest('.subrow').remove()\">✕</button></div>"}
function lrow(n,m,e){return "<div class=subrow><input class=lname placeholder='Nombre' value=\""+at(n)+"\"><input class=lmun placeholder='municipio' value=\""+at(m)+"\"><input class=lest placeholder='estacion' value=\""+at(e)+"\"><button class='btn d' onclick=\"this.closest('.subrow').remove()\">✕</button></div>"}
function addWifi(){document.getElementById("cfWifi").insertAdjacentHTML("beforeend",wrow("",""))}
function addLoc(){document.getElementById("cfLocs").insertAdjacentHTML("beforeend",lrow("","",""))}
function fld(id,lab,v,t){return "<div class=fld><label>"+lab+"</label><input type='"+(t||"text")+"' id='"+id+"' value=\""+at(v)+"\"></div>"}
function chk(id,lab,v){return "<div class=chk><input type=checkbox id='"+id+"' "+(v?"checked":"")+"><label for='"+id+"'>"+lab+"</label></div>"}
function cfgOpen(){setst("cfst","cargando...",true);
 fetch("/dl?path="+u("/config.json")).then(function(r){return r.text()}).then(function(t){
  try{g_cfg=JSON.parse(t)}catch(e){setst("cfst","config.json invalido; usa { } JSON",false);g_cfg={}}
  var m=g_cfg.modos||{},w=g_cfg.wifi_modo||{};
  var h="";
  h+="<div class=sec><h3>WiFi (redes)</h3><div id=cfWifi></div><button class=btn onclick='addWifi()'>+ red</button></div>";
  h+="<div class=sec><h3>AEMET y localidades</h3>"+fld("c_key","API key AEMET",g_cfg.aemet_api_key||"")+"<div id=cfLocs></div><button class=btn onclick='addLoc()'>+ localidad</button></div>";
  h+="<div class=sec><h3>General</h3>"+fld("c_tz","Zona horaria (TZ)",g_cfg.timezone||"")+fld("c_car","Segundos entre fotos",g_cfg.carousel_seconds||300,"number")+chk("c_rot","Auto-rotar fotos",g_cfg.photo_auto_rotate!==false)+fld("c_deep","Minutos para apagarse (deep sleep)",g_cfg.deep_sleep_minutes||60,"number")+"</div>";
  h+="<div class=sec><h3>Modos activos</h3>"+chk("m_clima","Clima",m.clima!==false)+chk("m_carrusel","Carrusel",m.carrusel!==false)+chk("m_musica","Música",m.musica!==false)+chk("m_libro","Libro",m.libro!==false)+chk("m_wifi","WiFi",m.wifi!==false)+"</div>";
  h+="<div class=sec><h3>Carpetas y fuentes</h3>"+fld("c_fotos","Carpeta fotos",g_cfg.fotos_dir||"/Fotos")+fld("c_mus","Carpeta música",g_cfg.musica_dir||"/Musica")+fld("c_lib","Carpeta libros",g_cfg.libros_dir||"/Libros")+fld("c_ft","Fuente título",g_cfg.font_title||"/fonts/title.vlw")+fld("c_fb","Fuente cuerpo",g_cfg.font_body||"/fonts/body.vlw")+"</div>";
  h+="<div class=sec><h3>Modo WiFi (este gestor)</h3>"+fld("c_ap","SSID del AP",w.ap_ssid||"PaperColor")+fld("c_app","Clave del AP (min 8)",w.ap_pass||"")+fld("c_wu","Usuario web",w.user||"admin")+fld("c_wp","Clave web",w.pass||"")+"</div>";
  document.getElementById("cfgbd").innerHTML=h;
  (g_cfg.wifi||[]).forEach(function(x){document.getElementById("cfWifi").insertAdjacentHTML("beforeend",wrow(x.ssid,x.pass))});
  if(!(g_cfg.wifi||[]).length)addWifi();
  (g_cfg.locations||[]).forEach(function(x){document.getElementById("cfLocs").insertAdjacentHTML("beforeend",lrow(x.name,x.municipio,x.estacion))});
  setst("cfst","");document.getElementById("cfg").style.display="flex";})}
function val(id){return document.getElementById(id).value}
function cfgSave(){
 var o=g_cfg||{};
 o.wifi=[];document.querySelectorAll("#cfWifi .subrow").forEach(function(r){var s=r.querySelector(".wssid").value.trim();if(s)o.wifi.push({ssid:s,pass:r.querySelector(".wpass").value})});
 o.aemet_api_key=val("c_key");o.timezone=val("c_tz");
 o.carousel_seconds=parseInt(val("c_car"))||300;o.photo_auto_rotate=document.getElementById("c_rot").checked;
 o.deep_sleep_minutes=parseInt(val("c_deep"))||60;
 o.modos={clima:document.getElementById("m_clima").checked,carrusel:document.getElementById("m_carrusel").checked,musica:document.getElementById("m_musica").checked,libro:document.getElementById("m_libro").checked,wifi:document.getElementById("m_wifi").checked};
 o.fotos_dir=val("c_fotos");o.musica_dir=val("c_mus");o.libros_dir=val("c_lib");o.font_title=val("c_ft");o.font_body=val("c_fb");
 o.wifi_modo={ap_ssid:val("c_ap"),ap_pass:val("c_app"),user:val("c_wu"),pass:val("c_wp")};
 o.locations=[];document.querySelectorAll("#cfLocs .subrow").forEach(function(r){var n=r.querySelector(".lname").value.trim();if(n)o.locations.push({name:n,municipio:r.querySelector(".lmun").value,estacion:r.querySelector(".lest").value})});
 var fd=new FormData();fd.append("path","/config.json");fd.append("data",JSON.stringify(o,null,2));
 fetch("/save",{method:"POST",body:fd}).then(function(){setst("cfst","Guardado, reinicia para aplicar",true)})}
function cfgRaw(){cfgClose();ed("/config.json")}
function cfgClose(){document.getElementById("cfg").style.display="none"}
// tema guardado + gestos del visor (swipe en movil) + teclado (flechas / Esc)
(function(){try{if(localStorage.getItem("pcTheme")==="l")document.body.classList.add("light")}catch(e){}
 document.getElementById("thb").textContent=document.body.classList.contains("light")?"☀️":"🌙";
 var sx=0,vc=document.getElementById("vc");
 vc.addEventListener("touchstart",function(e){sx=e.changedTouches[0].clientX},{passive:true});
 vc.addEventListener("touchend",function(e){var dx=e.changedTouches[0].clientX-sx;if(g_isImg&&Math.abs(dx)>40)navImg(dx<0?1:-1)},{passive:true});
 document.addEventListener("keydown",function(e){if(document.getElementById("vwr").style.display!=="flex")return;
  if(e.key==="ArrowLeft")navImg(-1);else if(e.key==="ArrowRight")navImg(1);else if(e.key==="Escape")cv()});
})();
ld("/");
</script></body></html>
)HTMLPAGE";
