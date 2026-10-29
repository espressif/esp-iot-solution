<template>
  <v-card class="mb-3">
    <div
      class="camera-stream-shell"
      :style="{ aspectRatio: `${props.resolution.width} / ${props.resolution.height}` }"
    >
      <img
        v-if="cameraImageSrc"
        :id="`cam-${props.camId}-img`"
        :src="cameraImageSrc"
        class="camera-stream-image"
        alt="Camera stream"
        @load="onImageLoad"
        @error="onImageError"
      >

      <div
        v-if="cameraStatus === 'pending'"
        class="camera-stream-overlay d-flex align-center justify-center fill-height"
      >
        <v-progress-circular color="grey-lighten-4" indeterminate />
      </div>

      <div
        v-else-if="cameraStatus === 'err'"
        class="camera-stream-overlay d-flex align-center justify-center fill-height"
      >
        <div style="height: 100px;" class="d-flex flex-column">
          <div class="my-auto" style="font-size: larger; font-weight: bold;">
            {{ errMsg || "Camera is inactive" }}
          </div>
        </div>
      </div>
    </div>
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
}>()

const cameraStatus = ref<'pending' | 'normal' | 'err'>('pending')
const cameraImageSrc = ref<string | undefined>(undefined)
const errMsg = ref<string | null>(null)
const capturedImage = ref<URL | string | null>(null)
const canvas = ref<HTMLCanvasElement | null>(null)

let imageLoadTimeoutId: ReturnType<typeof setTimeout> | null = null
let quitting = false

const clearImageLoadTimeout = () => {
  if (imageLoadTimeoutId) {
    clearTimeout(imageLoadTimeoutId)
    imageLoadTimeoutId = null
  }
}

const setPendingTimeout = () => {
  clearImageLoadTimeout()
  imageLoadTimeoutId = setTimeout(() => {
    if (cameraStatus.value === 'pending') {
      errMsg.value = 'Stream started but no frame was rendered'
      cameraStatus.value = 'err'
    }
  }, 12000)
}

const quit = async () => {
  if (quitting) return
  quitting = true
  clearImageLoadTimeout()
  cameraImageSrc.value = '/api/404'
  const deactivateEndpoint = new URL('/api/deactivate', location.href)
  try {
    await fetch(deactivateEndpoint, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ id: props.camId })
    })
  } catch (err) {
    console.log(err)
  }

  setTimeout(() => {
    mainStore.clientCameraActivedList = mainStore.clientCameraActivedList.filter(item => String(item.id) !== String(props.camId))
  }, 100)
}

const captureFrame = () => {
  if (!cameraImageSrc.value || !canvas.value) return
  const camImageElement = document.getElementById(`cam-${props.camId}-img`) as HTMLImageElement | null
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

const onImageLoad = () => {
  clearImageLoadTimeout()
  cameraStatus.value = 'normal'
  errMsg.value = null
}

const onImageError = () => {
  clearImageLoadTimeout()
  cameraStatus.value = 'err'
  errMsg.value = 'Failed to render MJPEG stream'
}

onBeforeMount(async () => {
  const activeEndPoint = new URL('/api/active', location.href)
  fetch(activeEndPoint, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({
      id: props.camId,
      resolution: {
        format: props.resolution.format ?? undefined,
        width: props.resolution.width,
        height: props.resolution.height,
        index: props.resolution.index,
      }
    })
  })
    .then(response => {
      if (response.ok) {
        cameraStatus.value = 'pending'
        errMsg.value = null
        const cameraImageSrcUrl = new URL(`/api/stream/${props.camId}`, location.href)
        cameraImageSrcUrl.searchParams.set('ts', String(Date.now()))
        cameraImageSrc.value = cameraImageSrcUrl.href
        setPendingTimeout()
      } else {
        cameraStatus.value = 'err'
        errMsg.value = `Failed to activate camera (${response.status})`
      }
    })
    .catch((err) => {
      console.log(err)
      cameraStatus.value = 'err'
      errMsg.value = 'Failed to activate camera'
    })
})

onBeforeUnmount(() => {
  clearImageLoadTimeout()
  void quit()
})

</script>

<style scoped>
.camera-stream-shell {
  position: relative;
  overflow: hidden;
  background: rgb(18, 18, 18);
}

.camera-stream-image {
  display: block;
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.camera-stream-overlay {
  position: absolute;
  inset: 0;
  background: rgba(0, 0, 0, 0.45);
  color: white;
}
</style>
