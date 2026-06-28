export type EaseMode = "in" | "out" | "both";

// The literal ExtendScript run by the CEP "Ease" button and the AEGP popup
// slider, kept as a plain string (not a compiled jsx export) so the exact
// same text can be shown in the panel UI and sent to evalES - what you see
// is what runs, no abstraction gap to audit through.
export const EASE_SCRIPT_TEMPLATE = `var comp = app.project.activeItem;
if (!comp || !(comp instanceof CompItem)) throw new Error("No active comp");
var INF = __VALUE__;
var MODE = "__MODE__";
var IS_PREVIEW = __IS_PREVIEW__;
// AEGP polls this script on every slider-drag tick (not just on commit), so
// the untouched side of an in/out-only ease can't be read live - a prior
// preview tick may have already overwritten it. $.__easeMemory persists
// across evalES calls (same ExtendScript engine instance) and snapshots
// each keyframe's original handle type + ease the first time it's touched
// in a drag, so every later tick (and the final commit) restores the SAME
// untouched-side values instead of compounding the previous tick's.
if (!$.__easeMemory) $.__easeMemory = {};
var MEM = $.__easeMemory;
function keyId(prop, keyIndex) {
  // PropertyBase has no .layer shortcut - propertyGroup(propertyDepth) is
  // the documented way to walk up to the owning layer from any depth.
  var layer = prop.propertyGroup(prop.propertyDepth);
  return layer.index + "|" + prop.name + "|" + prop.propertyIndex + "|" + prop.keyTime(keyIndex);
}
var sourceProps = comp.selectedProperties;
var props = [];
var propCount = sourceProps.length;
for (var p = 0; p < propCount; p++) {
  props.push(sourceProps[p]);
}
function copySelectedKeys(prop) {
  var sourceKeys = prop.selectedKeys;
  var keys = [];
  var keyCount = sourceKeys.length;
  for (var i = 0; i < keyCount; i++) {
    keys.push(sourceKeys[i]);
  }
  return keys;
}
function easeCount(prop, keyIndex) {
  if (prop.isSpatial) return 1;
  var ease = prop.keyOutTemporalEase(keyIndex);
  if (ease.length < 1) return 1;
  return ease.length;
}
app.beginUndoGroup("Ease Keyframes");
try {
  for (var p = 0; p < props.length; p++) {
    var pr = props[p];
    if (pr.propertyType !== PropertyType.PROPERTY || !pr.canVaryOverTime || pr.numKeys < 1) continue;
    var ks = copySelectedKeys(pr);
    var keyCount = ks.length;
    if (keyCount < 1) continue;
    for (var k = 0; k < keyCount; k++) {
      var keyIndex = ks[k];
      var count = easeCount(pr, keyIndex);

      var id = keyId(pr, keyIndex);
      var snap = MEM[id];
      if (!snap) {
        snap = {
          prop: pr,
          keyIndex: keyIndex,
          inType: pr.keyInInterpolationType(keyIndex),
          outType: pr.keyOutInterpolationType(keyIndex),
          inEase: pr.keyInTemporalEase(keyIndex),
          outEase: pr.keyOutTemporalEase(keyIndex)
        };
        MEM[id] = snap;
      }

      // KeyframeEase's influence must be 0.1-100 - INF=0 means "linear", not
      // "zero-influence bezier", so that case skips ease entirely.
      var targetType = INF === 0 ? KeyframeInterpolationType.LINEAR : KeyframeInterpolationType.BEZIER;
      var inType = MODE !== "out" ? targetType : snap.inType;
      var outType = MODE !== "in" ? targetType : snap.outType;

      if (targetType === KeyframeInterpolationType.BEZIER) {
        var easeIn = [];
        var easeOut = [];
        for (var i = 0; i < count; i++) {
          easeIn.push(MODE !== "out" ? new KeyframeEase(0, INF) : snap.inEase[i]);
          easeOut.push(MODE !== "in" ? new KeyframeEase(0, INF) : snap.outEase[i]);
        }
        pr.setTemporalEaseAtKey(keyIndex, easeIn, easeOut);
      }
      // setTemporalEaseAtKey forces BOTH sides to Bezier as a side effect,
      // even when only one side's ease array actually changed - reassert
      // the per-side types last so the untouched side (e.g. Linear) sticks.
      pr.setInterpolationTypeAtKey(keyIndex, inType, outType);
    }
  }
} finally {
  app.endUndoGroup();
}
// A commit (not a preview tick) is the new authoritative state - drop the
// snapshot so the next drag starts fresh instead of "restoring" stale data.
if (!IS_PREVIEW) $.__easeMemory = {};`;

export const buildEaseScript = (
  value: number,
  mode: EaseMode = "both",
  isPreview = false
): string =>
  EASE_SCRIPT_TEMPLATE.replace("__VALUE__", String(value))
    .replace("__MODE__", mode)
    .replace("__IS_PREVIEW__", String(isPreview));

// Esc dismisses the popup without a commit - the preview ticks already
// wrote real keyframe edits during the drag, so cancelling means restoring
// each touched keyframe from $.__easeMemory's pre-drag snapshot rather than
// just dropping the cache.
export const CANCEL_EASE_SCRIPT = `if ($.__easeMemory) {
  app.beginUndoGroup("Cancel Ease");
  try {
    for (var id in $.__easeMemory) {
      var snap = $.__easeMemory[id];
      snap.prop.setTemporalEaseAtKey(snap.keyIndex, snap.inEase, snap.outEase);
      // setTemporalEaseAtKey forces both sides to Bezier as a side effect -
      // reassert the original types last so they stick.
      snap.prop.setInterpolationTypeAtKey(snap.keyIndex, snap.inType, snap.outType);
    }
  } finally {
    app.endUndoGroup();
  }
  $.__easeMemory = {};
}`;
