// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {trustedResourceUrl} from 'safevalues';
import {setScriptSrc} from 'safevalues/dom';

async function fetchBytes(url: string): Promise<Uint8Array> {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Failed to fetch ${url}: ${response.statusText} at ${url}`);
  }
  return new Uint8Array(await response.arrayBuffer());
}

function toByteVector(module: any, bytes: Uint8Array): any {
  const v = new module.ByteVector();
  // Using a loop for simplicity, can be optimized later
  for (let i = 0; i < bytes.length; i++) {
    v.push_back(bytes[i]);
  }
  return v;
}

// Loading codec_wasm_bin.wasm can take a long time, especially if sanitizers
// were enabled during the build. See b/514217988.
jasmine.DEFAULT_TIMEOUT_INTERVAL = 60000;

describe('Codec WASM', () => {
  let module: any;
  let pngBytes: Uint8Array;

  beforeAll(async () => {
    const factory = (window as any).loadCodecWasm;
    console.log('factory (window.loadCodecWasm):', factory);

    if (typeof factory !== 'function') {
      throw new Error(`loadCodecWasm is not a function. window.loadCodecWasm: ${
          typeof factory}. It should have been loaded by Karma via deps.`);
    }

    module = await factory({
      locateFile: (path: string) => {
        if (path.endsWith('.wasm')) {
          const wasmPath =
              'codec_wasm_bin.wasm';
          console.log('locateFile:', wasmPath);
          return wasmPath;
        }
        return path;
      }
    });

    const pngPath =
        'gradient32x32.png';
    let fetchedBytes: Uint8Array|undefined;
    try {
      console.log('Trying to fetch PNG from:', pngPath);
      fetchedBytes = await fetchBytes(pngPath);
      console.log('Successfully fetched from:', pngPath);
    } catch (e) {
      console.log('Failed to fetch from:', pngPath);
    }
    if (!fetchedBytes) throw new Error('Could not find gradient32x32.png');
    pngBytes = fetchedBytes;
  });

  it('should encode and decode gradient32x32.png with WebP', async () => {
    // Decode PNG to ARGB
    const pngVector = toByteVector(module, pngBytes);
    const decoded = module.decodeToArgb(pngVector);
    expect(decoded.width).toBe(32);
    expect(decoded.height).toBe(32);
    expect(decoded.argb.size()).toBe(32 * 32 * 4);

    // Encode ARGB to WebP lossless
    const encodedVector = module.encode(
        decoded.argb, decoded.width, decoded.height, module.Codec.Webp,
        module.Subsampling.Default, /*effort=*/ 0, /*quality=*/ -1);
    expect(encodedVector.size()).toBeGreaterThan(0);

    // Decode WebP back to ARGB
    const decodedAgain = module.decodeToArgb(encodedVector);
    expect(decodedAgain).toEqual(decoded);

    // Clean up
    pngVector.delete();
    decoded.argb.delete();
    encodedVector.delete();
    decodedAgain.argb.delete();
  });

  it('should encode and decode with JPEG libraries', async () => {
    for (const codec
             of [module.Codec.Jpegturbo,
                 module.Codec.Jpegsimple,
                 module.Codec.Jpegmoz,
    ]) {
      const pngVector = toByteVector(module, pngBytes);
      const decoded = module.decodeToArgb(pngVector);

      const encodedVector = module.encode(
          decoded.argb, decoded.width, decoded.height, codec,
          module.Subsampling.Default, /*effort=*/ 0, /*quality=*/ 90);
      expect(encodedVector.size()).toBeGreaterThan(0);

      const decodedAgain = module.decodeToArgb(encodedVector);
      expect(decodedAgain.width).toBe(32);
      expect(decodedAgain.height).toBe(32);

      let maxDiff = 0;
      for (let i = 0; i < decoded.argb.size(); i++) {
        const expected = decoded.argb.get(i);
        const actual = decodedAgain.argb.get(i);
        const diff = Math.abs(expected - actual);
        if (diff > maxDiff) maxDiff = diff;
      }
      console.log(`${codec} Maximum pixel difference:`, maxDiff);
      // The codec is lossy, expect some difference. 25 is safe for quality 90.
      expect(maxDiff).toBeLessThanOrEqual(25);

      pngVector.delete();
      decoded.argb.delete();
      encodedVector.delete();
      decodedAgain.argb.delete();
    }
  });

  it('should throw an error on invalid bytes', () => {
    const invalidBytes = toByteVector(module, new Uint8Array([1, 2, 3]));
    expect(() => {
      module.decodeToArgb(invalidBytes);
    }).toThrowError(/Failed to decode image/);
    invalidBytes.delete();
  });
});
