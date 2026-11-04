<!--
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<script setup>
  // Text with a gutter of line numbers and/or per-line markers.
  //
  // Rendered as two <pre> elements (numbers, text) rather than one element
  // per line: the DOM stays at a handful of nodes however long the text is.
  // That keeps 100k+ line files interactive, and keeps in-page find working
  // (unlike windowed rendering).  The price is a fixed line height and no
  // soft wrapping.  Markers are sparse, so they are absolutely positioned
  // buttons in the gutter.
  import { computed } from 'vue'

  import MarkerButton from './MarkerButton.vue'

  const props = defineProps({
    text: { type: String, required: true },
    // The number shown for the first line of `text`.
    firstLine: { type: Number, default: 1 },
    showNumbers: { type: Boolean, default: true },
    // [{ line, label, title, disabled }], `line` being a displayed line
    // number (so `firstLine` applies).  Rendered as eye buttons.
    markers: { type: Array, default: () => [] },
    // A displayed line number to emphasize.
    highlightLine: { type: Number, default: null }
  })
  const emit = defineEmits(['marker-click'])

  const LINE_HEIGHT_PX = 18

  const lineCount = computed(() => {
    let count = 1
    let at = props.text.indexOf('\n')
    while (at !== -1) {
      count++
      at = props.text.indexOf('\n', at + 1)
    }
    // A final newline ends the last line rather than starting another.
    return props.text.endsWith('\n') && count > 1 ? count - 1 : count
  })

  const numbersText = computed(() => {
    if (!props.showNumbers) {
      return ''
    }
    const numbers = new Array(lineCount.value)
    for (let i = 0; i < numbers.length; i++) {
      numbers[i] = props.firstLine + i
    }
    return numbers.join('\n')
  })

  const numberDigits = computed(
    () => String(props.firstLine + lineCount.value - 1).length
  )

  const topOf = (line) => `${(line - props.firstLine) * LINE_HEIGHT_PX}px`

  const hasMarkers = computed(() => props.markers.length > 0)
</script>

<template>
  <div class="numbered-source"
       :style="{ '--line-height': `${LINE_HEIGHT_PX}px` }">
    <div v-if="showNumbers || hasMarkers" class="gutter" aria-hidden="false">
      <div v-if="hasMarkers" class="marker-column">
        <marker-button v-for="(marker, index) in markers"
                       :key="index"
                       class="marker-slot"
                       :marker="marker"
                       :style="{ top: topOf(marker.line) }"
                       @activate="(m, el) => emit('marker-click', m, el)" />
      </div>
      <pre v-if="showNumbers" class="numbers" aria-hidden="true"
           :style="{ minWidth: `${numberDigits}ch` }">{{ numbersText }}</pre>
    </div>
    <div class="code">
      <div v-if="highlightLine !== null" class="highlight"
           :style="{ top: topOf(highlightLine) }" />
      <pre>{{ text }}</pre>
    </div>
  </div>
</template>

<style scoped>
  .numbered-source {
    display: flex;
    align-items: flex-start;
    min-width: 0;
  }
  pre {
    margin: 0;
    font-family: monospace;
    font-size: 13px;
    line-height: var(--line-height);
    white-space: pre;
  }
  /* The gutter stays in view when the text is scrolled horizontally; it
     needs an opaque background for the text to pass beneath it.  It is
     stretched to the text's height since a markers-only gutter (raw
     output) has no in-flow content to give it one. */
  .gutter {
    align-self: stretch;
    position: sticky;
    left: 0;
    z-index: 1;
    background-color: white;
    display: flex;
    flex: none;
    /* See --content-inset in TestReviewView.vue. */
    padding-left: var(--content-inset, 0);
    padding-right: 10px;
    user-select: none;
    -webkit-user-select: none;
  }
  .numbers {
    text-align: right;
    color: #888;
    padding: 0 4px 0 6px;
  }
  .marker-column {
    position: relative;
    width: 22px;
    flex: none;
  }
  .marker-slot {
    position: absolute;
    left: 2px;
    height: var(--line-height);
  }
  .code:first-child {
    /* See --content-inset in TestReviewView.vue. */
    padding-left: var(--content-inset, 0);
  }
  .code {
    position: relative;
    flex: 1 0 auto;
  }
  .highlight {
    position: absolute;
    left: 0;
    right: 0;
    height: var(--line-height);
    background-color: rgba(255, 190, 0, 0.25);
    pointer-events: none;
  }
  @media (prefers-color-scheme: dark) {
    .gutter {
      background-color: #181A1B;
    }
  }
</style>
