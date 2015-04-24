/*
 * MEI functionalities for threft deterrent lock driver.
 *
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 */

#ifndef TD_LOCK_DRIVER_MEI_H
#define TD_LOCK_DRIVER_MEI_H

bool td_mei_is_td_supported(bool *is_supported);
bool td_mei_get_sc(u32 *tick);

#endif /* TD_LOCK_DRIVER_MEI_H */

