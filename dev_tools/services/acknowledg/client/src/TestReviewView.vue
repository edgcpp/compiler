<!--
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<script setup>
  import {
    computed,
    ref,
    onUnmounted,
    provide,
    inject,
    watch
  } from 'vue'
  import { useRoute, useRouter } from 'vue-router'

  import { protocolMessage } from './sockets.js'
  import { computeGradientStyleForConfigs } from './config-color.js'
  import { formatTestAnchor, parseTestAnchor } from './review-anchor.js'

  import DiffLine from './DiffLine.vue'
  import LoadingSpinner from './LoadingSpinner.vue'
  import ScrollableContentArea from './ScrollableContentArea.vue'
  import TestStatus from './TestStatus.vue'
  import AppButton from './AppButton.vue'
  import AppSelect from './AppSelect.vue'

  const route = useRoute()
  const router = useRouter()

  const socket = inject('reviewSocket')
  const socketOpen = inject('reviewSocketOpen')
  const connectionState = inject('reviewConnectionState')
  const reviewSectionSlug = inject('reviewSectionSlug')
  const displayUserNotice = inject('displayUserNotice')
  const beginPendingTask = inject('beginPendingTask')
  const testStatusSelection = ref(0)
  const testStatuses = ref([])
  const displayedTestName = ref("")
  const displayedDiffHash = ref("")
  const displayedGroupConfigs = ref([])
  const displayedTestAssocFiles = ref(null)
  const displayedTestAssocFileName = ref(null)
  const displayTruncated = ref(false)
  const currDiffDivergenceNotice = ref(null)
  const displayedOutput = ref("")
  const outputViewModeTarget = ref("test.src.show-diff")
  const isMultiConfig = ref(false)

  const displayedTestConfig = computed(() => {
    return displayedGroupConfigs.value[0] ?? ""
  })

  // Expose selection state to child components (namely to TestStatus).
  provide('activeTestName', displayedTestName)
  provide('activeDiffHash', displayedDiffHash)
  provide('multiConfig', isMultiConfig)
  provide('statusListSelection', testStatusSelection)

  const clearContentArea = () => {
    displayedOutput.value = null
  }

  const configuredIdForDisplayedTest = () => {
    return `(${displayedTestConfig.value}) ${displayedTestName.value}`
  }

  const sectionAvailable = computed(() => {
    const slug = reviewSectionSlug.value
    if (!slug) {
      return false
    }
    return (connectionState.value.sources ?? []).some(
      (source) => source.kind === 'test' && source.slug === slug
    )
  })

  const sendSectionMessage = (tag, value = undefined) => {
    socket.send(protocolMessage(tag, value, reviewSectionSlug.value))
  }

  const requestContentAreaUpdate = () => {
    clearContentArea()
    if (outputViewModeTarget.value === 'test.src.show-src') {
      if (displayedTestAssocFiles.value === null) {
        // If this is the first time seeing the test source for this test,
        // fetch the list of associated files.
        sendSectionMessage(
          'test.src.list-assoc-files',
          displayedTestName.value
        )
      }
      sendSectionMessage('test.src.show-src', {
        'id': displayedTestName.value,
        'assoc_file': displayedTestAssocFileName.value
      })
    } else if (outputViewModeTarget.value === 'test.src.show-diff-curr') {
      sendSectionMessage('test.src.show-diff-curr', {
        id: configuredIdForDisplayedTest(),
        configs: displayedGroupConfigs.value
      })
    } else {
      // Most requests fall into this format.
      sendSectionMessage(
        outputViewModeTarget.value,
        configuredIdForDisplayedTest()
      )
    }
  }

  const displayOutputView = (testName, group) => {
    if (displayedTestName.value !== testName ||
        displayedDiffHash.value !== group.diff_hash) {
      // Wipe out the state for the associated files.
      displayedTestAssocFiles.value = null
      displayedTestAssocFileName.value = null
      // Update the displayed selection.
      displayedTestName.value = testName
      displayedDiffHash.value = group.diff_hash
      displayedGroupConfigs.value = [...group.configs]
      // Rerefresh the content area.
      requestContentAreaUpdate();
      syncSelectionToHash()
    }
  }

  const syncSelectionToHash = () => {
    if (!displayedTestName.value || !displayedDiffHash.value) {
      return
    }
    const hash = `#${formatTestAnchor(
      displayedDiffHash.value,
      displayedTestName.value
    )}`
    if (route.hash !== hash) {
      router.replace({ hash })
    }
  }

  const applyHashSelection = () => {
    const anchor = parseTestAnchor(route.hash)
    if (!anchor) {
      return
    }
    if (testStatuses.value.length === 0) {
      return
    }

    const statuses = testStatuses.value[0].statuses
    const testInfo = statuses[anchor.testName]
    if (!testInfo || !testInfo.groups || testInfo.groups.length === 0) {
      return
    }
    let group = testInfo.groups.find(
      (candidate) => candidate.diff_hash === anchor.diffHash
    )
    if (!group) {
      group = testInfo.groups[0]
    }
    displayOutputView(anchor.testName, group)
  }

  const displayAssociatedFile = (assocFileName) => {
    if (displayedTestAssocFileName.value !== assocFileName) {
      displayedTestAssocFileName.value = assocFileName
      requestContentAreaUpdate();
    }
  }

  const setOutputMode = (targetValue) => {
    if (outputViewModeTarget.value !== targetValue) {
      outputViewModeTarget.value = targetValue
      requestContentAreaUpdate();
    }
  }

  // Recordings are updated by the section's editor; a section without one
  // (e.g. served by acknowledg-forge) is read-only.
  const canRerecord = computed(() => {
    return (connectionState.value.editors ?? []).some((editor) => {
      return editor.kind === 'test' && editor.slug === reviewSectionSlug.value
    })
  })

  const rerecord = () => {
    beginPendingTask('test.recording-update')
    sendSectionMessage(
      'test.edit.update-diff',
      {
        id: displayedTestName.value,
        configs: displayedGroupConfigs.value
      }
    )
  }

  const activeTestStatuses = computed(() => {
    if (testStatuses.value.length === 0) {
      return []
    }
    return testStatuses.value[testStatusSelection.value].statuses
  })

  const statusListOptions = computed(() => {
    return testStatuses.value.map((statusList, index) => ({
      value: index,
      label: statusList.name
    }))
  })

  const groupsForTest = (testName) => {
    return activeTestStatuses.value[testName]?.groups ?? []
  }

  const previous = () => {
    const groups = groupsForTest(displayedTestName.value)
    const currGroupIdx = groups.findIndex(
      (group) => group.diff_hash === displayedDiffHash.value
    )
    if (currGroupIdx <= 0) {
      // Reverse to the prior test's last group.
      const testStatusKeys = Object.keys(activeTestStatuses.value)
      const currIdx = testStatusKeys.findIndex((key) => {
        return key === displayedTestName.value
      })
      const previousKeyIndex = currIdx === 0
        ? testStatusKeys.length - 1
        : currIdx - 1
      const targetTestName = testStatusKeys[previousKeyIndex]
      const targetGroups = groupsForTest(targetTestName)
      displayOutputView(targetTestName, targetGroups[targetGroups.length - 1])
    } else {
      displayOutputView(displayedTestName.value, groups[currGroupIdx - 1])
    }
  }

  const next = () => {
    const groups = groupsForTest(displayedTestName.value)
    const currGroupIdx = groups.findIndex(
      (group) => group.diff_hash === displayedDiffHash.value
    )
    if (currGroupIdx === groups.length - 1) {
      // Advance to the next test's first group.
      const testStatusKeys = Object.keys(activeTestStatuses.value)
      const currIdx = testStatusKeys.findIndex((key) => {
        return key === displayedTestName.value
      })
      const nextKeyIndex = currIdx === testStatusKeys.length - 1
        ? 0
        : currIdx + 1
      const targetTestName = testStatusKeys[nextKeyIndex]
      const targetGroups = groupsForTest(targetTestName)
      displayOutputView(targetTestName, targetGroups[0])
    } else {
      displayOutputView(displayedTestName.value, groups[currGroupIdx + 1])
    }
  }

  const requestStatuses = () => {
    if (!reviewSectionSlug.value) {
      return
    }
    sendSectionMessage('test.src.list-statuses')
  }

  if (socket.readyState === WebSocket.OPEN) {
    requestStatuses()
  } else {
    socket.addEventListener("open", requestStatuses)
  }

  const onSocketMessage = (event) => {
    let message = JSON.parse(event.data)
    if (message.s != null && message.s !== reviewSectionSlug.value) {
      return
    }
    if (message.t === "test.src.list-statuses") {
      isMultiConfig.value = message.v.configs.length > 1
      testStatuses.value = message.v.statuses
      applyHashSelection()
    } else if (message.t === "test.src.show-diff" || message.t === "test.src.show-diff-curr" ||
               message.t === "test.src.show-output") {
      if (message.v.id === configuredIdForDisplayedTest()) {
        displayTruncated.value = false
        currDiffDivergenceNotice.value = null
        if (message.v.content === null) {
          displayedOutput.value = "Missing .rt.tar file!"
        } else {
          if (message.v.truncated) {
            displayTruncated.value = true
          }
          displayedOutput.value = message.v.content
        }
        if (message.t === "test.src.show-diff-curr" &&
            (message.v.divergent_configs ?? []).length > 0) {
          const divergent = message.v.divergent_configs.join(', ')
          currDiffDivergenceNotice.value =
            `Curr Diff may not represent all configs in this group. ` +
            `Divergent: ${divergent}`
        }
      }
    } else if (message.t === "test.src.list-assoc-files") {
      if (message.v.id === displayedTestName.value) {
        displayedTestAssocFiles.value = message.v.files
      }
    } else if (message.t === "test.src.show-src") {
      if (message.v.id === displayedTestName.value) {
        displayTruncated.value = false
        currDiffDivergenceNotice.value = null
        if (message.v.content === null) {
          displayedOutput.value = "File not found!"
        } else {
          if (message.v.truncated) {
            displayTruncated.value = true
          }
          displayedOutput.value = message.v.content
        }
      }
    } else if (message.t === "test.edit.update-diff") {
      // Pending-task accounting and notices are owned by ReviewShell.
      // Re-request the current view if it's the curr diff view
      // and we're looking at this particular test case.
      if (outputViewModeTarget.value === "test.src.show-diff-curr" &&
          message.v.id === displayedTestName.value) {
        requestContentAreaUpdate();
      }
    } else if (message.t === "invalid") {
      displayUserNotice(
        "Client <-> Server protocol error. See the console.",
        "error"
      )
      console.error("Invalid request:", message)
    } else if (message.t === "test.missing-src") {
      displayUserNotice(
        "No corresponding edgy-review test source instance could be" +
        " found. Is it still running?",
        "error"
      )
    } else if (message.t === "test.missing-edit") {
      displayUserNotice(
        "No corresponding edgy-review editor instance could be" +
        " found. Is it still running?",
        "error"
      )
    } else if (!message.t.startsWith('test.')) {
      // Do nothing.
    } else {
      displayUserNotice(
        `Unknown response from server: ${message.t}. See the console.`,
        "error"
      )
      console.error("Unknown response:", message)
    }
  }

  socket.addEventListener("message", onSocketMessage)

  watch(() => route.hash, applyHashSelection)
  watch(reviewSectionSlug, () => {
    testStatuses.value = []
    requestStatuses()
  })

  // Socket lifecycle is owned by ReviewShell.

  const keyListener = (event) => {
    if (!displayedTestName.value) {
      return
    }

    if (event.key === "n" || event.key === "j") {
      next()
    } else if (event.key === "p" || event.key === "k") {
      previous()
    } else if (event.key === "z") {
      setOutputMode("test.src.show-diff")
    } else if (event.key === "x") {
      setOutputMode("test.src.show-diff-curr")
    } else if (event.key === "c") {
      setOutputMode("test.src.show-output")
    } else if (event.key === "v") {
      setOutputMode("test.src.show-src")
    } else if (event.key === "Enter" && canRerecord.value) {
      event.preventDefault()
      rerecord()
    }
  }

  document.addEventListener("keydown", keyListener)

  onUnmounted(() => {
    document.removeEventListener("keydown", keyListener)
    socket.removeEventListener("open", requestStatuses)
    socket.removeEventListener("message", onSocketMessage)
  })
