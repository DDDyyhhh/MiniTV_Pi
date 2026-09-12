// THROWAWAY candidate A: warm diorama; static pixels first, clock owned by host.
export function drawScene(ctx, frame = 0) {
  const r=(x,y,w,h,c)=>{ctx.fillStyle=c;ctx.fillRect(x,y,w,h)};
  const p={wall:'#302c42',seam:'#3c354a',floor:'#473442',wood:'#a87058',edge:'#d7a476',dark:'#211f32',sky:'#666782',sky2:'#ad8192',light:'#f3d49b',leaf:'#78866c',leaf2:'#a6ac79'};
  r(0,0,320,240,p.wall);
  for(let x=14;x<320;x+=36)r(x,0,2,182,p.seam);
  // Deep window: distant dusk, city silhouettes and a crescent.
  r(20,18,184,122,p.dark);r(24,22,176,114,p.wood);r(28,26,168,106,p.sky);
  r(28,78,168,32,'#8d748a');r(28,110,168,22,p.sky2);
  r(44,42,8,2,p.light);r(40,44,16,4,p.light);r(38,48,18,12,p.light);r(42,60,12,2,p.light);r(44,42,14,14,p.sky);
  [[70,38],[96,54],[132,34],[182,58],[156,68],[80,72]].forEach(([x,y])=>r(x,y,2,2,'#c7b4ad'));
  [[28,112,16,20],[48,102,18,30],[68,118,18,14],[90,106,24,26],[116,98,20,34],[140,110,26,22],[170,96,26,36]].forEach(([x,y,w,h])=>{r(x,y,w,h,'#4d4d66');for(let yy=y+6;yy<130;yy+=10)for(let xx=x+4;xx<x+w-2;xx+=8)r(xx,yy,2,4,'#b39888')});
  r(108,26,6,106,p.wood);r(28,82,168,4,p.wood);r(20,132,186,8,p.edge);r(24,140,182,4,'#5e4350');
  // Curtains drawn to the sides, a little uneven like fabric.
  r(12,12,12,112,'#547274');r(24,12,8,94,'#547274');r(32,12,6,72,'#547274');r(12,12,190,4,'#95a39b');
  r(14,22,4,98,'#6f8b85');r(26,22,2,68,'#6f8b85');r(190,16,10,114,'#547274');r(196,22,2,102,'#6f8b85');
  // Far wall shelves and framed tiny art; top right stays quiet for the clock.
  r(224,54,62,4,p.edge);r(230,38,8,16,'#829183');r(240,34,6,20,'#be8670');r(248,42,12,12,'#d0b386');
  r(268,40,12,14,'#948479');r(270,38,8,2,'#d0b386');
  r(230,70,32,36,'#b18b72');r(234,74,24,28,'#263e4a');r(242,78,8,8,'#bfa58c');r(234,96,24,6,'#7e8b82');r(242,90,8,6,'#7e8b82');
  // Floorboards, a layered rug and the low table.
  r(0,178,320,62,p.floor);r(0,178,320,4,'#7a5150');
  for(let y=194;y<240;y+=16){r(0,y,320,2,'#382c3b');for(let x=(y%32?34:10);x<320;x+=68)r(x,y-12,2,12,'#3a2d3b');}
  r(40,200,198,32,'#916b68');r(34,206,210,20,'#916b68');r(44,204,190,24,'#bd9381');r(50,208,178,16,'#707976');
  for(let x=52;x<230;x+=12){r(x,204,4,2,'#725d62');r(x,226,4,2,'#725d62');}
  r(52,156,154,8,p.wood);r(48,150,162,6,p.edge);r(58,164,8,34,'#8b5c4e');r(192,164,8,34,'#8b5c4e');r(68,168,122,4,'#61414a');
  // Lamp: warm flat shapes, no real-time glow or blur.
  r(162,140,24,4,'#d0a774');r(172,108,4,32,'#bca37a');r(164,94,20,4,'#ecd7a3');r(160,98,28,4,'#ecd7a3');r(156,102,36,8,'#ecd7a3');r(152,110,44,4,'#b98b60');r(166,114,16,4,'#f4c981');
  r(158,144,34,6,'#d8b07c');r(100,142,26,6,'#82918b');r(104,138,24,4,'#cba88b');r(98,146,32,4,'#d4bb91');
  // Mug and two optional tiny steam strokes.
  r(132,134,12,14,'#d7c7a0');r(144,136,6,8,'#d7c7a0');r(144,138,4,4,p.wall);r(132,132,12,2,'#eee0b8');
  const steam=frame%4<2?0:2;r(134+steam,124,2,4,'#8b7c83');r(138-steam,118,2,4,'#8b7c83');
  // Large plant balances the desk without becoming a feature panel.
  r(264,166,24,26,'#af795d');r(260,160,32,8,'#d09a70');r(274,118,4,42,'#a3a679');
  [[258,132,18,6],[250,124,18,10],[276,138,20,6],[286,130,16,10],[268,114,12,14],[280,116,16,8]].forEach(([x,y,w,h])=>r(x,y,w,h,p.leaf));
  r(254,124,10,2,p.leaf2);r(270,114,4,10,p.leaf2);r(290,130,8,2,p.leaf2);r(278,120,8,2,p.leaf2);
  // Curled sleeping cat on a cushion: ears, tucked paws, a wrapped tail.
  r(102,204,58,12,'#bd9b83');r(108,200,46,20,'#bd9b83');
  r(112,192,36,16,'#b97d58');r(106,198,50,10,'#b97d58');r(116,188,28,4,'#cb9668');
  r(134,188,20,14,'#d5a576');r(134,184,6,6,'#d5a576');r(150,184,4,6,'#d5a576');r(136,194,4,2,'#624450');r(146,194,4,2,'#624450');
  r(110,204,30,4,'#e0b383');r(110,198,4,8,'#e0b383');r(114,196,10,4,'#e0b383');r(120,196,4,6,'#e0b383');
  // One floor-level book stack at the left, with sparse woven rug marks.
  r(16,182,22,6,'#778581');r(18,178,18,4,'#c6ac86');r(14,188,26,6,'#b07b68');
  [[64,214],[82,218],[172,214],[190,218],[210,214]].forEach(([x,y])=>r(x,y,6,2,'#9b9c86'));
}
