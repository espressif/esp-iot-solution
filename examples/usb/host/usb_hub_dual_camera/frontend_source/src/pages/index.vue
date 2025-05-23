<template>
  <v-container max-width="600">
    <CameraCard v-for="item of mainStore.clientCameraActivedList" :key="item.id" :cam-id="item.id"
      :resolution="item.resolution" :src="item.src || null" />

    <v-card v-if="mainStore.cameraNumberLimit > mainStore.clientCameraActivedList.length">
      <template #title>
        <div class="text-center">
          Add Camera
        </div>
      </template>
      <template #text>
        <v-select v-model="formInputCamera" :items="mainStore.clientCameraOptions" label="Select a Camera"
          density="comfortable" variant="outlined" item-title="id" return-object />
        <v-select v-model="formInputRes" :items="formInputCamera?.resolutions || []" label="Select its Resolution"
          density="comfortable" variant="outlined"
          :item-props="item => ({ title: `${item.format ? '[' + item.format + ']' : ''} ${item.width} x ${item.height}` })"
          return-object />
        <v-btn block variant="outlined" color="primary" :disabled="!formInputCamera || !formInputRes"
          @click="addCamera">
          Add
        </v-btn>
      </template>
    </v-card>
  </v-container>

  <v-dialog v-model="webRequestErr" persistent>
    <div class="mx-auto my-auto text-center font-large">
      Web Request Error <br>
      Please Refresh This Page
    </div>
  </v-dialog>
</template>

<script lang="ts" setup>
import { ref } from 'vue';

import type { Resolution, Camera } from '@/utils'
import { useMainStore } from '@/store/mainstore';
import CameraCard from '@/components/CameraCard.vue';

const mainStore = useMainStore()

let updateTimeoutId: ReturnType<typeof setTimeout> | null = null

const webRequestErr = ref<boolean>(false)
const formInputCamera = ref<Camera | null>(null)
const formInputRes = ref<Resolution | null>(null)

const addCamera = async () => {
  if (!formInputCamera.value || !formInputRes.value) return

  const cameraBaseUrl = mainStore.webBase.getAvailableUrl("/", false)
  if (!cameraBaseUrl) return

  mainStore.clientCameraActivedList.push({
    id: formInputCamera.value.id,
    resolution: formInputRes.value,
    src: cameraBaseUrl
  })

  mainStore.clientCameraOptions = mainStore.clientCameraOptions.filter(item => String(item.id) !== String(formInputCamera?.value?.id))
}

const updateServerSideStatus = async () => {
  await mainStore.fetchServerSideStatus()
  updateTimeoutId = setTimeout(updateServerSideStatus, 2000)
}

onBeforeMount(() => {
  updateServerSideStatus()
})

onBeforeUnmount(() => {
  if (updateTimeoutId) {
    clearInterval(updateTimeoutId)
  }
})

watch(formInputCamera, () => {
  formInputRes.value = null
})

watch(mainStore, (newValue) => {
  webRequestErr.value = newValue.netRequestStatus === "error"
  if (!newValue.clientCameraOptions.find(item => String(item.id) === String(formInputCamera.value?.id))) formInputCamera.value = null
})
</script>

<style lang="css" scoped>
.text-center {
  text-align: center;
}

.font-large {
  font-size: large;
}
</style>
