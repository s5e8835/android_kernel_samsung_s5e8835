# SPDX-License-Identifier: GPL-2.0
dtbo-y += samsung/m35x/m35x_eur_open_w00_r00.dtbo
dtbo-y += samsung/m35x/m35x_eur_open_w00_r01.dtbo
dtbo-y += samsung/m35x/m35x_eur_open_w00_r02.dtbo
dtb-y += exynos/s5e8835.dtb
dtbo-y += exynos/board/erd/s5e8835-erd8835.dtbo

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
