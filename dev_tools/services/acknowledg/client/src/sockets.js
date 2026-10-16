/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

const PROTOCOL_REVISION = 19

const getSocketHost = () => {
  if (process.env.NODE_ENV === "development") {
    return "ws://localhost:2727"
  }
  return import.meta.env.VITE_EDG_ACKNOWLEDG_API_HOST
}

export const acquireSocketForReviews = () => {
  return new WebSocket(
    `${getSocketHost()}/review`
  )
}

export const acquireSocketForReview = (reviewId) => {
  return new WebSocket(
    `${getSocketHost()}/review/${reviewId}`
  )
}

export const protocolMessage = (tag, value = undefined, section = undefined) => {
  const message = { t: tag, r: PROTOCOL_REVISION }
  if (value !== undefined) {
    message.v = value
  }
  if (section !== undefined) {
    message.s = section
  }
  return JSON.stringify(message)
}
