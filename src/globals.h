/*
 * Tux Manager - Linux system monitor
 * Copyright (C) 2026 Petr Bena <petr@bena.rocks>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#define TUX_MANAGER_VERSION_STRING     "1.0.6"
#define TUX_MANAGER_PRODUCT_NAME       "TuxManager"

/// Number of historical samples kept per metric (max graph window: 15 min at 1 Hz).
#define TUX_MANAGER_HISTORY_SIZE       900
#define TUX_MANAGER_TASK_HISTORY       20

#define TUX_MANAGER_MIN_RATE           1024.0

#endif // GLOBALS_H
