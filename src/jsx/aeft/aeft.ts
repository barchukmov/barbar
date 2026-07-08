export type EaseMode = "in" | "out" | "both";

// ---- selection helpers ------------------------------------------------------

// Selected properties that actually carry selected keyframes, snapshotted
// into a plain array first: setInterpolationTypeAtKey can deselect keys as a
// side effect, which would mutate selectedProperties/selectedKeys mid-walk.
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

// ---- easing -----------------------------------------------------------------

// One slider drag = one session. The AEGP popup calls applyEase on every
// slider tick (not just on commit), so anything that must be read from the
// keyframe's PRE-drag state - the untouched side of an in/out-only ease, and
// everything cancelEase restores - is snapshotted the first time a key is
// touched; later ticks would otherwise read back the previous tick's own
// output. Module state persists across evalTS calls (same ExtendScript
// engine), so no $-global is needed. A commit clears the session; the next
// drag snapshots fresh.
type KeySnapshot = {
  prop: any;
  keyIndex: number;
  inType: KeyframeInterpolationType;
  outType: KeyframeInterpolationType;
  inEase: KeyframeEase[];
  outEase: KeyframeEase[];
  autoBezier: boolean;
  continuous: boolean;
};

let easeSnapshots: { [id: string]: KeySnapshot } = {};

// PropertyBase has no .layer shortcut - propertyGroup(propertyDepth) is the
// documented way to walk up to the owning layer. keyTime (not key index)
// disambiguates keys within a property.
const keyId = (prop: any, keyIndex: number): string => {
  const layer = prop.propertyGroup(prop.propertyDepth);
  return layer.index + "|" + prop.name + "|" + prop.propertyIndex + "|" + prop.keyTime(keyIndex);
};

const snapshotKeyOnce = (prop: any, keyIndex: number): KeySnapshot => {
  const id = keyId(prop, keyIndex);
  let snap = easeSnapshots[id];
  if (!snap) {
    snap = {
      prop: prop,
      keyIndex: keyIndex,
      inType: prop.keyInInterpolationType(keyIndex),
      outType: prop.keyOutInterpolationType(keyIndex),
      inEase: prop.keyInTemporalEase(keyIndex),
      outEase: prop.keyOutTemporalEase(keyIndex),
      autoBezier: prop.keyTemporalAutoBezier(keyIndex),
      continuous: prop.keyTemporalContinuous(keyIndex),
    };
    easeSnapshots[id] = snap;
  }
  return snap;
};

// KeyframeEase arrays passed to setTemporalEaseAtKey need one entry per
// temporal dimension: always 1 for spatial properties, otherwise however
// many keyOutTemporalEase reports.
const temporalEaseSize = (prop: any, keyIndex: number): number => {
  if (prop.isSpatial) return 1;
  const size = prop.keyOutTemporalEase(keyIndex).length;
  return size < 1 ? 1 : size;
};

// Keyframe value as a numeric vector: scalars become [v], array values
// (Position/Scale/Anchor Point/colors) are copied through. Anything without
// per-dimension numbers (markers, text, shapes) returns null and never
// counts as interior.
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

// A key is "interior to a monotonic run" when the value passes straight
// through it rather than turning around at it. Interior keys get temporal
// auto-bezier - AE derives their tangents from the neighbors, which reads as
// the smooth case - instead of the slider's ease, which belongs on the run's
// ends. Two tests, by property kind:
//
// - Positional (isSpatial - layer Position/Anchor Point and effect 2D/3D
//   point controls, anything that can have a motion path): the key is
//   interior when the dot product of (a->b) and (b->c) is positive, i.e. the
//   incoming and outgoing motion agree within 90 degrees. Turning sharper
//   than that (or stopping) keeps the ease.
// - Everything else (scalars, Scale, colors): per-dimension - every
//   dimension keeps the direction it came in with (a dimension holding
//   still on both sides is fine), and at least one dimension is moving. Any
//   dimension reversing (an extremum on some axis) keeps the ease.
const interiorRunFlags = (prop: any, keyIndices: number[]): boolean[] => {
  const n = keyIndices.length;
  const flags: boolean[] = [];
  for (let i = 0; i < n; i++) flags.push(false);
  if (n < 3) return flags;

  const values: (number[] | null)[] = [];
  for (let i = 0; i < n; i++) {
    values.push(keyValueVector(prop, keyIndices[i]));
  }
  const diffSign = (a: number, b: number): number => {
    return a === b ? 0 : b > a ? 1 : -1;
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

        // KeyframeEase influence is clamped to 0.1-100, so a slider value of
        // 0 means "linear", not "zero-influence bezier".
        if (value === 0) {
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
          prop.setTemporalAutoBezierAtKey(keyIndex, true);
          continue;
        }

        const size = temporalEaseSize(prop, keyIndex);
        const inEase: KeyframeEase[] = [];
        const outEase: KeyframeEase[] = [];
        for (let d = 0; d < size; d++) {
          inEase.push(mode !== "out" ? new KeyframeEase(0, value) : snap.inEase[d]);
          outEase.push(mode !== "in" ? new KeyframeEase(0, value) : snap.outEase[d]);
        }
        prop.setTemporalEaseAtKey(keyIndex, inEase, outEase);
        // setTemporalEaseAtKey forces BOTH sides to bezier as a side effect,
        // even when only one side's ease actually changed - reassert the
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
  // A commit is the new authoritative state - drop the snapshots so the next
  // drag starts fresh instead of "restoring" stale data.
  if (!isPreview) easeSnapshots = {};
};

// Esc dismisses the popup without a commit - but the preview ticks already
// wrote real keyframe edits during the drag, so cancelling means restoring
// every touched key from its pre-drag snapshot, not just forgetting them.
export const cancelEase = (): void => {
  app.beginUndoGroup("Cancel Ease");
  try {
    for (const id in easeSnapshots) {
      const s = easeSnapshots[id];
      s.prop.setTemporalEaseAtKey(s.keyIndex, s.inEase, s.outEase);
      // setTemporalEaseAtKey forces both sides to bezier - reassert the
      // original types after it. Auto-bezier next (setting it also flips
      // continuous), then continuous last so both flags land as snapshotted.
      s.prop.setInterpolationTypeAtKey(s.keyIndex, s.inType, s.outType);
      s.prop.setTemporalAutoBezierAtKey(s.keyIndex, s.autoBezier);
      s.prop.setTemporalContinuousAtKey(s.keyIndex, s.continuous);
    }
  } finally {
    app.endUndoGroup();
  }
  easeSnapshots = {};
};

// ---- popup extras -----------------------------------------------------------

// Sets only the outgoing handle to Hold, leaving the incoming side as-is -
// the popup button for this exists so the user doesn't have to set both
// handles to Hold just to hold the outgoing one.
export const setOutgoingHandleHold = (): void => {
  app.beginUndoGroup("Hold Outgoing Handle");
  try {
    forEachSelectedKey((prop, key) => {
      prop.setInterpolationTypeAtKey(key, prop.keyInInterpolationType(key), KeyframeInterpolationType.HOLD);
    });
  } finally {
    app.endUndoGroup();
  }
};

export const isAnyKeyframeSelected = (): boolean => {
  return selectedKeyframeProperties().length > 0;
};
