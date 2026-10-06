# WinTaskMan

A Work-In-Progress program to bring the old-fashioned task manager to linux using Qt6 framework

![image](https://github.com/user-attachments/assets/26f1f2d2-aaf1-4653-84d0-976bb0cb169b)

#### Please Note
If you want to have the Aero theme as seen in the screenshot, take a look into [wackyideas/aerothemeplasma](https://gitgud.io/wackyideas/aerothemeplasma)

## Building
To build the program you need cmake and qt6 base. These should be available on all rolling release distros as well as on the latest Ubuntu. To build the program, simply run the `build.sh` script to streamline the process and compiled binary will be found from `build/` directory

When Qt Test is installed (it comes with qt6 base on Arch and Ubuntu), the unit tests are built too. Run them with `ctest --test-dir build`.

To install the dependencies on Arch:
```
sudo pacman -S cmake qt6-base
```

To install the dependencies on Ubuntu:
```
sudo apt install cmake qt6-base-dev
```

### What works
- Running applications being listed (wayland lists all current session apps, not just ones with windows open)
- Processes being listed
- User services being listed (systemd and openrc supported)
- Per process CPU usage
- Total CPU usage
- Total process count
- Performance tab in the Windows 7 style: CPU and memory meters and history graphs (total or per core, with kernel times), memory and system figures

### What is missing
- Network tab contents as a whole
- User tab contents as whole
- Control buttons from all tabs
- Menubar actions

### Known bugs
- Styling can be a bit wacky
- Service PIDs are not shown on systemd
