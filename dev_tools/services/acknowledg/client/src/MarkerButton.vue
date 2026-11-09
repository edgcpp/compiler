<!--
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<script setup>
  // The eye button marking a diagnostic whose source can be shown.
  //
  // `marker` is { title, label?, disabled } (see NumberedSource.vue).  A
  // disabled marker (source not available) is dimmed but still focusable, so
  // its title can explain why.  `activate` carries the marker and the button
  // element, for anchoring a popover to.  The parent positions it (class and
  // style fall through to the button).
  import AppButton from './AppButton.vue'

  defineProps({
    marker: { type: Object, required: true }
  })
  const emit = defineEmits(['activate'])
</script>

<template>
  <app-button bare
              class="marker"
              :class="{ unavailable: marker.disabled }"
              :title="marker.title"
              :aria-label="marker.label ?? marker.title"
              @click="emit('activate', marker, $event.currentTarget)">
    <svg viewBox="0 0 16 16" width="14" height="14" aria-hidden="true">
      <path d="M1 8c2-3.5 4.3-5 7-5s5 1.5 7 5c-2 3.5-4.3 5-7 5S3 11.5 1 8z"
            fill="none" stroke="currentColor" stroke-width="1.4" />
      <circle cx="8" cy="8" r="2.2" fill="currentColor" />
    </svg>
  </app-button>
</template>

<style scoped>
  .marker {
    width: 18px;
    padding: 0;
    border: none;
    background: none;
    color: #d68a00;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
  }
  .marker:hover,
  .marker:focus-visible {
    color: #ff9f00;
    outline: none;
    background-color: rgba(127, 127, 127, 0.25);
    border-radius: 4px;
  }
  .marker.unavailable {
    color: #888;
    opacity: 0.45;
    cursor: help;
  }
</style>
