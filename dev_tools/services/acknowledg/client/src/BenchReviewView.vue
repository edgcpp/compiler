<!--
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<script setup>
  import { computed, onUnmounted, ref, inject, provide, watch } from 'vue'
  import { useRoute, useRouter } from 'vue-router'

  import {
    computePercentChange,
    formChangeComment,
    formPercentChangeComment,
    formatSignedOps,
    isFaster,
    mean,
    median,
    opDelta,
    populationStdev
  } from './bench-metrics.js'
  import { formatBenchAnchor, parseBenchAnchor } from './review-anchor.js'
  import { protocolMessage } from './sockets.js'
  import AppButton from './AppButton.vue'
  import AppSelect from './AppSelect.vue'
  import BenchStatus from './BenchStatus.vue'
  import LoadingSpinner from './LoadingSpinner.vue'
  import ScrollableContentArea from './ScrollableContentArea.vue'

  const route = useRoute()
  const router = useRouter()

  const socket = inject('reviewSocket')
  const socketOpen = inject('reviewSocketOpen')
  const connectionState = inject('reviewConnectionState')
  const reviewSectionSlug = inject('reviewSectionSlug')
  const displayUserNotice = inject('displayUserNotice')
  const beginPendingTask = inject('beginPendingTask')

  const summary = ref(null)
  const benchmarks = ref([])
  const sortMode = ref('greatest-slowdown')
  const sortOptions = [
    { value: 'greatest-slowdown', label: 'Greatest slowdown' },
    { value: 'greatest-speedup', label: 'Greatest speedup' }
  ]
  const selectedBenchmarkId = ref(null)
  const displayTruncated = ref(false)
  const displayedOutput = ref(null)

  provide('activeBenchmarkId', selectedBenchmarkId)
  provide('benchSortMode', sortMode)

  const sectionAvailable = computed(() => {
    const slug = reviewSectionSlug.value
    if (!slug) {
      return false
    }
    return (connectionState.value.sources ?? []).some(
      (source) => source.kind === 'bench' && source.slug === slug
    )
  })

  const sendSectionMessage = (tag, value = undefined) => {
    socket.send(protocolMessage(tag, value, reviewSectionSlug.value))
  }

  const requestStatuses = () => {
    if (!reviewSectionSlug.value) {
      return
    }
    sendSectionMessage('bench.src.list-statuses')
  }

  if (socket.readyState === WebSocket.OPEN) {
    requestStatuses()
  } else {
    socket.addEventListener('open', requestStatuses)
  }

  const requestContent = () => {
    if (!selectedBenchmarkId.value) {
      return
    }
    displayedOutput.value = null
    sendSectionMessage(
      'bench.src.show-annotate',
      selectedBenchmarkId.value
    )
  }

  const selectBenchmark = (benchId) => {
    if (selectedBenchmarkId.value !== benchId) {
      selectedBenchmarkId.value = benchId
      requestContent()
      syncSelectionToHash()
    }
  }

  const syncSelectionToHash = () => {
    if (!selectedBenchmarkId.value) {
      return
    }
    const hash = `#${formatBenchAnchor(selectedBenchmarkId.value)}`
    if (route.hash !== hash) {
      router.replace({ hash })
    }
  }

  const applyHashSelection = () => {
    const anchor = parseBenchAnchor(route.hash)
    if (!anchor) {
      return
    }
    if (benchmarks.value.length === 0) {
      return
    }
    if (!benchmarks.value.some((bm) => bm.id === anchor.id)) {
      return
    }

    selectBenchmark(anchor.id)
  }

  // Baselines are updated by the section's editor; a section without one
  // (e.g. served by acknowledg-forge) is read-only.
  const canUpdateBaseline = computed(() => {
    return (connectionState.value.editors ?? []).some((editor) => {
      return editor.kind === 'bench' && editor.slug === reviewSectionSlug.value
    })
  })

  const updateBaseline = () => {
    if (!selectedBenchmarkId.value) {
      return
    }
    beginPendingTask('bench.baseline-update')
    sendSectionMessage(
      'bench.edit.update-baseline',
      selectedBenchmarkId.value
    )
  }

  const previous = () => {
    if (sortedBenchmarks.value.length === 0 || !selectedBenchmarkId.value) {
      return
    }
    const currIdx = sortedBenchmarks.value.findIndex(
      (bm) => bm.id === selectedBenchmarkId.value
    )
    const previousIdx = currIdx <= 0
      ? sortedBenchmarks.value.length - 1
      : currIdx - 1
    selectBenchmark(sortedBenchmarks.value[previousIdx].id)
  }

  const next = () => {
    if (sortedBenchmarks.value.length === 0 || !selectedBenchmarkId.value) {
      return
    }
    const currIdx = sortedBenchmarks.value.findIndex(
      (bm) => bm.id === selectedBenchmarkId.value
    )
    const nextIdx = currIdx === sortedBenchmarks.value.length - 1
      ? 0
      : currIdx + 1
    selectBenchmark(sortedBenchmarks.value[nextIdx].id)
  }

  const selectedBenchmark = computed(() => {
    return benchmarks.value.find((bm) => bm.id === selectedBenchmarkId.value)
  })

  // Match edg-bench-run-delta --sort choices; applied locally only.
  const sortedBenchmarks = computed(() => {
    const sorted = benchmarks.value.slice()
    sorted.sort((left, right) => {
      const leftChange = computePercentChange(left.baseline_ops, left.run_ops)
      const rightChange = computePercentChange(right.baseline_ops, right.run_ops)
      if (sortMode.value === 'greatest-speedup') {
        return rightChange - leftChange
      }
      return leftChange - rightChange
    })
    return sorted
  })

  // Per-benchmark series used by the summary distribution lines (same style as
  // edg-bench-run-delta's Average/Median/σ reporting).
  const samplePercentChanges = computed(() => {
    return benchmarks.value.map(
      (bm) => computePercentChange(bm.baseline_ops, bm.run_ops)
    )
  })

  const sampleOpDeltas = computed(() => {
    return benchmarks.value.map(
      (bm) => opDelta(bm.baseline_ops, bm.run_ops)
    )
  })

  const onSocketMessage = (event) => {
    const message = JSON.parse(event.data)
    if (message.s != null && message.s !== reviewSectionSlug.value) {
      return
    }
    if (message.t === 'bench.src.list-statuses') {
      summary.value = message.v.summary
      benchmarks.value = message.v.benchmarks
      applyHashSelection()
    } else if (message.t === 'bench.src.show-annotate') {
      if (message.v.id === selectedBenchmarkId.value) {
        displayTruncated.value = false
        if (message.v.content === null) {
          displayedOutput.value = 'Annotation unavailable!'
        } else {
          if (message.v.truncated) {
            displayTruncated.value = true
          }
          displayedOutput.value = message.v.content
        }
      }
    } else if (message.t === 'bench.edit.update-baseline') {
      // Pending-task accounting and notices are owned by ReviewShell.
      if (message.v.success) {
        requestStatuses()
        if (message.v.id === selectedBenchmarkId.value) {
          requestContent()
        }
      }
    } else if (message.t === 'bench.missing-src') {
      displayUserNotice(
        'No corresponding edg-bench-review source instance could be found. ' +
        'Is it still running?',
        'error'
      )
    } else if (message.t === 'bench.missing-edit') {
      displayUserNotice(
        'No corresponding edg-bench-review editor instance could be found. ' +
        'Is it still running?',
        'error'
      )
    } else if (message.t === 'invalid') {
      displayUserNotice(
        'Client <-> Server protocol error. See the console.',
        'error'
      )
      console.error('Invalid request:', message)
    } else if (!message.t.startsWith('bench.')) {
      // Do nothing.
    } else {
      displayUserNotice(
        `Unknown response from server: ${message.t}. See the console.`,
        "error"
      )
      console.error("Unknown response:", message)
    }
  }

  socket.addEventListener('message', onSocketMessage)

  watch(() => route.hash, applyHashSelection)
  watch(reviewSectionSlug, () => {
    summary.value = null
    benchmarks.value = []
    selectedBenchmarkId.value = null
    displayedOutput.value = null
    requestStatuses()
  })

  const keyListener = (event) => {
    if (!selectedBenchmarkId.value) {
      return
    }
    if (event.key === 'n' || event.key === 'j') {
      next()
    } else if (event.key === 'p' || event.key === 'k') {
      previous()
    } else if (event.key === 'Enter' && canUpdateBaseline.value) {
      event.preventDefault()
      updateBaseline()
    }
  }

  document.addEventListener('keydown', keyListener)
  onUnmounted(() => {
    document.removeEventListener('keydown', keyListener)
    socket.removeEventListener('open', requestStatuses)
    socket.removeEventListener('message', onSocketMessage)
  })