</script>

<template>
  <div v-if="socketOpen && sectionAvailable">
    <div class="cell cell-colors sidebar">
      <div class="heading cell-colors" style="text-align: center;">
        <app-select
            v-if="testStatuses.length !== 0"
            v-model="testStatusSelection"
            :options="statusListOptions"
            :disabled="testStatuses.length === 1"
            aria-label="Test status list" />
      </div>
      <div class="test-status-line">
        <test-status v-for="(status, test) in activeTestStatuses"
           :key="test"
           :test="test"
           :status-info="status"
           @status-clicked="displayOutputView" />
      </div>
    </div>
    <scrollable-content-area
        v-if="displayedTestName"
        class="cell cell-colors content-width content">
      <div class="heading cell-content cell-colors" style="text-align: left;">
        <div>
          <div class="heading-name" :title="displayedTestName">
            <div class="test-config-badge"
                  :style="{
                    background: computeGradientStyleForConfigs(
                      displayedGroupConfigs
                    )
                  }">
              {{ displayedGroupConfigs.join(', ') }}
            </div>
            {{ displayedTestName }}
          </div>
          <div class="view-mode-selector">
            <app-button
                :pressed="outputViewModeTarget === 'test.src.show-diff'"
                keyshortcuts="Z"
                aria-label="Static Diff"
                @click="setOutputMode('test.src.show-diff')">
              Static Diff (z)
            </app-button>
            <app-button
                :pressed="outputViewModeTarget === 'test.src.show-diff-curr'"
                keyshortcuts="X"
                aria-label="Current Diff"
                @click="setOutputMode('test.src.show-diff-curr')">
              Curr Diff (x)
            </app-button>
            <app-button
                :pressed="outputViewModeTarget === 'test.src.show-output'"
                keyshortcuts="C"
                aria-label="Raw Output"
                @click="setOutputMode('test.src.show-output')">
              Raw Output (c)
            </app-button>
            <app-button
                :pressed="outputViewModeTarget === 'test.src.show-src'"
                keyshortcuts="V"
                aria-label="Source"
                @click="setOutputMode('test.src.show-src')">
              Source (v)
            </app-button>
          </div>
        </div>
        <div v-if="outputViewModeTarget === 'test.src.show-src'" class="assoc-file-list">
          <div v-if="displayedTestAssocFiles === null">
            <loading-spinner />
          </div>
          <div v-if="displayedTestAssocFiles !== null">
            <app-button
                :pressed="displayedTestAssocFileName === null"
                aria-label="Primary Source"
                @click="displayAssociatedFile(null)">
              Primary Source
            </app-button>
            <app-button
                v-for="assocFileName in displayedTestAssocFiles"
                :key="assocFileName"
                :pressed="displayedTestAssocFileName === assocFileName"
                :aria-label="`Associated file ${assocFileName}`"
                @click="displayAssociatedFile(assocFileName)">
              {{ assocFileName }}
            </app-button>
          </div>
        </div>
        <div v-if="displayedOutput !== null && displayTruncated"
             class="content-notice">
          Content Too Long: Display Truncated
        </div>
        <div v-if="displayedOutput !== null && currDiffDivergenceNotice"
             class="content-notice">
          {{ currDiffDivergenceNotice }}
        </div>
      </div>
      <div v-if="displayedOutput === null" class="loader-container">
        <loading-spinner />
      </div>
      <div v-if="displayedOutput !== null" class="cell-content"
           style="overflow: visible; display: grid;">
        <template v-if="outputViewModeTarget === 'test.src.show-diff' ||
                        outputViewModeTarget === 'test.src.show-diff-curr'">
          <diff-line v-for="lineText in displayedOutput.split('\n')"
                     :line-text="lineText" />
        </template>
        <template v-if="outputViewModeTarget === 'test.src.show-output' ||
                        outputViewModeTarget === 'test.src.show-src'">
          <pre>{{ displayedOutput }}</pre>
        </template>
      </div>
    </scrollable-content-area>
    <div v-if="!displayedTestName" class="cell cell-colors content-width content">
      <div style="text-align: center; margin: 5em">
        Select a test status difference to continue...
      </div>
    </div>
    <div v-if="displayedTestName" class="cell cell-colors content-width content-footer">
      <div class="cell-content">
        <app-button
            keyshortcuts="P K"
            aria-label="Previous test"
            @click="previous()">
          Previous (p/k)
        </app-button>
        <app-button
            v-if="canRerecord"
            keyshortcuts="Enter"
            aria-label="Update recording"
            @click="rerecord()">
          Update Recording (enter)
        </app-button>
        <app-button
            keyshortcuts="N J"
            aria-label="Next test"
            @click="next()">
          Next (n/j)
        </app-button>
      </div>
    </div>
  </div>
  <div v-if="socketOpen && !sectionAvailable"
       class="cell cell-colors connection-loss">
    No test source is connected for this review.
    Start <code>edgy-review</code> with the same review UUID, or open the
    Benchmarks tab if a benchmark source is available.
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
    height: calc(100vh - 70px);
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
    a {
      color: skyblue;
    }
  }
  .test-config-badge {
    display: inline-block;
    color: white;
    padding: 3px;
    border-radius: 3px;
  }
  .heading {
    position: sticky;
    top: 0px;
    left: 0px;
    padding-top: 10px;
    padding-bottom: 10px;
  }
  /* Configure the heading selector.  This uses a responsive design to pin the
     view mode selector to the right side of the screen when there's sufficient
     space.  Otherwise, the selector stacks with the test name and any other
     buttons in the heading with logic to constrain the length of the test name
     in the heading. */
  .view-mode-selector {
    margin: 10px;
  }
  @media (width >= 1300px) {
    .heading-name {
      max-width: calc(100% - 400px);
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    .view-mode-selector {
      position: absolute;
      top: 0px;
      right: 0px;
    }
    .view-mode-selector :deep(.app-btn) {
      margin-left: 5px;
    }
  }
  @media (width < 1300px) {
    .view-mode-selector {
      display: flex;
      justify-content: center;
    }
    .view-mode-selector :deep(.app-btn) {
      margin-left: 2.5px;
      margin-right: 2.5px;
    }
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
  .assoc-file-list {
    display: flex;
    justify-content: center;
    margin-top: 5px;
  }
  .assoc-file-list :deep(.app-btn) {
    margin-left: 2.5px;
    margin-right: 2.5px;
    margin-top: 5px;
  }
  .loader-container {
    padding: 40px;
    font-size: 50px;
    display: flex;
    justify-content: center;
  }
  .sidebar {
    position: absolute;
    left: 0px;
    top: 0px;
    width: 28em;
    max-height: calc(100% - 20px);
  }
  .content-notice {
    color: white;
    background-color: orange;
    border-radius: 10px;
    text-align: center;
    padding: 5px;
    margin-top: 10px;
    margin-left: 7px;
    margin-right: 7px;
  }
  .content {
    position: absolute;
    right: 0px;
    top: 0px;
    height: calc(100% - 65px);
  }
</style>
