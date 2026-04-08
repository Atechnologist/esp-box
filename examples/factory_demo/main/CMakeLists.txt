- name: Fix CMakeLists
  shell: bash
  run: |
    FILE=esp-box/examples/factory_demo/main/CMakeLists.txt
    grep -q web_server.c $FILE || sed -i 's/SRCS "main.c"/SRCS "main.c" "web_server.c"/' $FILE
