set auto-solib-add on
set sysroot .
set solib-search-path symbols:lib/arm64-v8a
file app_process64
target remote localhost:8123
