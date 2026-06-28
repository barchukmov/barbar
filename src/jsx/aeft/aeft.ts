import {
  helloVoid,
  helloError,
  helloStr,
  helloNum,
  helloArrayStr,
  helloObj,
} from "../utils/samples";
export { helloError, helloStr, helloNum, helloArrayStr, helloObj, helloVoid };
import { dispatchTS } from "../utils/utils";

export const helloWorld = () => {
  alert("Hello from After Effects!");
  app.project.activeItem;
};

// Shared by applyEasing/isAnyKeyframeSelected/setOutgoingHandleHold - they
// all need the same selected-layers -> selected-properties walk.
const forEachSelectedProperty = (fn: (prop: any) => void) => {
  const comp = app.project.activeItem;
  if (!(comp instanceof CompItem)) return;
  for (let i = 0; i < comp.selectedLayers.length; i++) {
    const props = comp.selectedLayers[i].selectedProperties;
    for (let j = 0; j < props.length; j++) {
      const prop = props[j] as any;
      if (prop.selectedKeys && prop.selectedKeys.length > 0) fn(prop);
    }
  }
};

const forEachSelectedKey = (fn: (prop: any, key: number) => void) => {
  forEachSelectedProperty((prop) => {
    for (let k = 0; k < prop.selectedKeys.length; k++) fn(prop, prop.selectedKeys[k]);
  });
};

// ponytail: ExtendScript's engine is ES3-era - the build pipeline only
// transpiles syntax, it doesn't polyfill missing built-ins, so this avoids
// Set/Map and Math.sign (none exist at runtime) in favor of a plain object
// and a manual sign check.
const sign = (n: number): number => (n > 0 ? 1 : n < 0 ? -1 : 0);

// For N>=3 selected keyframes on a property whose values form a strictly
// ascending or descending run, the run's own interior keyframes (not its
// first/last) shouldn't get an artificial ease inserted - the motion is
// already heading the same direction through them, so a speed change
// there reads as a stutter rather than an intentional ease. Only defined
// for plain numeric (1D) properties; vectors/colors have no single
// "ascending" order. Returns selectedKeys indices to skip, keyed by index.
const interiorRunKeys = (prop: any): Record<number, boolean> => {
  const interior: Record<number, boolean> = {};
  if (prop.propertyValueType !== PropertyValueType.OneD) return interior;

  const keys: number[] = prop.selectedKeys;
  const values = keys.map((k) => prop.keyValue(k) as number);

  let i = 0;
  while (i < keys.length - 1) {
    const s = sign(values[i + 1] - values[i]);
    if (s === 0) {
      i++;
      continue;
    }
    let j = i;
    while (j < keys.length - 1 && sign(values[j + 1] - values[j]) === s) j++;
    if (j - i + 1 >= 3) {
      for (let r = i + 1; r < j; r++) interior[keys[r]] = true;
    }
    i = j;
  }
  return interior;
};

// value: 0 = linear, 1-100 = ease amount (becomes influence on a Bezier
// keyframe, speed left at 0). mode gates which handle(s) get touched -
// "in"/"out" only the matching side, "both" sets both.
export const applyEasing = (value: number, mode: "in" | "out" | "both"): void => {
  const interpType =
    value === 0 ? KeyframeInterpolationType.LINEAR : KeyframeInterpolationType.BEZIER;

  app.beginUndoGroup("Apply Easing");
  try {
    forEachSelectedProperty((prop) => {
      const skipEase = interiorRunKeys(prop);
      for (let k = 0; k < prop.selectedKeys.length; k++) {
        const key = prop.selectedKeys[k];

        // Interior-of-a-run keys keep Bezier (a smooth pass-through, not a
        // sudden Linear/Hold corner) but skip the slider's ease entirely -
        // ponytail: AE's scripting API has no literal "Continuous Bezier"
        // interpolation constant for temporal ease (that term is normally a
        // spatial-path concept), so "don't override this key's ease" is the
        // closest equivalent to leaving it smooth/continuous.
        const thisInterpType = skipEase[key] ? KeyframeInterpolationType.BEZIER : interpType;

        const inType = mode !== "out" ? thisInterpType : prop.keyInInterpolationType(key);
        const outType = mode !== "in" ? thisInterpType : prop.keyOutInterpolationType(key);
        prop.setInterpolationTypeAtKey(key, inType, outType);

        if (skipEase[key] || thisInterpType !== KeyframeInterpolationType.BEZIER) continue;
        const buildEase = (current: KeyframeEase[]) =>
          current.map(() => new KeyframeEase(0, value));
        const newIn = mode !== "out" ? buildEase(prop.keyInTemporalEase(key)) : prop.keyInTemporalEase(key);
        const newOut = mode !== "in" ? buildEase(prop.keyOutTemporalEase(key)) : prop.keyOutTemporalEase(key);
        prop.setTemporalEaseAtKey(key, newIn, newOut);
      }
    });
  } finally {
    app.endUndoGroup();
  }
};

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
  let found = false;
  forEachSelectedKey(() => {
    found = true;
  });
  return found;
};
