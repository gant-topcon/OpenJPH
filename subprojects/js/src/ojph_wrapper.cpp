/****************************************************************************/
// This software is released under the 2-Clause BSD license, included
// below.
//
// Copyright (c) 2019, Aous Naman
// Copyright (c) 2019, Kakadu Software Pty Ltd, Australia
// Copyright (c) 2019, The University of New South Wales, Australia
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/****************************************************************************/
// This file is part of the OpenJPH software implementation.
// File: ojph_wrapper.cpp
// Author: Aous Naman
// Date: 22 October 2019
/****************************************************************************/

#include <cstdint>
#include <cstdio>
#include <exception>
#include <emscripten.h>
#include <iterator>
#include <new>
#include <sys/types.h>

#include "ojph_arch.h"
#include "ojph_defs.h"
#include "ojph_file.h"
#include "ojph_mem.h"
#include "ojph_params.h"
#include "ojph_codestream.h"

//////////////////////////////////////////////////////////////////////////////
struct j2k_struct
{
  ojph::codestream codestream;
  ojph::mem_infile mem_file;
};

//////////////////////////////////////////////////////////////////////////////
j2k_struct* cpp_create_j2c_data(void)
{
  return new j2k_struct;
}

//////////////////////////////////////////////////////////////////////////////
void cpp_init_j2c_data(j2k_struct *j2c, const uint8_t *data, size_t size)
{
  try {
    j2c->mem_file.open(data, size);
    j2c->codestream.read_headers(&j2c->mem_file);
  }
  catch (const std::exception& e)
  {
    const char *p = e.what();
    if (strncmp(p, "ojph error", 10) != 0)
      printf("%s\n", p);
  }
}

//////////////////////////////////////////////////////////////////////////////
void cpp_parse_j2c_data(j2k_struct *j2c)
{
  try {
    j2c->codestream.set_planar(false);
    j2c->codestream.create();
  }
  catch (const std::exception& e)
  {
    const char *p = e.what();
    if (strncmp(p, "ojph error", 10) != 0)
      printf("%s\n", p);
  }
}

//////////////////////////////////////////////////////////////////////////////
void cpp_release_j2c_data(j2k_struct* j2c)
{
  delete j2c;
}

//////////////////////////////////////////////////////////////////////////////
signed int* cpp_pull_j2c_line(j2k_struct* j2c)
{
  try {
    ojph::ui32 comp_num;
    ojph::line_buf *line = j2c->codestream.pull(comp_num);
    return line->i32;
  }
  catch (const std::exception& e)
  {
    const char *p = e.what();
    if (strncmp(p, "ojph error", 10) != 0)
      printf("%s\n", p);
  }
  return NULL;
}

//////////////////////////////////////////////////////////////////////////////
void cpp_restrict_input_resolution(j2k_struct* j2c, 
                                   int skipped_res_for_read, 
                                   int skipped_res_for_recon)
{
  j2c->codestream.restrict_input_resolution(skipped_res_for_read, 
                                            skipped_res_for_recon);
}


// Allocates an rgba buffer into which a line of the given image can be decoded.
// 
// Also performs some sanity checks (like all components having the same bit depth
// and dimensions), so very useful to call once before decoding the lines to an
// rgba buffer.
//
// @returns nullptr on error or a valid pointer to hold the elements otherwise.
uint32_t* cpp_allocate_rgba_buffer(j2k_struct * const j2c) {
  if (!j2c) {
    printf("null-pointer given for j2c or buffer!\n");
    return nullptr;
  }
  // number of components per line
  const auto num_comps = j2c->codestream.access_siz().get_num_components();

  // NOTE: those fields are filled by querying component 0, but downstream
  // we make sure that this is the same for all components, which is what we
  // expect.
  
  const auto line_width_px = j2c->codestream.access_siz().get_recon_width(0);
  const auto bit_depth = j2c->codestream.access_siz().get_bit_depth(0);

  for (uint32_t c = 1; c < num_comps; ++c) {
    const auto cur_line_width_px = j2c->codestream.access_siz().get_recon_width(c);
    const auto cur_bit_depth = j2c->codestream.access_siz().get_bit_depth(c);
    if (cur_bit_depth != bit_depth || cur_line_width_px != line_width_px) {
      printf("different bit depth or line width for different coordinates!\n");
      return nullptr;
    }
  }

  return new (std::nothrow) uint32_t[line_width_px];
}

// free the rgba buffer (that must have been allocated by the corresponding
// function before).
void cpp_free_rgba_buffer(uint32_t * const buffer) {
  delete[] buffer;
}

