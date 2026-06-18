# regenerate GDS with the lef_files fix (one-off command or ./def2gds-croc if you added the line):
klayout -zz -rd design_name=croc_chip -rd in_def=../openroad/out/croc.def \
  -rd in_files="../technology/gds/sg13cmos5l_stdcell.gds ../technology/gds/ez130_8t.gds ../technology/gds/sg13cmos5l_io.gds ../technology/gds/bondpad5l_70x70.gds ../technology/gds/RM_IHPSG13_1P_512x32_c2_bm_bist.gds" \
  -rd out_file=./out/croc.gds -rd tech_file=./.klayout/tech/sg13cmos5l.lyt \
  -rd layer_map='' -rd seal_file='' -rd config_file='' -rd lef_files='' \
  -rd allow_empty='RM_IHPSG13_(1P|2P)_BITKIT_16x2_(CORNER|EDGE_TB|LE_con_corner|LE_con_edge_lr|LE_con_tap_lr|POWER_ramtap|TAP|TAP_LR)$' \
  -rm .klayout/def2stream.py
# then DRC (single-thread, completes):
python3 /usr/pack/ihp-sg13-kgf/open_ihp_sg13cmos5l/sg13cmos5l_tech/v0.2/klayout/tech/drc/run_drc.py \
  --path=./out/croc.gds --topcell=croc_chip --run_dir ./drc/out \
  --no_recommended --no_density --antenna --mp 1 --density_thr 1
