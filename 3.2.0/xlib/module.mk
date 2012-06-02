_xlib_:=xlib
nxp-$(_xlib_)-path=xlib
nxp-$(_xlib_)-src=$(wildcard $(nxp-$(_xlib_)-path)/*.c)
nxp-$(_xlib_)-obj=$(nxp-$(_xlib_)-src:%.c=$(nxp-build)/%.o)
