# Future
  * Memory view now shows compressed memory when zswap is used
  * Implemented Intel GPU backend
  * Performance side panel is now resizable

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
