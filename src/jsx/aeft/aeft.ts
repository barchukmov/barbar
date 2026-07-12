export type EaseMode = "in" | "out" | "both";

// ---- selection helpers ------------------------------------------------------

// Snapshot into a plain array first: setInterpolationTypeAtKey can deselect
// keys as a side effect, mutating selectedProperties/selectedKeys mid-walk.
const selectedKeyframeProperties = (): any[] => {
  const out: any[] = [];
  const comp = app.project.activeItem;
  if (!(comp instanceof CompItem)) return out;
  for (let i = 0; i < comp.selectedLayers.length; i++) {
    const props = comp.selectedLayers[i].selectedProperties;
    for (let j = 0; j < props.length; j++) {
      const prop = props[j] as any;
      if (prop.selectedKeys && prop.selectedKeys.length > 0) out.push(prop);
    }
  }
  return out;
};

const copySelectedKeys = (prop: any): number[] => {
  const keys: number[] = [];
  for (let k = 0; k < prop.selectedKeys.length; k++) keys.push(prop.selectedKeys[k]);
  return keys;
};

const forEachSelectedKey = (fn: (prop: any, key: number) => void) => {
  const props = selectedKeyframeProperties();
  for (let p = 0; p < props.length; p++) {
    const keys = copySelectedKeys(props[p]);
    for (let k = 0; k < keys.length; k++) fn(props[p], keys[k]);
  }
};

// ---- tangent mode -----------------------------------------------------------

// AE exposes keyframe smoothing as two overlapping booleans
// (keyTemporalAutoBezier / keyTemporalContinuous); this collapses them into
// one state. Auto bezier is always both-sided - there is no in-only or
// out-only auto bezier - and implies continuous. A key that is continuous but
// not auto has user-set matched tangents. Neither means broken tangents.
type TangentMode = "auto" | "continuous" | "broken";


// Only valid on a bezier/bezier key - AE throws on Linear/Hold sides.
// "auto" is set with the auto flag alone (it implies continuous, and an
// explicit setTemporalContinuousAtKey afterwards demotes the key back to
// plain continuous); the other modes clear auto, then set continuous.
const setTangentMode = (prop: any, keyIndex: number, mode: TangentMode): void => {
  prop.setTemporalAutoBezierAtKey(keyIndex, mode === "auto");
  if (mode !== "auto") prop.setTemporalContinuousAtKey(keyIndex, mode === "continuous");
};

// ---- drag session -----------------------------------------------------------

// One slider drag = one session. Preview ticks call applyEase repeatedly, so
// each key's pre-drag state is snapshotted the first time it's touched and
// every later tick reads from that snapshot, never from a previous tick's
// output. cancelEase restores it; a commit clears it. Module state survives
// across calls because the loading engine keeps the bundle alive.
type EaseData = { speed: number; influence: number };

type KeySnapshot = {
  id: string;
  prop: any;
  keyIndex: number;
  inType: KeyframeInterpolationType;
  outType: KeyframeInterpolationType;
  inEase: EaseData[];
  outEase: EaseData[];
  tangentMode: TangentMode;
};

let easeSnapshots: { [id: string]: KeySnapshot } = {};

// Tangents are stored as plain numbers: KeyframeEase host objects go stale
// across script calls and throw on restore.
const plainEases = (list: KeyframeEase[]): EaseData[] => {
  const out: EaseData[] = [];
  for (let i = 0; i < list.length; i++) {
    out.push({ speed: list[i].speed, influence: list[i].influence });
  }
  return out;
};

const hostEases = (list: EaseData[]): KeyframeEase[] => {
  const out: KeyframeEase[] = [];
  for (let i = 0; i < list.length; i++) {
    out.push(new KeyframeEase(list[i].speed, list[i].influence));
  }
  return out;
};

// propertyGroup(propertyDepth) walks up to the owning layer (PropertyBase has
// no .layer shortcut); keyTime disambiguates keys within a property.
const keyId = (prop: any, keyIndex: number): string => {
  const layer = prop.propertyGroup(prop.propertyDepth);
  return layer.index + "|" + prop.name + "|" + prop.propertyIndex + "|" + prop.keyTime(keyIndex);
};

