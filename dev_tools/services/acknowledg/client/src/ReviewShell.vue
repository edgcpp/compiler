<!--
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<script setup>
  import { computed, ref, provide, onUnmounted, watch } from 'vue'
  import { useRoute, useRouter, RouterView } from 'vue-router'

  import AnchoredPopover from './AnchoredPopover.vue'
  import AppButton from './AppButton.vue'
  import LoadingSpinner from './LoadingSpinner.vue'
  import Notice from './Notice.vue'
  import RichName from './RichName.vue'
  import { acquireSocketForReview } from './sockets.js'

  const route = useRoute()
  const router = useRouter()

  const reviewId = route.params.id
  const socket = acquireSocketForReview(reviewId)

  const socketOpen = ref(true)
  // Whether the server has told us which sources and editors are connected
  // yet.  Until then "no source is connected" would be a guess.
  const connectionStateReceived = ref(false)
  const connectionState = ref({
    name: [],
    sources: [],
    editors: []
  })
  const notices = ref([])

  // The review-info popover is toggled by clicking the icon (it closes on
  // Escape or a click elsewhere, see AnchoredPopover.vue).
  const reviewInfoRef = ref(null)
  const reviewInfoOpen = ref(false)
  const pendingTasks = ref({})
  let notificationIdCounter = 0

  const PENDING_TASK_LABELS = {
    'test.recording-update': {
      singular: 'Test Recording Update',
      plural: 'Test Recording Updates'
    },
    'bench.baseline-update': {
      singular: 'Baseline Update',
      plural: 'Baseline Updates'
    }
  }

  const displayUserNotice = (message, type) => {
    notices.value.unshift({
      id: notificationIdCounter++,
      text: message,
      type: type
    })
    // Note when changing this delay, change the corresponding delay in
    // Notice.vue.
    setTimeout(() => {
      notices.value.pop()
    }, 3 * 1000)
  }

  const beginPendingTask = (taskId) => {
    pendingTasks.value = {
      ...pendingTasks.value,
      [taskId]: (pendingTasks.value[taskId] ?? 0) + 1
    }
  }

  const completePendingTask = (taskId) => {
    const count = pendingTasks.value[taskId] ?? 0
    if (count <= 0) {
      return
    }
    if (count === 1) {
      const { [taskId]: _removed, ...rest } = pendingTasks.value
      pendingTasks.value = rest
    } else {
      pendingTasks.value = {
        ...pendingTasks.value,
        [taskId]: count - 1
      }
    }
  }

  const formatPendingTaskLabel = (taskId, count) => {
    const labels = PENDING_TASK_LABELS[taskId]
    if (!labels) {
      return taskId
    }
    return count === 1 ? labels.singular : labels.plural
  }

  const pendingTaskCount = computed(() => {
    return Object.values(pendingTasks.value).reduce(
      (total, count) => total + count,
      0
    )
  })

  const pendingTasksTooltip = computed(() => {
    return Object.entries(pendingTasks.value)
      .map(([taskId, count]) => {
        return `${count} - ${formatPendingTaskLabel(taskId, count)}`
      })
      .join('\n')
  })

  // A rich name (see PROTOCOL.md); the id until the server sends it.
  const reviewName = computed(() => {
    const name = connectionState.value.name
    return name?.length ? name : [{ type: 'text', text: reviewId }]
  })

  const sectionSources = computed(() => {
    const sources = [...(connectionState.value.sources ?? [])]
    sources.sort((a, b) => a.name.localeCompare(b.name))
    return sources
  })

  const reviewSectionSlug = computed(() => {
    const slug = route.params.slug
    return slug && slug !== '_' ? slug : null
  })

  const reviewSectionKind = computed(() => {
    if (route.name === 'review-benchmarks') {
      return 'bench'
    }
    if (route.name === 'review-tests') {
      return 'test'
    }
    return null
  })

  provide('reviewSocket', socket)
  provide('reviewSocketOpen', socketOpen)
  provide('reviewConnectionState', connectionState)
  provide('reviewId', reviewId)
  provide('reviewSectionSlug', reviewSectionSlug)
  provide('displayUserNotice', displayUserNotice)
  provide('beginPendingTask', beginPendingTask)

  const sectionNameForSlug = (slug, kind) => {
    const sources = sectionSources.value
    const match = sources.find(
      (source) => source.slug === slug &&
        (kind == null || source.kind === kind)
    )
    if (match?.name) {
      return match.name
    }
    return slug || 'unknown section'
  }

  const onSocketMessage = (event) => {
    const message = JSON.parse(event.data)
    if (message.t === 'review.connection-state') {
      connectionState.value = message.v
      connectionStateReceived.value = true
    } else if (message.t === 'test.edit.update-diff') {
      completePendingTask('test.recording-update')
      const sectionLabel = sectionNameForSlug(message.s, 'test')
      const configsLabel = (message.v.configs ?? []).join(', ')
      const targetLabel = configsLabel
        ? `${message.v.id} [${configsLabel}]`
        : message.v.id
      if (message.v.success) {
        displayUserNotice(
          `[${sectionLabel}] Recording updated successfully for: ` +
          `${targetLabel}`,
          'success'
        )
      } else {
        displayUserNotice(
          `[${sectionLabel}] Recording update failed for: ${targetLabel}. ` +
          'See (presumably) your terminal.',
          'error'
        )
      }
    } else if (message.t === 'bench.edit.update-baseline') {
      completePendingTask('bench.baseline-update')
      const sectionLabel = sectionNameForSlug(message.s, 'bench')
      if (message.v.success) {
        displayUserNotice(
          `[${sectionLabel}] Baseline updated successfully for: ` +
          `${message.v.id}`,
          'success'
        )
      } else {
        displayUserNotice(
          `[${sectionLabel}] Baseline update failed for: ${message.v.id}. ` +
          'See (presumably) your terminal.',
          'error'
        )
      }
    }
  }

  const onSocketClose = () => {
    socketOpen.value = false
  }

  socket.addEventListener('message', onSocketMessage)
  socket.addEventListener('close', onSocketClose)

  const routeNameForKind = (kind) => {
    return kind === 'bench' ? 'review-benchmarks' : 'review-tests'
  }

  const redirectToAvailableSection = () => {
    const sources = sectionSources.value
    if (sources.length === 0) {
      return
    }

    const currentSlug = reviewSectionSlug.value
    const currentKind = reviewSectionKind.value
    const currentOk = sources.some(
      (source) => source.kind === currentKind && source.slug === currentSlug
    )
    if (currentOk) {
      return
    }

    const sameKind = sources.find((source) => source.kind === currentKind)
    const target = sameKind ?? sources[0]
    router.replace({
      name: routeNameForKind(target.kind),
      params: { id: reviewId, slug: target.slug },
      hash: route.hash
    })
  }

  watch(
    [sectionSources, () => route.name, () => route.params.slug],
    redirectToAvailableSection,
    { immediate: true }
  )

  onUnmounted(() => {
    socket.removeEventListener('message', onSocketMessage)
    socket.removeEventListener('close', onSocketClose)
    socket.close()
  })
