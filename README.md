# os-usb-viewer

An SDL GUI for viewing connected USB devices.

## Compile

1. Install dependencies: `sudo apt-get install libusb-1.0-0-dev libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev`
2. Open this directory in a terminal.
3. Run `make`.

## Run

After compiling, open this directory in a terminal, then run `sudo ./bin/usbviewer`. (If you'd rather not give the application full privileges, set udev rules for the devices you want available to access, then run without sudo.)

If running under a VM in VirtualBox, many devices (such as USB flash drives) will work without any special configuration. In order for some devices like mice and keyboards to appear, go to Settings > USB and add a blank filter with the blue circle button (leave all fields blank.) Such devices still may not appear until plugged in (potentially re-plugged if already plugged in.) Know that when doing so, VirtualBox will completely cede control of the devices to the guest OS: for example, if a mouse is replugged, the VirtualBox interface will not work and the mouse pointer will not render until the guest OS is shut down.

## Dependencies

- [libusb](https://github.com/libusb/libusb) (USB library)
- [SDL](https://github.com/libsdl-org/SDL) (GUI)
- [Inter](https://github.com/rsms/inter) (font)
- Based on [@tmarrinan's](https://github.com/tmarrinan) file explorer GUI