const snapshotKeyOnce = (prop: any, keyIndex: number): KeySnapshot => {
  const id = keyId(prop, keyIndex);
  let snap = easeSnapshots[id];
  if (!snap) {
    const auto = prop.keyTemporalAutoBezier(keyIndex);
    const cont = prop.keyTemporalContinuous(keyIndex);
    // No nested ternary here (or anywhere jsx-bound): ExtendScript parses
    // the conditional operator left-associatively, so
    // `auto ? "auto" : cont ? "continuous" : "broken"` runs as
    // `(auto ? "auto" : cont) ? ...` and classifies every auto key as
    // "continuous" ("auto" is a truthy string).
    let tangentMode: TangentMode = "broken";
    if (auto) tangentMode = "auto";
    else if (cont) tangentMode = "continuous";
    snap = {
      id: id,
      prop: prop,
      keyIndex: keyIndex,
      inType: prop.keyInInterpolationType(keyIndex),
      outType: prop.keyOutInterpolationType(keyIndex),
      inEase: plainEases(prop.keyInTemporalEase(keyIndex)),
      outEase: plainEases(prop.keyOutTemporalEase(keyIndex)),
      tangentMode: tangentMode,
    };
    easeSnapshots[id] = snap;
  }
  return snap;
};

// ---- easing -----------------------------------------------------------------

// setTemporalEaseAtKey wants one KeyframeEase per temporal dimension:
// always 1 for spatial properties, otherwise whatever the key reports.
const temporalEaseSize = (prop: any, keyIndex: number): number => {
  if (prop.isSpatial) return 1;
  const size = prop.keyOutTemporalEase(keyIndex).length;
  return size < 1 ? 1 : size;
};

// Keyframe value as a numeric vector; null for values without per-dimension
// numbers (markers, text, shapes), which never count as interior.
const keyValueVector = (prop: any, keyIndex: number): number[] | null => {
  const v = prop.keyValue(keyIndex);
  if (typeof v === "number") return [v];
  if (v && typeof v.length === "number") {
    const out: number[] = [];
    for (let d = 0; d < v.length; d++) {
      if (typeof v[d] !== "number") return null;
      out.push(v[d]);
    }
    return out;
  }
  return null;
};

// A key is interior to a monotonic run when the value passes straight through
// it rather than turning around at it. Interior keys get auto bezier (AE
// derives their tangents from the neighbors); the slider's ease belongs on
// the run's ends. Spatial props: incoming and outgoing motion agree within
// 90 degrees (dot product > 0). Everything else: every dimension keeps its
// direction and at least one dimension is moving.
const interiorRunFlags = (prop: any, keyIndices: number[]): boolean[] => {
  const n = keyIndices.length;
  const flags: boolean[] = [];
  for (let i = 0; i < n; i++) flags.push(false);
  if (n < 3) return flags;

  const values: (number[] | null)[] = [];
  for (let i = 0; i < n; i++) {
    values.push(keyValueVector(prop, keyIndices[i]));
  }
  // No nested ternary (ExtendScript parses them left-associatively - the
  // one-liner returns -1 for equal values, making flat dimensions read as
  // "moving" and inflating interior detection on plateaus).
  const diffSign = (a: number, b: number): number => {
    if (a === b) return 0;
    if (b > a) return 1;
    return -1;
  };
  const spatial = !!prop.isSpatial;

  for (let k = 1; k < n - 1; k++) {
    const before = values[k - 1];
    const at = values[k];
    const after = values[k + 1];
    if (!before || !at || !after) continue;
    if (before.length !== at.length || at.length !== after.length) continue;

    if (spatial) {
      let dot = 0;
      for (let d = 0; d < at.length; d++) {
        dot += (at[d] - before[d]) * (after[d] - at[d]);
      }
      flags[k] = dot > 0;
      continue;
    }

    let moving = false;
    let sameDirection = true;
    for (let d = 0; d < at.length; d++) {
      const into = diffSign(before[d], at[d]);
      const outOf = diffSign(at[d], after[d]);
      if (into !== outOf) {
        sameDirection = false;
        break;
      }
      if (into !== 0) moving = true;
    }
    flags[k] = sameDirection && moving;
  }
  return flags;
};

