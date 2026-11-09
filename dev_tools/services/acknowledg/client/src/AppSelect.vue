<!--
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<script setup>
  import {
    computed,
    nextTick,
    onMounted,
    onUnmounted,
    ref,
    useId,
    watch
  } from 'vue'

  defineOptions({ inheritAttrs: false })

  const props = defineProps({
    modelValue: {
      type: [String, Number, Boolean, Object],
      default: null
    },
    // Array of { value, label } entries.
    options: {
      type: Array,
      required: true
    },
    disabled: {
      type: Boolean,
      default: false
    },
    ariaLabel: {
      type: String,
      default: undefined
    }
  })

  const emit = defineEmits(['update:modelValue'])

  const open = ref(false)
  const activeIndex = ref(-1)
  const rootRef = ref(null)
  const triggerRef = ref(null)
  const listRef = ref(null)
  const listStyle = ref({})
  const optionIdPrefix = useId()
  const listboxId = `${optionIdPrefix}-listbox`

  const selectedOption = computed(() => {
    return props.options.find((option) => option.value === props.modelValue)
  })

  const selectedLabel = computed(() => {
    return selectedOption.value?.label ?? ''
  })

  const selectedIndex = computed(() => {
    return props.options.findIndex(
      (option) => option.value === props.modelValue
    )
  })

  const activeOptionId = computed(() => {
    if (activeIndex.value < 0) {
      return undefined
    }
    return `${optionIdPrefix}-option-${activeIndex.value}`
  })

  const updateListPosition = async () => {
    const trigger = triggerRef.value
    if (!trigger) {
      return
    }

    const rect = trigger.getBoundingClientRect()
    const viewportPadding = 8
    const gap = 4
    const maxMenuHeight = 16 * 16 // 16em at 16px
    const spaceBelow = window.innerHeight - rect.bottom - viewportPadding
    const spaceAbove = rect.top - viewportPadding
    const openUpward = spaceBelow < Math.min(maxMenuHeight, 160) &&
                       spaceAbove > spaceBelow

    const maxHeight = Math.max(
      80,
      Math.min(maxMenuHeight, openUpward ? spaceAbove - gap : spaceBelow - gap)
    )
    const maxWidth = window.innerWidth - (2 * viewportPadding)

    // Size to the option content; never narrower than the trigger.
    listStyle.value = {
      position: 'fixed',
      left: `${rect.left}px`,
      width: 'max-content',
      minWidth: `${rect.width}px`,
      maxWidth: `${maxWidth}px`,
      maxHeight: `${maxHeight}px`,
      ...(openUpward
        ? { bottom: `${window.innerHeight - rect.top + gap}px`, top: 'auto' }
        : { top: `${rect.bottom + gap}px`, bottom: 'auto' })
    }

    await nextTick()
    const list = listRef.value
    if (!list) {
      return
    }

    const listRect = list.getBoundingClientRect()
    let left = rect.left
    if (listRect.right > window.innerWidth - viewportPadding) {
      left = Math.max(
        viewportPadding,
        window.innerWidth - listRect.width - viewportPadding
      )
    }
    if (left !== rect.left) {
      listStyle.value = {
        ...listStyle.value,
        left: `${left}px`
      }
    }
  }

  const close = () => {
    open.value = false
    activeIndex.value = -1
  }

  const openList = async () => {
    if (props.disabled || props.options.length === 0) {
      return
    }
    open.value = true
    activeIndex.value = selectedIndex.value >= 0 ? selectedIndex.value : 0
    await nextTick()
    await updateListPosition()
    listRef.value?.focus()
    document.getElementById(activeOptionId.value)?.scrollIntoView({
      block: 'nearest'
    })
  }

  const toggle = () => {
    if (open.value) {
      close()
    } else {
      openList()
    }
  }

  const selectIndex = (index) => {
    const option = props.options[index]
    if (!option) {
      return
    }
    emit('update:modelValue', option.value)
    close()
  }

  const moveActive = (delta) => {
    if (props.options.length === 0) {
      return
    }
    const count = props.options.length
    if (activeIndex.value < 0) {
      activeIndex.value = selectedIndex.value >= 0 ? selectedIndex.value : 0
      return
    }
    activeIndex.value = (activeIndex.value + delta + count) % count
    document.getElementById(activeOptionId.value)?.scrollIntoView({
      block: 'nearest'
    })
  }

  const onTriggerKeydown = (event) => {
    if (props.disabled) {
      return
    }
    if (event.key === 'ArrowDown' || event.key === 'ArrowUp' ||
        event.key === 'Enter' || event.key === ' ') {
      event.preventDefault()
      // Enter opens the list; keep it from the review views' shortcut.
      event.stopPropagation()
      openList()
    }
  }

  const onListKeydown = (event) => {
    if (event.key === 'Escape') {
      event.preventDefault()
      close()
      triggerRef.value?.focus()
      return
    }
    if (event.key === 'ArrowDown') {
      event.preventDefault()
      moveActive(1)
      return
    }
    if (event.key === 'ArrowUp') {
      event.preventDefault()
      moveActive(-1)
      return
    }
    if (event.key === 'Home') {
      event.preventDefault()
      activeIndex.value = 0
      return
    }
    if (event.key === 'End') {
      event.preventDefault()
      activeIndex.value = props.options.length - 1
      return
    }
    if (event.key === 'Enter' || event.key === ' ') {
      event.preventDefault()
      event.stopPropagation()
      if (activeIndex.value >= 0) {
        selectIndex(activeIndex.value)
        triggerRef.value?.focus()
      }
    }
  }

  const onDocumentPointerDown = (event) => {
    if (!open.value) {
      return
    }
    const target = event.target
    if (rootRef.value?.contains(target) || listRef.value?.contains(target)) {
      return
    }
    close()
  }

  const onViewportChange = () => {
    if (open.value) {
      updateListPosition()
    }
  }

  watch(
    () => props.disabled,
    (disabled) => {
      if (disabled) {
        close()
      }
    }
  )

  onMounted(() => {
    document.addEventListener('pointerdown', onDocumentPointerDown)
    window.addEventListener('resize', onViewportChange)
    window.addEventListener('scroll', onViewportChange, true)
  })

  onUnmounted(() => {
    document.removeEventListener('pointerdown', onDocumentPointerDown)
    window.removeEventListener('resize', onViewportChange)
    window.removeEventListener('scroll', onViewportChange, true)
  })
