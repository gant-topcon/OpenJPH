// HTJ2K/JPEG2000 Decoder Module
// High-level JavaScript wrapper for OpenJPH WASM

import { simd } from 'https://unpkg.com/wasm-feature-detect?module';

// Initialize WASM module at import time (top-level await)
const simdSupported = await simd();
const openjph = simdSupported 
  ? await import('./libopenjph_simd.js') 
  : await import('./libopenjph.js');
const wasmModule = await openjph.default();

// Wrap C functions once at initialization
const wasmFunctions = {
  create_j2c_data: wasmModule.cwrap('create_j2c_data', 'number'),
  init_j2c_data: wasmModule.cwrap('init_j2c_data', 'void', ['number', 'number', 'number']),
  parse_j2c_data: wasmModule.cwrap('parse_j2c_data', 'void', ['number']),
  get_j2c_width: wasmModule.cwrap('get_j2c_width', 'number', ['number', 'number']),
  get_j2c_height: wasmModule.cwrap('get_j2c_height', 'number', ['number', 'number']),
  get_j2c_num_components: wasmModule.cwrap('get_j2c_num_components', 'number', ['number']),
  get_j2c_bit_depth: wasmModule.cwrap('get_j2c_bit_depth', 'number', ['number', 'number']),
  restrict_input_resolution: wasmModule.cwrap('restrict_input_resolution', 'void', ['number', 'number', 'number']),
  enable_resilience: wasmModule.cwrap('enable_resilience', 'void', ['number']),
  pull_j2c_line: wasmModule.cwrap('pull_j2c_line', 'number', ['number']),
  release_j2c_data: wasmModule.cwrap('release_j2c_data', 'void', ['number']),
  calc_rgba_buffer_len: wasmModule.cwrap('calc_rgba_buffer_len', 'number', ['number']),
  decode_next_line_into_rgba_buffer: wasmModule.cwrap('decode_next_line_into_rgba_buffer', 'number', ['number', 'number', 'number'])
};

/**
 * Decode HTJ2K/JPEG2000 data to ImageData
 * @param {ArrayBuffer|Uint8Array} encodedData - The compressed J2K data
 * @param {Object} options - Decoding options
 * @param {number} options.skipResForData - Skip resolution levels for data reading (0 = full res)
 * @param {number} options.skipResForRecon - Skip resolution levels for reconstruction (0 = full res)
 * @param {boolean} options.enableResilience - Enable error resilience (default: true)
 * @returns {ImageData} - Decoded image ready for canvas rendering
 */
export function decodeHTJ2K(encodedData, options = {}) {
  const Module = wasmModule;
  
  const {
    skipResForData = 0,
    skipResForRecon = 0,
    enableResilience = true
  } = options;
  
  // Convert to Uint8Array if needed
  const dataArray = encodedData instanceof Uint8Array 
    ? encodedData 
    : new Uint8Array(encodedData);
  
  // Allocate input buffer in WASM
  const buffer = Module._malloc(dataArray.length);
  let j2c = null;
  let rgbaBuffer = null;

  if (buffer === 0) {
    throw new Error(`error allocating WASM memory`);
  }

  try {
    // Copy data to WASM memory
    Module.HEAPU8.set(dataArray, buffer);
    
    // Create and initialize decoder
    j2c = wasmFunctions.create_j2c_data();

    if (j2c === 0) {
      throw new Error(`error creating j2c data in WASM module`);
    }
      
    if (enableResilience) {
      wasmFunctions.enable_resilience(j2c);
    }

    wasmFunctions.init_j2c_data(j2c, buffer, dataArray.length);
    wasmFunctions.restrict_input_resolution(j2c, skipResForData, skipResForRecon);
    
    const rgbaBufferLen = wasmFunctions.calc_rgba_buffer_len(j2c) | 0;
    if (rgbaBufferLen === 0) {
      throw new Error(`error calculating rgba buffer length (indicates inconsistencies in image dims or depth)`);
    }

    rgbaBuffer = Module._malloc(rgbaBufferLen);
    if (rgbaBuffer === 0) {
      throw new Error(`error allocating RGBA buffer in WASM module`);
    }


    // Get image metadata
    const width = wasmFunctions.get_j2c_width(j2c, 0) | 0;
    const height = wasmFunctions.get_j2c_height(j2c, 0) | 0;
    
    // Parse codestream
    wasmFunctions.parse_j2c_data(j2c);
    
    // Create output ImageData
    const imageData = new ImageData(width, height);
    const dst = imageData.data;
    
    const heap8 = Module.HEAPU8;

    for (let y = 0; y < height; y++) {
      const offset = y*width*4;
      if (wasmFunctions.decode_next_line_into_rgba_buffer(j2c, rgbaBuffer, rgbaBufferLen) !==0) {
        throw new Error(`error decoding line ${y}`);
      }
      dst.set(heap8.subarray(rgbaBuffer, rgbaBuffer+rgbaBufferLen),offset);
    }
   
    return imageData;
  } finally {
    // Always cleanup, even if error occurred
    if (buffer) {
      Module._free(buffer);
    }
    if (rgbaBuffer) {
      Module._free(rgbaBuffer);
    }
    if (j2c) {
      wasmFunctions.release_j2c_data(j2c);
    }
  }
}

/**
 * Check if WebAssembly SIMD is supported
 * @returns {boolean} - True if SIMD is supported
 */
export function isSIMDSupported() {
  return simdSupported;
}