export const applyEase = (value: number, mode: EaseMode, isPreview: boolean): void => {
  const props = selectedKeyframeProperties();
  // Fallback snapshot pass for callers that skipped beginEaseSession (the
  // panel's Ease button, or the popup when the sync session call timed out):
  // snapshot every key before the tick's first write, so no first-touch
  // snapshot trails a mutation. In the popup flow the session call already
  // did this and these are all cache hits.
  for (let p = 0; p < props.length; p++) {
    const prop = props[p];
    if (!prop.canVaryOverTime || prop.numKeys < 1) continue;
    const keys = copySelectedKeys(prop);
    for (let k = 0; k < keys.length; k++) snapshotKeyOnce(prop, keys[k]);
  }
  app.beginUndoGroup("Ease Keyframes");
  try {
    for (let p = 0; p < props.length; p++) {
      const prop = props[p];
      if (!prop.canVaryOverTime || prop.numKeys < 1) continue;
      const keys = copySelectedKeys(prop);
      const interior = interiorRunFlags(prop, keys);

      for (let k = 0; k < keys.length; k++) {
        const keyIndex = keys[k];
        const snap = snapshotKeyOnce(prop, keyIndex);

        // Influence clamps to 0.1-100, so value 0 means "linear", not
        // "zero-influence bezier". Restore the snapshot tangents first: an
        // earlier tick may have left slider tangents on a side this tick no
        // longer touches.
        if (value === 0) {
          prop.setTemporalEaseAtKey(keyIndex, hostEases(snap.inEase), hostEases(snap.outEase));
          prop.setInterpolationTypeAtKey(
            keyIndex,
            mode !== "out" ? KeyframeInterpolationType.LINEAR : snap.inType,
            mode !== "in" ? KeyframeInterpolationType.LINEAR : snap.outType
          );
          continue;
        }

        if (interior[k]) {
          prop.setInterpolationTypeAtKey(
            keyIndex,
            KeyframeInterpolationType.BEZIER,
            KeyframeInterpolationType.BEZIER
          );
          setTangentMode(prop, keyIndex, "auto");
          continue;
        }

        // The mode's untouched side keeps its pre-drag ease and type.
        const size = temporalEaseSize(prop, keyIndex);
        const inEase: KeyframeEase[] = [];
        const outEase: KeyframeEase[] = [];
        for (let d = 0; d < size; d++) {
          inEase.push(
            mode !== "out"
              ? new KeyframeEase(0, value)
              : new KeyframeEase(snap.inEase[d].speed, snap.inEase[d].influence)
          );
          outEase.push(
            mode !== "in"
              ? new KeyframeEase(0, value)
              : new KeyframeEase(snap.outEase[d].speed, snap.outEase[d].influence)
          );
        }
        prop.setTemporalEaseAtKey(keyIndex, inEase, outEase);
        // setTemporalEaseAtKey forces both sides to bezier - reassert the
        // per-side types last so an untouched side (e.g. Linear) sticks.
        prop.setInterpolationTypeAtKey(
          keyIndex,
          mode !== "out" ? KeyframeInterpolationType.BEZIER : snap.inType,
          mode !== "in" ? KeyframeInterpolationType.BEZIER : snap.outType
        );
      }
    }
  } finally {
    app.endUndoGroup();
  }
  // A commit is the new authoritative state; the next drag snapshots fresh.
  if (!isPreview) easeSnapshots = {};
};

// A skipped restore is invisible to the user (the popup is long gone), so
// leave a trace. Logging must never break the restore itself.
const logCancel = (line: string): void => {
  try {
    const f = new File(Folder.temp.fsName + "/barbar-cancel.log");
    if (f.open("a")) {
      f.writeln(new Date().toString() + " " + line);
      f.close();
    }
  } catch (ignored) {}
};

