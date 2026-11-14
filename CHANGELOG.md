# 1.1.0
  * Added application icons to the process table and tree, with an option to hide them
  * Process names now appear before PIDs by default, with the tree hierarchy beside names and icons
  * Process task and thread counts now reflect filters, with overall totals shown when items are hidden
  * Added Brazilian Portuguese (pt_BR) localization
  * Added the Delete keyboard shortcut to terminate selected processes in the table and tree
  * Extended truncated process names using matching command-line names while preserving custom process names
  * Fixed AMD SMI GPU temperatures incorrectly showing as 0 degrees Celsius
  * Fixed Qt 5 build compatibility for process identity hashing
  * Fixed Nix builds failing to find the translation compiler (lrelease)
  * Debian/Ubuntu packaging now preserves the existing changelog history
  * AppImages now bundle SVG icon support and avoid stripping failures with modern libraries
  * Updated process screenshots and documented installation from Nixpkgs

# 1.0.8
  * Added localization support based on the system locale for:
    * Simplified Chinese
    * Czech
  * Added shared, text/code and data/stack memory columns to the process table and tree
    * Existing saved column layouts are migrated safely to the new column schema
  * Improved consistency when switching between flat and tree process views
  * Performance side panel context menu now follows the configured group order
  * Added detection for guests running under Apple Virtualization
  * Packaging support for openSUSE distro

# 1.0.7
  * Performance side panel is now resizable
  * Implemented compressed memory support
  * Visual improvements of process tree
  * Refactored run new task

# 1.0.6
  * Memory view now shows compressed memory when zswap is used
  * Implemented Intel GPU backend

# 1.0.5
  * Perf -> Network:
      * Show all IPv6 addrs instead of just the first one
      * Implemented toggle between bytes vs bits for NICs
  * UI: minor visual fixes of services table
  * Implemented ability to reorder performance widgets in side panel
  * Expose more NVML metrics: freq and power usage
  * Support for AMD GPUs via AMDSMI

# 1.0.4
  * Services control (via d-bus / systemctl)
  * Option to display stats for each swap device separately
  * Dynamically load / unload swap devices as they are added / disabled
  * Notify user when they try to send signal to processes owned by different UID
  * Run new task option
  * Open terminal option
  * New icon
  * IO metrics for processes
  * Updated virtual GPU / CD driver blacklist

# 1.0.3
  * Performance improvements (app uses less RAM and CPU now)
  * Fixed various visual glitches (graphs now look better)
  * Fixed disk enumeration logic

# 1.0.2
  * Improved context menu (copy, confirmations)
  * Show if running as root in title
  * Implemented tree view in processes list
  * Persistent table header settings
  * Added 15s interval
  * Moved interval settings to context menus

# 1.0.1

  * New color scheme dialog allowing users to override colors
  * Improved support for light theme
  * Improved services tab (faster refresh and search)
  * Support for DRM GPUs
