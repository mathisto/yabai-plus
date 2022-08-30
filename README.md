# fyabai

Personal patches for yabai. Cherry-pick them to your own fork if you want to use them:

- Upgrade IPC from unix sockets to mach messages
- Add rounded borders and background blur
- Add animations for moving windows
- Allow zooming to fullscreen if previously zoomed to parent
- Allow only one child at a time to zoom to parent node
- Zoomed windows occlude windows below in the next-window-in-direction calculation
- Focus closest window on application termination