</script>

<template>
  <div ref="rootRef" class="app-select" v-bind="$attrs">
    <button
        ref="triggerRef"
        type="button"
        class="app-btn app-select-trigger"
        :class="{ 'app-btn-active': open }"
        :disabled="disabled"
        :aria-label="ariaLabel"
        aria-haspopup="listbox"
        :aria-expanded="open ? 'true' : 'false'"
        :aria-controls="listboxId"
        @click="toggle"
        @keydown="onTriggerKeydown">
      <span class="app-select-label">{{ selectedLabel }}</span>
      <span class="app-select-chevron" aria-hidden="true">▾</span>
    </button>
    <Teleport to="body">
      <ul
          v-show="open"
          :id="listboxId"
          ref="listRef"
          class="app-select-list"
          role="listbox"
          :aria-label="ariaLabel"
          :aria-activedescendant="activeOptionId"
          :style="listStyle"
          tabindex="-1"
          @keydown="onListKeydown">
        <li
            v-for="(option, index) in options"
            :id="`${optionIdPrefix}-option-${index}`"
            :key="String(option.value)"
            class="app-select-option"
            :class="{
              'app-select-option-active': index === activeIndex,
              'app-select-option-selected': option.value === modelValue
            }"
            role="option"
            :aria-selected="option.value === modelValue ? 'true' : 'false'"
            @click="selectIndex(index)"
            @pointermove="activeIndex = index">
          {{ option.label }}
        </li>
      </ul>
    </Teleport>
  </div>
</template>

<style>
  .app-select {
    position: relative;
    display: inline-block;
    max-width: 100%;
  }
  .app-select-trigger {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    max-width: 100%;
  }
  .app-select-label {
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .app-select-chevron {
    flex-shrink: 0;
    opacity: 0.85;
  }
  .app-select-list {
    z-index: 10000;
    margin: 0;
    padding: 4px;
    overflow: auto;
    list-style: none;
    border-radius: 6px;
    background-color: #444444;
    color: white;
    font-size: 14px;
    line-height: normal;
    box-shadow: 0 4px 16px rgba(0, 0, 0, 0.35);
  }
  .app-select-option {
    padding: 4px 8px;
    border-radius: 4px;
    cursor: pointer;
    white-space: nowrap;
    font-size: 14px;
    line-height: normal;
  }
  .app-select-option-active,
  .app-select-option:hover {
    background-color: black;
  }
  .app-select-option-selected {
    font-weight: 700;
  }
</style>
