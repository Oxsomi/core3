/* theme.js: appearance for the page. A theme is a named CSS-variable palette (data-ox-theme on <html>,
 * app.css holds the palettes) paired with a Bootstrap color mode and a CodeMirror theme; on top of it
 * sit the personal knobs (accent color, editor font size and family). Everything persists in
 * localStorage and applies before first paint, so a reload doesn't flash the default. */
(function () {
"use strict";
const { $, esc } = window.OxUtil;

const THEMES = [
  { id: "oxsomi-dark", label: "Oxsomi dark", bs: "dark", cm: "material-darker", swatch: "#2dd4bf" },
  { id: "oxsomi-light", label: "Oxsomi light", bs: "light", cm: "default", swatch: "#0d9488" },
  { id: "midnight", label: "Midnight", bs: "dark", cm: "material-darker", swatch: "#a78bfa" },
  { id: "slate", label: "Slate", bs: "dark", cm: "material-darker", swatch: "#f59e0b" },
  { id: "hicontrast", label: "High contrast", bs: "dark", cm: "material-darker", swatch: "#ffd400" },
  { id: "crt", label: "CRT", bs: "dark", cm: "material-darker", swatch: "#22ff88" }
];

const FONTS = [
  { id: "system", label: "System mono", css: "" },
  { id: "cascadia", label: "Cascadia Code", css: '"Cascadia Code","Cascadia Mono",var(--ox-mono)' },
  { id: "jetbrains", label: "JetBrains Mono", css: '"JetBrains Mono",var(--ox-mono)' },
  { id: "courier", label: "Courier", css: '"Courier New",Courier,monospace' }
];

const DEFAULTS = { theme: "oxsomi-dark", accent: "", fontSize: 13, font: "system" };

function load() {
  try { return { ...DEFAULTS, ...JSON.parse(localStorage.getItem("ox.appearance") || "{}") }; }
  catch (e) { return { ...DEFAULTS }; }
}

function save(a) {
  try { localStorage.setItem("ox.appearance", JSON.stringify(a)); } catch (e) { /* private mode */ }
}

let current = load();

function apply() {

  const theme = THEMES.find(t => t.id === current.theme) || THEMES[0];
  const root = document.documentElement;

  root.setAttribute("data-bs-theme", theme.bs);

  if (theme.id === "oxsomi-dark" || theme.id === "oxsomi-light") root.removeAttribute("data-ox-theme");
  else root.setAttribute("data-ox-theme", theme.id);

  /* The personal accent overrides whatever the theme picked; clearing it hands control back. */
  if (current.accent) {
    root.style.setProperty("--ox-accent", current.accent);
    root.style.setProperty("--ox-accent-soft", current.accent);
  } else {
    root.style.removeProperty("--ox-accent");
    root.style.removeProperty("--ox-accent-soft");
  }

  root.style.setProperty("--ox-editor-size", (current.fontSize || 13) + "px");

  const font = FONTS.find(f => f.id === current.font);
  if (font && font.css) root.style.setProperty("--ox-editor-font", font.css);
  else root.style.removeProperty("--ox-editor-font");

  if (window.OxEditor && window.OxEditor.setTheme) window.OxEditor.setTheme(theme.cm);
}

function set(patch) {
  current = { ...current, ...patch };
  save(current);
  apply();
  renderMenu();
}

function renderMenu() {

  const el = $("#appearanceMenu");
  if (!el) return;

  el.innerHTML =
    `<li><h6 class="dropdown-header">Theme</h6></li>` +
    THEMES.map(t => `<li><button class="dropdown-item small d-flex align-items-center gap-2" data-theme="${t.id}">
      <span style="width:12px;height:12px;border-radius:3px;background:${t.swatch};display:inline-block"></span>
      ${esc(t.label)}${t.id === current.theme ? ' <i class="bi bi-check ms-auto"></i>' : ""}</button></li>`).join("") +
    `<li><hr class="dropdown-divider"></li>
     <li class="px-3 py-1 small d-flex align-items-center gap-2">Accent
       <input type="color" id="apAccent" class="form-control form-control-color form-control-sm ms-auto"
         value="${current.accent || (THEMES.find(t => t.id === current.theme) || THEMES[0]).swatch}"
         title="personal accent color; double-click the swatch of a theme to reset">
     </li>
     <li class="px-3 py-1 small d-flex align-items-center gap-2">Editor font
       <select id="apFont" class="form-select form-select-sm ms-auto" style="width:auto">
         ${FONTS.map(f => `<option value="${f.id}" ${f.id === current.font ? "selected" : ""}>${esc(f.label)}</option>`).join("")}
       </select>
     </li>
     <li class="px-3 py-1 small d-flex align-items-center gap-2">Font size
       <input type="number" id="apSize" class="form-control form-control-sm ms-auto" style="width:70px"
         min="10" max="20" value="${current.fontSize}">
     </li>
     <li><hr class="dropdown-divider"></li>
     <li><button class="dropdown-item small" id="apReset">Reset appearance</button></li>`;

  el.querySelectorAll("[data-theme]").forEach(b => {
    b.addEventListener("click", e => { e.stopPropagation(); set({ theme: b.dataset.theme, accent: "" }); });
  });
  $("#apAccent").addEventListener("change", e => set({ accent: e.target.value }));
  $("#apAccent").addEventListener("click", e => e.stopPropagation());
  $("#apFont").addEventListener("change", e => set({ font: e.target.value }));
  $("#apFont").addEventListener("click", e => e.stopPropagation());
  $("#apSize").addEventListener("change", e => set({ fontSize: Math.max(10, Math.min(20, +e.target.value || 13)) }));
  $("#apSize").addEventListener("click", e => e.stopPropagation());
  $("#apReset").addEventListener("click", () => set({ ...DEFAULTS }));
}

window.OxTheme = {
  THEMES, FONTS,
  init() { apply(); renderMenu(); },
  set, current: () => ({ ...current })
};

/* Applied immediately so the palette is right before app.js even boots. */
apply();
})();
