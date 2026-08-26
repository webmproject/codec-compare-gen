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

async function fetchBytes(url: string): Promise<Uint8Array> {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Failed to fetch ${url}: ${response.statusText} at ${url}`);
  }
  return new Uint8Array(await response.arrayBuffer());
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
    const result = module.getEncodedBytesAndDecodeStats(
        pngBytes, module.Codec.Webp, module.Subsampling.Default, /*effort=*/ 0,
        /*quality=*/ -1);
    expect(result.width).toBe(32);
    expect(result.height).toBe(32);
    expect(result.encoded_bytes.length).toBeGreaterThan(0);
    expect(result.encoded_size).toBe(result.encoded_bytes.length);
    expect(result.psnr).toBe(99);

    const resultAgain = module.getEncodedBytesAndDecodeStats(
        result.encoded_bytes, module.Codec.Webp, module.Subsampling.Default,
        /*effort=*/ 0, /*quality=*/ -1);
    expect(resultAgain.width).toBe(32);
    expect(resultAgain.height).toBe(32);
    expect(resultAgain.psnr).toBe(99);
  });

  it('should encode and decode gradient32x32.png with AVIF', async () => {
    const result = module.getEncodedBytesAndDecodeStats(
        pngBytes, module.Codec.Avif, module.Subsampling.Default, /*effort=*/ 0,
        /*quality=*/ 90);
    expect(result.width).toBe(32);
    expect(result.height).toBe(32);
    expect(result.encoded_bytes.length).toBeGreaterThan(0);
    expect(result.encoded_size).toBe(result.encoded_bytes.length);
    expect(result.psnr).toBeGreaterThan(20);
    expect(result.psnr).toBeLessThan(99);
  });

  it('should encode and decode with JPEG libraries', async () => {
    for (const codec
             of [module.Codec.Jpegturbo,
                 module.Codec.Jpegsimple,
                 module.Codec.Jpegmoz,
    ]) {
      const result = module.getEncodedBytesAndDecodeStats(
          pngBytes, codec, module.Subsampling.Default, /*effort=*/ 0,
          /*quality=*/ 90);
      expect(result.width).toBe(32);
      expect(result.height).toBe(32);
      expect(result.encoded_bytes.length).toBeGreaterThan(0);
      expect(result.encoded_size).toBe(result.encoded_bytes.length);
      console.log(`${codec} PSNR:`, result.psnr);
      expect(result.psnr).toBeGreaterThan(20);
      expect(result.psnr).toBeLessThan(99);

      const resultAgain = module.getEncodedBytesAndDecodeStats(
          result.encoded_bytes, codec, module.Subsampling.Default,
          /*effort=*/ 0, /*quality=*/ 90);
      expect(resultAgain.width).toBe(32);
      expect(resultAgain.height).toBe(32);
    }
  });

  it('should throw an error on invalid bytes', () => {
    const invalidBytes = new Uint8Array([1, 2, 3]);
    expect(() => {
      module.getEncodedBytesAndDecodeStats(
          invalidBytes, module.Codec.Webp, module.Subsampling.Default,
          /*effort=*/ 0, /*quality=*/ 90);
    }).toThrowError(/Failed to decode image/);
  });
});
