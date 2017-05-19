import xml.etree.ElementTree as ET


def restart_power_hal(target):
  """Kill power HAL service so it can pick up new values in powerhint.xml."""
  target.execute('pkill android\.hardware\.power')


def set_touch_param(target, opcode, new_val):
  """Set a new value for the touch hint parameter with the specified opcode."""
  hinttype = '0x1A00'

  # Get current powerhint.xml file
  device_path = '/vendor/etc/powerhint.xml'
  host_path = '/tmp/powerhint.xml'
  target.pull(device_path, host_path)

  # Replace current parameter value
  tree = ET.parse(host_path)
  xpath = './Hint[@type="{}"]/Resource[@opcode="{}"]'.format(hinttype, opcode)
  tree.findall(xpath)[0].set('value', '{:#x}'.format(new_val))

  # Write new powerhint.xml file to device
  tree.write(host_path)
  target.push(host_path, device_path)

  # Restart power HAL to pick up new value
  restart_power_hal(target)


def set_touchboost(target, boost=50):
  """Change the top-app schedtune.boost value to use after touch events."""
  opcode = '0x42C18000'
  if boost < 0:
    boost = 100-boost
  set_touch_param(target, opcode, boost)


def set_min_freq(target, cluster, freq=1100):
  """Change the CPU cluster min frequency (in Mhz) to use after touch events."""
  opcode = '0x40800000' if cluster == 'big' else '0x40800100'
  set_touch_param(target, opcode, freq)


def set_cpubw_hysteresis(target, enable=False):
  """Set whether to leave CPUBW hysteresis enabled after touch events."""
  opcode = '0x4180C000'
  enable_num = 1 if enable else 0
  set_touch_param(target, opcode, enable_num)


def set_cpubw_min_freq(target, freq=51):
  """Set CPUBW min freq used after touch events. See mapping in msm8998.dtsi."""
  opcode = '0x41800000'
  set_touch_param(target, opcode, freq)
