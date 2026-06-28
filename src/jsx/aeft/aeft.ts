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

// value: 0 = linear, 1-100 = ease amount (becomes influence on a Bezier
// keyframe, speed left at 0). mode gates which handle(s) get touched -
// "in"/"out" only the matching side, "both" sets both.
export const applyEasing = (value: number, mode: "in" | "out" | "both"): void => {
  const comp = app.project.activeItem;
  if (!(comp instanceof CompItem)) return;

  const interpType =
    value === 0 ? KeyframeInterpolationType.LINEAR : KeyframeInterpolationType.BEZIER;

  app.beginUndoGroup("Apply Easing");
  try {
    for (let i = 0; i < comp.selectedLayers.length; i++) {
      const props = comp.selectedLayers[i].selectedProperties;
      for (let j = 0; j < props.length; j++) {
        const prop = props[j] as any;
        if (!prop.selectedKeys) continue;
        for (let k = 0; k < prop.selectedKeys.length; k++) {
          const key = prop.selectedKeys[k];
          const inType = mode !== "out" ? interpType : prop.keyInInterpolationType(key);
          const outType = mode !== "in" ? interpType : prop.keyOutInterpolationType(key);
          prop.setInterpolationTypeAtKey(key, inType, outType);

          if (interpType !== KeyframeInterpolationType.BEZIER) continue;
          const buildEase = (current: KeyframeEase[]) =>
            current.map(() => new KeyframeEase(0, value));
          const newIn = mode !== "out" ? buildEase(prop.keyInTemporalEase(key)) : prop.keyInTemporalEase(key);
          const newOut = mode !== "in" ? buildEase(prop.keyOutTemporalEase(key)) : prop.keyOutTemporalEase(key);
          prop.setTemporalEaseAtKey(key, newIn, newOut);
        }
      }
    }
  } finally {
    app.endUndoGroup();
  }
};

export const isAnyKeyframeSelected = (): boolean => {
  const comp = app.project.activeItem;
  if (!(comp instanceof CompItem)) return false;
  for (let i = 0; i < comp.selectedLayers.length; i++) {
    const props = comp.selectedLayers[i].selectedProperties;
    for (let j = 0; j < props.length; j++) {
      const prop = props[j] as Property;
      if (prop.selectedKeys && prop.selectedKeys.length > 0) return true;
    }
  }
  return false;
};
