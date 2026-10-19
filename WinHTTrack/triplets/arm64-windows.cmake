set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

# Overrides the built-in triplet of the same name, so keep the lines above identical to it.
# webview2 is the one port taken statically, because its loader DLL is Microsoft's to sign.
if(PORT STREQUAL "webview2")
    set(VCPKG_LIBRARY_LINKAGE static)
endif()
