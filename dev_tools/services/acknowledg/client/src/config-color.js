/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

const hslCache = {}
const mutedHslCache = {}
const gradientCache = {}
const mutedGradientCache = {}

const hueForConfig = (configName) => {
  let strSum = 0
  for (let i = 0; i < configName.length; ++i) {
    strSum += (strSum << 5) + configName.charCodeAt(i)
  }
  return strSum
}

export const computeHSLStyleForConfig = (configName) => {
  if (!hslCache[configName]) {
    hslCache[configName] = `hsl(${hueForConfig(configName)} 90% 30%)`
  }
  return hslCache[configName]
}

export const computeMutedHSLStyleForConfig = (configName) => {
  if (!mutedHslCache[configName]) {
    mutedHslCache[configName] =
      `hsl(${hueForConfig(configName)} 90% 30% / .6)`
  }
  return mutedHslCache[configName]
}

export const computeGradientStyleForConfigs = (
  configs,
  { muted = false } = {}
) => {
  if (!configs || configs.length === 0) {
    return muted
      ? 'hsl(0 0% 40% / .6)'
      : 'hsl(0 0% 40%)'
  }
  if (configs.length === 1) {
    return muted
      ? computeMutedHSLStyleForConfig(configs[0])
      : computeHSLStyleForConfig(configs[0])
  }

  const cache = muted ? mutedGradientCache : gradientCache
  const key = configs.join('\0')
  if (!cache[key]) {
    const stops = configs.map((configName, index) => {
      const color = muted
        ? computeMutedHSLStyleForConfig(configName)
        : computeHSLStyleForConfig(configName)
      const pct = (index / (configs.length - 1)) * 100
      return `${color} ${pct}%`
    })
    cache[key] = `linear-gradient(90deg, ${stops.join(', ')})`
  }
  return cache[key]
}
