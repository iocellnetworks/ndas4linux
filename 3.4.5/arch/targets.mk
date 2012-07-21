nxp-targets-path=$(nxp-root)/arch/targets
nxp-target-mks=$(wildcard $(nxp-targets-path)/*.mk)

include $(nxp-target-mks)
