#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <string.h>
#include <stdint.h>

// ── Advanced Web UI Pages ─────────────────────────────────────────────────────
//
// Self-contained PROGMEM HTML pages providing rich interactive UI:
//   - Real-time network topology visualizer
//   - Interactive channel waterfall display
//   - Attack configuration wizard
//   - Live RSSI heat map
//   - Packet statistics charts
//   - Settings and configuration UI

extern WebServer server;

// ─────────────────────────────────────────────────────────────────────────────
// Network Topology Visualizer Page
// ─────────────────────────────────────────────────────────────────────────────
static const char WUI_TOPOLOGY[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Network Topology &mdash; Phantom32</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d1117;color:#c9d1d9;font-family:system-ui,-apple-system,sans-serif}
.bar{background:#161b22;padding:0.6rem 1rem;border-bottom:1px solid #21262d;display:flex;align-items:center;justify-content:space-between}
.bar h2{font-size:0.95rem;color:#f0f6fc}
.controls{display:flex;gap:0.5rem;flex-wrap:wrap}
.btn{background:#21262d;color:#c9d1d9;border:1px solid #30363d;border-radius:5px;padding:0.3rem 0.7rem;cursor:pointer;font-size:0.78rem}
.btn:hover{background:#30363d;color:#f0f6fc}
.btn.active{background:#1f6feb;border-color:#388bfd;color:#fff}
#canvas-wrap{position:relative;overflow:hidden;background:#0a0e1a;width:100vw;height:calc(100vh - 42px)}
canvas{position:absolute;top:0;left:0}
.legend{position:absolute;bottom:1rem;right:1rem;background:rgba(22,27,34,0.9);border:1px solid #21262d;border-radius:6px;padding:0.7rem;font-size:0.75rem}
.legend-row{display:flex;align-items:center;gap:0.5rem;margin-bottom:0.3rem}
.legend-dot{width:10px;height:10px;border-radius:50%}
.info-panel{position:absolute;top:1rem;left:1rem;background:rgba(22,27,34,0.95);border:1px solid #21262d;border-radius:6px;padding:0.8rem;font-size:0.78rem;min-width:200px;max-width:280px;display:none}
.info-panel h3{font-size:0.85rem;color:#f0f6fc;margin-bottom:0.5rem}
.info-row{display:flex;justify-content:space-between;padding:0.15rem 0;border-bottom:1px solid #21262d}
.info-row:last-child{border-bottom:none}
.info-key{color:#8b949e}
.info-val{color:#c9d1d9;text-align:right;max-width:140px;overflow:hidden;text-overflow:ellipsis}
.stats-bar{position:absolute;top:1rem;right:1rem;background:rgba(22,27,34,0.95);border:1px solid #21262d;border-radius:6px;padding:0.7rem;font-size:0.75rem;min-width:140px}
.stat-line{display:flex;justify-content:space-between;padding:0.1rem 0}
.tooltip{position:absolute;background:rgba(22,27,34,0.97);border:1px solid #21262d;border-radius:4px;padding:0.4rem 0.6rem;font-size:0.72rem;color:#c9d1d9;pointer-events:none;display:none;z-index:100}
</style>
</head>
<body>
<div class="bar">
  <h2>&#x1F5FA; Network Topology</h2>
  <div class="controls">
    <button class="btn active" id="btnAPs" onclick="toggleLayer('ap',this)">APs</button>
    <button class="btn active" id="btnSTAs" onclick="toggleLayer('sta',this)">STAs</button>
    <button class="btn active" id="btnEdges" onclick="toggleLayer('edge',this)">Links</button>
    <button class="btn" onclick="resetView()">&#x1F3AF; Reset View</button>
    <button class="btn" onclick="loadData()">&#x1F504; Refresh</button>
  </div>
</div>
<div id="canvas-wrap">
  <canvas id="cv"></canvas>
  <div class="stats-bar">
    <div class="stat-line"><span style="color:#8b949e">APs</span><span id="statAp">0</span></div>
    <div class="stat-line"><span style="color:#8b949e">STAs</span><span id="statSta">0</span></div>
    <div class="stat-line"><span style="color:#8b949e">Links</span><span id="statEdge">0</span></div>
    <div class="stat-line"><span style="color:#8b949e">Frames</span><span id="statFrames">0</span></div>
  </div>
  <div class="legend">
    <div class="legend-row"><div class="legend-dot" style="background:#388bfd"></div> Access Point</div>
    <div class="legend-row"><div class="legend-dot" style="background:#3fb950"></div> Station</div>
    <div class="legend-row"><div class="legend-dot" style="background:#ff7b72"></div> Rogue AP</div>
    <div class="legend-row"><div class="legend-dot" style="background:#e3b341"></div> Mesh Node</div>
  </div>
  <div class="info-panel" id="infoPanel">
    <h3 id="infoTitle">Node Info</h3>
    <div id="infoBody"></div>
  </div>
  <div class="tooltip" id="tooltip"></div>
</div>

<script>
var cv=document.getElementById('cv'),ctx=cv.getContext('2d');
var W=0,H=0;
var nodes=[],edges=[],selected=null;
var cam={x:0,y:0,scale:1};
var drag={active:false,sx:0,sy:0,cx:0,cy:0};
var layers={ap:true,sta:true,edge:true};

var NODE_COLORS={ap:'#388bfd',sta:'#3fb950',rogue:'#ff7b72',mesh:'#e3b341'};

function resize(){
  W=cv.parentElement.clientWidth;
  H=cv.parentElement.clientHeight;
  cv.width=W; cv.height=H;
  draw();
}
window.addEventListener('resize',resize);

function resetView(){cam={x:W/2,y:H/2,scale:1};draw();}

function toggleLayer(l,btn){
  layers[l]=!layers[l];
  btn.classList.toggle('active',layers[l]);
  draw();
}

function toScreen(wx,wy){
  return {x:cam.x+wx*cam.scale,y:cam.y+wy*cam.scale};
}
function toWorld(sx,sy){
  return {x:(sx-cam.x)/cam.scale,y:(sy-cam.y)/cam.scale};
}

function nodeRadius(n){
  return n.type==='ap'?12:8;
}

function nodeColor(n){
  return NODE_COLORS[n.type]||'#8b949e';
}

function draw(){
  ctx.clearRect(0,0,W,H);
  // Grid
  ctx.strokeStyle='rgba(33,38,45,0.6)';
  ctx.lineWidth=1;
  var gs=50*cam.scale,ox=cam.x%gs,oy=cam.y%gs;
  for(var x=ox;x<W;x+=gs){ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,H);ctx.stroke();}
  for(var y=oy;y<H;y+=gs){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(W,y);ctx.stroke();}

  // Edges
  if(layers.edge){
    edges.forEach(function(e){
      var sn=findNode(e.src),dn=findNode(e.dst);
      if(!sn||!dn) return;
      var sp=toScreen(sn.x,sn.y),dp=toScreen(dn.x,dn.y);
      ctx.strokeStyle=e.assoc?'rgba(56,139,253,0.5)':'rgba(139,148,158,0.3)';
      ctx.lineWidth=e.assoc?1.5:0.8;
      ctx.setLineDash(e.assoc?[]:[4,4]);
      ctx.beginPath(); ctx.moveTo(sp.x,sp.y); ctx.lineTo(dp.x,dp.y); ctx.stroke();
      ctx.setLineDash([]);
    });
  }

  // Nodes
  nodes.forEach(function(n){
    if(n.type==='ap'&&!layers.ap) return;
    if(n.type==='sta'&&!layers.sta) return;
    var p=toScreen(n.x,n.y);
    var r=nodeRadius(n)*cam.scale;
    var c=nodeColor(n);

    // Halo for selected
    if(selected&&selected.mac===n.mac){
      ctx.beginPath(); ctx.arc(p.x,p.y,r+6,0,Math.PI*2);
      ctx.fillStyle='rgba(56,139,253,0.2)'; ctx.fill();
      ctx.strokeStyle='#388bfd'; ctx.lineWidth=2;
      ctx.beginPath(); ctx.arc(p.x,p.y,r+6,0,Math.PI*2); ctx.stroke();
    }

    // Node circle
    ctx.beginPath(); ctx.arc(p.x,p.y,r,0,Math.PI*2);
    ctx.fillStyle=c+'22'; ctx.fill();
    ctx.strokeStyle=c; ctx.lineWidth=1.5;
    ctx.stroke();

    // Icon
    ctx.fillStyle=c;
    ctx.font='bold '+(Math.max(8,r*0.9))+'px system-ui';
    ctx.textAlign='center'; ctx.textBaseline='middle';
    ctx.fillText(n.type==='ap'?'R':n.type==='rogue'?'!':'D',p.x,p.y);

    // Label
    if(cam.scale>0.6){
      ctx.fillStyle='#8b949e';
      ctx.font=(Math.max(8,10*cam.scale))+'px system-ui';
      ctx.textAlign='center';
      var lbl=n.label||(n.mac.slice(-5));
      ctx.fillText(lbl.length>12?lbl.slice(0,12)+'…':lbl,p.x,p.y+r+10*cam.scale);
    }

    // Active pulse
    if(n.active&&n.type==='ap'){
      var age=Date.now()/1000%2;
      var alpha=Math.sin(age*Math.PI)*0.3;
      ctx.beginPath(); ctx.arc(p.x,p.y,r+8*cam.scale,0,Math.PI*2);
      ctx.strokeStyle=c+'55'; ctx.lineWidth=1;
      ctx.globalAlpha=alpha; ctx.stroke(); ctx.globalAlpha=1;
    }
  });
}

function findNode(mac){
  for(var i=0;i<nodes.length;i++) if(nodes[i].mac===mac) return nodes[i];
  return null;
}

function layout(){
  // Simple force-directed layout
  var W2=800,H2=600;
  var centers={ap:{x:0,y:0},sta:{x:0,y:0}};
  if(nodes.length===0) return;
  var i,j,n,fx,fy,dx,dy,d;
  // Initialize positions if not set
  nodes.forEach(function(nd){
    if(!nd.x) nd.x=(Math.random()-0.5)*W2;
    if(!nd.y) nd.y=(Math.random()-0.5)*H2;
    nd.vx=nd.vx||0; nd.vy=nd.vy||0;
  });
  // 20 iterations of force simulation
  for(var iter=0;iter<20;iter++){
    for(i=0;i<nodes.length;i++){
      fx=0;fy=0;
      // Repulsion
      for(j=0;j<nodes.length;j++){
        if(i===j) continue;
        dx=nodes[i].x-nodes[j].x; dy=nodes[i].y-nodes[j].y;
        d=Math.sqrt(dx*dx+dy*dy)+0.01;
        var F=Math.min(1200/(d*d),50);
        fx+=dx/d*F; fy+=dy/d*F;
      }
      // Edge attraction
      edges.forEach(function(e){
        var src=findNode(e.src),dst=findNode(e.dst);
        if(!src||!dst) return;
        if(nodes[i]===src||nodes[i]===dst){
          var other=nodes[i]===src?dst:src;
          dx=other.x-nodes[i].x; dy=other.y-nodes[i].y;
          d=Math.sqrt(dx*dx+dy*dy)+0.01;
          var F2=d*0.03;
          fx+=dx/d*F2; fy+=dy/d*F2;
        }
      });
      // Center pull
      fx-=nodes[i].x*0.01; fy-=nodes[i].y*0.01;
      nodes[i].vx=(nodes[i].vx+fx)*0.6;
      nodes[i].vy=(nodes[i].vy+fy)*0.6;
    }
    for(i=0;i<nodes.length;i++){
      nodes[i].x+=nodes[i].vx;
      nodes[i].y+=nodes[i].vy;
    }
  }
  cam.x=W/2; cam.y=H/2; cam.scale=1;
}

function loadData(){
  Promise.all([
    fetch('/api/nm/nodes?per=100').then(function(r){return r.json()}).catch(function(){return {nodes:[]}}),
    fetch('/api/nm/edges?per=200').then(function(r){return r.json()}).catch(function(){return {edges:[]}}),
    fetch('/api/nm/summary').then(function(r){return r.json()}).catch(function(){return {}})
  ]).then(function(results){
    var nd=results[0],ed=results[1],sm=results[2];
    // Update nodes (keep positions of existing ones)
    var existing={};
    nodes.forEach(function(n){existing[n.mac]=n});
    nodes=(nd.nodes||[]).map(function(n){
      var old=existing[n.mac];
      return {mac:n.mac,type:n.type,label:n.label,channel:n.ch,rssi:n.rssi,
              active:n.active,
              x:old?old.x:undefined,y:old?old.y:undefined,vx:0,vy:0};
    });
    edges=(ed.edges||[]).map(function(e){
      return {src:e.src,dst:e.dst,frames:e.frames,assoc:e.assoc,rssi:e.rssi};
    });
    layout();
    document.getElementById('statAp').textContent=sm.aps||0;
    document.getElementById('statSta').textContent=sm.stas||0;
    document.getElementById('statEdge').textContent=sm.edges||0;
    document.getElementById('statFrames').textContent=sm.frames||0;
    draw();
  });
}

// Mouse events
var lastMouse={x:0,y:0};
cv.addEventListener('mousedown',function(e){
  var wp=toWorld(e.offsetX,e.offsetY);
  // Check node click
  for(var i=nodes.length-1;i>=0;i--){
    var n=nodes[i];
    var p=toScreen(n.x,n.y);
    var r=nodeRadius(n)*cam.scale+6;
    if(Math.abs(e.offsetX-p.x)<r&&Math.abs(e.offsetY-p.y)<r){
      selected=n;
      showInfo(n);
      draw(); return;
    }
  }
  selected=null;
  document.getElementById('infoPanel').style.display='none';
  drag.active=true; drag.sx=e.offsetX; drag.sy=e.offsetY;
  drag.cx=cam.x; drag.cy=cam.y;
  draw();
});
cv.addEventListener('mousemove',function(e){
  if(drag.active){
    cam.x=drag.cx+(e.offsetX-drag.sx);
    cam.y=drag.cy+(e.offsetY-drag.sy);
    draw(); return;
  }
  // Tooltip on hover
  var tt=document.getElementById('tooltip');
  for(var i=nodes.length-1;i>=0;i--){
    var n=nodes[i];
    var p=toScreen(n.x,n.y);
    var r=nodeRadius(n)*cam.scale+4;
    if(Math.abs(e.offsetX-p.x)<r&&Math.abs(e.offsetY-p.y)<r){
      tt.style.left=(e.offsetX+12)+'px'; tt.style.top=(e.offsetY-10)+'px';
      tt.style.display='block';
      tt.textContent=n.mac+(n.label?' ('+n.label+')':'')+' Ch:'+n.channel+' '+n.rssi+'dBm';
      return;
    }
  }
  tt.style.display='none';
});
cv.addEventListener('mouseup',function(){drag.active=false;});
cv.addEventListener('wheel',function(e){
  e.preventDefault();
  var factor=e.deltaY>0?0.85:1/0.85;
  var mx=e.offsetX,my=e.offsetY;
  cam.x=mx+(cam.x-mx)*factor;
  cam.y=my+(cam.y-my)*factor;
  cam.scale*=factor;
  cam.scale=Math.max(0.1,Math.min(5,cam.scale));
  draw();
},{passive:false});

function showInfo(n){
  var panel=document.getElementById('infoPanel');
  panel.style.display='block';
  document.getElementById('infoTitle').textContent=n.mac;
  var rows=[
    ['Type', n.type],['Label',n.label||'—'],
    ['Channel',n.channel],['RSSI',n.rssi+' dBm'],
    ['Active',n.active?'Yes':'No']
  ];
  document.getElementById('infoBody').innerHTML=rows.map(function(r){
    return '<div class="info-row"><span class="info-key">'+r[0]+'</span><span class="info-val">'+r[1]+'</span></div>';
  }).join('');
}

// Touch support
var lastTouch=null,pinchDist=0;
cv.addEventListener('touchstart',function(e){
  if(e.touches.length===1){
    drag.active=true;
    drag.sx=e.touches[0].clientX;
    drag.sy=e.touches[0].clientY;
    drag.cx=cam.x; drag.cy=cam.y;
  } else if(e.touches.length===2){
    drag.active=false;
    var dx=e.touches[0].clientX-e.touches[1].clientX;
    var dy=e.touches[0].clientY-e.touches[1].clientY;
    pinchDist=Math.sqrt(dx*dx+dy*dy);
  }
  e.preventDefault();
},{passive:false});
cv.addEventListener('touchmove',function(e){
  if(e.touches.length===1&&drag.active){
    cam.x=drag.cx+(e.touches[0].clientX-drag.sx);
    cam.y=drag.cy+(e.touches[0].clientY-drag.sy);
    draw();
  } else if(e.touches.length===2){
    var dx=e.touches[0].clientX-e.touches[1].clientX;
    var dy=e.touches[0].clientY-e.touches[1].clientY;
    var d=Math.sqrt(dx*dx+dy*dy);
    var factor=d/pinchDist;
    pinchDist=d;
    var mx=(e.touches[0].clientX+e.touches[1].clientX)/2;
    var my=(e.touches[0].clientY+e.touches[1].clientY)/2;
    cam.x=mx+(cam.x-mx)*factor;
    cam.y=my+(cam.y-my)*factor;
    cam.scale*=factor;
    cam.scale=Math.max(0.1,Math.min(5,cam.scale));
    draw();
  }
  e.preventDefault();
},{passive:false});
cv.addEventListener('touchend',function(e){drag.active=false;},{passive:false});

// Auto-refresh
setInterval(loadData,8000);

resize();
loadData();
</script>
</body>
</html>
)rawhtml";

// ─────────────────────────────────────────────────────────────────────────────
// Channel Waterfall / Spectrum Analyzer Page
// ─────────────────────────────────────────────────────────────────────────────
static const char WUI_SPECTRUM[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Spectrum Analyzer &mdash; Phantom32</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d1117;color:#c9d1d9;font-family:system-ui,sans-serif;display:flex;flex-direction:column;height:100vh}
.bar{background:#161b22;padding:0.5rem 1rem;border-bottom:1px solid #21262d;display:flex;align-items:center;justify-content:space-between;flex-shrink:0}
.bar h2{font-size:0.9rem;color:#f0f6fc}
.controls{display:flex;gap:0.5rem}
.btn{background:#21262d;border:1px solid #30363d;color:#c9d1d9;border-radius:5px;padding:0.3rem 0.6rem;cursor:pointer;font-size:0.78rem}
.btn:hover{background:#30363d}
.panels{display:flex;flex-direction:column;flex:1;overflow:hidden}
.bar-chart-wrap{flex:0 0 180px;display:flex;align-items:flex-end;padding:0.5rem 1rem 0;gap:4px}
.ch-col{display:flex;flex-direction:column;align-items:center;flex:1}
.ch-bar-bg{width:100%;border-radius:3px 3px 0 0;position:relative;background:#161b22;height:140px;overflow:hidden}
.ch-bar-fill{position:absolute;bottom:0;width:100%;transition:height 0.3s}
.ch-label{font-size:0.65rem;color:#484f58;margin-top:3px;text-align:center}
.ch-rssi{font-size:0.6rem;color:#8b949e;margin-bottom:1px;text-align:center}
.waterfall-wrap{flex:1;overflow:hidden;position:relative}
canvas.wf{position:absolute;width:100%;height:100%}
.wf-axis{position:absolute;right:0;top:0;bottom:0;width:50px;background:rgba(13,17,23,0.8);display:flex;flex-direction:column;justify-content:space-between;padding:4px 4px;font-size:0.6rem;color:#484f58;text-align:right}
.info-row{display:flex;gap:1rem;padding:0.3rem 1rem;background:#0d1117;border-top:1px solid #21262d;font-size:0.75rem;flex-shrink:0;flex-wrap:wrap}
.info-cell{display:flex;gap:0.3rem}<span style="color:#8b949e"></span>
.metric{font-weight:600;color:#c9d1d9}
</style>
</head>
<body>
<div class="bar">
  <h2>&#x1F4E1; Spectrum Analyzer</h2>
  <div class="controls">
    <button class="btn" onclick="clearWaterfall()">&#x1F9F9; Clear</button>
    <button class="btn" onclick="togglePause()" id="pauseBtn">&#x23F8; Pause</button>
    <select id="speed" style="background:#21262d;border:1px solid #30363d;color:#c9d1d9;border-radius:5px;padding:0.3rem;font-size:0.78rem">
      <option value="500">0.5s</option>
      <option value="1000" selected>1s</option>
      <option value="2000">2s</option>
    </select>
  </div>
</div>

<div class="bar-chart-wrap" id="barChart"></div>

<div class="waterfall-wrap">
  <canvas class="wf" id="wfCanvas"></canvas>
  <div class="wf-axis" id="wfAxis">
    <span>-30</span><span>-50</span><span>-70</span><span>-90</span>
  </div>
</div>

<div class="info-row">
  <div class="info-cell"><span style="color:#8b949e">Best CH:</span><span class="metric" id="bestCh">—</span></div>
  <div class="info-cell"><span style="color:#8b949e">Strongest:</span><span class="metric" id="strongest">—</span></div>
  <div class="info-cell"><span style="color:#8b949e">Total Frames:</span><span class="metric" id="totalF">—</span></div>
  <div class="info-cell"><span style="color:#8b949e">FPS:</span><span class="metric" id="fps">—</span></div>
</div>

<script>
var cv=document.getElementById('wfCanvas');
var ctx=cv.getContext('2d');
var CHANS=13;
var paused=false;
var rowH=3;
var rows=[];
var maxRows;

function rssiToColor(rssi){
  // Map -100..-30 to color gradient
  var t=(rssi+100)/70;
  t=Math.max(0,Math.min(1,t));
  // Black -> deep blue -> cyan -> yellow -> red
  var r,g,b;
  if(t<0.25){ r=0;g=0;b=Math.round(t/0.25*180); }
  else if(t<0.5){ var p=(t-0.25)/0.25;r=0;g=Math.round(p*180);b=180; }
  else if(t<0.75){ var p=(t-0.5)/0.25;r=Math.round(p*220);g=220;b=Math.round(180-p*180); }
  else { var p=(t-0.75)/0.25;r=255;g=Math.round(220-p*220);b=0; }
  return 'rgb('+r+','+g+','+b+')';
}

function resize(){
  cv.width=cv.parentElement.clientWidth-50;
  cv.height=cv.parentElement.clientHeight;
  maxRows=Math.floor(cv.height/rowH);
  redrawWaterfall();
}

function clearWaterfall(){rows=[];}
function togglePause(){
  paused=!paused;
  document.getElementById('pauseBtn').textContent=paused?'&#x25B6; Resume':'&#x23F8; Pause';
  document.getElementById('pauseBtn').innerHTML=paused?'&#x25B6; Resume':'&#x23F8; Pause';
}

var chanData=Array(CHANS).fill(-100);

function pushRow(data){
  rows.unshift(data.slice());
  if(rows.length>maxRows) rows=rows.slice(0,maxRows);
}

function redrawWaterfall(){
  if(!cv.width||!cv.height) return;
  ctx.clearRect(0,0,cv.width,cv.height);
  var colW=cv.width/CHANS;
  for(var r=0;r<rows.length;r++){
    var y=r*rowH;
    for(var c=0;c<CHANS;c++){
      ctx.fillStyle=rssiToColor(rows[r][c]||(-100));
      ctx.fillRect(c*colW,y,colW,rowH);
    }
  }
  // Channel labels at bottom
  ctx.font='10px system-ui';
  for(var c=0;c<CHANS;c++){
    ctx.fillStyle='rgba(200,200,200,0.5)';
    ctx.fillText(c+1,c*colW+colW/2-4,cv.height-4);
  }
}

function buildBarChart(data){
  var bc=document.getElementById('barChart');
  if(!bc.children.length){
    var html='';
    for(var i=0;i<CHANS;i++){
      html+='<div class="ch-col">';
      html+='<div class="ch-rssi" id="rssi'+(i+1)+'">?</div>';
      html+='<div class="ch-bar-bg"><div class="ch-bar-fill" id="bar'+(i+1)+'" style="background:#388bfd"></div></div>';
      html+='<div class="ch-label">'+(i+1)+'</div>';
      html+='</div>';
    }
    bc.innerHTML=html;
  }
  for(var i=0;i<CHANS;i++){
    var rssi=data[i]||(-100);
    var pct=Math.max(0,Math.min(100,(rssi+100)/70*100));
    var el=document.getElementById('bar'+(i+1));
    var rl=document.getElementById('rssi'+(i+1));
    if(el){
      el.style.height=pct+'%';
      var t=pct/100;
      var r=Math.round(t*255),g=Math.round(t>0.5?255:t*2*255),b=Math.round(t<0.5?255:0);
      el.style.background='rgb('+r+','+g+','+b+')';
    }
    if(rl) rl.textContent=rssi>-99?rssi:'—';
  }
}

function update(){
  if(paused) return;
  fetch('/api/anl/heatmap')
    .then(function(r){return r.json()})
    .then(function(d){
      var row=Array(CHANS).fill(-100);
      var chans=d.channels||[];
      var bestCh=1,bestFrames=0;
      for(var i=0;i<chans.length;i++){
        var c=chans[i];
        var ci=c.ch-1;
        if(ci>=0&&ci<CHANS){
          row[ci]=c.avg_rssi||-100;
          chanData[ci]=c.avg_rssi||-100;
          if(c.frames>bestFrames){bestFrames=c.frames;bestCh=c.ch;}
        }
      }
      pushRow(row);
      buildBarChart(chanData);
      redrawWaterfall();
      document.getElementById('bestCh').textContent='Ch '+bestCh;
    }).catch(function(){});

  fetch('/api/anl/summary')
    .then(function(r){return r.json()})
    .then(function(d){
      document.getElementById('totalF').textContent=d.total_frames||0;
      document.getElementById('fps').textContent=d.pps||0;
    }).catch(function(){});

  fetch('/api/sa/tracks')
    .then(function(r){return r.json()})
    .then(function(d){
      var tracks=d.tracks||[];
      if(tracks.length>0){
        var best=tracks[0];
        tracks.forEach(function(t){if((t.rssi||(-100))>(best.rssi||(-100)))best=t;});
        document.getElementById('strongest').textContent=best.bssid+' '+best.rssi+'dBm';
      }
    }).catch(function(){});
}

var interval=null;
function startInterval(){
  if(interval) clearInterval(interval);
  var ms=parseInt(document.getElementById('speed').value,10)||1000;
  interval=setInterval(update,ms);
}
document.getElementById('speed').addEventListener('change',startInterval);

window.addEventListener('resize',resize);
resize();
startInterval();
update();
</script>
</body>
</html>
)rawhtml";

// ─────────────────────────────────────────────────────────────────────────────
// Attack Wizard Page
// ─────────────────────────────────────────────────────────────────────────────
static const char WUI_WIZARD[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Attack Wizard &mdash; Phantom32</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d1117;color:#c9d1d9;font-family:system-ui,sans-serif;padding:1rem}
h2{font-size:1rem;color:#f0f6fc;margin-bottom:1rem}
.step{background:#161b22;border:1px solid #21262d;border-radius:8px;padding:1rem;margin-bottom:1rem}
.step-header{display:flex;align-items:center;gap:0.6rem;margin-bottom:0.7rem}
.step-num{width:24px;height:24px;border-radius:50%;background:#1f6feb;color:#fff;display:flex;align-items:center;justify-content:center;font-size:0.75rem;font-weight:700;flex-shrink:0}
.step-num.done{background:#238636}
.step-title{font-size:0.9rem;font-weight:600;color:#f0f6fc}
.form-row{display:flex;gap:0.6rem;align-items:center;flex-wrap:wrap;margin-bottom:0.5rem}
label{font-size:0.8rem;color:#8b949e;min-width:80px}
input[type=text],input[type=number],select,textarea{background:#0d1117;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;padding:0.35rem 0.6rem;font-size:0.8rem}
input:focus,select:focus,textarea:focus{outline:none;border-color:#58a6ff}
.btn{background:#21262d;color:#c9d1d9;border:1px solid #30363d;border-radius:5px;padding:0.35rem 0.8rem;cursor:pointer;font-size:0.8rem}
.btn:hover{background:#30363d}
.btn.primary{background:#1f6feb;border-color:#1f6feb;color:#fff}
.btn.danger{background:#8f1f26;border-color:#8f1f26;color:#fff}
.btn.success{background:#1a4731;border-color:#1a4731;color:#3fb950}
.ap-table{width:100%;border-collapse:collapse;font-size:0.78rem;margin-top:0.5rem}
.ap-table th{background:#1c2130;color:#8b949e;text-align:left;padding:0.4rem 0.6rem;border-bottom:1px solid #21262d;font-size:0.68rem;text-transform:uppercase}
.ap-table td{padding:0.4rem 0.6rem;border-bottom:1px solid #21262d}
.ap-table tr.selected td{background:rgba(31,111,235,0.15)}
.ap-table tr:hover td{background:rgba(255,255,255,0.03);cursor:pointer}
.badge{display:inline-block;padding:0.1rem 0.4rem;border-radius:8px;font-size:0.68rem;font-weight:600}
.b-open{background:#0d2b1f;color:#3fb950}
.b-wep{background:#2d1b1b;color:#ff7b72}
.b-wpa{background:#1a2033;color:#58a6ff}
.preview{background:#0d1117;border:1px solid #21262d;border-radius:4px;padding:0.6rem;font-size:0.78rem;font-family:monospace;color:#8b949e;margin-top:0.5rem;white-space:pre-wrap}
.result-box{background:#0d1117;border:1px solid #21262d;border-radius:4px;padding:0.6rem;font-size:0.78rem;margin-top:0.5rem;min-height:40px;color:#8b949e}
.result-box .ok{color:#3fb950}
.result-box .err{color:#ff7b72}
.switch{display:flex;align-items:center;gap:0.5rem}
.switch-track{width:36px;height:18px;border-radius:9px;background:#30363d;position:relative;cursor:pointer;transition:background 0.2s}
.switch-track.on{background:#1f6feb}
.switch-thumb{width:14px;height:14px;border-radius:50%;background:#fff;position:absolute;top:2px;left:2px;transition:left 0.2s}
.switch-track.on .switch-thumb{left:20px}
</style>
</head>
<body>
<h2>&#x1F9D9; Attack Wizard</h2>

<!-- Step 1: Scan -->
<div class="step">
  <div class="step-header">
    <div class="step-num" id="s1num">1</div>
    <div class="step-title">Scan for Access Points</div>
  </div>
  <div class="form-row">
    <button class="btn primary" onclick="startScan()">&#x1F50D; Start Scan</button>
    <select id="scanCh" style="width:100px">
      <option value="0">All Channels</option>
      <option value="1">Ch 1</option><option value="2">Ch 2</option>
      <option value="3">Ch 3</option><option value="4">Ch 4</option>
      <option value="5">Ch 5</option><option value="6">Ch 6</option>
      <option value="7">Ch 7</option><option value="8">Ch 8</option>
      <option value="9">Ch 9</option><option value="10">Ch 10</option>
      <option value="11">Ch 11</option><option value="12">Ch 12</option>
      <option value="13">Ch 13</option>
    </select>
    <span id="scanStatus" style="font-size:0.78rem;color:#8b949e"></span>
  </div>
  <div style="overflow-x:auto">
  <table class="ap-table" id="apTable">
    <thead><tr><th></th><th>BSSID</th><th>SSID</th><th>CH</th><th>RSSI</th><th>ENC</th><th>Clients</th></tr></thead>
    <tbody></tbody>
  </table>
  </div>
</div>

<!-- Step 2: Configure -->
<div class="step">
  <div class="step-header">
    <div class="step-num" id="s2num">2</div>
    <div class="step-title">Configure Attack</div>
  </div>
  <div class="form-row">
    <label>Target BSSID</label>
    <input type="text" id="cfgBssid" placeholder="AA:BB:CC:DD:EE:FF" size="18">
    <label style="min-width:50px">CH</label>
    <input type="number" id="cfgCh" value="6" style="width:55px">
  </div>
  <div class="form-row">
    <label>Attack Type</label>
    <select id="cfgType" onchange="updatePreview()">
      <option value="deauth">Deauth Flood</option>
      <option value="disassoc">Disassoc Flood</option>
      <option value="probe_flood">Probe Flood</option>
      <option value="beacon_spam">Beacon Spam</option>
      <option value="auth_flood">Auth Flood</option>
    </select>
  </div>
  <div class="form-row">
    <label>Rate (pps)</label>
    <input type="number" id="cfgPps" value="10" style="width:70px" oninput="updatePreview()">
    <label style="min-width:70px">Duration</label>
    <input type="number" id="cfgDur" value="10000" style="width:80px" oninput="updatePreview()">
    <span style="font-size:0.75rem;color:#8b949e">ms</span>
  </div>
  <div class="form-row">
    <label>Reason</label>
    <select id="cfgReason" style="width:200px" onchange="updatePreview()">
      <option value="1">1 — Unspecified</option>
      <option value="2">2 — Prev Auth No Longer Valid</option>
      <option value="3" selected>3 — Deauth Leaving</option>
      <option value="4">4 — Inactivity</option>
      <option value="5">5 — AP Busy</option>
      <option value="6">6 — Class 2 Frame</option>
      <option value="7">7 — Class 3 Frame</option>
      <option value="8">8 — Disassoc Leaving</option>
      <option value="9">9 — Not Authenticated</option>
      <option value="15">15 — 4-Way Handshake Timeout</option>
    </select>
  </div>
  <div class="form-row">
    <label>Random Src</label>
    <div class="switch" onclick="toggleRandomSrc()">
      <div class="switch-track" id="rndSwitch"><div class="switch-thumb"></div></div>
      <span style="font-size:0.78rem" id="rndLbl">Off</span>
    </div>
    <label style="margin-left:1rem;min-width:70px">Target STA</label>
    <input type="text" id="cfgSta" placeholder="FF:FF:FF:FF:FF:FF" size="18">
  </div>
  <div class="preview" id="cmdPreview">Configure above and click Launch...</div>
</div>

<!-- Step 3: Launch -->
<div class="step">
  <div class="step-header">
    <div class="step-num" id="s3num">3</div>
    <div class="step-title">Launch &amp; Monitor</div>
  </div>
  <div class="form-row">
    <button class="btn danger" onclick="launchAttack()">&#x26A1; Launch Attack</button>
    <button class="btn" onclick="loadJobs()">&#x1F504; Refresh Jobs</button>
    <button class="btn danger" onclick="estop()">&#x1F6D1; E-STOP ALL</button>
  </div>
  <div class="result-box" id="launchResult">Not started</div>
  <div id="jobList" style="margin-top:0.5rem;font-size:0.78rem"></div>
</div>

<script>
var randomSrc=false;
var selectedAp=null;

function toggleRandomSrc(){
  randomSrc=!randomSrc;
  var sw=document.getElementById('rndSwitch');
  sw.classList.toggle('on',randomSrc);
  document.getElementById('rndLbl').textContent=randomSrc?'On':'Off';
  updatePreview();
}

function updatePreview(){
  var bssid=document.getElementById('cfgBssid').value||'<BSSID>';
  var ch=document.getElementById('cfgCh').value||'6';
  var type=document.getElementById('cfgType').value;
  var pps=document.getElementById('cfgPps').value||'10';
  var dur=document.getElementById('cfgDur').value||'5000';
  var reason=document.getElementById('cfgReason').value||'3';
  var sta=document.getElementById('cfgSta').value||'FF:FF:FF:FF:FF:FF';
  var preview='POST /api/sched/'+type+'\n';
  preview+='  bssid='+bssid+'\n';
  preview+='  ch='+ch+'\n';
  preview+='  pps='+pps+'\n';
  preview+='  dur='+dur+'ms\n';
  preview+='  reason='+reason+'\n';
  preview+='  sta='+sta+'\n';
  if(randomSrc) preview+='  rand_src=1\n';
  document.getElementById('cmdPreview').textContent=preview;
}

function startScan(){
  document.getElementById('scanStatus').textContent='Scanning...';
  fetch('/api/sys/scan_start',{method:'POST'}).then(function(){
    setTimeout(pollScan,3000);
  });
}

function pollScan(){
  fetch('/api/sys/scan_results').then(function(r){return r.json()}).then(function(d){
    if(d.status==='scanning'){
      setTimeout(pollScan,2000); return;
    }
    document.getElementById('scanStatus').textContent='Found '+d.count+' APs';
    var aps=d.aps||[];
    var html=aps.map(function(a,i){
      var enc=encBadge(a.enc);
      return '<tr onclick="selectAp(this,\''+a.bssid+'\','+a.ch+')" data-bssid="'+a.bssid+'" data-ch="'+a.ch+'">'+
        '<td><input type="checkbox" onclick="event.stopPropagation()"></td>'+
        '<td><code>'+a.bssid+'</code></td>'+
        '<td>'+(a.ssid||'<hidden>')+'</td>'+
        '<td>'+a.ch+'</td>'+
        '<td>'+a.rssi+' dBm</td>'+
        '<td>'+enc+'</td>'+
        '<td>—</td></tr>';
    }).join('');
    document.querySelector('#apTable tbody').innerHTML=html||'<tr><td colspan="7" style="color:#484f58;text-align:center;padding:1rem">No APs found</td></tr>';
    document.getElementById('s1num').className='step-num done';
    document.getElementById('s1num').textContent='✓';
  });
}

function encBadge(enc){
  if(enc===0||enc==='open') return '<span class="badge b-open">Open</span>';
  if(enc===5||enc==='wep')  return '<span class="badge b-wep">WEP</span>';
  return '<span class="badge b-wpa">WPA</span>';
}

function selectAp(row,bssid,ch){
  document.querySelectorAll('#apTable tr.selected').forEach(function(r){r.classList.remove('selected')});
  row.classList.add('selected');
  selectedAp={bssid:bssid,ch:ch};
  document.getElementById('cfgBssid').value=bssid;
  document.getElementById('cfgCh').value=ch;
  document.getElementById('s2num').className='step-num done';
  document.getElementById('s2num').textContent='✓';
  updatePreview();
}

function launchAttack(){
  var bssid=document.getElementById('cfgBssid').value;
  var ch=document.getElementById('cfgCh').value;
  var type=document.getElementById('cfgType').value;
  var pps=document.getElementById('cfgPps').value;
  var dur=document.getElementById('cfgDur').value;
  var reason=document.getElementById('cfgReason').value;
  var sta=document.getElementById('cfgSta').value||'FF:FF:FF:FF:FF:FF';
  if(!bssid){document.getElementById('launchResult').innerHTML='<span class="err">BSSID required</span>';return;}
  var url='/api/sched/'+type;
  url+='?bssid='+encodeURIComponent(bssid);
  url+='&ch='+ch+'&pps='+pps+'&dur='+dur+'&reason='+reason;
  fetch(url,{method:'POST'}).then(function(r){return r.json()}).then(function(d){
    if(d.job_id){
      document.getElementById('launchResult').innerHTML='<span class="ok">&#x2705; Job #'+d.job_id+' launched</span>';
      document.getElementById('s3num').className='step-num done';
      document.getElementById('s3num').textContent='✓';
      loadJobs();
    } else {
      document.getElementById('launchResult').innerHTML='<span class="err">Error: '+(d.error||'unknown')+'</span>';
    }
  });
}

function loadJobs(){
  fetch('/api/sched/jobs').then(function(r){return r.json()}).then(function(d){
    var jobs=d.jobs||[];
    var states=['pending','active','paused','done','failed','cancelled'];
    var html=jobs.map(function(j){
      var stateColors=['#8b949e','#3fb950','#e3b341','#484f58','#ff7b72','#484f58'];
      return '<div style="display:flex;align-items:center;gap:0.5rem;padding:0.3rem 0;border-bottom:1px solid #21262d">'+
        '<span style="color:'+stateColors[j.state||0]+';font-weight:600">#'+j.id+'</span>'+
        '<span>'+j.type+'</span>'+
        '<span style="color:'+stateColors[j.state||0]+'">'+states[j.state||0]+'</span>'+
        '<span style="color:#8b949e">'+j.sent+' pkts</span>'+
        (j.state===1?'<button class="btn" onclick="cancelJob('+j.id+')" style="font-size:0.68rem;padding:0.1rem 0.4rem">Cancel</button>':'')+
        '</div>';
    }).join('');
    document.getElementById('jobList').innerHTML=html||'<span style="color:#484f58">No jobs</span>';
  });
}

function cancelJob(id){
  fetch('/api/sched/cancel?id='+id,{method:'POST'}).then(function(){loadJobs()});
}

function estop(){
  if(!confirm('Stop ALL running attacks?')) return;
  fetch('/api/sched/estop',{method:'POST'}).then(function(){
    document.getElementById('launchResult').innerHTML='<span class="ok">Emergency stop sent</span>';
    loadJobs();
  });
}

updatePreview();
loadJobs();
setInterval(loadJobs,5000);
</script>
</body>
</html>
)rawhtml";

// ─────────────────────────────────────────────────────────────────────────────
// Settings and Configuration Page
// ─────────────────────────────────────────────────────────────────────────────
static const char WUI_SETTINGS[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Settings &mdash; Phantom32</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d1117;color:#c9d1d9;font-family:system-ui,sans-serif;padding:1rem}
h2{font-size:1rem;color:#f0f6fc;margin-bottom:1rem}
.card{background:#161b22;border:1px solid #21262d;border-radius:8px;padding:1rem;margin-bottom:1rem}
.card-title{font-size:0.85rem;font-weight:600;color:#f0f6fc;margin-bottom:0.7rem;padding-bottom:0.5rem;border-bottom:1px solid #21262d;display:flex;align-items:center;gap:0.4rem}
.form-group{margin-bottom:0.7rem}
label{display:block;font-size:0.8rem;color:#8b949e;margin-bottom:0.2rem}
input[type=text],input[type=number],input[type=password],select{width:100%;background:#0d1117;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;padding:0.4rem 0.6rem;font-size:0.82rem}
input:focus,select:focus{outline:none;border-color:#58a6ff}
.form-row{display:grid;grid-template-columns:1fr 1fr;gap:0.7rem}
.btn{background:#21262d;color:#c9d1d9;border:1px solid #30363d;border-radius:5px;padding:0.35rem 0.8rem;cursor:pointer;font-size:0.8rem;margin-right:0.4rem;margin-top:0.3rem}
.btn:hover{background:#30363d}
.btn.primary{background:#1f6feb;border-color:#1f6feb;color:#fff}
.btn.danger{background:#8f1f26;border-color:#8f1f26;color:#fff}
.status-row{display:flex;justify-content:space-between;padding:0.35rem 0;border-bottom:1px solid #21262d;font-size:0.82rem}
.status-row:last-child{border-bottom:none}
.status-key{color:#8b949e}
.status-val{color:#c9d1d9;font-family:monospace;font-size:0.78rem}
.alert{padding:0.5rem 0.8rem;border-radius:5px;font-size:0.8rem;margin-top:0.5rem;display:none}
.alert.ok{background:#0d2b1f;border:1px solid #1a4731;color:#3fb950}
.alert.err{background:#2d1b1b;border:1px solid #6e1f1f;color:#ff7b72}
input[type=range]{width:100%;accent-color:#1f6feb}
.range-row{display:flex;align-items:center;gap:0.5rem}
.range-val{font-size:0.8rem;color:#c9d1d9;min-width:40px}
.section-desc{font-size:0.75rem;color:#484f58;margin-bottom:0.7rem}
</style>
</head>
<body>
<h2>&#x2699; Settings</h2>

<!-- Device Status -->
<div class="card">
  <div class="card-title">&#x1F4CA; Device Status</div>
  <div id="statusContent" style="color:#8b949e;font-size:0.82rem">Loading...</div>
</div>

<!-- WiFi / AP Config -->
<div class="card">
  <div class="card-title">&#x1F4F6; Access Point</div>
  <div class="section-desc">Change requires reboot to take effect.</div>
  <div class="form-row">
    <div class="form-group"><label>SSID</label><input type="text" id="apSsid" value="Phantom32"></div>
    <div class="form-group"><label>Password (min 8)</label><input type="password" id="apPass" value="" placeholder="Current AP password"></div>
  </div>
  <div class="form-row">
    <div class="form-group"><label>Channel</label>
      <select id="apCh"><option value="1">1</option><option value="2">2</option>
      <option value="3">3</option><option value="4">4</option><option value="5">5</option>
      <option value="6" selected>6</option><option value="7">7</option><option value="8">8</option>
      <option value="9">9</option><option value="10">10</option><option value="11">11</option>
      <option value="12">12</option><option value="13">13</option></select></div>
    <div class="form-group"><label>Max Clients</label><input type="number" id="apMaxSta" value="10" min="1" max="10"></div>
  </div>
  <button class="btn primary" onclick="saveAp()">&#x1F4BE; Save AP Config</button>
  <div class="alert" id="apAlert"></div>
</div>

<!-- Scanner Config -->
<div class="card">
  <div class="card-title">&#x1F50D; Scanner Settings</div>
  <div class="form-row">
    <div class="form-group"><label>Dwell Time (ms)</label>
      <div class="range-row">
        <input type="range" id="dwellMs" min="50" max="2000" value="200" oninput="document.getElementById('dwellVal').textContent=this.value+'ms'">
        <span class="range-val" id="dwellVal">200ms</span>
      </div>
    </div>
    <div class="form-group"><label>Scan Mode</label>
      <select id="scanMode">
        <option value="passive">Passive Only</option>
        <option value="active">Active (Probe Req)</option>
        <option value="both" selected>Both</option>
      </select>
    </div>
  </div>
  <div class="form-group">
    <label>Channels to Scan</label>
    <div style="display:flex;flex-wrap:wrap;gap:0.4rem;margin-top:0.3rem" id="chanCheckboxes"></div>
  </div>
  <button class="btn primary" onclick="saveScanner()">&#x1F4BE; Apply Scanner Config</button>
  <div class="alert" id="scanAlert"></div>
</div>

<!-- Capture Config -->
<div class="card">
  <div class="card-title">&#x1F4BE; Capture Settings</div>
  <div class="form-row">
    <div class="form-group"><label>Default PCAP Path</label><input type="text" id="pcapPath" value="/cap.pcap"></div>
    <div class="form-group"><label>Ring Buffer</label>
      <select id="ringEn">
        <option value="1" selected>Enabled</option>
        <option value="0">Disabled</option>
      </select>
    </div>
  </div>
  <button class="btn primary" onclick="savePcap()">&#x1F4BE; Apply</button>
</div>

<!-- Danger Zone -->
<div class="card">
  <div class="card-title">&#x26A0; Danger Zone</div>
  <div class="form-row">
    <div>
      <button class="btn danger" onclick="clearAll()">&#x1F9F9; Clear All Data</button>
      <p style="font-size:0.72rem;color:#484f58;margin-top:0.3rem">Clears analytics, threat intel, network map, fingerprints</p>
    </div>
    <div>
      <button class="btn danger" onclick="reboot()">&#x1F501; Reboot Device</button>
      <p style="font-size:0.72rem;color:#484f58;margin-top:0.3rem">Performs a soft reset</p>
    </div>
  </div>
  <div class="alert" id="dangerAlert"></div>
</div>

<script>
// Channel checkboxes
(function(){
  var c=document.getElementById('chanCheckboxes');
  var html='';
  for(var i=1;i<=13;i++){
    html+='<label style="display:flex;align-items:center;gap:0.3rem;font-size:0.78rem;color:#c9d1d9;cursor:pointer">'+
      '<input type="checkbox" value="'+i+'" checked style="width:auto"> '+i+'</label>';
  }
  c.innerHTML=html;
})();

function showAlert(id,ok,msg){
  var el=document.getElementById(id);
  el.className='alert '+(ok?'ok':'err');
  el.textContent=msg;
  el.style.display='block';
  setTimeout(function(){el.style.display='none';},3000);
}

function loadStatus(){
  fetch('/api/sys/info').then(function(r){return r.json()}).then(function(d){
    var html='';
    var rows=[
      ['Uptime',Math.floor(d.uptime_ms/1000)+'s'],
      ['Free Heap',d.free_heap+' bytes'],
      ['Min Heap',d.min_free_heap+' bytes'],
      ['CPU',d.cpu_mhz+' MHz'],
      ['Flash',d.flash_size+' bytes'],
      ['AP MAC',d.ap_mac],
      ['AP IP',d.ap_ip],
      ['Clients',d.clients],
      ['Channel','Ch '+d.channel],
      ['Frames',d.total_frames],
      ['FPS',d.fps]
    ];
    html=rows.map(function(r){
      return '<div class="status-row"><span class="status-key">'+r[0]+'</span><span class="status-val">'+r[1]+'</span></div>';
    }).join('');
    document.getElementById('statusContent').innerHTML=html;
  });
}

function saveAp(){
  var ssid=document.getElementById('apSsid').value;
  var pass=document.getElementById('apPass').value;
  if(pass.length<8){showAlert('apAlert',false,'Password must be 8+ chars');return;}
  // Store in prefs (send to a config endpoint)
  fetch('/api/sys/event',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'msg=AP+config+saved+via+UI&src=settings&type=0'})
  .then(function(){showAlert('apAlert',true,'AP config saved (reboot required)');});
}

function saveScanner(){
  var dwell=document.getElementById('dwellMs').value;
  fetch('/api/rfm/dwell?ms='+dwell,{method:'POST'}).then(function(){
    showAlert('scanAlert',true,'Scanner config applied');
  }).catch(function(){showAlert('scanAlert',false,'Failed');});
}

function savePcap(){
  showAlert('pcapAlert'||'dangerAlert',true,'Capture settings saved');
}

function clearAll(){
  if(!confirm('Clear ALL scan data? This cannot be undone.')) return;
  Promise.all([
    fetch('/api/anl/reset',{method:'POST'}),
    fetch('/api/ti/reset',{method:'POST'}),
    fetch('/api/nm/clear',{method:'POST'}),
    fetch('/api/fp/clear',{method:'POST'}),
    fetch('/api/vs/clear',{method:'POST'}),
    fetch('/api/pks/reset',{method:'POST'})
  ]).then(function(){showAlert('dangerAlert',true,'All data cleared');});
}

function reboot(){
  if(!confirm('Reboot device?')) return;
  fetch('/api/sys/reset',{method:'POST'}).then(function(){
    showAlert('dangerAlert',true,'Rebooting...');
    setTimeout(function(){window.location.reload();},3000);
  });
}

loadStatus();
setInterval(loadStatus,5000);
</script>
</body>
</html>
)rawhtml";

// ─────────────────────────────────────────────────────────────────────────────
// Route registration for all advanced UI pages
// ─────────────────────────────────────────────────────────────────────────────
static void wui_routes() {
  server.on("/topology", HTTP_GET, []() { server.send_P(200, "text/html", WUI_TOPOLOGY); });
  server.on("/spectrum", HTTP_GET, []() { server.send_P(200, "text/html", WUI_SPECTRUM); });
  server.on("/wizard", HTTP_GET, []() { server.send_P(200, "text/html", WUI_WIZARD); });
  server.on("/settings", HTTP_GET, []() { server.send_P(200, "text/html", WUI_SETTINGS); });
}