</script>

<template>
  <div class="notice-block">
    <notice v-for="notice in notices"
            :message="notice.text"
            :type="notice.type"
            :key="notice.id" />
  </div>
  <div v-if="socketOpen" class="shell">
    <nav class="tabs cell-colors" aria-label="Review sections">
      <app-button bare
                  class="review-info"
                  ref="reviewInfoRef"
                  aria-label="Review information"
                  :aria-expanded="reviewInfoOpen"
                  @click="reviewInfoOpen = !reviewInfoOpen">
        <span class="review-info-icon" aria-hidden="true">i</span>
      </app-button>
      <anchored-popover :anchor="reviewInfoRef?.$el"
                        label="Review name"
                        :open="reviewInfoOpen"
                        @close="reviewInfoOpen = false">
        <rich-name :name="reviewName" />
      </anchored-popover>
      <app-button
          v-for="source in sectionSources"
          :key="`${source.kind}:${source.slug}`"
          :to="{
            name: routeNameForKind(source.kind),
            params: { id: reviewId, slug: source.slug }
          }"
          :aria-label="`${source.name} review`">
        {{ source.name }}
      </app-button>
      <div class="tabs-end">
        <div v-if="pendingTaskCount > 0"
             class="pending-tasks-counter"
             :title="pendingTasksTooltip">
          {{ pendingTaskCount }}
        </div>
        <app-button
            class="back-link"
            :to="{ name: 'reviews' }"
            aria-label="All open reviews">
          All Reviews
        </app-button>
      </div>
    </nav>
    <div v-if="connectionStateReceived" class="tab-body">
      <RouterView />
    </div>
    <div v-else class="initial-loader">
      <loading-spinner />
    </div>
  </div>
  <div v-if="!socketOpen" class="cell cell-colors connection-loss">
    <!-- This box is a flex container, which would make each piece of text
         and inline element below a separate flex item and drop the
         whitespace between them.  This div keeps them one inline flow. -->
    <div>
      Connection to session could not be established;
      please reload the page and try again or
      <app-button
          :to="{ name: 'reviews' }"
          class="inline-link-btn"
          aria-label="All open reviews">
        go back to the list of open reviews
      </app-button>.
    </div>
  </div>
