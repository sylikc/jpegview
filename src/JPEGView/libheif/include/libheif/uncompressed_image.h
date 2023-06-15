/*
 * HEIF codec.
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
 *
 * This file is part of libheif.
 *
 * libheif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libheif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libheif.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef LIBHEIF_UNCOMPRESSED_IMAGE_H
#define LIBHEIF_UNCOMPRESSED_IMAGE_H

#include "box.h"
#include "bitstream.h"
#include "pixelimage.h"
#include "file.h"
#include "context.h"

#include <string>
#include <vector>
#include <memory>


class Box_cmpd : public Box
{
public:
  Box_cmpd()
  {
    set_short_type(fourcc("cmpd"));
  }

  std::string dump(Indent&) const override;

  Error write(StreamWriter& writer) const override;

  struct Component
  {
    uint16_t component_type;
    std::string component_type_uri;
  };

  const std::vector<Component>& get_components() const { return m_components; }

protected:
  Error parse(BitstreamRange& range) override;

  std::vector<Component> m_components;
};

class Box_uncC : public FullBox
{
public:
  Box_uncC()
  {
    set_short_type(fourcc("uncC"));
  }

  std::string dump(Indent&) const override;

  bool get_headers(std::vector<uint8_t>* dest) const;

  Error write(StreamWriter& writer) const override;

  struct Component
  {
    uint16_t component_index;
    uint8_t component_bit_depth_minus_one;
    uint8_t component_format;
    uint8_t component_align_size;
  };

  const std::vector<Component>& get_components() const { return m_components; }

  uint8_t get_sampling_type() { return m_sampling_type; }

  uint8_t get_interleave_type() { return m_interleave_type; }

  uint8_t get_block_size() { return m_block_size; }

  bool is_components_little_endian() { return m_components_little_endian; }

  bool is_block_pad_lsb() { return m_block_pad_lsb; }

  bool is_block_little_endian() { return m_block_little_endian; }

  bool is_block_reversed() { return m_block_reversed; }

  bool is_pad_unknown() { return m_pad_unknown; }

  uint8_t get_pixel_size() { return m_pixel_size; }

  uint32_t get_row_align_size() { return m_row_align_size; }

  uint32_t get_tile_align_size() { return m_tile_align_size; }

  uint32_t get_number_of_tile_columns() { return m_num_tile_cols_minus_one + 1; }

  uint32_t get_number_of_tile_rows() { return m_num_tile_rows_minus_one + 1; }

protected:
  Error parse(BitstreamRange& range) override;

  uint32_t m_profile;

  std::vector<Component> m_components;
  uint8_t m_sampling_type;
  uint8_t m_interleave_type;
  uint8_t m_block_size;
  bool m_components_little_endian;
  bool m_block_pad_lsb;
  bool m_block_little_endian;
  bool m_block_reversed;
  bool m_pad_unknown;
  uint8_t m_pixel_size;
  uint32_t m_row_align_size;
  uint32_t m_tile_align_size;
  uint32_t m_num_tile_cols_minus_one;
  uint32_t m_num_tile_rows_minus_one;
};


class UncompressedImageCodec
{
public:
  static int get_luma_bits_per_pixel_from_configuration_unci(const HeifFile& heif_file, heif_item_id imageID);

  static Error decode_uncompressed_image(const std::shared_ptr<const HeifFile>& heif_file,
                                         heif_item_id ID,
                                         std::shared_ptr<HeifPixelImage>& img,
                                         uint32_t maximum_image_width_limit,
                                         uint32_t maximum_image_height_limit,
                                         const std::vector<uint8_t>& uncompressed_data);

  static Error encode_uncompressed_image(const std::shared_ptr<HeifFile>& heif_file,
                                         const std::shared_ptr<HeifPixelImage>& src_image,
                                         void* encoder_struct,
                                         const struct heif_encoding_options& options,
                                         std::shared_ptr<HeifContext::Image> out_image);
};

#endif //LIBHEIF_UNCOMPRESSED_IMAGE_H