</script>

<template>
  <div v-if="socketOpen && sectionAvailable">
    <div class="cell cell-colors sidebar">
      <div class="heading cell-colors sidebar-header">
        <div v-if="summary" class="sidebar-header-row">
          <div class="baseline-label" :title="summary.baseline">
            Baseline: {{ summary.baseline }}
          </div>
          <div class="sort-control">
            <span class="sort-label">Sort</span>
            <app-select
                v-model="sortMode"
                :options="sortOptions"
                aria-label="Benchmark sort order" />
          </div>
        </div>
      </div>
      <div v-if="summary" class="summary-box">
        <div class="summary-row">
          <div class="summary-label">Overall</div>
          <div class="summary-value">
            <span class="change-badge"
                  :class="isFaster(summary.baseline_ops, summary.run_ops)
                           ? 'faster' : 'slower'">
              {{ formChangeComment(summary.baseline_ops, summary.run_ops) }}
            </span>
          </div>
        </div>
        <div class="summary-row">
          <div class="summary-label">Baseline OPs</div>
          <div class="summary-value">
            {{ summary.baseline_ops.toLocaleString() }}
          </div>
        </div>
        <div class="summary-row">
          <div class="summary-label">Run OPs</div>
          <div class="summary-value">
            {{ summary.run_ops.toLocaleString() }}
          </div>
        </div>
        <div class="summary-row">
          <div class="summary-label">Benchmarks</div>
          <div class="summary-value">{{ benchmarks.length }}</div>
        </div>
        <template v-if="samplePercentChanges.length !== 0">
          <div class="summary-row">
            <div class="summary-label">Average Change (%)</div>
            <div class="summary-value">
              {{ formPercentChangeComment(mean(samplePercentChanges)) }}
              σ {{ populationStdev(samplePercentChanges).toFixed(2) }}%
            </div>
          </div>
          <div class="summary-row">
            <div class="summary-label">Median Change (%)</div>
            <div class="summary-value">
              {{ formPercentChangeComment(median(samplePercentChanges)) }}
            </div>
          </div>
          <div class="summary-row">
            <div class="summary-label">Average Change (OPs)</div>
            <div class="summary-value">
              {{ formatSignedOps(Math.round(mean(sampleOpDeltas))) }}
              σ {{ Math.round(populationStdev(sampleOpDeltas)).toLocaleString() }}
            </div>
          </div>
          <div class="summary-row">
            <div class="summary-label">Median Change (OPs)</div>
            <div class="summary-value">
              {{ formatSignedOps(Math.round(median(sampleOpDeltas))) }}
            </div>
          </div>
        </template>
      </div>
      <div v-else class="loader-container">
        <loading-spinner />
      </div>
      <div class="bench-list">
        <bench-status
            v-for="bm in sortedBenchmarks"
            :key="bm.id"
            :benchmark="bm"
            @status-clicked="selectBenchmark" />
      </div>
    </div>
    <scrollable-content-area
        v-if="selectedBenchmarkId"
        class="cell cell-colors content-width content">
      <div class="heading cell-content cell-colors">
        <div v-if="selectedBenchmark" class="selected-benchmark-heading">
          <div class="heading-name"
               :title="selectedBenchmark.id">
            {{ selectedBenchmark.id }}
          </div>
          <div class="change-badge-container">
            <span class="change-badge"
                  :class="isFaster(selectedBenchmark.baseline_ops,
                                   selectedBenchmark.run_ops)
                           ? 'faster' : 'slower'">
              {{ formChangeComment(selectedBenchmark.baseline_ops,
                                   selectedBenchmark.run_ops) }}
            </span>
          </div>
        </div>
        <div v-if="selectedBenchmark" class="benchmark-op-commentary">
          {{ selectedBenchmark.baseline_ops.toLocaleString() }} ->
          {{ selectedBenchmark.run_ops.toLocaleString() }} OPs
          ({{ formatSignedOps(opDelta(selectedBenchmark.baseline_ops,
                                      selectedBenchmark.run_ops)) }})
        </div>
        <div v-if="displayedOutput !== null && displayTruncated"
             class="truncation-warning">
          Content Too Long: Display Truncated
        </div>
      </div>
      <div v-if="displayedOutput === null" class="loader-container">
        <loading-spinner />
      </div>
      <div v-if="displayedOutput !== null" class="cell-content">
        <pre>{{ displayedOutput }}</pre>
      </div>
    </scrollable-content-area>
    <div v-if="!selectedBenchmarkId" class="cell cell-colors content-width content">
      <div style="text-align: center; margin: 5em">
        Select a benchmark to continue...
      </div>
    </div>
    <div v-if="selectedBenchmarkId"
         class="cell cell-colors content-width content-footer">
      <div class="cell-content">
        <app-button
            keyshortcuts="P K"
            aria-label="Previous benchmark"
            @click="previous()">
          Previous (p/k)
        </app-button>
        <app-button
            v-if="canUpdateBaseline"
            keyshortcuts="Enter"
            aria-label="Update baseline"
            @click="updateBaseline()">
          Update Baseline (enter)
        </app-button>
        <app-button
            keyshortcuts="N J"
            aria-label="Next benchmark"
            @click="next()">
          Next (n/j)
        </app-button>
      </div>
    </div>
  </div>
  <div v-if="socketOpen && !sectionAvailable"
       class="cell cell-colors connection-loss">
    No benchmark source is connected for this review.
    Start <code>edg-bench-review</code> with the same review UUID, or open the
    Tests tab if a test source is available.
  </div>
