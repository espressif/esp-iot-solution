<template>
  <v-card class="mb-3">
    <v-img :src="cameraImageSrc" :aspect-ratio="props.resolution.width / props.resolution.height" cover :id="`cam-${props.camId}-v-img`" crossorigin="anonymous">
      <template #placeholder>
        <div class="d-flex align-center justify-center fill-height">
          <v-progress-circular color="grey-lighten-4" indeterminate />
        </div>
      </template>
      <template #error>
        <div class="d-flex align-center justify-center fill-height">
          <div style="height: 100px;" class="d-flex flex-column">
            <div class="my-auto" style="font-size: larger; font-weight: bold;">
              {{ errMsg || "Camera is inactive" }}
            </div>
          </div>
        </div>
      </template>
    </v-img>
    <div class="d-flex justify-space-between align-center my-2 mx-3">
      <div>
        <div style="font-weight: bold; font-size: larger;">
          Camera #{{ props.camId }}
        </div>
        <div style="font-size: smaller; opacity: 70%;">
          {{ props.resolution.format ? `[${props.resolution.format}] ` : '' }}{{ props.resolution.width }} x {{
            props.resolution.height }}
        </div>
      </div>
      <div class="d-flex">
        <v-btn variant="tonal" @click="captureFrame" :icon="mdiCameraOutline" class="mr-1" />
        <v-btn variant="tonal" @click="quit" :icon="mdiWindowClose" />
      </div>
    </div>
  </v-card>
  <canvas ref="canvas" style="display: none;" />
</template>

<script setup lang="ts">

import { ref, onBeforeMount, onBeforeUnmount } from 'vue'
import { mdiCameraOutline, mdiWindowClose } from '@mdi/js';
import type { Resolution } from '@/utils';
import { useMainStore } from '@/store/mainstore';

const mainStore = useMainStore()

const props = defineProps<{
  camId: number | string,
  resolution: Resolution,
  src: URL | null,
}>()

const cameraStatus = ref<'pending' | 'normal' | 'err'>('pending')
const cameraImageSrc = ref<string | undefined>(undefined)
const errMsg = ref<string | null>(null)
const capturedImage = ref<URL | string | null>(null)
const canvas = ref<HTMLCanvasElement | null>(null)

const quit = () => {
  cameraImageSrc.value = '/api/404'

  setTimeout(() => {
    if (props.src) mainStore.webBase.releasePort(props.src.port || (props.src.protocol === 'https:' ? 443 : 80))
    mainStore.clientCameraActivedList = mainStore.clientCameraActivedList.filter(item => String(item.id) !== String(props.camId))
  }, 100)
}

const captureFrame = () => {
  if (!cameraImageSrc.value || !canvas.value) return
  const camElement = document.getElementById(`cam-${props.camId}-v-img`)
  if (!camElement) return
  const camImageElement = camElement.querySelector('img');
  if (!camImageElement) return
  const ctx = canvas.value.getContext("2d")
  if (!ctx) return

  canvas.value.width = camImageElement.width
  canvas.value.height = camImageElement.height
  ctx.drawImage(camImageElement, 0, 0, canvas.value.width, canvas.value.height)
  capturedImage.value = canvas.value.toDataURL("image/jpeg")
  if (!capturedImage.value) return

  const link = document.createElement("a")
  link.href = capturedImage.value
  link.download = `capture_${props.camId}_${Date.now()}.jpg`
  document.body.appendChild(link)
  link.click()
  document.body.removeChild(link)
};


onBeforeMount(async () => {
  if (!props.src) {
    console.log("props.src is null")
    return
  }
  const activeEndPoint = new URL(props.src)
  activeEndPoint.pathname = '/api/active'
  activeEndPoint.search = ''
  fetch(activeEndPoint, {
    method: 'POST',
    body: JSON.stringify({
      id: props.camId,
      resolution: {
        format: props.resolution.format ?? undefined,
        width: props.resolution.height,
        height: props.resolution.width,
        index: props.resolution.index,
      }
    })
  })
    .then(response => {
      if (response.ok) {
        if (!props.src) {
          throw new Error("props.src is null")
        }
        cameraStatus.value = 'normal'
        const cameraImageSrcUrl = new URL(props.src)
        cameraImageSrcUrl.pathname = `/api/stream/${props.camId}`
        cameraImageSrc.value = cameraImageSrcUrl.href
      } else {
        cameraStatus.value = 'err'
      }
    })
    .catch((err) => {
      console.log(err)
      cameraStatus.value = 'err'
    })
})

onBeforeUnmount(() => {
  quit()
})

</script>
