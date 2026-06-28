export type EaseMode = "in" | "out" | "both";

// The literal ExtendScript run by the CEP "Ease" button and the AEGP popup
// slider, kept as a plain string (not a compiled jsx export) so the exact
// same text can be shown in the panel UI and sent to evalES - what you see
// is what runs, no abstraction gap to audit through.
export const EASE_SCRIPT_TEMPLATE = `var comp = app.project.activeItem;
if (!comp || !(comp instanceof CompItem)) throw new Error("No active comp");
var INF = __VALUE__;
var MODE = "__MODE__";
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
      // KeyframeEase's influence must be 0.1-100 - INF=0 means "linear", not
      // "zero-influence bezier", so that case skips ease entirely.
      var targetType = INF === 0 ? KeyframeInterpolationType.LINEAR : KeyframeInterpolationType.BEZIER;
      var inType = MODE !== "out" ? targetType : pr.keyInInterpolationType(keyIndex);
      var outType = MODE !== "in" ? targetType : pr.keyOutInterpolationType(keyIndex);
      pr.setInterpolationTypeAtKey(keyIndex, inType, outType);

      if (targetType === KeyframeInterpolationType.BEZIER) {
        var currentIn = pr.keyInTemporalEase(keyIndex);
        var currentOut = pr.keyOutTemporalEase(keyIndex);
        var easeIn = [];
        var easeOut = [];
        for (var i = 0; i < count; i++) {
          easeIn.push(MODE !== "out" ? new KeyframeEase(0, INF) : currentIn[i]);
          easeOut.push(MODE !== "in" ? new KeyframeEase(0, INF) : currentOut[i]);
        }
        pr.setTemporalEaseAtKey(keyIndex, easeIn, easeOut);
      }
    }
  }
} finally {
  app.endUndoGroup();
}`;

export const buildEaseScript = (value: number, mode: EaseMode = "both"): string =>
  EASE_SCRIPT_TEMPLATE.replace("__VALUE__", String(value)).replace("__MODE__", mode);
