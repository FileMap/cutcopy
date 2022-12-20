#!/usr/bin/env zx

await $`yarn prebuildify --strip --napi --arch x64`;

if (process.platform === 'darwin') {
    await $`yarn prebuildify --strip --napi --arch arm64`;
}
