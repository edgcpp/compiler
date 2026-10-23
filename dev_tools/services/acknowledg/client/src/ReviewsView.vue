<!--
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<script setup>
  import { computed, ref, onMounted, onUnmounted } from 'vue'
  import { useRouter } from 'vue-router'

  import { acquireSocketForReviews } from './sockets.js'

  import LoadingSpinner from './LoadingSpinner.vue'
  import RichName from './RichName.vue'

  const router = useRouter()

  // Open a socket to discover reviews
  const socket = acquireSocketForReviews()

  const socketOpen = ref(true)
  const reviews = ref({})

  // The table's header sticks just below the (sticky) page heading.
  const heading = ref(null)
  const headingHeight = ref(0)
  const measureHeading = () => {
    headingHeight.value = heading.value?.offsetHeight ?? 0
  }
  const headingObserver = new ResizeObserver(measureHeading)
  onMounted(() => {
    measureHeading()
    headingObserver.observe(heading.value)
  })

  const defaultTabForReview = (review) => {
    if (review.sockets.test_source) {
      return 'review-tests'
    }
    if (review.sockets.bench_source) {
      return 'review-benchmarks'
    }
    return 'review-tests'
  }

  const reviewIsOpenable = (review) => {
    return review.sockets.test_source || review.sockets.bench_source
  }

  const describeReviewtype = (review) => {
    if (review.sockets.test_source && review.sockets.bench_source) {
      return "Test & Benchmark"
    } else if (review.sockets.test_source) {
      return "Test"
    } else if (review.sockets.bench_source) {
      return "Benchmark"
    }
    return "Orphan"
  }

  const formatCreatedAt = (createdAt) => {
    if (!createdAt) {
      return ''
    }
    const date = new Date(createdAt)
    if (Number.isNaN(date.getTime())) {
      return createdAt
    }
    return date.toLocaleString()
  }

  const reviewRoute = (review) => {
    if (!reviewIsOpenable(review)) {
      return null
    }
    return {
      name: defaultTabForReview(review),
      params: { id: review.id, slug: '_' }
    }
  }

  // The whole row opens the review; links in it (the review's own, or one
  // in its name) do what they say. Keyboard users have the review link.
  const onRowClick = (review, event) => {
    const route = reviewRoute(review)
    if (route === null || event.target.closest('a') !== null) {
      return
    }
    router.push(route)
  }

  const sortedReviews = computed(() => {
    return Object.values(reviews.value).sort((a, b) => {
      const aTime = Date.parse(a.created_at) || 0
      const bTime = Date.parse(b.created_at) || 0
      return bTime - aTime
    })
  })

  const reviewsFromList = (list) => {
    const mapped = {}
    for (const review of list) {
      mapped[String(review.id)] = review
    }
    return mapped
  }

  const applyReviewListPatch = (patch) => {
    if (patch.remove) {
      for (const id of patch.remove) {
        delete reviews.value[String(id)]
      }
    }

    if (patch.upsert) {
      for (const review of patch.upsert) {
        reviews.value[String(review.id)] = review
      }
    }
  }

  const onSocketMessage = (event) => {
    let message = JSON.parse(event.data)

    if (message.t === "list-reviews") {
      reviews.value = reviewsFromList(message.v)
    } else if (message.t === "list-reviews-patch") {
      applyReviewListPatch(message.v)
    } else {
      console.assert(false, `Unexpected reviews list message: ${message.t}`)
    }
  }

  const onSocketClose = () => {
    socketOpen.value = false
  }

  socket.addEventListener("message", onSocketMessage)
  socket.addEventListener("close", onSocketClose)

  onUnmounted(() => {
    headingObserver.disconnect()
    socket.removeEventListener("message", onSocketMessage)
    socket.removeEventListener("close", onSocketClose)
    socket.close()
  })
</script>

<template>
  <div class="cell cell-colors">
    <div ref="heading" class="heading cell-colors">
      <h1>
        AcknowlEDG
        <loading-spinner
            v-if="socketOpen"
            style="position: absolute; right: 0px;"/>
      </h1>
    </div>
    <div v-if="socketOpen">
      <table style="width: 100%;">
        <colgroup>
          <col span="1">
          <col span="3" width="0*">
        </colgroup>
        <thead :style="{ '--heading-height': `${headingHeight}px` }">
          <tr>
            <th class="cell-colors" style="text-align: left;">Name</th>
            <th class="cell-colors">Started</th>
            <th class="cell-colors">Review Connections</th>
            <th class="cell-colors">Review Type</th>
          </tr>
        </thead>
        <transition-group name="review-list" tag="tbody" appear>
          <tr v-for="review in sortedReviews"
              :key="review.id"
              :class="{ openable: reviewIsOpenable(review) }"
              @click="onRowClick(review, $event)">
            <td>
              <div class="name-cell">
                <!-- A dot until the review is opened (see .unseen-marker). -->
                <router-link v-if="reviewRoute(review)"
                             class="unseen-marker"
                             :to="reviewRoute(review)"
                             tabindex="-1"
                             aria-hidden="true" />
                <span v-else class="unseen-marker-space" />
                <rich-name :name="review.name" :to="reviewRoute(review)" />
              </div>
            </td>
            <td class="started">
              {{ formatCreatedAt(review.created_at) }}
            </td>
            <td style="text-align: center;">
              {{ review.sockets.reviewer }}
            </td>
            <td class="review-type" :title="describeReviewtype(review)">
              <span v-if="reviewIsOpenable(review)"
                    class="type-icons"
                    role="img"
                    :aria-label="describeReviewtype(review)">
                <!-- A flask: tests. -->
                <svg v-if="review.sockets.test_source"
                     class="type-icon test-icon"
                     viewBox="0 0 16 16"
                     aria-hidden="true"
                     focusable="false">
                  <path d="M5.5 1.5h5M6.5 1.5v4.25L2.75 12.4a1.4 1.4 0 0 0
                           1.2 2.1h8.1a1.4 1.4 0 0 0 1.2-2.1L9.5 5.75V1.5"/>
                  <path d="M4.25 9.75h7.5"/>
                </svg>
                <!-- A gauge: benchmarks. -->
                <svg v-if="review.sockets.bench_source"
                     class="type-icon bench-icon"
                     viewBox="0 0 16 16"
                     aria-hidden="true"
                     focusable="false">
                  <path d="M3.76 13.24A6 6 0 1 1 12.24 13.24"/>
                  <path d="M8 9l3-3M8 3v1.25"/>
                  <circle cx="8" cy="9" r="1.1"/>
                </svg>
              </span>
              <span v-else class="orphan">{{ describeReviewtype(review) }}</span>
            </td>
          </tr>
        </transition-group>
      </table>
    </div>
    <div v-if="!socketOpen">
      Connection to server could not be established;
      please reload the page and try again.
    </div>
  </div>
