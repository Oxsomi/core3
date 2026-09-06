/* workspace.js: named projects persisted in localStorage, so opening a share link or importing a
 * snapshot never overwrites work: those arrive as their OWN workspace and the one you were in stays
 * on the list. Pinned workspaces are never auto-evicted; unpinned ones go oldest-first when storage
 * runs out. Clearing browser storage still clears everything, pins included; the durable form of a
 * project is an .oiCA download.
 *
 * Storage: ox.wsIndex (the meta list), ox.wsCurrent (the open id), ox.ws.<id> (one payload per
 * workspace, encoded with the share codec so it compresses where the browser can and stays readable
 * where it can't). No DOM in here; app.js owns the menu and the state handover. */
(function () {
"use strict";
const U = window.OxUtil;

const store = () => window.localStorage;

function readIndex() {
  try { return JSON.parse(store().getItem("ox.wsIndex") || "[]"); } catch (e) { return []; }
}

function writeIndex(index) {
  try { store().setItem("ox.wsIndex", JSON.stringify(index)); } catch (e) { }
}

function newId() {
  return Date.now().toString(36) + "-" + Math.random().toString(36).slice(2, 8);
}

let dirty = false;                       // edits newer than the last successful save
let saveFailed = false;                  // the last save could not land even after eviction

const api = {

  list: () => readIndex(),
  currentId() { try { return store().getItem("ox.wsCurrent"); } catch (e) { return null; } },
  current() { const id = this.currentId(); return readIndex().find(w => w.id === id) || null; },
  isDirty: () => dirty || saveFailed,
  markDirty() { dirty = true; },

  create(name, kind, srcHash) {
    const index = readIndex();
    const meta = { id: newId(), name, kind: kind || "local", pinned: false, updated: Date.now() };
    if (srcHash) meta.srcHash = srcHash;
    index.push(meta);
    writeIndex(index);
    return meta;
  },

  findBySrcHash(srcHash) {
    return readIndex().find(w => w.srcHash === srcHash) || null;
  },

  switchTo(id) {
    try { store().setItem("ox.wsCurrent", id); } catch (e) { }
    dirty = false; saveFailed = false;
  },

  rename(id, name) {
    const index = readIndex();
    const meta = index.find(w => w.id === id);
    if (meta) { meta.name = name; writeIndex(index); }
  },

  pin(id, pinned) {
    const index = readIndex();
    const meta = index.find(w => w.id === id);
    if (meta) { meta.pinned = !!pinned; writeIndex(index); }
  },

  remove(id) {
    writeIndex(readIndex().filter(w => w.id !== id));
    try { store().removeItem("ox.ws." + id); } catch (e) { }
  },

  async load(id) {
    let raw = null;
    try { raw = store().getItem("ox.ws." + id); } catch (e) { }
    if (!raw) return null;
    try { return await U.decodeShare("#" + raw); } catch (e) { return null; }
  },

  /* Persists the current workspace. On a full store, unpinned workspaces other than this one are
   * evicted oldest-first and the write retried; when nothing evictable is left the failure is
   * reported (returns false) so the page can fall back to warning before unload. */
  async save(payload) {

    const id = this.currentId();
    if (!id) return false;

    const encoded = await U.encodeShare(payload);

    for (;;) {

      try {
        store().setItem("ox.ws." + id, encoded);

        const index = readIndex();
        const meta = index.find(w => w.id === id);
        if (meta) { meta.updated = Date.now(); writeIndex(index); }

        dirty = false; saveFailed = false;
        return true;
      }

      catch (e) {
        const victims = readIndex()
          .filter(w => !w.pinned && w.id !== id)
          .sort((a, b) => a.updated - b.updated);
        if (!victims.length) { saveFailed = true; return false; }
        this.remove(victims[0].id);
        //A store too full to even rewrite the shrunken index can't make progress; fail rather than spin.
        if (readIndex().some(w => w.id === victims[0].id)) { saveFailed = true; return false; }
      }
    }
  }
};

window.OxWorkspace = api;
})();
