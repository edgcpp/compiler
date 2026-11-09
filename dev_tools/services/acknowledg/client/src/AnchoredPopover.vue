<!--
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<script setup>
  // A rounded box whose edge carries a chevron ("/\" when the box is below
  // its anchor, "\/" when above) pointing at the element it came from.
  //
  // The box is teleported to <body> and positioned with `position: fixed`
  // from the anchor's bounding rect, so scroll containers cannot clip it.
  // The parent owns the open state: this component only asks to be closed
  // (Escape, or a pointer press outside both the box and the anchor).
  //
  // Focus moves into the box when it opens and returns to the anchor when it
  // closes (unless the user has since focused something else), and the box is
  // named by `label`.  Enter inside the box is kept from the review views'
  // document-level Enter shortcut.
  import {
    nextTick,
    onBeforeUnmount,
    ref,
    useTemplateRef,
    watch
  } from 'vue'

  const props = defineProps({
    // The element the chevron points at.
    anchor: { type: Object, default: null },
    // The dialog's accessible name.  Set here rather than as an attribute
    // on <anchored-popover>: the root is a <teleport>, so attributes fall
    // through to nothing.
    label: { type: String, required: true },
    open: { type: Boolean, default: false },
    maxWidth: { type: String, default: 'min(600px, 70vw)' }
  })
  const emit = defineEmits(['close'])

  const boxRef = useTemplateRef('box')
  const style = ref({ top: '0px', left: '0px', visibility: 'hidden' })
  const placement = ref('below')
  const tailLeft = ref('20px')

  const VIEWPORT_MARGIN = 8
  const ANCHOR_GAP = 12
  // Preferred distance of the chevron from the box's left edge, clear of the
  // rounded corner.
  const TAIL_INSET = 22
  // Nearest the chevron may sit to either edge of the box.  Smaller than
  // TAIL_INSET so an anchor near the viewport edge is still pointed at
  // exactly, rather than the chevron being pushed off-center.
  const TAIL_MIN_INSET = 10
  // The chevron is positioned from the padding edge, inside the border.
  const BORDER_WIDTH = 1

  const reposition = () => {
    const box = boxRef.value
    const anchor = props.anchor
    if (!box || !anchor) {
      return
    }
    const a = anchor.getBoundingClientRect()
    const width = box.offsetWidth
    const height = box.offsetHeight
    const vw = document.documentElement.clientWidth
    const vh = document.documentElement.clientHeight

    const spaceBelow = vh - a.bottom - ANCHOR_GAP - VIEWPORT_MARGIN
    const spaceAbove = a.top - ANCHOR_GAP - VIEWPORT_MARGIN
    placement.value = (height <= spaceBelow || spaceBelow >= spaceAbove) ?
      'below' : 'above'

    const anchorCenter = a.left + a.width / 2
    let left = anchorCenter - TAIL_INSET - 6
    left = Math.min(left, vw - width - VIEWPORT_MARGIN)
    left = Math.max(left, VIEWPORT_MARGIN)
    const top = placement.value === 'below' ?
      a.bottom + ANCHOR_GAP : a.top - ANCHOR_GAP - height

    tailLeft.value = `${Math.min(
      Math.max(anchorCenter - left, TAIL_MIN_INSET),
      width - TAIL_MIN_INSET
    ) - BORDER_WIDTH}px`
    style.value = {
      top: `${Math.max(VIEWPORT_MARGIN, top)}px`,
      left: `${left}px`,
      maxWidth: props.maxWidth,
      visibility: 'visible'
    }
  }

  const onKeyDown = (event) => {
    if (event.key === 'Escape') {
      emit('close')
    }
  }
  const onPointerDown = (event) => {
    const target = event.target
    if (boxRef.value && !boxRef.value.contains(target) &&
        !(props.anchor && props.anchor.contains(target))) {
      emit('close')
    }
  }

  let resizeObserver = null
  const attach = () => {
    window.addEventListener('keydown', onKeyDown)
    document.addEventListener('pointerdown', onPointerDown, true)
    // Capture phase, so scrolling any ancestor repositions the box.
    window.addEventListener('scroll', reposition, true)
    window.addEventListener('resize', reposition)
    if (boxRef.value && typeof ResizeObserver !== 'undefined') {
      resizeObserver = new ResizeObserver(reposition)
      resizeObserver.observe(boxRef.value)
    }
  }
  const detach = () => {
    window.removeEventListener('keydown', onKeyDown)
    document.removeEventListener('pointerdown', onPointerDown, true)
    window.removeEventListener('scroll', reposition, true)
    window.removeEventListener('resize', reposition)
    if (resizeObserver) {
      resizeObserver.disconnect()
      resizeObserver = null
    }
  }

  // Where focus was when the box opened, for restoring it on close.
  let focusBeforeOpen = null
  const restoreFocus = (box) => {
    const target = props.anchor || focusBeforeOpen
    // Only if focus is still in the box (or lost with it); a click that
    // closed the popover by focusing something else keeps that focus.
    const active = document.activeElement
    const focusLost = !active || active === document.body ||
      (box && box.contains(active))
    if (focusLost && target && target.isConnected) {
      target.focus({ preventScroll: true })
    }
  }

  watch(() => [props.open, props.anchor], async ([isOpen], old) => {
    const wasOpen = old !== undefined && old[0]
    const box = boxRef.value
    detach()
    if (isOpen) {
      if (!wasOpen) {
        focusBeforeOpen = document.activeElement
      }
      style.value = { ...style.value, visibility: 'hidden' }
      await nextTick()
      reposition()
      attach()
      if (boxRef.value) {
        boxRef.value.focus({ preventScroll: true })
      }
    } else if (wasOpen) {
      restoreFocus(box)
    }
  }, { flush: 'post' })

  onBeforeUnmount(detach)
