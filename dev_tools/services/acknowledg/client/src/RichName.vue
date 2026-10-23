<!--
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<script setup>
  import { computed } from 'vue'
  import { RouterLink } from 'vue-router'

  const props = defineProps({
    // Rich name segments (see PROTOCOL.md, "Rich names").
    name: {
      type: Array,
      required: true
    },
    // When set, the text segments link here; link segments always open their
    // own URL.
    to: {
      type: [String, Object],
      default: null
    }
  })

  // Only web URLs are linked; anything else is shown as text.
  const webUrl = (url) => {
    try {
      const parsed = new URL(url)
      if (parsed.protocol === 'http:' || parsed.protocol === 'https:') {
        return parsed.href
      }
    } catch {
      // Not a URL.
    }
    return null
  }

  const segments = computed(() => {
    return props.name.map((segment) => {
      if (segment.type === 'link') {
        const text = segment.text || segment.url
        const url = webUrl(segment.url)
        return url ? { link: true, url, text } : { link: false, text }
      }
      return { link: false, text: segment.text }
    })
  })
</script>

<template>
  <span class="rich-name">
    <template v-for="(segment, index) in segments" :key="index">
      <a v-if="segment.link"
         class="rich-link"
         :href="segment.url"
         target="_blank"
         rel="noopener noreferrer"
         :aria-label="`${segment.text} (opens in new tab)`">
        <span>{{ segment.text }}</span>
        <svg class="rich-link-icon"
             viewBox="0 0 16 16"
             aria-hidden="true"
             focusable="false">
          <path d="M9.5 2.5h4v4M13.5 2.5 7.5 8.5M11.5 9.5v3a1 1 0 0 1-1 1h-7
                   a1 1 0 0 1-1-1v-7a1 1 0 0 1 1-1h3"/>
        </svg>
      </a>
      <RouterLink v-else-if="to"
                  class="rich-text-link"
                  :to="to">{{ segment.text }}</RouterLink>
      <span v-else>{{ segment.text }}</span>
    </template>
  </span>
</template>

<style scoped>
  .rich-link {
    display: inline-flex;
    align-items: center;
    gap: 3px;
    margin-left: 1px;
    margin-right: 1px;
    padding-left: 5px;
    padding-right: 4px;
    border: 1px solid rgba(127, 127, 127, 0.45);
    border-radius: 5px;
    line-height: 1.35;
    white-space: nowrap;
    text-decoration: none;
  }
  /* Reads as the name itself; it shows it's a link only when pointed at. */
  .rich-text-link {
    color: inherit;
    text-decoration: none;
  }
  .rich-link:hover,
  .rich-text-link:hover,
  .rich-text-link:focus-visible {
    text-decoration: underline;
  }
  .rich-link:focus-visible,
  .rich-text-link:focus-visible {
    outline: 2px solid skyblue;
    outline-offset: 2px;
  }
  .rich-link-icon {
    flex: none;
    width: 0.8em;
    height: 0.8em;
    fill: none;
    stroke: currentColor;
    stroke-width: 1.6;
    stroke-linecap: round;
    stroke-linejoin: round;
  }
  /* The same color visited or not. */
  @media (prefers-color-scheme: light) {
    .rich-link {
      color: #0b57d0;
      background-color: rgba(127, 127, 127, 0.12);
    }
  }
  @media (prefers-color-scheme: dark) {
    .rich-link {
      color: skyblue;
      background-color: rgba(255, 255, 255, 0.08);
    }
  }
</style>