</template>

<style scoped>
  .cell {
    border-radius: 8px;
    margin: 10px;
    overflow: auto;
  }
  .cell-content {
    padding-left: 5px;
    padding-right: 5px;
  }
  .connection-loss {
    height: calc(100vh - 80px);
    display: flex;
    align-items: center;
    justify-content: center;
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
    left: 0px;
    padding-top: 10px;
    padding-bottom: 10px;
  }
  .sidebar-header {
    z-index: 1;
  }
  .sidebar-header-row {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 0 8px;
    min-height: 2em;
  }
  .baseline-label {
    flex: 1;
    min-width: 0;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    font-size: 0.95em;
  }
  .sort-control {
    display: flex;
    align-items: center;
    gap: 6px;
    flex-shrink: 0;
    font-size: 0.9em;
  }
  .sort-label {
    opacity: 0.85;
  }
  .summary-box {
    font-size: 0.9em;
    line-height: 1.4;
    padding: 0px 8px 0 8px;
  }
  .summary-row {
    display: flex;
    align-items: center;
    gap: 1em;
    padding: 2px 0;
  }
  .summary-label {
    flex-shrink: 0;
    white-space: nowrap;
    opacity: 0.85;
  }
  .summary-value {
    flex: 1;
    min-width: 0;
    text-align: right;
  }
  .bench-list {
    padding-bottom: 10px;
  }
  .change-badge {
    display: inline-block;
    padding: 2px 6px;
    border-radius: 3px;
    font-weight: bold;
  }
  .change-badge.faster {
    background-color: #618555;
    color: white;
  }
  .change-badge.slower {
    background-color: #d1173e;
    color: white;
  }
  .content-width {
    width: calc(100% - (28em + 40px));
  }
  .content-footer {
    position: absolute;
    bottom: 0px;
    right: 0px;
    margin: 10px;
  }
  .content-footer .cell-content {
    display: flex;
    justify-content: space-between;
    padding: 5px;
  }
  .sidebar {
    position: absolute;
    left: 0px;
    top: 0px;
    width: 28em;
    max-height: calc(100vh - 70px);
  }
  .content {
    position: absolute;
    right: 0px;
    top: 0px;
    height: calc(100% - 65px);
  }
  .selected-benchmark-heading {
    display: flex;
    align-items: center;
    gap: 10px;
  }
  .heading-name {
    flex: 1;
    min-width: 0;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .change-badge-container {
    flex-shrink: 0;
  }
  .benchmark-op-commentary {
    font-size: 0.9em;
    line-height: 1.45;
    white-space: pre-wrap;
    overflow-wrap: anywhere;
    margin-top: 6px;
    text-align: center;
  }
  .loader-container {
    padding: 40px;
    font-size: 50px;
    display: flex;
    justify-content: center;
  }
  .truncation-warning {
    color: white;
    background-color: orange;
    border-radius: 10px;
    text-align: center;
    padding: 5px;
    margin-top: 10px;
    margin-left: 7px;
    margin-right: 7px;
  }
</style>
