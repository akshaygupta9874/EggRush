# Fonts

No font files are bundled yet (licensing: pick and vendor one deliberately).

`App/src/Theme.cpp` uses the first installed family of:
Poppins, Inter, Segoe UI, Helvetica Neue, Noto Sans, DejaVu Sans, Arial (then the system sans-serif).

To ship a font: add the `.ttf` here, put it in a `.qrc`, load it with
`QFontDatabase::addApplicationFont(":/fonts/Poppins-Medium.ttf")` before the first `Theme::font()` call,
and keep its OFL/licence text next to it. The UI was laid out against Poppins (the widest of the stack),
so narrower fallbacks only leave extra space, they do not overflow.
