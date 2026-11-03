<!--
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<script setup>
  import {
    computed,
    inject,
    nextTick,
    onMounted,
    useTemplateRef,
    watch
  } from 'vue'

  import AppButton from './AppButton.vue'
  import {
    formChangeComment,
    isFaster
  } from './bench-metrics.js'

  const emit = defineEmits(['status-clicked'])
  const props = defineProps({
    benchmark: {
      type: Object,
      required: true
    }
  })

  const activeBenchmarkId = inject('activeBenchmarkId')
  const benchSortMode = inject('benchSortMode')

  const containerDivRef = useTemplateRef('container-div')
  const isActive = computed(
    () => activeBenchmarkId.value === props.benchmark.id
  )

  const scrollIntoView = () => {
    containerDivRef.value?.scrollIntoView({
      behavior: 'smooth',
      block: 'center'
    })
  }

  const scrollIfActive = async () => {
    if (!isActive.value) {
      return
    }
    await nextTick()
    scrollIntoView()
  }

  onMounted(scrollIfActive)
  watch(isActive, (active) => {
    if (active) {
      scrollIfActive()
    }
  })
  watch(benchSortMode, scrollIfActive)

  const emitStatusClick = () => {
    emit('status-clicked', props.benchmark.id)
  }
</script>

<template>
  <div
      class="bench-status-container"
      ref="container-div"
      :class="{ 'active-bench': isActive }">
    <app-button
        bare
        class="bench-status-inner-container"
        @click="emitStatusClick">
      <div class="bench-id" :title="benchmark.id">{{ benchmark.id }}</div>
      <div class="change-badge"
           :class="isFaster(benchmark.baseline_ops, benchmark.run_ops)
                    ? 'faster' : 'slower'">
        {{ formChangeComment(benchmark.baseline_ops, benchmark.run_ops) }}
      </div>
    </app-button>
  </div>
</template>

<style scoped>
  .bench-status-container {
    padding-left: 5px;
    padding-right: 5px;
    padding-top: 4px;
    padding-bottom: 4px;
  }
  .bench-status-inner-container {
    display: block;
    width: 100%;
    text-align: left;
    border: none;
    cursor: pointer;
    padding: 3px;
    border-radius: 3px;
  }
  .bench-id {
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
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
  @media (prefers-color-scheme: light) {
    .bench-status-inner-container {
      background-color: #cbcbcb;
      color: black;
    }
    .active-bench .bench-status-inner-container {
      background-color: #555555;
      color: white;
    }
  }
  @media (prefers-color-scheme: dark) {
    .bench-status-inner-container {
      background-color: #252525;
      color: white;
    }
    .active-bench .bench-status-inner-container {
      background-color: #484848;
      color: white;
    }
  }
</style>
