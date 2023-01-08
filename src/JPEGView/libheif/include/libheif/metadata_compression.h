/*
 * HEIF codec.
 * Copyright (c) 2022 Dirk Farin <dirk.farin@gmail.com>
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
#ifndef LIBHEIF_METADATA_COMPRESSION_H
#define LIBHEIF_METADATA_COMPRESSION_H

#include <vector>
#include <cinttypes>

#if WITH_DEFLATE_HEADER_COMPRESSION
std::vector<uint8_t> deflate(const uint8_t* input, int size);

std::vector<uint8_t> inflate(const std::vector<uint8_t>&);
#endif

#endif //LIBHEIF_METADATA_COMPRESSION_H
