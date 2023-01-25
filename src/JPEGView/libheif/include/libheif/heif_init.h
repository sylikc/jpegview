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


#ifndef LIBHEIF_HEIF_INIT_H
#define LIBHEIF_HEIF_INIT_H

namespace heif {
  // TODO: later, we might defer the default plugin initialization to when they are actually used for the first time.
  // That would prevent them from being initialized every time at program start, even when the application software uses heif_init() later on.

  // void implicit_plugin_registration();
}

#endif //LIBHEIF_HEIF_INIT_H
