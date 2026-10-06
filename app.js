const $=id=>document.getElementById(id);
const pages=[['index.html','Live'],['how-it-works.html','How it works'],['hardware.html','Hardware'],['setup.html','Connect ESP32']];
const cur=location.pathname.split('/').pop()||'index.html';
document.body.insertAdjacentHTML('afterbegin','<nav><b>Campus<span>Eye</span></b>'+pages.map(p=>`<a href="${p[0]}" class="${p[0]==cur?'on':''}">${p[1]}</a>`).join('')+'</nav>');
document.body.insertAdjacentHTML('beforeend','<footer>CampusEye · Mini project 24CA3301</footer>');
if($('live')){
 const get=k=>localStorage.getItem(k)||'';
 $('ip').value=get('ce_ip');$('key').value=get('ce_key');
 let seen='';
 const badge=(id,t,cls)=>{$(id).textContent=t;$(id).className='st '+cls};
 const base=()=>'http://'+$('ip').value.trim();
 async function api(path,ms=2000){
  const ac=new AbortController();const t=setTimeout(()=>ac.abort(),ms);
  try{const r=await fetch(base()+path,{signal:ac.signal});return await r.json()}finally{clearTimeout(t)}
 }
 $('save').onclick=()=>{localStorage.setItem('ce_ip',$('ip').value.trim());localStorage.setItem('ce_key',$('key').value.trim());poll()};
 document.querySelectorAll('[data-cmd]').forEach(b=>b.onclick=async()=>{
  try{await api('/control?cmd='+b.dataset.cmd+'&key='+encodeURIComponent($('key').value));poll()}catch(e){alert('Could not reach the ESP32')}
 });
 async function poll(){
  if(!$('ip').value.trim()){badge('conn','NOT CONFIGURED','');return}
  try{
   const d=await api('/status');
   badge('conn','CONNECTED','ok');
   $('clk').textContent=d.time;
   badge('mon',d.monitoring?'ACTIVE':'INACTIVE',d.monitoring?'ok':'');
   badge('arm',d.armed?'ARMED':'DISARMED',d.armed?'ok':'');
   badge('pir',d.pir?'MOTION':'NO MOTION',d.pir?'bad':'');
   badge('alm',d.alarm?'ALARM!':'OFF',d.alarm?'bad':'');
   const s=d.events.join('\n');
   if(s!==seen){seen=s;$('log').innerHTML=d.events.slice().reverse().join('<br>')||'No events yet'}
  }catch(e){badge('conn','OFFLINE','bad')}
 }
 setInterval(poll,1000);poll();
}
