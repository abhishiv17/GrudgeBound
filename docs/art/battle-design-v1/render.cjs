// Offline design exporter only. Does not change either game client.
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '../../..');
const fonts = path.join(root, 'unreal/GrudgeBound/Content/Grudgebound/UI/Fonts');
const cache = path.join(root, 'unreal/GrudgeBound/Saved/BattleDesignFontCache');
fs.mkdirSync(cache, {recursive:true});
const fc = path.join(cache,'fonts.conf');
fs.writeFileSync(fc, `<fontconfig><dir>${fonts.replaceAll('\\','/')}</dir><cachedir>${cache.replaceAll('\\','/')}</cachedir></fontconfig>`);
process.env.FONTCONFIG_FILE = fc;
const sharp = require('sharp');
const out = __dirname;
const source = path.join(out,'source');
const assets = path.join(out,'assets');
fs.mkdirSync(assets,{recursive:true});
const C={ink:'#1A1726',paper:'#F3E9D2',light:'#FBF5E6',dark:'#E9DBBB',edge:'#D6C29A',soft:'#6B6276',crimson:'#B0153A',gold:'#D9A532',lantern:'#F7C35C',night:'#0E0B26',rage:'#C8402A',sorrow:'#2B6CB0',dread:'#6B46C1',health:'#4F8A3C'};
const manifest=[];
const svg=(w,h,body)=>Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" shape-rendering="crispEdges">${body}</svg>`);
const r=(x,y,w,h,c)=>`<rect x="${x}" y="${y}" width="${w}" height="${h}" fill="${c}"/>`;
async function pair(name,input,w,h,alpha=true){
 const b=await sharp(input).resize(w,h,{kernel:'nearest',fit:'fill'}).png().toBuffer();
 fs.writeFileSync(path.join(assets,name+'@1x.png'),b);
 await sharp(b).resize(w*2,h*2,{kernel:'nearest'}).png().toFile(path.join(assets,name+'@2x.png'));
 manifest.push({name,width:w,height:h,alpha,files:[name+'@1x.png',name+'@2x.png']});return b;
}
const frame=(x,y,w,h,fill=C.paper,stud=false)=>{
 let s=r(x+3,y,w-6,h,C.ink)+r(x,y+3,w,h-6,C.ink)+r(x+3,y+3,w-6,h-6,C.light)+r(x+6,y+6,w-12,h-12,fill);
 if(stud)for(const [a,b] of [[x+12,y+12],[x+w-15,y+12],[x+12,y+h-15],[x+w-15,y+h-15]])s+=r(a,b,3,3,C.crimson)+r(a-3,b,9,3,C.crimson)+r(a,b-3,3,9,C.crimson);
 return s;
};
const textCache=new Map();
async function text(s,size,color=C.ink,body=false){
 const k=[s,size,color,body].join('|'); if(textCache.has(k))return textCache.get(k);
 const family=body?'Courier Prime':'Pixelify Sans';
 const input={text:{text:`<span foreground="${color}">${s}</span>`,font:`${family} ${body?'':'Bold '}${size}`,fontfile:path.join(fonts,body?'CourierPrime-Regular.ttf':'PixelifySans-Bold.ttf'),rgba:true,dpi:72}};
 const b=await sharp(input).png().toBuffer();textCache.set(k,b);return b;
}
async function label(layers,s,x,y,size,color=C.ink,body=false,maxWidth){const b=await text(s,size,color,body);const m=await sharp(b).metadata();if(maxWidth&&m.width>maxWidth)throw Error(`Text overflow: ${s}: ${m.width}>${maxWidth}`);layers.push({input:b,left:x,top:y});}
async function main(){
 const art={};
 for(const id of ['attack','ward','salve','seal','gilded','flee'])art[id]=await pair('action-'+(id==='gilded'?'gilded-seal':id),path.join(source,'icon_'+id+'.png'),64,64);
 for(const id of ['rage','sorrow','dread']){
  const small=await sharp(path.join(source,'emo_'+id+'.png')).resize(22,22,{kernel:'nearest'}).extend({top:5,left:5,right:5,bottom:5,background:'#00000000'}).png().toBuffer();
  art[id]=await pair('emotion-'+id,small,32,32);
 }
 for(const id of ['weepwick','riot_bell'])art[id]=await pair('curse-'+id.replaceAll('_','-'),path.join(source,'curse_'+id+'.png'),id==='weepwick'?48:64,id==='weepwick'?48:64);
 // A new flat, code-native market backdrop. All shapes lie on a 4px screen grid.
 let bg=r(0,0,480,180,C.night);
 const timber='#463C50',roof='#716077',mint='#6B8C89',stone='#BCAF98',water='#376D79';
 bg+=r(0,107,480,73,stone)+r(178,63,124,44,water);
 for(const [x,w,h,roofC] of [[0,70,85,roof],[68,55,65,mint],[115,52,45,roof],[305,55,49,mint],[357,65,70,roof],[416,64,94,mint]]){
  const y=108-h;bg+=r(x,y,w,h,timber)+r(x-3,y-5,w+6,8,roofC)+r(x+4,y+8,w-8,h-8,'#827885');
  for(let a=x+9;a<x+w-8;a+=19)bg+=r(a,y+15,10,15,C.ink)+r(a+2,y+17,6,10,C.lantern);
  bg+=r(x+4,y+39,w-8,4,timber)+r(x+Math.floor(w/2),y+8,3,h-8,timber);
 }
 bg+=r(180,93,119,4,C.ink)+r(189,88,4,18,timber)+r(281,88,4,18,timber);
 for(const [x,y]of [[0,76],[382,73]]){
  bg+=r(x,y,98,6,C.ink);for(let i=0;i<7;i++)bg+=r(x+14*i,y+6,14,15,i%2?C.paper:C.crimson);
  bg+=r(x+4,y+21,4,37,timber)+r(x+90,y+21,4,37,timber)+r(x,y+43,98,15,timber)+r(x,y+42,98,3,C.edge);
  for(let i=0;i<5;i++)bg+=r(x+13+i*15,y+34,8,8,[C.gold,C.paper,C.dread][i%3]);
 }
 for(let x=0;x<480;x+=4){const y=16+Math.round(12*Math.sin(x/480*Math.PI));bg+=r(x,y,4,1,C.ink);if(x%48===0){bg+=r(x,y+1,1,8,C.edge)+r(x-4,y+8,10,13,C.ink)+r(x-3,y+10,8,9,C.lantern)+r(x-2,y+19,6,2,C.crimson);}}
 for(let y=120;y<180;y+=20){bg+=r(0,y,480,1,'#958A7C');for(let x=(y%40?0:25);x<480;x+=50)bg+=r(x,y,1,20,'#958A7C');}
 // Uncluttered battle ground, no lettering, no textures, no baked characters or effects.
 art.backdrop=await pair('night-market-backdrop',svg(480,180,bg),1920,720,false);
 // 6 frames per effect, authored as simple 32px pixel shapes then exported at 128px.
 for(const effect of ['attack-hit','ward-shield','seal'])for(let f=0;f<6;f++){
  let s='';
  if(effect==='attack-hit')for(let i=0;i<6;i++){const d=3+f*2+i; s+=r(16-d,16-d,3,3,i%2?C.paper:C.gold)+r(16+d-2,16-d,3,3,C.crimson)+r(16-d,16+d-2,3,3,C.gold);}
  if(effect==='ward-shield'){const h=[6,12,18,24,24,20][f],y=5; s+=r(7,y,18,h,C.ink)+r(9,y+2,14,h-4,C.sorrow)+r(11,y+3,10,Math.max(1,h-7),C.paper);if(f>1)s+=r(10,y+h,12,2,C.ink)+r(13,y+h+2,6,2,C.ink);}
  if(effect==='seal'){const stampY=[0,4,8,12,10,5][f];s+=r(6,18,20,10,C.paper)+r(9,21,14,3,C.crimson);if(f>=3)s+=r(11,22,10,7,C.crimson)+r(13,23,6,2,C.gold);s+=r(13,stampY,6,7,timber)+r(15,stampY+7,2,5,timber)+r(8,stampY+12,16,4,C.gold)+r(8,stampY+16,16,2,C.crimson);}
  await pair(`${effect}-${String(f+1).padStart(2,'0')}`,svg(32,32,s),128,128);
 }
 const layout={canvas:[1920,1080],scene:[0,0,1920,720],controls:[0,720,1920,360],partner:[24,744,420,228],wild:[1476,744,420,228],log:[24,996,1872,60],partnerSprite:[348,336,288,288],wildSprite:[1196,188,448,448],partnerPortrait:[48,768,96,96],wildPortrait:[1500,768,96,96],partnerName:[162,770,258,36],wildName:[1614,770,258,36],partnerEmotion:[162,820,234,32],wildEmotion:[1614,820,234,32],partnerBar:[48,888,372,24],wildBar:[1500,888,372,24],partnerHp:[48,928,372,28],wildHp:[1500,928,372,28],result:[540,240,840,210],resultText:[576,276,768,40],back:[744,354,432,66],buttons:[]};
 const names=['Attack','Ward','Salve','Seal','Gilded Seal','Flee'];const ids=['attack','ward','salve','seal','gilded','flee'];
 names.forEach((name,i)=>{const x=468+(i%3)*336,y=744+Math.floor(i/3)*120;layout.buttons.push({name,rect:[x,y,312,108],icon:[x+18,y+21,64,64],label:[x+96,y+20,198,34],detail:[x+96,y+64,198,28]});});
 async function render(end=false,states=false){
  let shapes=frame(0,720,1920,360,C.paper,true)+frame(...layout.partner,C.dark)+frame(...layout.wild,C.dark)+frame(...layout.log,C.ink);
  for(const q of [layout.partnerPortrait,layout.wildPortrait])shapes+=frame(...q,C.light);
  for(const [q,ratio]of [[layout.partnerBar,.8],[layout.wildBar,.85]])shapes+=r(...q,C.ink)+r(q[0]+3,q[1]+3,q[2]-6,q[3]-6,C.edge)+r(q[0]+3,q[1]+3,Math.floor((q[2]-6)*ratio),q[3]-6,C.health);
  const stateNames=['normal','hover','pressed','selected','disabled','normal'];
  layout.buttons.forEach((b,i)=>{let fill=i===0||i===3?C.crimson:i===4?C.gold:C.paper;const state=end?'disabled':states?stateNames[i]:'normal';if(state==='disabled')fill=C.dark;if(state==='hover')fill=C.light;if(state==='pressed')fill=C.edge;shapes+=frame(...b.rect,fill);const[x,y,w,h]=b.rect;shapes+=r(x+6,y+h-9,w-12,3,state==='disabled'?C.edge:C.soft);if(state==='selected'||state==='hover')shapes+=r(x+9,y+9,w-18,3,C.gold)+r(x+9,y+h-12,w-18,3,C.gold);});
  if(end)shapes+=frame(...layout.result,C.ink,true)+frame(...layout.back,C.gold);
  const layers=[{input:art.backdrop,left:0,top:0},{input:svg(1920,1080,shapes),left:0,top:0}];
  for(const[id,q]of [['weepwick',layout.partnerSprite],['riot_bell',layout.wildSprite],['weepwick',layout.partnerPortrait],['riot_bell',layout.wildPortrait]]){
   const portrait=q[2]===96;const size=portrait?(id==='weepwick'?72:64):q[2];const inset=portrait?Math.floor((96-size)/2):0;
   layers.push({input:await sharp(art[id]).resize(size,size,{kernel:'nearest'}).png().toBuffer(),left:q[0]+inset,top:q[1]+inset});
  }
  for(const[x,emo,hp]of [[48,'sorrow','32 / 40'],[1500,'rage','68 / 80']]){
   await label(layers,'Curse name',x+114,770,30,C.ink,false,258);
   layers.push({input:art[emo],left:x+114,top:820});await label(layers,emo,x+156,824,24,C[emo]);await label(layers,hp,x,928,26,C.ink,true);
  }
  for(let i=0;i<6;i++){
   const b=layout.buttons[i],off=end||(states&&i===4),pressed=states&&i===2,dy=pressed?3:0;
   layers.push({input:art[ids[i]],left:b.icon[0],top:b.icon[1]+dy});
   await label(layers,names[i],b.label[0],b.label[1]+dy,28,off?C.soft:i===0||i===3?C.paper:C.ink,false,198);
   const detail=['','','x2','64%','80%  x1',''][i];if(detail)await label(layers,detail,b.detail[0],b.detail[1]+dy,22,off?C.soft:i===3?C.paper:C.ink,true,198);
  }
  await label(layers,'Battle log line',48,1005,22,C.paper,true);await label(layers,'Battle log line',48,1030,22,C.paper,true);
  if(end){await label(layers,'Result text',576,276,36,C.paper);await label(layers,'Back to the Ward',786,375,30,C.ink,false,372);}
  const b=await sharp({create:{width:1920,height:1080,channels:4,background:C.night}}).composite(layers).png().toBuffer();
  const name=states?'battle-states':end?'battle-result':'battle-active';fs.writeFileSync(path.join(out,name+'.png'),b);
  if(!states)await sharp(b).resize(1280,720,{kernel:'nearest'}).png().toFile(path.join(out,name+'-720.png'));
 }
 await render();await render(true);await render(false,true);
 fs.writeFileSync(path.join(out,'layout.json'),JSON.stringify(layout,null,2)+'\n');
 fs.writeFileSync(path.join(out,'assets.json'),JSON.stringify(manifest,null,2)+'\n');
 console.log(`Exported 5 mockups and ${manifest.length*2} asset PNGs.`);
}
main().catch(e=>{console.error(e);process.exitCode=1});
