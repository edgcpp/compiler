<!--
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<script setup>
  import { computed } from 'vue'
  import { RouterLink } from 'vue-router'

  // Enter on a focused button or link activates that control.  The review
  // views also bind Enter on the document (rerecord / update baseline), whose
  // preventDefault() would otherwise suppress the native click, so every
  // AppButton stops Enter from reaching it.  Use `bare` for controls with
  // their own look (e.g. icon buttons) that still need this behavior.
  defineOptions({ inheritAttrs: false })

  const props = defineProps({
    // When set, render as a Vue Router link with the shared button chrome.
    to: {
      type: [String, Object],
      default: null
    },
    disabled: {
      type: Boolean,
      default: false
    },
    // Omit the shared button chrome; the caller styles the control.
    bare: {
      type: Boolean,
      default: false
    },
    // Toggle / selected state for mode buttons (maps to aria-pressed).
    pressed: {
      type: Boolean,
      default: undefined
    },
    type: {
      type: String,
      default: 'button'
    },
    ariaLabel: {
      type: String,
      default: undefined
    },
    // Space-separated ARIA key shortcuts, e.g. "P K" or "Enter".
    keyshortcuts: {
      type: String,
      default: undefined
    }
  })

  const emit = defineEmits(['click'])

  const isLink = computed(() => props.to != null)

  const ariaPressed = computed(() => {
    if (props.pressed === undefined) {
      return undefined
    }
    return props.pressed ? 'true' : 'false'
  })

  const onClick = (event) => {
    if (props.disabled) {
      event.preventDefault()
      event.stopPropagation()
      return
    }
    emit('click', event)
  }
</script>

<template>
  <RouterLink
      v-if="isLink"
      :to="to"
      :class="{ 'app-btn': !bare, 'app-btn-disabled': disabled && !bare }"
      :aria-disabled="disabled ? 'true' : undefined"
      :aria-label="ariaLabel"
      :aria-keyshortcuts="keyshortcuts"
      :tabindex="disabled ? -1 : undefined"
      v-bind="$attrs"
      @click="onClick"
      @keydown.enter.stop>
    <slot />
  </RouterLink>
  <button
      v-else
      :type="type"
      :class="{ 'app-btn': !bare, 'app-btn-active': pressed && !bare }"
      :disabled="disabled"
      :aria-pressed="ariaPressed"
      :aria-label="ariaLabel"
      :aria-keyshortcuts="keyshortcuts"
      v-bind="$attrs"
      @click="onClick"
      @keydown.enter.stop>
    <slot />
  </button>
</template>

<style>
  /* Unscoped so RouterLink and native button roots share one chrome. */
  .app-btn {
    display: inline-block;
    box-sizing: border-box;
    margin: 0;
    padding: 4px 8px;
    border: none;
    border-radius: 6px;
    background-color: darkgrey;
    color: white;
    font: inherit;
    font-size: 14px;
    line-height: normal;
    text-align: center;
    text-decoration: none;
    cursor: pointer;
    transition: background-color .4s;
  }
  .app-btn:hover:not(:disabled):not(.app-btn-disabled) {
    background-color: black;
  }
  .app-btn:focus-visible {
    outline: 2px solid skyblue;
    outline-offset: 2px;
  }
  .app-btn.router-link-active,
  .app-btn.app-btn-active,
  .app-btn[aria-pressed="true"] {
    background-color: black;
  }
  .app-btn:disabled,
  .app-btn.app-btn-disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }
</style>
