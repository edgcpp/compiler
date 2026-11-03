<!--
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<script setup>
  import { defineProps } from 'vue'

  import MarkerButton from './MarkerButton.vue'

  // `marker`, when set, is a diagnostic marker (see NumberedSource.vue) shown
  // as an eye button in the gutter before the line.
  const props = defineProps(["lineText", "marker"])
  const emit = defineEmits(["marker-click"])

  const colorClass = (line) => {
    if (line.startsWith("#")) {
      return "comment"
    } else if (line.startsWith("+")) {
      return "addition"
    } else if (line.startsWith("-")) {
      return "deletion"
    }
    return '';
  }
</script>

<template>
  <div class="diff-row">
    <span class="gutter">
      <marker-button v-if="props.marker"
                     class="marker-slot"
                     :marker="props.marker"
                     @activate="(m, el) => emit('marker-click', m, el)" />
    </span>
    <pre :class="colorClass(props.lineText)">{{ props.lineText }}</pre>
  </div>
</template>

<style scoped>
  .addition {
    background-color: #618555;
    color: white;
  }
  .deletion {
    background-color: #d1173e;
    color: white;
  }
  .comment {
    background-color: cyan;
  }
  pre {
    margin: 0px;
    flex: 1;
  }
  .diff-row {
    display: flex;
  }
  .gutter {
    flex: none;
    width: 22px;
    display: flex;
    user-select: none;
    -webkit-user-select: none;
  }
  .marker-slot {
    margin-left: 2px;
  }
</style>