</template>

<style scoped>
  .notice-block {
    position: fixed;
    margin-right: 15px;
    right: 0px;
    z-index: 1000;
  }
  .shell {
    height: 100vh;
    display: flex;
    flex-direction: column;
  }
  .tabs {
    display: flex;
    gap: 8px;
    margin-top: 10px;
    margin-left: 10px;
    margin-right: 10px;
    padding: 5px;
    border-radius: 8px;
    align-items: center;
    border-bottom: 1px solid rgba(127, 127, 127, 0.35);
  }
  .review-info {
    padding: 0;
    border: none;
    background: none;
    font: inherit;
    display: flex;
    align-items: center;
    margin-left: 4px;
    margin-right: 4px;
    border-radius: 9px;
  }
  .review-info:focus-visible {
    outline: 2px solid skyblue;
    outline-offset: 2px;
  }
  .review-info-icon {
    height: 18px;
    width: 18px;
    line-height: 18px;
    border-radius: 9px;
    font-family: serif;
    font-size: 13px;
    font-weight: bold;
    text-align: center;
    color: white;
    background-color: darkgrey;
    cursor: pointer;
    transition: background-color .4s;
  }
  .review-info:hover .review-info-icon,
  .review-info:focus-visible .review-info-icon,
  .review-info[aria-expanded="true"] .review-info-icon {
    background-color: black;
  }
  .tabs-end {
    margin-left: auto;
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .pending-tasks-counter {
    padding-top: 2px;
    padding-bottom: 2px;
    padding-left: 12px;
    padding-right: 12px;
    margin-right: 10px;
    border-radius: 15px;
    color: white;
    background-color: tan;
  }
  :deep(.inline-link-btn) {
    margin-left: .5em;
  }
  .tab-body {
    position: relative;
    flex: 1;
    min-height: 0;
  }
  .initial-loader {
    height: 70vh;
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 50px;
  }
  .connection-loss {
    height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
    margin: 10px;
    border-radius: 8px;
  }
  @media (prefers-color-scheme: light) {
    .cell-colors {
      color: black;
      background-color: white;
    }
  }
  @media (prefers-color-scheme: dark) {
    .cell-colors {
      color: white;
      background-color: #181A1B;
    }
  }
</style>