</template>

<style scoped>
  .cell {
    padding-left: 5px;
    padding-right: 5px;
    border-radius: 8px;
    margin: 10px;
    padding: 20px;
    /* The (sticky) heading has the top padding, so rows can't scroll into
       view above it. */
    padding-top: 0px;
    height: calc(100vh - 40px);
    overflow: auto;
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
  .heading {
    position: sticky;
    top: 0px;
    z-index: 2;
    padding-top: 20px;
    padding-bottom: 8px;
  }
  h1 {
   margin: 0px;
  }
  th {
    position: sticky;
    top: var(--heading-height, 0px);
    z-index: 1;
    padding-top: 4px;
    padding-bottom: 4px;
    padding-left: 1em;
    padding-right: 1em;
    white-space: nowrap;
    box-shadow: inset 0 -1px rgba(127, 127, 127, 0.35);
  }
  th:first-child {
    padding-left: 8px;
  }
  td {
    padding: 6px 1em;
  }
  td:first-child {
    padding-left: 8px;
    border-top-left-radius: 6px;
    border-bottom-left-radius: 6px;
  }
  td:last-child {
    border-top-right-radius: 6px;
    border-bottom-right-radius: 6px;
  }
  tr.openable {
    cursor: pointer;
  }
  .name-cell {
    display: flex;
    align-items: flex-start;
    gap: 8px;
  }
  /* Browsers only let :visited change colors (and ignore their alpha), so
     the dot is "hidden" once its review has been opened by painting it the
     color of what's behind it: the cell, or the row while hovered. Keep
     these colors in sync with .cell-colors and the row hover colors. */
  .unseen-marker,
  .unseen-marker-space {
    flex: none;
    width: 7px;
    height: 7px;
    margin-top: 0.5em;
    border-radius: 50%;
  }
  @media (prefers-color-scheme: light) {
    tr.openable:hover td {
      background-color: #f0f0f0;
    }
    .unseen-marker {
      background-color: #0b57d0;
    }
    .unseen-marker:visited {
      background-color: white;
    }
    tr.openable:hover .unseen-marker:visited {
      background-color: #f0f0f0;
    }
  }
  @media (prefers-color-scheme: dark) {
    tr.openable:hover td {
      background-color: #242627;
    }
    .unseen-marker {
      background-color: skyblue;
    }
    .unseen-marker:visited {
      background-color: #181A1B;
    }
    tr.openable:hover .unseen-marker:visited {
      background-color: #242627;
    }
  }
  .started {
    text-align: center;
    white-space: nowrap;
    opacity: 0.65;
  }
  .review-type {
    text-align: center;
  }
  .type-icons {
    display: inline-flex;
    gap: 6px;
    vertical-align: middle;
  }
  .type-icon {
    width: 18px;
    height: 18px;
    fill: none;
    stroke: currentColor;
    stroke-width: 1.4;
    stroke-linecap: round;
    stroke-linejoin: round;
  }
  .bench-icon circle {
    fill: currentColor;
    stroke: none;
  }
  .orphan {
    opacity: 0.65;
  }
  @media (prefers-color-scheme: light) {
    .test-icon {
      color: #1a7f37;
    }
    .bench-icon {
      color: #8250df;
    }
  }
  @media (prefers-color-scheme: dark) {
    .test-icon {
      color: #56d364;
    }
    .bench-icon {
      color: #bc8cff;
    }
  }
  .review-list-move {
    transition: transform 0.45s ease;
  }
  .review-list-enter-active,
  .review-list-leave-active {
    transition: opacity 0.35s ease, transform 0.35s ease;
  }
  .review-list-enter-from {
    opacity: 0;
    transform: translateX(-12px);
  }
  .review-list-leave-to {
    opacity: 0;
    transform: translateX(24px);
  }
  @media (prefers-reduced-motion: reduce) {
    .review-list-move,
    .review-list-enter-active,
    .review-list-leave-active {
      transition: none;
    }
  }
</style>
