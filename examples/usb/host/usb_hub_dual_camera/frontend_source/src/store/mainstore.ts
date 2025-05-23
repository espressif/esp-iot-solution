import { defineStore } from "pinia";
import { ref } from "vue";
import type { Camera, CameraActived } from "@/utils";
import { WebBase } from "@/utils";

export const useMainStore = defineStore("main", () => {
  const cameraNumberLimit = ref<number>(1);
  const serverCameraList = ref<Camera[]>([]);
  const serverCameraActivedList = ref<CameraActived[]>([]);
  const clientCameraActivedList = ref<CameraActived[]>([]);
  const clientCameraOptions = ref<Camera[]>([]);

  const netRequestStatus = ref<"normal" | "error">("normal");

  const webBase = new WebBase(new URL(location.href));

  const fetchServerSideStatus = async () => {
    const statusApiEndpoint = webBase.getAvailableUrl("/api/cameras", true);
    if (!statusApiEndpoint) return;

    try {
      const response = await fetch(statusApiEndpoint);
      const resjson = await response.json();

      if (resjson.limit && typeof resjson.limit === "number") {
        cameraNumberLimit.value = resjson.limit;
      }

      if (!resjson.cameras) {
        netRequestStatus.value = "error";
        return;
      }

      serverCameraList.value = resjson.cameras as Camera[];
      serverCameraActivedList.value = resjson.activated as CameraActived[];

      const newAvailableList = [] as Camera[];

      for (const cam of resjson.cameras as Camera[]) {
        console.log(cam);
        // The camera has been activated on the client side.
        if (
          clientCameraActivedList.value.find(
            (item) => String(item.id) === String(cam.id)
          )
        )
          continue;

        // The camera has been activated on the server side but not locally.
        const findServerActivated = (resjson.activated as CameraActived[]).find(
          (item) => String(item.id) === String(cam.id)
        );
        if (findServerActivated) {
          newAvailableList.push({
            id: findServerActivated.id,
            resolutions: [findServerActivated.resolution],
          });
          continue;
        }

        // The free camera
        newAvailableList.push(cam);
      }
      clientCameraOptions.value = newAvailableList;
    } catch (err) {
      console.log(err);
      netRequestStatus.value = "error";
      clientCameraOptions.value.length = 0;
      return;
    }
  };

  return {
    cameraNumberLimit,
    serverCameraList,
    serverCameraActivedList,
    clientCameraActivedList,
    clientCameraOptions,
    netRequestStatus,
    webBase,
    fetchServerSideStatus,
  };
});
