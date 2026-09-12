/* layout.js: the pane splitters between the file rail, the editor, the output pane and the problems
 * strip. Dragging resizes, double-clicking collapses (the problems splitter shares app.js' own
 * prob-collapsed toggle), and sizes persist in this browser's local storage the way the appearance
 * settings do. Dependency-free on purpose: it only writes flex-basis and height, so every pane keeps
 * its own scrolling and the CSS defaults stay the no-storage fallback. */
(function () {
"use strict";

const KEY = "ox.layout";

function load() {
  try { return JSON.parse(localStorage.getItem(KEY)) || {}; } catch (e) { return {}; }
}

function save() {
  try { localStorage.setItem(KEY, JSON.stringify(state)); } catch (e) { /* private mode: not persisted */ }
}

const state = load();
const clamp = (v, lo, hi) => Math.max(lo, Math.min(Math.max(lo, hi), Math.round(v)));

const rail = document.querySelector(".rail");
const outPane = document.querySelector(".output-pane");
const problems = document.getElementById("problems");

/* The editor keeps at least this much; the side panes' maxima derive from it so a stored size from a
 * wide window cannot squeeze the editor away on a narrow one. */
const EDITOR_MIN = 220;

const railMax = () => window.innerWidth * 0.4;
const outMax = () => window.innerWidth - (state.railCollapsed ? 0 : rail.getBoundingClientRect().width) - EDITOR_MIN;

function apply() {
  if (state.rail) rail.style.flexBasis = clamp(state.rail, 140, railMax()) + "px";
  if (state.out) {
    outPane.style.flexGrow = "0";
    outPane.style.flexShrink = "0";
    outPane.style.flexBasis = clamp(state.out, 220, outMax()) + "px";
  }
  if (state.prob) problems.style.height = clamp(state.prob, 64, window.innerHeight * 0.7) + "px";
  document.body.classList.toggle("rail-collapsed", !!state.railCollapsed);
  document.body.classList.toggle("out-collapsed", !!state.outCollapsed);
  document.body.classList.toggle("prob-collapsed", !!state.probCollapsed);
}

/* One splitter: size/resize read and write the controlled pane's dimension; sign points from the
 * splitter towards the pane it grows (the output and problems panes grow against the pointer). */
function wire(id, opts) {
  const el = document.getElementById(id);
  if (!el) return;

  el.addEventListener("dblclick", () => { opts.toggle(); save(); apply(); });

  el.addEventListener("pointerdown", e => {
    if (document.body.classList.contains(opts.collapsedClass)) return;
    e.preventDefault();
    el.setPointerCapture(e.pointerId);
    el.classList.add("dragging");
    const start = opts.horizontal ? e.clientY : e.clientX;
    const base = opts.size();

    const move = ev =>
      opts.resize(base + ((opts.horizontal ? ev.clientY : ev.clientX) - start) * opts.sign);

    const up = () => {
      el.classList.remove("dragging");
      el.removeEventListener("pointermove", move);
      el.removeEventListener("pointerup", up);
      save();
    };

    el.addEventListener("pointermove", move);
    el.addEventListener("pointerup", up);
  });
}

wire("splitRail", {
  collapsedClass: "rail-collapsed", sign: 1, horizontal: false,
  size: () => rail.getBoundingClientRect().width,
  resize: w => { state.rail = clamp(w, 140, railMax()); rail.style.flexBasis = state.rail + "px"; },
  toggle: () => { state.railCollapsed = !state.railCollapsed; }
});

wire("splitOut", {
  collapsedClass: "out-collapsed", sign: -1, horizontal: false,
  size: () => outPane.getBoundingClientRect().width,
  resize: w => {
    state.out = clamp(w, 220, outMax());
    outPane.style.flexGrow = "0";
    outPane.style.flexShrink = "0";
    outPane.style.flexBasis = state.out + "px";
  },
  toggle: () => { state.outCollapsed = !state.outCollapsed; }
});

wire("splitProb", {
  collapsedClass: "prob-collapsed", sign: -1, horizontal: true,
  size: () => problems.getBoundingClientRect().height,
  resize: h => { state.prob = clamp(h, 64, window.innerHeight * 0.7); problems.style.height = state.prob + "px"; },
  toggle: () => { state.probCollapsed = !state.probCollapsed; }
});

/* app.js' chevron flips prob-collapsed itself; read the class after that handler ran so the stored
 * state follows whichever control the user reached for. */
const probToggle = document.getElementById("probToggle");

if (probToggle)
  probToggle.addEventListener("click", () => setTimeout(() => {
    state.probCollapsed = document.body.classList.contains("prob-collapsed");
    save();
  }, 0));

apply();
})();
