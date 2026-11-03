<!--
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<script setup>
  // The source around a diagnostic's location, in a popover pointing at the
  // marker that was clicked.  The parent fetches the lines and passes the
  // state in `source`:
  //
  //   Still loading:
  //     null
  //   The file cannot be shown:
  //     { missing: true }
  //   The lines to show:
  //     { content, firstLine, linesTruncated, truncated }
  import AnchoredPopover from './AnchoredPopover.vue'
  import LoadingSpinner from './LoadingSpinner.vue'
  import NumberedSource from './NumberedSource.vue'

  defineProps({
    anchor: { type: Object, default: null },
    open: { type: Boolean, default: false },
    // The diagnostic location: { file, line, col }.
    location: { type: Object, default: null },
    source: { type: Object, default: null }
  })
  const emit = defineEmits(['close'])

  const labelFor = (location) => location === null ? 'Source' :
    `Source at ${location.file}, line ${location.line}`
</script>

<template>
  <anchored-popover :anchor="anchor" :open="open"
                    :label="labelFor(location)"
                    max-width="min(900px, 90vw)"
                    @close="emit('close')">
    <div v-if="location" class="location">
      {{ location.file }}:{{ location.line
      }}<template v-if="location.col !== null">:{{ location.col }}</template>
    </div>
    <div v-if="source === null" class="status loading">
      <loading-spinner />
    </div>
    <div v-else-if="source.missing" class="status">
      {{ source.reason ?? 'File not found!' }}
    </div>
    <template v-else>
      <div v-if="source.truncated" class="content-notice">
        Source Read Limit Reached: Display Truncated
      </div>
      <div v-if="source.linesTruncated?.length > 0" class="content-notice">
        Some Lines Truncated
      </div>
      <numbered-source :text="source.content"
                       :first-line="source.firstLine"
                       :highlight-line="location ? location.line : null" />
    </template>
  </anchored-popover>
</template>

<style scoped>
  .location {
    font-family: monospace;
    font-size: 12px;
    color: #888;
    padding-bottom: 4px;
  }
  .status {
    padding: 8px 0;
    text-align: center;
  }
  .loading {
    font-size: 28px;
  }
  .content-notice {
    color: white;
    background-color: orange;
    border-radius: 10px;
    text-align: center;
    padding: 5px;
    margin-bottom: 8px;
  }
</style>
