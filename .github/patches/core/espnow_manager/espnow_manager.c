- name: Copy ESP-NOW Manager

  run: |

    mkdir -p examples/factory_demo/main/core/espnow_manager

    cp -r patches/core/espnow_manager/* \
       examples/factory_demo/main/core/espnow_manager/