// returns the number of bytes needed for an rgba buffer for the j2c handle.
// Also performs some sanity checks (like all components having the same bit depth
// and dimensions), so very useful to call once before decoding the lines to an
// rgba buffer.
//
// @returns 0 on error, a nonzero number of bytes needed for the RGBA buffer
// to hold the elements otherwise.
// 
uint32_t cpp_calc_rgba_buffer_len(j2k_struct * const j2c) {
  if (!j2c) {
    printf("null-pointer given for j2c or buffer!\n");
    return 0;
  }
  // number of components per line
  const auto num_comps = j2c->codestream.access_siz().get_num_components();

  // NOTE: those fields are filled by querying component 0, but downstream
  // we make sure that this is the same for all components, which is what we
  // expect.
  
  const auto line_width_px = j2c->codestream.access_siz().get_recon_width(0);
  const auto bit_depth = j2c->codestream.access_siz().get_bit_depth(0);

  for (uint32_t c = 1; c < num_comps; ++c) {
    const auto cur_line_width_px = j2c->codestream.access_siz().get_recon_width(c);
    const auto cur_bit_depth = j2c->codestream.access_siz().get_bit_depth(c);
    if (cur_bit_depth != bit_depth || cur_line_width_px != line_width_px) {
      printf("different bit depth or line width for different coordinates!\n");
      return 0;
    }
  }
  return 4*line_width_px;
}

void write_grayscale_to_rgba(int32_t const* const __restrict src_grayscale,
                             uint32_t * const __restrict rgba_out,
                             uint32_t pixel_count,
                             int32_t const half,
                             uint32_t const shift) {
    // NOTE1 (gant): hopefully this loop auto-vectorizes when SIMD is available,
    // but without using intrinsics directly, we can't be 100% sure. But this
    // is measurably faster than the previous javascript implementation because
    // we're not writing bytes individually
    // NOTE2 (gant): I also tried an if branch with a dedicated for loop for
    // 8bit image without the need for shifting, but this wasn't faster. In
    // fact, it was slower.
    for (uint32_t x = 0; x < pixel_count; ++x) {
      // the potentially shifted value clamped to 8 bits
      const auto val = static_cast<uint8_t>((src_grayscale[x] + half) >> shift);
      // we write the RGBA value into a single 32-bit int RGBA and write it at once.
      const uint32_t pixel = val | (val << 8) | (val << 16) | (0xFF << 24);
      rgba_out[x] = pixel;
    }
}

// Decode the next line of the image into the given byte buffer to RGBA format.
//
// The buffer must be non-null and must have been allocated with the `allocate rgba buffer`
// function using the same struct pointer.
//
// @returns 0 if everything went fine, nonzero otherwise. In case of nonzero
// return, the elements inside the buffer should be considered invalid and must
// not be read.
int cpp_decode_next_line_into_rgba_buffer(j2k_struct* const j2c, uint32_t * const buffer) {
  if (!j2c || !buffer) {
    printf("null-pointer given for j2c or buffer\n");
    return -1;
  }

  // number of components per line
  const auto num_comps = j2c->codestream.access_siz().get_num_components();

  // NOTE(gant): this is only the depth of component 0, but we assume they are the same
  // for all components. The user should have requested the line buffer size
  // with the `get_rgba_buffer_len` function, which does check that this is the
  // case. So by checking only component 0 here we strike a balance between
  // safety and efficiency.
  const auto line_width_px = j2c->codestream.access_siz().get_recon_width(0);
  const auto bit_depth = j2c->codestream.access_siz().get_bit_depth(0);

  // NOTE(gant): these calculations are used to shift a value that is potentially
  // in a bit-range > 8 into an 8 bit range. Taken from the original code in
  // the index.html in the OpenJPH javascript/wasm subproject.
  const auto shift = (bit_depth >= 8 ? bit_depth - 8 : 0);
  const auto half = (bit_depth > 8 ? (1 << (shift - 1)) : 0);

  if (num_comps == 1) {
    // we can ignore the component number here, because there's only
    // one component anyways.
    ojph::ui32 comp_num{};
    ojph::line_buf* const line = j2c->codestream.pull(comp_num);
    (void)(comp_num);

    int32_t const* const src = line->i32;
    
    write_grayscale_to_rgba(src, buffer, line_width_px, half, shift);
  } else if (num_comps == 3) {
    // the values are pulled for the components individually and components
    // 0 = R, 1 = G, 2 = B, if I understand correctly.

    auto const set_byte = [](uint32_t bytes4, uint8_t value, uint32_t byte_idx){
      // this is some bit-twiddling to set only one byte inside a 4-byte
      // integer to a certain value.
      uint32_t mask = ~(0xFFu << (8 * byte_idx));
      return (bytes4 & mask) | ((uint32_t)value << (8 * byte_idx));
    };

    for (uint32_t c = 0; c < num_comps; ++c) {
      ojph::ui32 comp_num{};
      ojph::line_buf* const line = j2c->codestream.pull(comp_num);
      // NOTE(gant): for added safety we could compare that comp_num == c, but
      // this might be excessive...
      (void)(comp_num);
      int32_t const* const src = line->i32;
      for (uint32_t x = 0; x < line_width_px; ++x) {
        const auto val = static_cast<uint8_t>((src[x] + half) >> shift);
        buffer[x] = set_byte(buffer[x], val, c);
      }
    }

    // now set the alpha channel to fully opaque
    for (uint32_t x = 0; x < line_width_px; ++x) {
      buffer[x] = set_byte(buffer[x], 255, 3);
    }
  } else {
    printf("Unsupported number of components (%d)\n",num_comps);
    return -1;
  }
  return 0;
}
  

