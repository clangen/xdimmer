# xdimmer 

a little curses-based app that allows you to quickly adjust monitor brightness in x windows.

it's a hack that just executes `xrandr` commands under the hood.

![xdimmer screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/xdimmer/screenshots/xdimmer01.png)

# building

1. `git clone https://github.com/clangen/xdimmer.git`
2. `cd xdimmer`
3. `git submodule update --init --recursive`
4. `cmake .`
5. `make`
6. `__output/xdimmer`
