// Shared by isAnyKeyframeSelected/setOutgoingHandleHold - they both need the
// same selected-layers -> selected-properties walk.
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
