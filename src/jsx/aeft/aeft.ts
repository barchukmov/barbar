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
