# Third-party notices

barbar is licensed under GPL-3.0-or-later (see [LICENSE](LICENSE)). It builds
on the following third-party components, each under its own license:

| Component | License | Where |
|---|---|---|
| [bolt-cep](https://github.com/hyperbrew/bolt-cep) (scaffold: build pipeline, CEP glue, `evalTS`) | MIT (full text below) | `src/js/lib/cep`, `vite.config.ts`, project layout |
| [raylib](https://github.com/raysan5/raylib) | zlib/libpng | fetched at build time into `AEGP/Vendor/raylib/` |
| [IXWebSocket](https://github.com/machinezone/IXWebSocket) | BSD-3-Clause | fetched at build time into `AEGP/Vendor/IXWebSocket/` |
| [nanosvg](https://github.com/memononen/nanosvg) | zlib | vendored in `AEGP/Vendor/nanosvg/` |
| [ws](https://github.com/websockets/ws) | MIT | npm dependency, bundled into the CEP extension |
| Mannin font | Ubuntu Font Licence 1.0 | `AEGP/Resources/Fonts/`, `src/Fonts/` — licence text in [AEGP/Resources/Fonts/LICENCE_UFL.txt](AEGP/Resources/Fonts/LICENCE_UFL.txt) |
| json2.js | Public domain | `src/jsx/lib/json2.js` |

**Adobe After Effects SDK**: required to build the AEGP plugin but **not
included in this repository** — Adobe's SDK license does not permit
redistribution. Download it from Adobe (free) and see the README's
"Adobe SDK" setup step. The `AegpDemo*` plugin scaffolding files are derived
from Adobe's SDK sample code and retain Adobe's original notices.

---

## bolt-cep — MIT License

```
MIT License

Copyright (c) Hyper Brew LLC

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
