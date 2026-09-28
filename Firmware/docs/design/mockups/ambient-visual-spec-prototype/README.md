# #19 暖灯小屋视觉收敛原型（可丢弃）

问题：在已选 A 构图内，右上角的小号时间、日期和带可见来源的温度该如何排布？

本原型借用 #14 的 `candidate-A.png` 截图作构图参考，但信息层重新排布；不复用原浏览器原型代码。它不是生产素材，也不代表 LVGL 的实际字形、色彩、触控或刷新成本。

从本分支仓库根目录运行：

```sh
python3 -m http.server 8765 --directory Firmware/docs/design/mockups/ambient-visual-spec-prototype
```

访问 `http://localhost:8765/?variant=a&state=normal`。浮动控件可切换三种信息层与新鲜、过期、无温度、未校时、低亮度状态。每次切换会把完整选择写入 URL。

参考图来源：#14 原型固定提交 `ec3d6b4`，路径 `Firmware/docs/design/mockups/retro-pixel-prototype/candidate-A.png`。浏览器预览没有测量真机表现。