</script>

<template>
  <teleport to="body">
    <div v-if="props.open" ref="box" class="popover"
         :class="placement" :style="style" role="dialog"
         :aria-label="label" tabindex="-1"
         :data-placement="placement"
         @keydown.enter.stop>
      <span class="tail" :style="{ left: tailLeft }" aria-hidden="true" />
      <div class="popover-body">
        <slot />
      </div>
    </div>
  </teleport>
</template>

<style scoped>
  .popover {
    position: fixed;
    z-index: 10001;
    width: max-content;
    box-sizing: border-box;
    border: 1px solid rgba(127, 127, 127, 0.5);
    border-radius: 15px;
    box-shadow: 0 2px 10px rgba(0, 0, 0, 0.3);
    font-size: 14px;
    background-color: white;
    color: black;
  }
  @media (prefers-color-scheme: dark) {
    .popover {
      background-color: #181A1B;
      color: white;
    }
  }
  .popover:focus {
    outline: none;
  }
  .popover-body {
    padding: 6px 12px;
    border-radius: 15px;
    overflow: auto;
    overflow-wrap: anywhere;
    max-height: calc(100vh - 40px);
  }
  .tail {
    position: absolute;
    width: 12px;
    height: 12px;
    margin-left: -6px;
    background: inherit;
    border: 1px solid rgba(127, 127, 127, 0.5);
    transform: rotate(45deg);
  }
  /* The tail is a rotated square clipped to its half outside the box, so
     nothing of it overlaps the contents; the half-square's flat edge sits on
     the box's border and hides it, leaving an open chevron. */
  /* Box below the anchor: chevron on top edge, "/\". */
  .below .tail {
    top: -6px;
    border-right: none;
    border-bottom: none;
    clip-path: polygon(0 0, 100% 0, 0 100%);
  }
  /* Box above the anchor: chevron on bottom edge, "\/". */
  .above .tail {
    bottom: -6px;
    border-left: none;
    border-top: none;
    clip-path: polygon(100% 0, 100% 100%, 0 100%);
  }
</style>
