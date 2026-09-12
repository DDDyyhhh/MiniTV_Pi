// Throwaway variant B: the landscape is primary; the host owns the clock.
// Reserved quiet sky: x=12..85, y=12..36. Every shape uses whole pixels.
export function drawScene(ctx, frame = 0) {
  const p = {
    sky: '#b98caa', rose: '#d7a1ac', peach: '#edb4a7', apricot: '#f5c7aa',
    sun: '#ffe2b2', far: '#a693b0', farLight: '#b5a0bb', mountain: '#827b9e',
    ridge: '#635f83', haze: '#ae98b0', water: '#968baa', waterDark: '#777e9b',
    reflection: '#d6adb5', pale: '#f3c7b2', bank: '#555a70', meadow: '#717b7d',
    grass: '#919389', pine: '#354957', pineLight: '#4d656b', ground: '#2f414d',
    earth: '#877c7e', roof: '#4d4558', wood: '#a8817d', light: '#ffd496',
  };
  const r = (x, y, w, h, color) => {
    ctx.fillStyle = color;
    ctx.fillRect(x, y, w, h);
  };
  // A stepped contour, filled to the bottom; no antialiased diagonal edges.
  const ridge = (points, bottom, color) => {
    for (let i = 0; i < points.length - 1; i++) {
      r(points[i][0], points[i][1], points[i + 1][0] - points[i][0], bottom - points[i][1], color);
    }
  };
  const pine = (x, y, size, color, lit = false) => {
    r(x, y, 2 * size, 24 * size, color);
    r(x - 2 * size, y + 4 * size, 6 * size, 4 * size, color);
    r(x - 4 * size, y + 8 * size, 10 * size, 4 * size, color);
    r(x - 6 * size, y + 12 * size, 14 * size, 4 * size, color);
    r(x - 8 * size, y + 16 * size, 18 * size, 4 * size, color);
    if (lit) {
      r(x - 2 * size, y + 8 * size, 2 * size, 2 * size, p.pineLight);
      r(x - 4 * size, y + 12 * size, 4 * size, 2 * size, p.pineLight);
      r(x - 6 * size, y + 16 * size, 4 * size, 2 * size, p.pineLight);
    }
  };

  r(0, 0, 320, 240, p.sky);
  r(0, 40, 320, 24, p.rose);
  r(0, 64, 320, 24, p.peach);
  r(0, 88, 320, 40, p.apricot);

  // Small stepped sun; broad open sky is deliberately left untouched at left.
  r(226, 46, 14, 2, p.sun);
  r(222, 48, 22, 4, p.sun);
  r(220, 52, 26, 16, p.sun);
  r(222, 68, 22, 4, p.sun);
  r(226, 72, 14, 2, p.sun);
  r(166, 46, 22, 2, p.peach);
  r(176, 44, 30, 2, p.peach);
  r(258, 82, 40, 2, p.apricot);
  r(274, 80, 20, 2, p.apricot);
  r(112, 74, 30, 2, p.apricot);
  r(122, 72, 28, 2, p.apricot);

  ridge([[0,108],[12,104],[22,98],[34,92],[44,86],[52,82],[60,78],
    [68,82],[76,88],[88,92],[98,98],[110,102],[128,108],[146,112],
    [164,108],[174,102],[186,96],[196,90],[206,94],[214,100],[228,106],
    [240,102],[252,94],[264,90],[274,92],[288,98],[302,102],[320,102]], 138, p.far);
  ridge([[22,108],[34,102],[44,96],[52,90],[60,86],[68,90],[74,94],[80,100],[86,108]], 114, p.farLight);
  ridge([[0,126],[16,122],[26,116],[38,112],[48,106],[58,108],[72,114],
    [88,120],[102,124],[118,130],[144,130],[158,124],[170,118],[182,114],
    [194,108],[206,104],[216,108],[226,114],[240,120],[252,122],[266,116],
    [278,110],[290,108],[306,112],[320,112]], 148, p.mountain);
  r(94, 132, 120, 4, p.haze);
  r(74, 138, 182, 4, p.haze);

  // Lake and the distant wooded headlands.
  r(0, 146, 320, 94, p.water);
  r(68, 146, 214, 2, p.reflection);
  r(110, 150, 108, 2, p.reflection);
  ridge([[0,128],[14,130],[26,134],[42,138],[58,142],[82,146],
    [102,150],[122,154],[140,158],[148,160]], 166, p.ridge);
  ridge([[244,146],[258,142],[270,136],[282,132],[292,130],[306,128],[320,128]], 158, p.ridge);
  for (const [x, y] of [[4,116],[18,122],[34,126],[56,132],[276,122],[292,114],[310,116]]) {
    pine(x, y, 1, p.ridge);
  }
  r(214, 154, 36, 2, p.pale);
  r(224, 158, 16, 2, p.reflection);
  r(192, 164, 42, 2, p.reflection);
  r(212, 170, 20, 2, p.pale);
  r(170, 176, 34, 2, p.reflection);
  r(120, 174, 26, 2, p.waterDark);
  r(152, 162, 20, 2, p.waterDark);
  r(264, 162, 28, 2, p.waterDark);

  // Left shoreline, right meadow: together they turn the lake into an S river.
  ridge([[0,166],[26,168],[48,172],[72,176],[90,180],[108,184],
    [124,188],[134,192],[138,196],[130,200],[112,204],[90,208],
    [72,214],[64,220],[72,228],[92,234],[108,238],[124,240]], 240, p.bank);
  ridge([[178,240],[178,234],[168,228],[156,222],[152,216],[156,210],
    [170,204],[192,198],[212,194],[234,190],[252,186],[266,180],
    [278,174],[290,168],[306,164],[320,162]], 240, p.meadow);
  // Flat shoreline ledges and water contours keep the curve pixel-stepped.
  r(0, 168, 28, 2, p.grass);
  r(28, 172, 20, 2, p.grass);
  r(48, 176, 26, 2, p.earth);
  r(74, 180, 18, 2, p.earth);
  r(94, 184, 16, 2, p.earth);
  r(116, 188, 10, 2, p.reflection);
  r(144, 190, 62, 2, p.reflection);
  r(146, 194, 34, 2, p.pale);
  r(136, 204, 24, 2, p.reflection);
  r(114, 214, 22, 2, p.waterDark);
  r(90, 220, 42, 2, p.reflection);
  r(104, 226, 30, 2, p.reflection);
  r(126, 234, 34, 2, p.waterDark);
  r(256, 188, 38, 2, p.grass);
  r(220, 198, 36, 2, p.grass);
  r(180, 212, 18, 2, p.grass);

  // One remote warm hut, its tiny boardwalk aimed at the water.
  r(246, 176, 34, 4, p.bank);
  r(254, 160, 20, 16, p.wood);
  r(254, 170, 20, 2, p.earth);
  r(262, 152, 4, 2, p.roof);
  r(258, 154, 12, 2, p.roof);
  r(254, 156, 20, 2, p.roof);
  r(250, 158, 28, 4, p.roof);
  r(268, 150, 4, 6, p.roof);
  r(256, 164, 6, 6, p.light);
  r(266, 164, 4, 12, p.roof);
  r(266, 164, 2, 8, p.light);
  r(248, 182, 18, 2, p.earth);
  r(244, 184, 16, 2, p.earth);
  r(244, 186, 2, 4, p.roof);
  r(254, 186, 2, 2, p.roof);

  // Foreground framing remains low and asymmetric, not a surrounding border.
  pine(16, 146, 2, p.pine, true);
  pine(42, 164, 1, p.pine, true);
  pine(0, 172, 2, p.ground);
  pine(298, 162, 2, p.pine, true);
  pine(318, 180, 2, p.ground);
  ridge([[0,212],[16,216],[30,222],[46,226],[60,232],[78,238],[92,240]], 240, p.ground);
  ridge([[236,240],[254,236],[270,230],[284,224],[300,220],[320,218]], 240, p.ground);
  for (const [x, y, w] of [[24,198,12],[52,192,8],[76,198,10],[32,206,6],
    [194,222,12],[208,212,6],[244,216,16],[264,206,8],[274,218,6],
    [230,228,8],[180,230,4],[282,196,8]]) {
    r(x, y, w, 2, p.grass);
    r(x + 2, y - 4, 2, 4, p.meadow);
  }
  for (const [x, y] of [[10,212],[30,220],[48,224],[62,232],[274,230],[288,222],[308,216]]) {
    r(x, y - 6, 2, 8, p.ground);
    r(x - 2, y - 2, 2, 4, p.ground);
    r(x + 2, y - 4, 2, 4, p.ground);
  }
  // Two resting birds, far from the clock's quiet sky.
  r(144, 94, 4, 2, p.ridge);
  r(148, 96, 2, 2, p.ridge);
  r(150, 94, 4, 2, p.ridge);
  r(162, 90, 2, 2, p.ridge);
  r(164, 92, 2, 2, p.ridge);
  r(166, 90, 2, 2, p.ridge);

  // Only two water glints change; no camera, cloud, sun, or scenery movement.
  const glint = Math.floor(frame / 2) % 3;
  r(202 + glint * 4, 160, 4, 2, p.pale);
  r(144 - glint * 2, 218, 6, 2, p.pale);
}