//////////////////////////////////////////////////////////////////////////////
extern "C"
{
  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  j2k_struct* create_j2c_data(void)
  {
    return cpp_create_j2c_data();
  }
  
  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  void init_j2c_data(j2k_struct *j2c, const uint8_t *data, size_t size)
  {
    cpp_init_j2c_data(j2c, data, size);
  }
  
  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  int get_j2c_width(j2k_struct* j2c, int comp_num)
  {
    ojph::param_siz siz = j2c->codestream.access_siz();
    return siz.get_recon_width(comp_num);
  }

  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  int get_j2c_height(j2k_struct* j2c, int comp_num)
  {
    ojph::param_siz siz = j2c->codestream.access_siz();
    return siz.get_recon_height(comp_num);
  }
  
  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  int get_j2c_bit_depth(j2k_struct* j2c, int comp_num)
  {
    ojph::param_siz siz = j2c->codestream.access_siz();
    if (comp_num >= 0 && comp_num < siz.get_num_components())
      return siz.get_bit_depth(comp_num);
    else
      return -1;
  }
  
  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  int get_j2c_is_signed(j2k_struct* j2c, int comp_num)
  {
    ojph::param_siz siz = j2c->codestream.access_siz();
    if (comp_num >= 0 && comp_num < siz.get_num_components())
      return siz.is_signed(comp_num) ? 1 : 0;
    else
      return -1;
  }
  
  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  int get_j2c_num_components(j2k_struct* j2c)
  {
    ojph::param_siz siz = j2c->codestream.access_siz();
    return siz.get_num_components();
  }

  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  int get_j2c_downsampling_x(j2k_struct* j2c, int comp_num)
  {
    ojph::param_siz siz = j2c->codestream.access_siz();
    if (comp_num >= 0 && comp_num < siz.get_num_components())
    { return siz.get_downsampling(comp_num).x; }
    else
    { return -1; }
  }

  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  int get_j2c_downsampling_y(j2k_struct* j2c, int comp_num)
  {
    ojph::param_siz siz = j2c->codestream.access_siz();
    if (comp_num >= 0 && comp_num < siz.get_num_components())
    { return siz.get_downsampling(comp_num).y; }
    else
    { return -1; }
  }
  
  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  void parse_j2c_data(j2k_struct *j2c)
  {
    cpp_parse_j2c_data(j2c);
  }

  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  void restrict_input_resolution(j2k_struct* j2c, 
                                 int skipped_res_for_read, 
                                 int skipped_res_for_recon)
  {
    cpp_restrict_input_resolution(j2c, skipped_res_for_read, 
                                  skipped_res_for_recon);
  }
  
  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  void enable_resilience(j2k_struct* j2c)
  {
    j2c->codestream.enable_resilience();
  }  
  
  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  signed int* pull_j2c_line(j2k_struct* j2c)
  {
    return cpp_pull_j2c_line(j2c);
  }
  
  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  void release_j2c_data(j2k_struct* j2c)
  {
    cpp_release_j2c_data(j2c);
  }

  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  uint32_t calc_rgba_buffer_len(j2k_struct * const j2c)
  {
      return cpp_calc_rgba_buffer_len(j2c);
  }

  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  int decode_next_line_into_rgba_buffer(j2k_struct* const j2c,
                                         uint32_t * const buffer)
  {
    return cpp_decode_next_line_into_rgba_buffer(j2c,buffer);
  }

  
  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  uint32_t* allocate_rgba_buffer(j2k_struct * const j2c) {
    return cpp_allocate_rgba_buffer(j2c);
  }
  
  ////////////////////////////////////////////////////////////////////////////
  EMSCRIPTEN_KEEPALIVE
  void free_rgba_buffer(uint32_t * const buffer) {
    return cpp_free_rgba_buffer(buffer);
  }
}

