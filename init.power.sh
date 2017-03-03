#!/system/bin/sh

################################################################################
# helper functions to allow Android init like script

function write() {
    echo -n $2 > $1
}

function copy() {
    cat $1 > $2
}

################################################################################

soc_revision=`cat /sys/devices/soc0/revision`
if [ "$soc_revision" == "1.0" ]; then
    # Disable retention low power modes
    write /sys/module/lpm_levels/system/pwr/cpu0/ret/idle_enabled N
    write /sys/module/lpm_levels/system/pwr/cpu1/ret/idle_enabled N
    write /sys/module/lpm_levels/system/pwr/cpu2/ret/idle_enabled N
    write /sys/module/lpm_levels/system/pwr/cpu3/ret/idle_enabled N
    write /sys/module/lpm_levels/system/perf/cpu4/ret/idle_enabled N
    write /sys/module/lpm_levels/system/perf/cpu5/ret/idle_enabled N
    write /sys/module/lpm_levels/system/perf/cpu6/ret/idle_enabled N
    write /sys/module/lpm_levels/system/perf/cpu7/ret/idle_enabled N
    # Enable all LPMs by default
    write /sys/module/lpm_levels/parameters/sleep_disabled N
fi

# disable thermal hotplug to switch governor
write /sys/module/msm_thermal/core_control/enabled 0

# bring back main cores CPU 0,4
write /sys/devices/system/cpu/cpu0/online 1
write /sys/devices/system/cpu/cpu4/online 1

# Setting b.L scheduler parameters
write /proc/sys/kernel/sched_migration_fixup 1
write /proc/sys/kernel/sched_upmigrate 99
write /proc/sys/kernel/sched_downmigrate 85
write /proc/sys/kernel/sched_freq_inc_notify 400000
write /proc/sys/kernel/sched_freq_dec_notify 400000
write /proc/sys/kernel/sched_spill_nr_run 5
write /proc/sys/kernel/sched_restrict_cluster_spill 1

# configure governor settings for little cluster
write /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor "sched"

# configure governor settings for big cluster
write /sys/devices/system/cpu/cpu4/cpufreq/scaling_governor "sched"

# re-enable thermal hotplug
write /sys/module/msm_thermal/core_control/enabled 1

# Enable bus-dcvs
for cpubw in /sys/class/devfreq/*qcom,cpubw*
do
    write $cpubw/governor "bw_hwmon"
    write $cpubw/polling_interval 50
    write $cpubw/min_freq 1525
    write $cpubw/bw_hwmon/mbps_zones "3143 5859 11863 13763"
    write $cpubw/bw_hwmon/sample_ms 4
    write $cpubw/bw_hwmon/io_percent 34
    write $cpubw/bw_hwmon/hist_memory 20
    write $cpubw/bw_hwmon/hyst_length 10
    write $cpubw/bw_hwmon/low_power_ceil_mbps 0
    write $cpubw/bw_hwmon/low_power_io_percent 34
    write $cpubw/bw_hwmon/low_power_delay 20
    write $cpubw/bw_hwmon/guard_band_mbps 0
    write $cpubw/bw_hwmon/up_scale 250
    write $cpubw/bw_hwmon/idle_mbps 1600
done

write /sys/class/devfreq/soc:qcom,mincpubw/governor "cpufreq"