const logCancelSkip = (id: string, e: any): void => {
  logCancel("skipped " + id + ": " + (e && e.message ? e.message : String(e)));
};

// Preview ticks write real keyframe edits during the drag, so cancelling
// means restoring every touched key from its pre-drag snapshot.
export const cancelEase = (): void => {
  app.beginUndoGroup("Cancel Ease");
  try {
    for (const id in easeSnapshots) {
      const s = easeSnapshots[id];
      // Per-key try: a snapshot can outlive its keyframe (layer deleted,
      // keys moved, comp switched); a dead entry must not abort the rest.
      try {
        // Auto bezier keys are restored without touching the tangents: AE
        // derives them itself, and an explicit setTemporalEaseAtKey demotes
        // the key to continuous. Auto is always bezier/bezier, so setting
        // the types and the flag is the whole restore.
        if (s.tangentMode === "auto") {
          s.prop.setInterpolationTypeAtKey(
            s.keyIndex,
            KeyframeInterpolationType.BEZIER,
            KeyframeInterpolationType.BEZIER
          );
          setTangentMode(s.prop, s.keyIndex, "auto");
          continue;
        }
        // Ordered so no call's side effects corrupt the restore:
        // (1) force both sides bezier while the tangents are disposable -
        //     type changes can reset tangents;
        // (2) write the snapshot tangents (needs bezier sides anyway);
        // (3) reassert the original per-side types - a tangent reset here
        //     only lands on Linear/Hold sides, which have none;
        // (4) tangent mode last, and only on bezier/bezier keys (it doesn't
        //     exist elsewhere and the setters throw).
        s.prop.setInterpolationTypeAtKey(
          s.keyIndex,
          KeyframeInterpolationType.BEZIER,
          KeyframeInterpolationType.BEZIER
        );
        s.prop.setTemporalEaseAtKey(s.keyIndex, hostEases(s.inEase), hostEases(s.outEase));
        s.prop.setInterpolationTypeAtKey(s.keyIndex, s.inType, s.outType);
        if (
          s.inType === KeyframeInterpolationType.BEZIER &&
          s.outType === KeyframeInterpolationType.BEZIER
        ) {
          setTangentMode(s.prop, s.keyIndex, s.tangentMode);
        }
      } catch (e) {
        logCancelSkip(id, e);
      }
    }
  } finally {
    app.endUndoGroup();
    // Clear even when a restore threw - stale snapshots would re-throw on
    // every later cancel and "restore" dead state onto live keys.
    easeSnapshots = {};
  }
};

// ---- popup extras -----------------------------------------------------------

// Sets only the outgoing handle to Hold, leaving the incoming side as-is.
export const setOutgoingHandleHold = (): void => {
  app.beginUndoGroup("Hold Outgoing Handle");
  try {
    forEachSelectedKey((prop, key) => {
      prop.setInterpolationTypeAtKey(key, prop.keyInInterpolationType(key), KeyframeInterpolationType.HOLD);
    });
  } finally {
    app.endUndoGroup();
    // Hold-Out ends the drag like a commit does (the previewed ease stays):
    // the session's snapshots are spent, and keeping them would make the
    // *next* drag's cancel restore two-drags-ago state.
    easeSnapshots = {};
  }
};

export const isAnyKeyframeSelected = (): boolean => {
  return selectedKeyframeProperties().length > 0;
};

// The popup calls this synchronously at hotkey time, before its window
// exists: it answers the popup-vs-toast question and snapshots every
// selected key's pre-drag state at a moment when nothing else is queued or
// mid-write. It is also the session boundary - a leftover snapshot from a
// session that never reached its commit/cancel must not leak into this one.
export const beginEaseSession = (): boolean => {
  easeSnapshots = {};
  const props = selectedKeyframeProperties();
  for (let p = 0; p < props.length; p++) {
    const prop = props[p];
    if (!prop.canVaryOverTime || prop.numKeys < 1) continue;
    const keys = copySelectedKeys(prop);
    for (let k = 0; k < keys.length; k++) snapshotKeyOnce(prop, keys[k]);
  }
  return props.length > 0;
};
